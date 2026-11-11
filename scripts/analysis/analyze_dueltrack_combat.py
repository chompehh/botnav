#!/usr/bin/env python3
"""Summarize throw trajectories and human counter timing from Dueltrack CSV exports.

Example:
  python3 scripts/analysis/analyze_dueltrack_combat.py \
    --events dueltrack_events_part44.csv dueltrack_events_part45.csv \
    --geometry dueltrack_geometry_part8.csv \
    --sessions dueltrack_sessions.csv --participants dueltrack_participants.csv

Use bounded event parts and their matching geometry/metadata exports first. Optional
telemetry columns may be absent; unavailable measurements stay unknown, while malformed
numeric values fail closed. Output uses numeric summary IDs only; participant keys and
display names are never printed.
"""

import argparse
import csv
import hashlib
import math
from collections import Counter, defaultdict
from pathlib import Path

BOT = "0"
HUMAN = {"1", "2"}
BUTTON_ATTACK = 1
BUTTON_ALT_ATTACK = 128
BUTTON_FORCEPOWER = 512

EVENT_COLUMNS = ("record_type", "source_context", "parent_id", "session_id", "capture_revision",
	"participant_key", "participant_kind", "event_type", "event_index", "rel_time")
GEOMETRY_COLUMNS = ("record_type", "source_context", "parent_id", "participant_key", "event_index",
	"capture_revision", "rel_time")
SESSION_COLUMNS = ("record_type", "source_context", "record_id", "type", "duration", "capture_revision")
PARTICIPANT_COLUMNS = ("record_type", "source_context", "summary_id", "participant_key",
	"participant_kind", "capture_revision")
EVENT_NUMERIC_OPTIONAL = ("amount", "state", "range_bucket", "buttons", "yaw_delta", "self_hp",
	"self_armor", "self_force", "enemy_hp", "enemy_armor", "enemy_force", "radial_speed",
	"yaw_sweep", "attack_elapsed_ms", "throw_yaw_offset", "forwardmove", "rightmove", "upmove",
	"saber_stance", "grounded", "controller_owns_inputs", "controller_enabled", "controller_fanbias",
	"controller_candidate", "controller_mistakebias", "controller_skill", "span_id", "span_kind",
	"span_outcome", "command_time", "command_msec", "command_source", "self_pitch", "enemy_pitch",
	"target_yaw_error", "target_pitch_error", "defense", "enemy_defense", "recovery",
	"enemy_recovery", "enemy_grounded", "saber_position_valid", "saber_x", "saber_y", "saber_z",
	"aim_x", "aim_y", "aim_z", "learned_stimulus", "learned_response", "learned_follow",
	"learned_bonus", "learned_context", "saber_defense_state", "enemy_saber_defense_state",
	"saber_blocked", "enemy_saber_blocked", "aim_mask", "aim_endpoint_valid", "self_footing",
	"enemy_footing", "torso_timer", "torso_anim")
GEOMETRY_NUMERIC_OPTIONAL = ("self_x", "self_y", "self_z", "enemy_x", "enemy_y", "enemy_z",
	"self_vx", "self_vy", "self_vz", "enemy_vx", "enemy_vy", "enemy_vz", "self_yaw", "enemy_yaw")


def file_digest(path):
	digest = hashlib.sha256()
	with Path(path).open("rb") as src:
		for chunk in iter(lambda: src.read(1024 * 1024), b""):
			digest.update(chunk)
	return digest.hexdigest()


def unique_paths(paths):
	seen = set()
	unique = []
	duplicates = 0
	for path in paths:
		digest = file_digest(path)
		if digest in seen:
			duplicates += 1
		else:
			seen.add(digest)
			unique.append(path)
	return unique, duplicates


def read_csv(path, keep=None, required=()):
	with Path(path).open(encoding="utf-8-sig", errors="replace", newline="") as src:
		reader = csv.DictReader(src)
		if not reader.fieldnames:
			raise ValueError(f"{path}: missing CSV header")
		missing = sorted(set(required) - set(reader.fieldnames))
		if missing:
			raise ValueError(f"{path}: unsupported schema; missing columns: {', '.join(missing)}")
		if keep is None:
			return [dict(row) for row in reader]
		return [dict(row) for row in reader if keep(row)]


def deduplicate(rows, identity_fields, label):
	seen = {}
	result = []
	duplicates = 0
	for row in rows:
		identity = tuple(row.get(field, "") for field in identity_fields)
		payload = tuple(sorted(row.items()))
		if identity in seen:
			if seen[identity] != payload:
				raise ValueError(f"{label}: conflicting duplicate identity; refusing to merge rows")
			duplicates += 1
			continue
		seen[identity] = payload
		result.append(row)
	return result, duplicates


def require_columns(rows, columns, label):
	if not rows:
		raise ValueError(f"{label}: no rows")
	missing = sorted(set(columns) - set(rows[0]))
	if missing:
		raise ValueError(f"{label}: unsupported schema; missing columns: {', '.join(missing)}")


def n(row, key):
	value = row.get(key)
	if value is None or not value.strip():
		return None
	try:
		number = float(value)
	except (TypeError, ValueError) as error:
		raise ValueError(f"malformed numeric field {key!r}: {value!r}") from error
	if not math.isfinite(number):
		raise ValueError(f"non-finite numeric field {key!r}: {value!r}")
	return number


def required_n(row, key, label):
	value = n(row, key)
	if value is None:
		raise ValueError(f"{label}: missing numeric field {key!r}")
	return value


def continuous_press_hold(rows, release_time):
	candidates = [r for r in rows if r["event_type"] == "input_start"
		and r.get("note") in ("attack", "alt") and n(r, "rel_time") is not None
		and n(r, "rel_time") <= release_time]
	command_rows = [r for r in rows if r["event_type"] == "command_sample"
		and n(r, "rel_time") is not None]
	for press in reversed(sorted(candidates, key=lambda r: n(r, "rel_time"))):
		start_time = n(press, "rel_time")
		mask = BUTTON_ALT_ATTACK if press["note"] == "alt" else BUTTON_ATTACK
		samples = sorted((r for r in command_rows if start_time <= n(r, "rel_time") <= release_time),
			key=lambda r: n(r, "rel_time"))
		if len(samples) < 2:
			continue
		times = [n(r, "rel_time") for r in samples]
		if times[0] - start_time > 100 or release_time - times[-1] > 100:
			continue
		if any(later - earlier > 100 for earlier, later in zip(times, times[1:])):
			continue
		buttons = [n(r, "buttons") for r in samples]
		if any(value is None for value in buttons):
			continue
		if all(int(value) & mask for value in buttons):
			return {"button": press["note"], "duration": release_time - start_time, "samples": len(samples)}
	return None


def button_runs(rows, bit, gap_limit=100):
	ordered = sorted(rows, key=lambda r: n(r, "rel_time"))
	runs = []
	active = []
	for row in ordered:
		t = n(row, "rel_time")
		buttons = n(row, "buttons")
		if buttons is None:
			if len(active) > 1:
				runs.append((n(active[0], "rel_time"), n(active[-1], "rel_time"),
					n(active[-1], "rel_time") - n(active[0], "rel_time"), len(active)))
			active = []
			continue
		held = bool(int(buttons) & bit)
		if not held:
			if len(active) > 1:
				runs.append((n(active[0], "rel_time"), n(active[-1], "rel_time"),
					n(active[-1], "rel_time") - n(active[0], "rel_time"), len(active)))
			active = []
		elif active and t - n(active[-1], "rel_time") <= gap_limit:
			active.append(row)
		else:
			if len(active) > 1:
				runs.append((n(active[0], "rel_time"), n(active[-1], "rel_time"),
					n(active[-1], "rel_time") - n(active[0], "rel_time"), len(active)))
			active = [row]
	if len(active) > 1:
		runs.append((n(active[0], "rel_time"), n(active[-1], "rel_time"),
			n(active[-1], "rel_time") - n(active[0], "rel_time"), len(active)))
	return runs


def next_paired_end(start, starts, ends):
	start_index = int(n(start, "event_index"))
	next_start = next((int(n(row, "event_index")) for row in starts
		if int(n(row, "event_index")) > start_index), None)
	return next((row for row in ends if int(n(row, "event_index")) > start_index
		and (next_start is None or int(n(row, "event_index")) < next_start)), None)


def angle_delta(a, b):
	return (a - b + 180.0) % 360.0 - 180.0


def flight_features(flight, geometry, release_time, duration):
	ordered = sorted(flight, key=lambda row: n(row, "rel_time"))
	prefix, first_off, previous = None, None, release_time
	for row in ordered:
		when, buttons = n(row, "rel_time"), n(row, "buttons")
		if when - previous > 100 or buttons is None:
			break
		if not int(buttons) & BUTTON_ALT_ATTACK:
			first_off = when - release_time
			prefix = prefix if prefix is not None else 0
			break
		prefix = when - release_time
		previous = when
	full_buttons = bool(ordered) and duration is not None and (
		n(ordered[0], "rel_time") - release_time <= 100 and
		release_time + duration - n(ordered[-1], "rel_time") <= 100 and
		all(n(row, "buttons") is not None for row in ordered) and
		all(n(b, "rel_time") - n(a, "rel_time") <= 100 for a, b in zip(ordered, ordered[1:])))
	early = [n(row, "command_pitch") for row in ordered
		if n(row, "rel_time") - release_time <= 250 and n(row, "command_pitch") is not None]
	known_buttons = [n(row, "buttons") for row in ordered if n(row, "buttons") is not None]
	alt_runs = button_runs(ordered, BUTTON_ALT_ATTACK)
	grows = [g for g in geometry if all(n(g, field) is not None
		for field in ("self_x", "self_y", "enemy_x", "enemy_y"))]
	errors, rear, above, isolated_at, reacquired = [], [], [], None, []
	for row in ordered:
		if not grows:
			break
		when = n(row, "rel_time")
		near = min(grows, key=lambda g: abs(n(g, "rel_time") - when))
		if abs(n(near, "rel_time") - when) > 60:
			continue
		yaw = n(row, "command_yaw")
		if yaw is not None:
			bearing = math.degrees(math.atan2(n(near, "enemy_y") - n(near, "self_y"),
				n(near, "enemy_x") - n(near, "self_x")))
			error = abs(angle_delta(yaw, bearing))
			errors.append(error)
			if error > 150 and isolated_at is None:
				isolated_at = when
			elif error < 45 and isolated_at is not None:
				reacquired.append(when - isolated_at)
				isolated_at = None
		if row.get("saber_position_valid") != "1":
			continue
		values = [n(row, "saber_" + axis) for axis in "xyz"]
		enemy_yaw, enemy_z = n(near, "enemy_yaw"), n(near, "enemy_z")
		if any(value is None for value in values):
			continue
		if enemy_yaw is not None:
			radians = math.radians(enemy_yaw)
			rear.append((values[0] - n(near, "enemy_x")) * math.cos(radians) +
				(values[1] - n(near, "enemy_y")) * math.sin(radians) < -24)
		if enemy_z is not None:
			above.append(values[2] - enemy_z > 96)
	return {"early_pitch_above_horizon": any(pitch < -5 for pitch in early) if early else None,
		"alt_observed_prefix_ms": prefix, "first_observed_alt_off_ms": first_off,
		"alt_on_samples": sum(bool(int(value) & BUTTON_ALT_ATTACK) for value in known_buttons),
		"known_button_samples": len(known_buttons),
		"longest_observed_alt_run_ms": max((run[2] for run in alt_runs), default=0) if known_buttons else None,
		"full_button_window": full_buttons,
		"max_view_bearing_error": round(max(errors), 1) if errors else None,
		"isolation_reacquire_ms": reacquired,
		"saber_rear_24u_seen": any(rear) if rear else None,
		"saber_above_96u_seen": any(above) if above else None}


def median(values):
	values = sorted(values)
	if not values:
		return None
	m = len(values) // 2
	return values[m] if len(values) % 2 else (values[m - 1] + values[m]) / 2.0


def load(args):
	sessions = read_csv(args.sessions, required=SESSION_COLUMNS)
	participants = read_csv(args.participants, required=PARTICIPANT_COLUMNS)
	sessions = [r for r in sessions
		if r.get("record_type") == "duel_summary" and r.get("source_context") == "duel"]
	participants = [r for r in participants
		if r.get("record_type") == "duel_participant" and r.get("source_context") == "duel"]
	for row in sessions:
		required_n(row, "type", "sessions")
		required_n(row, "duration", "sessions")
	sessions, duplicate_rows = deduplicate(sessions, ("record_id", "capture_revision"), "sessions")
	participants, count = deduplicate(participants,
		("summary_id", "participant_key", "capture_revision"), "participants")
	duplicate_rows += count
	session_by_key = {(r["record_id"], r["capture_revision"]): r for r in sessions}
	participant_kinds = defaultdict(set)
	for row in participants:
		participant_kinds[(row["summary_id"], row["capture_revision"])].add(row["participant_kind"])
	selected = {key for key, kinds in participant_kinds.items()
		if key in session_by_key and BOT in kinds and kinds.intersection(HUMAN)}
	if args.summary_ids:
		selected = {key for key in selected if key[0] in args.summary_ids}
	if args.revision:
		selected = {key for key in selected if key[1] == args.revision}
	if not selected:
		raise ValueError("no bot-v-human session/revision rows match the requested scope")
	selected_ids = {key[0] for key in selected}
	selected_revisions = {key[1] for key in selected}
	# Read only rows belonging to selected bot-v-human duels from each part.
	event_paths, duplicate_files = unique_paths(args.events)
	events = [row for path in event_paths for row in read_csv(path,
		lambda r: r.get("record_type") == "duel_event" and r.get("source_context") == "duel"
		and r.get("parent_id") in selected_ids and r.get("capture_revision") in selected_revisions
		and (not args.session_id or r.get("session_id") == args.session_id), EVENT_COLUMNS)]
	require_columns(events, EVENT_COLUMNS, "events")
	for row in events:
		required_n(row, "event_index", "events")
		required_n(row, "rel_time", "events")
		required_n(row, "participant_kind", "events")
		for field in EVENT_NUMERIC_OPTIONAL:
			n(row, field)
	events, count = deduplicate(events,
		("session_id", "parent_id", "participant_key", "event_index", "capture_revision"), "events")
	duplicate_rows += count
	session_ids_by_meta = defaultdict(set)
	for row in events:
		meta_key = (row["parent_id"], row["capture_revision"])
		session_ids_by_meta[meta_key].add(row["session_id"])
		if meta_key not in selected:
			continue
		if not row["session_id"]:
			raise ValueError("selected event row has no session_id; refusing summary_id-only grouping")
	ambiguous = [key for key, values in session_ids_by_meta.items() if key in selected and len(values) > 1]
	if ambiguous:
		raise ValueError("summary_id/capture_revision maps to multiple session_id values; "
			"scope input with --session-id before joining metadata/geometry")
	if args.session_id and not events:
		raise ValueError("no duel event rows match the requested session_id")
	cohort_by_meta = {}
	for meta_key, values in session_ids_by_meta.items():
		if meta_key in selected and values:
			cohort_by_meta[meta_key] = (next(iter(values)), meta_key[0], meta_key[1])
	geometry_paths, geometry_duplicate_files = unique_paths([args.geometry])
	geometry = read_csv(geometry_paths[0],
		lambda r: r.get("record_type") == "duel_geometry" and r.get("source_context") == "duel"
		and (r.get("parent_id"), r.get("capture_revision")) in cohort_by_meta, GEOMETRY_COLUMNS)
	geometry, count = deduplicate(geometry,
		("parent_id", "participant_key", "event_index", "capture_revision"), "geometry")
	duplicate_rows += count
	require_columns(geometry, GEOMETRY_COLUMNS, "geometry")
	for row in geometry:
		required_n(row, "event_index", "geometry")
		required_n(row, "rel_time", "geometry")
		for field in GEOMETRY_NUMERIC_OPTIONAL:
			n(row, field)
	by_group_participant = defaultdict(list)
	for row in participants:
		meta_key = (row["summary_id"], row["capture_revision"])
		if meta_key in cohort_by_meta:
			by_group_participant[cohort_by_meta[meta_key]].append(row)
	events = [dict(row, _group=cohort_by_meta[(row["parent_id"], row["capture_revision"])])
		for row in events if (row["parent_id"], row["capture_revision"]) in cohort_by_meta]
	geometry = [dict(row, _group=cohort_by_meta[(row["parent_id"], row["capture_revision"])])
		for row in geometry if (row["parent_id"], row["capture_revision"]) in cohort_by_meta]
	sessions_by_group = {cohort_by_meta[key]: session_by_key[key] for key in cohort_by_meta}
	info = {"duplicate_files": duplicate_files + geometry_duplicate_files, "duplicate_rows": duplicate_rows}
	return events, geometry, sessions_by_group, by_group_participant, info


def output(query, rows):
	print("\nQUERY: " + query)
	for row in rows:
		print("  " + " | ".join(str(value) for value in row))


def report(args):
	events, geometry, sessions, by_participant, info = load(args)
	ids = sorted({row["_group"] for row in events}, key=lambda key: (int(key[1]), key[2], key[0]))
	display = lambda key: (key[1], key[2])
	if args.summary_ids:
		present = {sid[1] for sid in ids}
		missing = set(args.summary_ids) - present
		if missing:
			raise ValueError("requested summary ID(s) are not in the selected session/revision cohort: " +
				", ".join(sorted(missing, key=int)))
	if not ids:
		raise ValueError("no bot-v-human duels found in the selected duel-context exports")
	event_groups = defaultdict(list)
	geometry_groups = defaultdict(list)
	for row in events:
		event_groups[row["_group"]].append(row)
	for row in geometry:
		geometry_groups[(row["_group"], row["participant_key"])].append(row)
	for rows in geometry_groups.values():
		rows.sort(key=lambda r: int(n(r, "rel_time")))

	print("INPUT_DEDUP: exact_duplicate_files_skipped=%d exact_duplicate_rows_skipped=%d" %
		(info["duplicate_files"], info["duplicate_rows"]))
	print("SCOPE: source_context=duel; participant_kind 0=bot, 1=account human, 2=IP-identified human; "
		"session type 0=saber-only, 1=full-force; event cohort key is session_id+summary_id+capture_revision. "
		"Metadata/geometry without session_id are joined only when that mapping is unique. No names/keys printed.")
	output("duel participant rows grouped by session_id+summary_id+capture_revision; "
		"metadata joins on summary_id+participant_key+capture_revision",
		[(display(sid), "type=" + sessions[sid]["type"], "seconds=" + sessions[sid]["duration"],
		  "participants=" + str(len(by_participant[sid])),
		  "bot_dropped=" + ",".join(p.get("event_dropped") or "unknown" for p in by_participant[sid] if p["participant_kind"] == BOT),
		  "human_dropped=" + ",".join(p.get("event_dropped") or "unknown" for p in by_participant[sid] if p["participant_kind"] in HUMAN),
		  "bot_push_pull_spent=" + ",".join((p.get("force_push") or "unknown") + "/" + (p.get("force_pull") or "unknown")
			  for p in by_participant[sid] if p["participant_kind"] == BOT),
		  "human_push_pull_spent=" + ",".join((p.get("force_push") or "unknown") + "/" + (p.get("force_pull") or "unknown")
			  for p in by_participant[sid] if p["participant_kind"] in HUMAN))
		 for sid in ids])

	# BUTTON_ATTACK and BUTTON_ALT_ATTACK bits are emitted from resolved usercmd samples.
	command_rows = defaultdict(list)
	for row in events:
		if row["event_type"] == "command_sample":
			command_rows[(row["_group"], row["participant_key"])].append(row)
	command_result = []
	for sid in ids:
		actors = {r["participant_key"]: r["participant_kind"] for r in event_groups[sid]}
		for key, kind in actors.items():
			rows = command_rows[(sid, key)]
			if not rows:
				continue
			button_rows = [r for r in rows if n(r, "buttons") is not None]
			move_rows = [r for r in rows if n(r, "forwardmove") is not None]
			attack = sum(bool(int(n(r, "buttons")) & BUTTON_ATTACK) for r in button_rows)
			alt = sum(bool(int(n(r, "buttons")) & BUTTON_ALT_ATTACK) for r in button_rows)
			back = [r for r in move_rows if n(r, "forwardmove") < 0]
			back_with_buttons = [r for r in back if n(r, "buttons") is not None]
			back_attack = sum(bool(int(n(r, "buttons")) & BUTTON_ATTACK) for r in back_with_buttons)
			back_alt = sum(bool(int(n(r, "buttons")) & BUTTON_ALT_ATTACK) for r in back_with_buttons)
			anykey = sum(bool(int(n(r, "buttons")) & 256) for r in button_rows)
			command_result.append((display(sid), "bot" if kind == BOT else "human", len(rows),
				len(button_rows), len(rows) - len(button_rows), attack, alt,
				len(move_rows), len(rows) - len(move_rows), len(back), back_attack, back_alt, anykey))
	output("command_sample rows; known/missing button and forwardmove values are explicit; "
		"count bit 1, bit 128; negative forwardmove; backward+bits; BUTTON_ANY bit 256",
		command_result)
	mode_by_label = {display(sid): sessions[sid]["type"] for sid in ids}
	command_summary = []
	for mode in sorted(set(mode_by_label.values())):
		for actor in ("bot", "human"):
			subset = [row for row in command_result if mode_by_label[row[0]] == mode and row[1] == actor]
			command_summary.append((mode, actor, "samples=" + str(sum(row[2] for row in subset)),
				"known_forwardmove=" + str(sum(row[7] for row in subset)),
				"negative_forwardmove=" + str(sum(row[9] for row in subset)),
				"negative_forwardmove_with_attack=" + str(sum(row[10] for row in subset))))
	output("command aggregate by session type/actor; negative forwardmove is an input proxy, not a retreat label",
		command_summary)
	hold_rows, input_to_swing_rows = [], []
	for sid in ids:
		for p in by_participant[sid]:
			if p["participant_kind"] not in HUMAN:
				continue
			rows = event_groups[sid]
			key = p["participant_key"]
			commands = command_rows[(sid, key)]
			known_buttons = [r for r in commands if n(r, "buttons") is not None]
			attack_runs = button_runs(commands, BUTTON_ATTACK)
			alt_runs = button_runs(commands, BUTTON_ALT_ATTACK)
			hold_rows.append((display(sid), len(commands), len(known_buttons), len(commands) - len(known_buttons),
				sum(bool(int(n(r, "buttons")) & BUTTON_ATTACK) for r in known_buttons),
				sum(bool(int(n(r, "buttons")) & BUTTON_ALT_ATTACK) for r in known_buttons),
				len(attack_runs), sum(run[2] >= 500 for run in attack_runs),
				max((run[2] for run in attack_runs), default=None),
				len(alt_runs), sum(run[2] >= 500 for run in alt_runs),
				max((run[2] for run in alt_runs), default=None)))
			inputs = sorted((r for r in rows if r["participant_key"] == key and r["event_type"] == "input_start"),
				key=lambda r: n(r, "rel_time"))
			accepted = sorted((r for r in rows if r["participant_key"] == key and r["event_type"] == "swing_start"),
				key=lambda r: n(r, "rel_time"))
			latencies = []
			for swing in accepted:
				prior = [r for r in inputs if n(r, "rel_time") <= n(swing, "rel_time")
					and n(swing, "rel_time") - n(r, "rel_time") <= 2000]
				if prior:
					last = prior[-1]
					latencies.append((last.get("note") or "unknown",
						int(n(swing, "rel_time") - n(last, "rel_time"))))
			input_to_swing_rows.append((display(sid), len(inputs), len(accepted), len(latencies),
				"latency_ms=" + str((min(x[1] for x in latencies), max(x[1] for x in latencies)))
					if latencies else None,
				"prior_input_notes=" + str(dict(Counter(x[0] for x in latencies)))))
	output("human held-command runs: contiguous button samples with <=100ms gaps, >=2 samples; "
		"duration is first-to-last observed sample, not attack charge time",
		hold_rows)
	hold_summary = []
	for mode in ("0", "1"):
		mode_commands, mode_attack_runs, mode_alt_runs = [], [], []
		for sid in ids:
			if sessions[sid].get("type") != mode:
				continue
			for p in by_participant[sid]:
				if p["participant_kind"] not in HUMAN:
					continue
				commands = command_rows[(sid, p["participant_key"])]
				mode_commands.extend(commands)
				mode_attack_runs.extend(button_runs(commands, BUTTON_ATTACK))
				mode_alt_runs.extend(button_runs(commands, BUTTON_ALT_ATTACK))
		known = [r for r in mode_commands if n(r, "buttons") is not None]
		attack_on = sum(bool(int(n(r, "buttons")) & BUTTON_ATTACK) for r in known)
		alt_on = sum(bool(int(n(r, "buttons")) & BUTTON_ALT_ATTACK) for r in known)
		attack_durations = [run[2] for run in mode_attack_runs]
		hold_summary.append((mode, len(mode_commands), len(known), len(mode_commands) - len(known),
			"attack_on_samples=" + str(attack_on), "alt_on_samples=" + str(alt_on),
			"attack_runs=" + str(len(mode_attack_runs)),
			"attack_runs_ge500ms=" + str(sum(value >= 500 for value in attack_durations)),
			"attack_run_median_ms=" + str(median(attack_durations)) if attack_durations else None,
			"attack_run_max_ms=" + str(max(attack_durations)) if attack_durations else None,
			"alt_runs=" + str(len(mode_alt_runs)),
			"alt_runs_ge500ms=" + str(sum(run[2] >= 500 for run in mode_alt_runs))))
	output("human command-hold aggregate by session type; only contiguous known BUTTON_ATTACK/BUTTON_ALT_ATTACK samples, <=100ms gaps",
		hold_summary)
	output("human accepted swing_start preceded by input_start within 2000ms; input_start is button-down, "
		"swing_start is accepted BG_SaberInAttack telemetry",
		input_to_swing_rows)

	# Geometry bearing is reconstructed from actor yaw and the recorded horizontal
	# actor-to-opponent vector. Thresholds are descriptive, not game state labels.
	bearing_result = []
	for sid in ids:
		for p in by_participant[sid]:
			if p["participant_kind"] not in HUMAN:
				continue
			seq = []
			rows = geometry_groups[(sid, p["participant_key"])]
			for row in rows:
				values = [n(row, field) for field in ("enemy_x", "self_x", "enemy_y", "self_y", "self_yaw")]
				if any(value is None for value in values):
					continue
				enemy_x, self_x, enemy_y, self_y, self_yaw = values
				dx = enemy_x - self_x
				dy = enemy_y - self_y
				error = abs(angle_delta(self_yaw, math.degrees(math.atan2(dy, dx))))
				seq.append((int(n(row, "rel_time")), error, math.hypot(dx, dy)))
			seq.sort()
			isolated = sum(error > 150 for _, error, _ in seq)
			reacq, active, start = [], False, None
			for t, error, _ in seq:
				if error > 150 and not active:
					active, start = True, t
				elif error <= 45 and active:
					reacq.append(t - start)
					active = False
			bearing_result.append((display(sid), len(rows), len(seq), len(rows) - len(seq),
				isolated, round(100 * isolated / len(seq), 1) if seq else None,
				len(reacq), ",".join(map(str, reacq)),
				round(max(e for _, e, _ in seq), 1) if seq else None,
				round(median([d for _, _, d in seq]), 1) if seq else None))
	output("geometry: error=abs(wrap(self_yaw-degrees(atan2(enemy_y-self_y,enemy_x-self_x)))); isolated>150deg, reacquire<45deg",
		bearing_result)

	# Throw spans are available for both sides on newer captures. Missing span or
	# command fields are excluded from that metric rather than interpreted as zero.
	throw_results, trace_rows = [], []
	for sid in ids:
		actors = {r["participant_key"]: r["participant_kind"] for r in event_groups[sid]}
		for key, kind in actors.items():
			if kind not in HUMAN | {BOT}:
				continue
			rows = sorted((r for r in event_groups[sid] if r["participant_key"] == key),
				key=lambda r: int(n(r, "event_index")))
			starts = [r for r in rows if r["event_type"] == "throw_start"]
			ends = [r for r in rows if r["event_type"] == "throw_end"]
			for start in starts:
				span_value = n(start, "span_id")
				# throw_end can carry span_id=0 because command-span closure precedes
				# the state-transition event; pair the next end before another launch.
				end = next_paired_end(start, starts, ends)
				span = str(int(span_value)) if span_value is not None else ""
				t0 = int(n(start, "rel_time"))
				duration = int(n(end, "rel_time") - t0) if end else None
				flight = [r for r in rows if r["event_type"] == "command_sample"
					and n(r, "span_kind") == 2 and span_value is not None
					and n(r, "span_id") == span_value and n(r, "rel_time") >= t0
					and (duration is None or n(r, "rel_time") <= t0 + duration)]
				pre = [r for r in rows if r["event_type"] == "command_sample" and t0 - 1000 <= int(n(r, "rel_time")) < t0]
				span_start = next((r for r in rows if r["event_type"] == "span_start"
					and n(r, "span_kind") == 2 and span_value is not None
					and n(r, "span_id") == span_value), None)
				positions = [r for r in flight if r.get("saber_position_valid") == "1"
					and all(n(r, "saber_" + axis) is not None for axis in "xyz")]
				matched_distances = []
				grows = geometry_groups[(sid, key)]
				for sample in positions:
					when = int(n(sample, "rel_time"))
					candidates = [g for g in grows if all(n(g, "enemy_" + axis) is not None for axis in "xyz")]
					if candidates:
						near = min(candidates, key=lambda g: abs(int(n(g, "rel_time")) - when))
					else:
						continue
					if abs(int(n(near, "rel_time")) - when) <= 60:
						matched_distances.append(math.dist(
							tuple(n(sample, "saber_" + axis) for axis in "xyz"),
							tuple(n(near, "enemy_" + axis) for axis in "xyz")))
				yaws = [n(r, "command_yaw") for r in flight if n(r, "command_yaw") is not None]
				pitches = [n(r, "command_pitch") for r in flight if n(r, "command_pitch") is not None]
				early_pitches = [n(r, "command_pitch") for r in flight
					if n(r, "command_pitch") is not None and 0 <= int(n(r, "rel_time")) - t0 <= 250]
				def bit_count(rows, bit):
					known = [n(r, "buttons") for r in rows if n(r, "buttons") is not None]
					return sum(bool(int(value) & bit) for value in known), len(known), len(rows) - len(known)
				hold = continuous_press_hold(rows, t0)
				throw_results.append({"sid": sid, "kind": kind, "time": t0,
					"range": n(start, "amount"),
					"outcome": (end.get("note") or "unknown") if end else "no_end", "duration": duration,
					"n": len(flight), "valid": len(positions), "aim_valid": sum(r.get("aim_endpoint_valid") == "1" for r in flight),
					"yaw": (min(yaws), max(yaws)) if yaws else None,
					"pitch": (min(pitches), max(pitches)) if pitches else None,
					"early_pitch": (min(early_pitches), max(early_pitches)) if early_pitches else None,
					"offset": n(start, "throw_yaw_offset"),
					"preoffset": n(span_start, "throw_yaw_offset") if span_start else None,
					"enemy_force": n(start, "enemy_force"), "enemy_defense": n(start, "enemy_defense"),
					"pre_attack": bit_count(pre, BUTTON_ATTACK),
					"pre_alt": bit_count(pre, BUTTON_ALT_ATTACK),
					"pre_force": bit_count(pre, BUTTON_FORCEPOWER),
					"closest": min(matched_distances) if matched_distances else None,
					"hold": hold,
					"features": flight_features(flight, grows, t0, duration)})
				for sample in pre + flight:
					buttons = n(sample, "buttons")
					b = int(buttons) if buttons is not None else None
					phase = "flight" if sample in flight else "pre_release_1000ms"
					near = None
					candidates = [g for g in grows if all(n(g, "enemy_" + axis) is not None for axis in "xyz")]
					if phase == "flight" and candidates:
						near = min(candidates, key=lambda g: abs(int(n(g, "rel_time")) - int(n(sample, "rel_time"))))
						if abs(n(near, "rel_time") - n(sample, "rel_time")) > 60:
							near = None
					trace_rows.append((sid[1], sid[2], kind, span, phase,
					int(n(sample, "rel_time")), int(n(sample, "rel_time")) - t0,
					n(sample, "command_msec"), n(sample, "command_yaw"), n(sample, "command_pitch"), b,
					int(bool(b & BUTTON_ATTACK)) if b is not None else None,
					int(bool(b & BUTTON_ALT_ATTACK)) if b is not None else None,
					int(bool(b & BUTTON_FORCEPOWER)) if b is not None else None,
					n(sample, "saber_x") if sample.get("saber_position_valid") == "1" else None,
					n(sample, "saber_y") if sample.get("saber_position_valid") == "1" else None,
					n(sample, "saber_z") if sample.get("saber_position_valid") == "1" else None,
					n(near, "enemy_x") if near else None, n(near, "enemy_y") if near else None,
					n(near, "enemy_z") if near else None))
	throw_rows = []
	for sid in ids:
		throws = [r for r in throw_results if r["sid"] == sid]
		if not throws:
			continue
		for kind in ("0", "1", "2"):
			actor_throws = [r for r in throws if r["kind"] == kind]
			if not actor_throws:
				continue
			hits = [r for r in actor_throws if r["outcome"] == "hit"]
			misses = [r for r in actor_throws if r["outcome"] == "miss"]
			holds = [r["hold"]["duration"] for r in actor_throws if r["hold"]]
			def value_summary(values):
				return (min(values), median(values), max(values)) if values else None
			throw_rows.append((display(sid), kind, len(actor_throws), len(hits), len(misses),
				sum(r["outcome"] == "unknown" for r in actor_throws),
				sum(r["outcome"] == "no_end" for r in actor_throws),
				"hit_flight_min_median_max_ms=" + str(value_summary(
					[r["duration"] for r in hits if r["duration"] is not None])),
				"miss_flight_min_median_max_ms=" + str(value_summary(
					[r["duration"] for r in misses if r["duration"] is not None])),
				"release_2d_min_median_max=" + str(value_summary(
					[r["range"] for r in actor_throws if r["range"] is not None])),
				"trajectory_n=" + str(sum(r["closest"] is not None for r in actor_throws)),
				"median_hit_closest_3d=" + str(round(median([r["closest"] for r in hits if r["closest"] is not None]), 1)
					if any(r["closest"] is not None for r in hits) else None),
				"median_miss_closest_3d=" + str(round(median([r["closest"] for r in misses if r["closest"] is not None]), 1)
					if any(r["closest"] is not None for r in misses) else None),
				"continuous_press_to_release_n=" + str(len(holds)),
				"positive_hold_n=" + str(sum(value > 0 for value in holds)),
				"positive_hold_min_median_max_ms=" + str(value_summary([value for value in holds if value > 0]))))
	output("throw_start/throw_end by actor; participant_kind 0=bot, 1=account human, 2=IP-identified human; "
		"flight trajectories use span_kind=2 samples paired with opponent geometry <=60ms",
		throw_rows)
	output("individual throws: pre-release attack/alt/force bit counts plus known/missing command samples; "
		"hold duration is reported only when an input_start and continuous known button samples cover press-to-release",
		[(display(r["sid"]), r["kind"], r["time"], r["range"], r["outcome"], r["duration"],
		  r["n"], r["valid"], r["aim_valid"], r["yaw"], r["pitch"], r["early_pitch"], r["offset"],
		  r["enemy_force"], r["enemy_defense"], r["pre_attack"], r["pre_alt"], r["pre_force"],
		  round(r["closest"], 1) if r["closest"] is not None else None, r["hold"])
		 for r in throw_results])
	output("actual flight only (throw_start through throw_end): sampled pitch<-5deg above horizontal in first 250ms; "
		"observed ALT prefix/first off require <=100ms gaps; rear<-24u relative to opponent facing; "
		"above>96u relative to opponent origin; bearing isolation>150deg then reacquire<45deg. "
		"These are sampled geometric/input features, not wall traces, intent, or continuous button edges",
		[(display(r["sid"]), r["kind"], r["time"], r["outcome"], r["features"]) for r in throw_results])
	feature_summary = []
	for actor in ("bot", "human"):
		for outcome in ("hit", "miss", "unknown", "no_end"):
			subset = [r for r in throw_results if (r["kind"] == BOT) == (actor == "bot")
				and r["outcome"] == outcome]
			if not subset:
				continue
			covered = [r["features"] for r in subset if r["features"]["full_button_window"]]
			prefixes = [f["alt_observed_prefix_ms"] for f in covered
				if f["alt_observed_prefix_ms"] is not None]
			rates = []
			for field in ("early_pitch_above_horizon", "saber_rear_24u_seen", "saber_above_96u_seen"):
				values = [r["features"][field] for r in subset if r["features"][field] is not None]
				rates.append(field + "=" + str(sum(values)) + "/" + str(len(values)))
			feature_summary.append((actor, outcome, len(subset), "covered_button_windows=" + str(len(covered)),
				"initial_alt_off=" + str(sum(f["first_observed_alt_off_ms"] == 0 for f in covered)),
				"alt_on_samples=" + str(sum(r["features"]["alt_on_samples"] for r in subset)) + "/" +
					str(sum(r["features"]["known_button_samples"] for r in subset)),
				"median_longest_alt_run_ms=" + str(median([r["features"]["longest_observed_alt_run_ms"]
					for r in subset if r["features"]["longest_observed_alt_run_ms"] is not None])),
				"alt_prefix_min_median_max_ms=" + str((min(prefixes), median(prefixes), max(prefixes)) if prefixes else None),
				*rates))
	output("actual-flight feature aggregate by actor/outcome; pitch/rear/above counts are true/known; "
		"covered button windows have endpoints within 100ms and no unknown samples or >100ms gaps",
		feature_summary)

	# Link human counter_success events to source-side accepted bot swing windows.
	swing_rows, counter_rows = [], []
	for sid in ids:
		rows = event_groups[sid]
		botkeys = {r["participant_key"] for r in rows if r["participant_kind"] == BOT}
		for key in botkeys:
			ordered = sorted((r for r in rows if r["participant_key"] == key), key=lambda r: int(n(r, "event_index")))
			starts = [r for r in ordered if r["event_type"] == "swing_start"]
			ends = [r for r in ordered if r["event_type"] == "swing_end"]
			for start in starts:
				end = next_paired_end(start, starts, ends)
				if not end:
					continue
				side = start.get("swing_side") or end.get("swing_side") or "unclassified"
				swing_rows.append((display(sid), side, int(n(start, "rel_time")), int(n(end, "rel_time")),
					int(n(end, "rel_time") - n(start, "rel_time")), n(end, "yaw_sweep")))
			for p in by_participant[sid]:
				if p["participant_kind"] not in HUMAN:
					continue
				for counter in (r for r in rows if r["participant_key"] == p["participant_key"] and r["event_type"] == "counter_success"):
					t = int(n(counter, "rel_time"))
					matches = []
					for start in starts:
						end = next_paired_end(start, starts, ends)
						if not end:
							continue
						t0, t1 = int(n(start, "rel_time")), int(n(end, "rel_time"))
						if t0 <= t <= t1:
							matches.append((0, "active", start, end))
						elif t1 < t <= t1 + 350:
							matches.append((t - t1, "late_0_350ms", start, end))
						elif t1 + 350 < t <= t1 + 1000:
							matches.append((t - t1, "late_351_1000ms", start, end))
					if matches:
						_, band, start, end = min(matches, key=lambda x: x[0])
						grows = geometry_groups[(sid, p["participant_key"])]
						geo_candidates = [g for g in grows if all(n(g, axis + "_" + suffix) is not None
							for axis in ("self", "enemy") for suffix in ("x", "y", "z"))]
						near = min(geo_candidates, key=lambda g: abs(int(n(g, "rel_time")) - t)) if geo_candidates else None
						if near and abs(int(n(near, "rel_time")) - t) > 60:
							near = None
						if near:
							delta = tuple(n(near, "enemy_" + axis) - n(near, "self_" + axis) for axis in ("x", "y", "z"))
							distance = math.sqrt(sum(value * value for value in delta))
							velocity_fields = ["enemy_v" + axis for axis in "xyz"] + ["self_v" + axis for axis in "xyz"]
							if distance and all(n(near, field) is not None for field in velocity_fields):
								relvel = tuple(n(near, "enemy_v" + axis) - n(near, "self_v" + axis) for axis in "xyz")
								closing = -sum(a * b for a, b in zip(delta, relvel)) / distance
							else:
								closing = None
						else:
							distance, closing = None, None
						start_elapsed = t - int(n(start, "rel_time"))
						if start_elapsed < 350:
							start_bin = "start_0_349ms"
						elif start_elapsed < 400:
							start_bin = "start_350_399ms"
						elif start_elapsed <= 700:
							start_bin = "start_400_700ms"
						elif start_elapsed <= 1000:
							start_bin = "start_701_1000ms"
						else:
							start_bin = "start_over_1000ms"
						counter_rows.append((display(sid), start.get("swing_side") or end.get("swing_side") or "unclassified",
							band, start_elapsed, start_bin, t - int(n(end, "rel_time")),
							round(distance, 1) if distance is not None else None,
							round(closing, 1) if closing is not None else None,
							counter.get("saber_defense_state"), counter.get("enemy_saber_defense_state"),
							counter.get("recovery"), counter.get("enemy_recovery"),
										counter.get("attack_elapsed_ms"), int(n(start, "rel_time")), t,
										int(n(end, "rel_time"))))
	output("bot swing_start/end pairs by source swing_side; end yaw_sweep; no inferred red/fan/poke animation labels",
		swing_rows)
	counter_totals = []
	for sid in ids:
		total = sum(r["participant_kind"] in HUMAN and r["event_type"] == "counter_success"
			for r in event_groups[sid])
		matched = sum(row[0] == display(sid) for row in counter_rows)
		counter_totals.append((display(sid), total, matched, total - matched))
	output("human counter_success rows; paired means an observed bot swing active or <=1000ms after its end",
		counter_totals)
	swing_summary, counter_summary = [], []
	for sid in ids:
		label = display(sid)
		pairs = [r for r in swing_rows if r[0] == label]
		start_count = sum(r["participant_kind"] == BOT and r["event_type"] == "swing_start"
			for r in event_groups[sid])
		side_counts = Counter(r[1] for r in pairs)
		durations = [r[4] for r in pairs]
		swing_summary.append((label, sessions[sid].get("type"), start_count, len(pairs),
			start_count - len(pairs), dict(side_counts),
			"duration_ms=" + str((min(durations), max(durations))) if durations else None))
		total = sum(r["participant_kind"] in HUMAN and r["event_type"] == "counter_success"
			for r in event_groups[sid])
		matches = [r for r in counter_rows if r[0] == label]
		bins = Counter(r[4] for r in matches)
		target = [r for r in matches if r[4] == "start_400_700ms"]
		counter_summary.append((label, sessions[sid].get("type"), total, len(matches),
			bins["start_0_349ms"], bins["start_350_399ms"], bins["start_400_700ms"],
			bins["start_701_1000ms"], bins["start_over_1000ms"],
			"direction_400_700=" + str(dict(Counter(r[1] for r in target))),
			"<=120u_400_700=" + str(sum(r[6] is not None and r[6] <= 120 for r in target)),
			"nonbroken_nonknockdown_400_700=" + str(sum(r[8] not in ("3", "4", None) for r in target))))
	output("paired bot accepted swing windows by duel; starts without an observed end are not paired",
		swing_summary)
	output("human counter_success timing relative to the accepted bot swing_start; "
		"matched only during the window or <=1000ms after its end",
		counter_summary)
	output("human counter_success rows paired only when active or <=1000ms after a bot swing end; "
		"start bins use accepted swing_start timestamp; geometry uses nearest valid sample, "
		"closing speed=-(enemy-self) dot (enemy_velocity-self_velocity)/3D distance; "
		"defense_state 0=none, 3=broken, 4=knockdown",
		counter_rows)

	if args.trace_output:
		with Path(args.trace_output).open("w", encoding="utf-8", newline="") as dest:
			writer = csv.writer(dest)
			writer.writerow(("summary_id", "capture_revision", "participant_kind", "span_id", "phase",
				"rel_time", "ms_from_release", "command_msec",
				"command_yaw", "command_pitch", "buttons", "attack_bit", "alt_attack_bit", "forcepower_bit",
				"saber_x", "saber_y", "saber_z", "enemy_x", "enemy_y", "enemy_z"))
			writer.writerows(trace_rows)
		print("TRACE: wrote", len(trace_rows), "command rows to", Path(args.trace_output).name)


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--events", nargs="+", type=Path, required=True, help="one or more dueltrack_events CSV parts")
	parser.add_argument("--geometry", type=Path, required=True)
	parser.add_argument("--sessions", type=Path, required=True)
	parser.add_argument("--participants", type=Path, required=True)
	parser.add_argument("--summary-ids", nargs="*", help="optional numeric summary IDs to restrict report scope")
	parser.add_argument("--revision", help="optional exact capture_revision cohort")
	parser.add_argument("--session-id", help="optional exact event session_id to disambiguate reused summary IDs")
	parser.add_argument("--trace-output", type=Path, help="optional CSV of per-command throw-flight and pre-release samples")
	args = parser.parse_args()
	try:
		report(args)
	except (OSError, ValueError, KeyError) as error:
		parser.error(str(error))


if __name__ == "__main__":
	main()
