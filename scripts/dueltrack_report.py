#!/usr/bin/env python3
"""Reproducible duel-track report for botnav bot tuning.

Every figure printed comes from a printed query, so reports can quote the same
numbers. The general report requires duckdb; --saber-evidence uses stdlib CSV/SQLite.

Usage: python3 scripts/dueltrack_report.py [csv_dir] [--revision REV]
       python3 scripts/dueltrack_report.py . --saber-evidence --release-dir PATH
       python3 scripts/dueltrack_report.py . --throw-evidence

Loading rules (see docs/bot-cvar-outline.md, "Duel tracking"):
- Some export parts contain latin-1 bytes; files are re-encoded to UTF-8 first.
- Event and geometry parts have different column sets; they are unioned by name
  and read as text, so numbers are cast explicitly.
- The general report removes exact duplicate rows. Saber evidence scopes dedup
  to source batches, preserves variants and excludes unresolved identity conflicts.
- participant_kind: 0 bot, 1 account human, 2 guest human.
- range_bucket 1 = under 128 units. self_footing: 0 still, 1 advance, 2 retreat, 3 lateral.
- dueltrack_learned rows are only current-format when (ctx_key & 0x60000000) = 0x60000000;
  mode = (ctx_key >> 16) & 3 (0 saber-only, 1 full force).
"""

import argparse
import codecs
import csv
import glob
import hashlib
import json
import os
import re
import sqlite3
import sys
import tempfile


def csv_provenance(path):
	"""Hash original bytes while validating UTF-8 across chunk boundaries."""
	digest = hashlib.sha256()
	decoder = codecs.getincrementaldecoder("utf-8")()
	valid_utf8 = True
	with open(path, "rb") as src:
		for chunk in iter(lambda: src.read(1024 * 1024), b""):
			digest.update(chunk)
			if valid_utf8:
				try:
					decoder.decode(chunk)
				except UnicodeDecodeError:
					valid_utf8 = False
	if valid_utf8:
		try:
			decoder.decode(b"", final=True)
		except UnicodeDecodeError:
			valid_utf8 = False
	return digest.hexdigest(), "utf-8-sig" if valid_utf8 else "latin-1"


def print_batch_inventory(batch, directory):
	"""Detect internal split gaps; absent trailing parts need an exporter manifest."""
	print(json.dumps({"batch": batch, "provenance":
		"https://github.com/chompehh/botnav/releases/tag/Dueltracks"
		if batch == "Dueltracks1" else "repository-local export; not superseded by release"}))
	families = ["dueltrack_" + name for name in
		("events", "geometry", "aggregate", "learned", "learned_evidence", "participants", "sessions")]
	families += ["strafetrack_" + name for name in ("inputs", "nodes", "routes")]
	for family in families:
		pattern = re.compile(re.escape(family) + r"(?:_part([0-9]+))?\.csv$")
		parts = set()
		for path in glob.glob(os.path.join(directory, family + "*.csv")):
			match = pattern.fullmatch(os.path.basename(path))
			if match:
				parts.add(int(match.group(1)) if match.group(1) else 1)
		missing = sorted(set(range(1, max(parts) + 1)) - parts) if parts else [1]
		print(json.dumps({"batch": batch, "split_family": family, "present_parts": sorted(parts),
			"missing_internal_parts": missing,
			"trailing_completeness": "unknown without exporter manifest"}))
	required = ["dueltrack_" + name + ".csv" for name in
		("aggregate", "learned", "learned_evidence", "participants", "sessions")]
	print(json.dumps({"batch": batch, "missing_support_files":
		[name for name in required if not os.path.isfile(os.path.join(directory, name))]}))


def saber_evidence(batches, throws_only=False):
	"""Keep exports separate; identity overlap is not proof of identical payloads."""
	con = sqlite3.connect(":memory:")
	con.execute("PRAGMA temp_store = MEMORY")
	fields = ["source_context", "record_id", "parent_id", "participant_key", "event_index",
		"rel_time", "participant_kind", "event_type", "saber_move", "saber_stance",
		"note", "capture_version", "capture_revision", "torso_timer", "torso_anim",
		"yaw_sweep", "self_pitch", "sequence_id", "opponent_key", "export_format_version",
		"amount", "attack_elapsed_ms", "grounded", "range_bucket", "forwardmove", "rightmove",
		"upmove", "self_footing", "enemy_stance", "buttons", "power", "damage_source",
		"saber_defense_state",
		"enemy_saber_defense_state", "recovery", "enemy_recovery", "controller_owns_inputs"]
	con.execute("CREATE TABLE raw(batch TEXT, filename TEXT, identity TEXT, payload TEXT, observation TEXT, " +
		", ".join(f"{f} TEXT" for f in fields) + ")")
	con.execute("""CREATE TABLE learned(batch TEXT, source TEXT, ctx_key INTEGER,
		stimulus TEXT, response TEXT, follow1 TEXT, follow2 TEXT, samples INTEGER,
		skill_band INTEGER, wins INTEGER, net_damage REAL)""")
	con.execute("""CREATE TABLE sessions(batch TEXT, source_context TEXT, record_id TEXT,
		start_time TEXT, end_time TEXT, mapname TEXT, duel_type TEXT)""")
	con.execute("""CREATE TABLE snapshots(batch TEXT, family TEXT, schema_json TEXT,
		payload TEXT)""")
	if throws_only:
		con.execute("""CREATE TABLE geometry_raw(batch TEXT, source_context TEXT, parent_id TEXT,
			participant_key TEXT, opponent_key TEXT, rel_time INTEGER,
			self_yaw REAL, distance REAL)""")
	for batch, directory in batches:
		print_batch_inventory(batch, directory)
		for path in sorted(glob.glob(os.path.join(directory, "dueltrack_events*.csv"))):
			digest, encoding = csv_provenance(path)
			# Preserve valid UTF-8; use Latin-1 only for legacy invalid-UTF-8 files.
			# Do not use labels as identity keys or print participant/chat content.
			with open(path, encoding=encoding, newline="") as src:
				reader = csv.DictReader(src)
				rows = []
				n = 0
				for row in reader:
					identity = json.dumps([row.get(f, "") for f in fields[:6]])
					payload = hashlib.sha256(json.dumps(
						{k: v for k, v in row.items() if v}, sort_keys=True).encode()).hexdigest()
					core = ["participant_key", "opponent_key", "rel_time", "event_index",
						"event_type", "saber_move", "enemy_saber_move", "amount", "quality", "note",
						"self_hp", "self_armor", "self_force", "enemy_hp", "enemy_armor",
						"enemy_force", "buttons", "state", "range_bucket", "yaw_delta",
						"saber_stance", "grounded", "forwardmove", "rightmove", "upmove"]
					observation = hashlib.sha256(json.dumps(
						[row.get(f, "") for f in core]).encode()).hexdigest()
					rows.append([batch, os.path.basename(path), identity, payload, observation] +
						[row.get(f, "") for f in fields])
					n += 1
					if len(rows) == 2048:
						con.executemany("INSERT INTO raw VALUES (" +
							",".join("?" for _ in rows[0]) + ")", rows)
						rows.clear()
				if rows:
					con.executemany("INSERT INTO raw VALUES (" +
						",".join("?" for _ in rows[0]) + ")", rows)
				print(json.dumps({"batch": batch, "file": os.path.basename(path),
					"path": path, "rows": n, "sha256": digest,
					"encoding": encoding, "columns": reader.fieldnames}))
		for path in sorted(glob.glob(os.path.join(directory, "*.csv"))):
			if os.path.basename(path).startswith("dueltrack_events"):
				continue
			family = re.sub(r"_part[0-9]+(?=\.csv$)", "", os.path.basename(path))
			digest, encoding = csv_provenance(path)
			with open(path, encoding=encoding, newline="") as src:
				reader = csv.DictReader(src)
				n = 0
				for row in reader:
					n += 1
					if family in ("dueltrack_sessions.csv", "strafetrack_routes.csv"):
						# Match complete row tuples only when the column sets agree;
						# record IDs alone do not identify historical snapshots.
						payload = hashlib.sha256(json.dumps(
							[row[k] for k in sorted(row)]).encode()).hexdigest()
						con.execute("INSERT INTO snapshots VALUES (?,?,?,?)",
							[batch, family, json.dumps(sorted(row)), payload])
					if family == "dueltrack_learned.csv":
						con.execute("INSERT INTO learned VALUES (?,?,?,?,?,?,?,?,?,?,?)",
							[batch, row["source"], int(row["ctx_key"]), row["stimulus"],
							 row["response"], row["follow1"], row["follow2"], int(row["samples"]),
							 int(row["skill_band"]), int(row["wins"]), float(row["net_damage"])])
					elif family == "dueltrack_sessions.csv":
						con.execute("INSERT INTO sessions VALUES (?,?,?,?,?,?,?)",
							[batch, row["source_context"], row["record_id"], row["start_time"],
							 row["end_time"], row["mapname"], row["type"]])
					elif throws_only and family == "dueltrack_geometry.csv":
						distance = sum((float(row["self_" + axis]) -
							float(row["enemy_" + axis])) ** 2 for axis in ("x", "y")) ** 0.5
						con.execute("INSERT INTO geometry_raw VALUES (?,?,?,?,?,?,?,?)",
							[batch, row["source_context"], row["parent_id"],
							 row["participant_key"], row["opponent_key"], int(row["rel_time"]),
							 float(row["self_yaw"]), distance])
				print(json.dumps({"batch": batch, "file": os.path.basename(path),
					"path": path, "rows": n, "sha256": digest,
					"encoding": encoding, "columns": reader.fieldnames}))
	con.execute("""CREATE TABLE event_variants AS SELECT * FROM (
		SELECT *, row_number() OVER (PARTITION BY batch, identity, observation
			ORDER BY CAST(export_format_version AS INTEGER) DESC, rowid) AS n
		FROM raw) WHERE n = 1""")
	con.execute("""CREATE TABLE conflicts AS
		SELECT batch, identity FROM event_variants GROUP BY batch, identity HAVING count(*) > 1
		UNION SELECT batch, identity FROM raw GROUP BY batch, identity, export_format_version
		HAVING count(DISTINCT payload) > 1""")
	con.execute("""CREATE TABLE events AS SELECT v.* FROM event_variants v
		LEFT JOIN conflicts c ON c.batch = v.batch AND c.identity = v.identity
		WHERE c.identity IS NULL""")
	if throws_only:
		throw_evidence(con)
		return
	con.execute("""CREATE INDEX accepted_links ON events
		(batch,source_context,parent_id,participant_key,rel_time,saber_move)
		WHERE event_type = 'attack_chain' AND note = 'accepted_link'""")
	# Capture stores the actual start timestamp as rel_time - attack_elapsed_ms.
	# Match that timestamp, actual move and opponent; do not infer hits from generic
	# damage/quality labels, or from an adjacent token/sequence window.
	con.execute("""CREATE TABLE swing_hits AS
		SELECT batch, source_context, parent_id, participant_key, opponent_key, saber_move,
		 CAST(rel_time AS INTEGER) - CAST(attack_elapsed_ms AS INTEGER) AS started_at,
		 sum(CAST(amount AS INTEGER)) AS damage
		FROM events WHERE event_type = 'swing_damage' AND note = 'confirmed'
		 AND CAST(saber_move AS INTEGER) BETWEEN 4 AND 10
		 AND attack_elapsed_ms != '' AND CAST(attack_elapsed_ms AS INTEGER) >= 0
		GROUP BY 1,2,3,4,5,6,7""")
	con.execute("""CREATE INDEX swing_hit_lookup ON swing_hits
		(batch,source_context,parent_id,participant_key,opponent_key,saber_move,started_at)""")
	con.execute("""CREATE TABLE swing_outcomes AS
		SELECT a.*, s.duel_type, coalesce(h.damage, 0) AS confirmed_damage
		FROM events a LEFT JOIN swing_hits h
		 ON h.batch = a.batch AND h.source_context = a.source_context
		 AND h.parent_id = a.parent_id AND h.participant_key = a.participant_key
		 AND h.opponent_key = a.opponent_key AND h.saber_move = a.saber_move
		 AND h.started_at = CAST(a.rel_time AS INTEGER)
		LEFT JOIN sessions s ON s.batch = a.batch AND s.source_context = a.source_context
		 AND s.record_id = a.parent_id
		WHERE a.event_type = 'swing_start' AND a.note = 'accepted'
		 AND CAST(a.saber_move AS INTEGER) BETWEEN 4 AND 10""")
	queries = [
		("Released human directional priors (single attacks, minimum eight starts)", """
SELECT CAST(duel_type AS INTEGER) AS mode, CAST(saber_stance AS INTEGER) AS self_stance,
 CAST(enemy_stance AS INTEGER) AS enemy_stance, CAST(saber_move AS INTEGER) AS move,
 count(*) AS starts, sum(confirmed_damage > 0) AS confirmed_hit_starts
FROM swing_outcomes WHERE batch = 'Dueltracks1' AND participant_kind IN ('1','2')
 AND duel_type IN ('0','1') AND grounded = '1' AND range_bucket = '1'
 AND saber_defense_state = '0' AND recovery = '0'
GROUP BY 1,2,3,4 HAVING count(*) >= 8 AND sum(confirmed_damage > 0) > 0
ORDER BY 1,2,3,4"""),
		("Directional grounded close-range outcomes by stance/context (not combo causality)", """
SELECT batch, participant_kind, duel_type, saber_stance, enemy_stance,
 saber_move, count(*) AS accepted_starts,
 sum(confirmed_damage > 0) AS confirmed_hit_starts, sum(confirmed_damage) AS confirmed_damage
FROM swing_outcomes WHERE grounded = '1' AND range_bucket = '1'
 AND saber_defense_state = '0' AND recovery = '0'
GROUP BY 1,2,3,4,5,6 ORDER BY 1,2,3,4,5,6"""),
		("Saber close-range input ownership and lateral bias (samples, not independent frames)", """
SELECT batch, participant_kind, capture_revision, event_type, controller_owns_inputs,
 count(*) AS samples,
 sum(CAST(rightmove AS INTEGER) > 0) AS right_input,
 sum(CAST(rightmove AS INTEGER) < 0) AS left_input,
 sum(forwardmove = '0' AND rightmove = '0' AND upmove = '0') AS no_move_input
FROM events WHERE range_bucket = '1' AND grounded = '1'
 AND event_type IN ('swing_start','attack_sample')
GROUP BY 1,2,3,4,5 ORDER BY 1,2,3,4,5"""),
		("Grounded close-range attack-sample ownership summary (not duration or causal stalls)", """
SELECT batch,
 CASE WHEN participant_kind IN ('1','2') THEN 'human'
 WHEN controller_owns_inputs = '1' THEN 'bot-owned' ELSE 'bot-other' END AS input_owner,
 count(*) AS samples,
 sum(CAST(rightmove AS INTEGER) > 0) AS right_input,
 sum(CAST(rightmove AS INTEGER) < 0) AS left_input,
 sum(forwardmove = '0' AND rightmove = '0' AND upmove = '0') AS no_move_input
FROM events WHERE range_bucket = '1' AND grounded = '1' AND event_type = 'attack_sample'
GROUP BY 1,2 ORDER BY 1,2"""),
		("Unmatched confirmed directional damage (coverage limitation, not counted as misses)", """
SELECT h.batch, count(*) AS unmatched_hit_windows FROM swing_hits h
LEFT JOIN swing_outcomes a ON a.batch = h.batch AND a.source_context = h.source_context
 AND a.parent_id = h.parent_id AND a.participant_key = h.participant_key
 AND a.opponent_key = h.opponent_key AND a.saber_move = h.saber_move
 AND CAST(a.rel_time AS INTEGER) = h.started_at
WHERE a.identity IS NULL GROUP BY 1 ORDER BY 1"""),
		("Historical snapshot row inventory (batches are never concatenated)", """
SELECT batch, family, count(*) AS rows, count(DISTINCT payload) AS distinct_complete_rows
FROM snapshots GROUP BY 1,2 ORDER BY 1,2"""),
		("Historical complete-row snapshot overlap (requires identical schemas)", """
SELECT a.family, a.batch, b.batch,
 count(DISTINCT CASE WHEN a.payload = b.payload THEN a.payload END) AS identical_complete_rows
FROM snapshots a JOIN snapshots b ON a.batch < b.batch AND a.family = b.family
 AND a.schema_json = b.schema_json
GROUP BY 1,2,3 ORDER BY 1,2,3"""),
		("Reused session IDs versus session intervals (diagnostic, not a cross-batch join key)", """
SELECT a.batch, b.batch,
 sum(a.source_context = b.source_context AND a.record_id = b.record_id) AS reused_session_ids,
 sum(a.source_context = b.source_context AND a.start_time = b.start_time
  AND a.end_time = b.end_time AND a.mapname = b.mapname) AS matching_session_intervals
FROM sessions a JOIN sessions b ON a.batch < b.batch
GROUP BY 1,2 ORDER BY 1,2"""),
		("Batch duplicate identities and payload variants (including schema upgrades)", """
SELECT batch, count(*) AS raw_rows, count(DISTINCT identity) AS identities,
 count(*) - count(DISTINCT identity) AS repeated_identities,
 count(DISTINCT identity || payload) - count(DISTINCT identity) AS payload_variants,
 count(DISTINCT identity || observation) - count(DISTINCT identity) AS core_payload_conflicts
FROM raw GROUP BY batch"""),
		("Conflicting event identities preserved in raw/variants, excluded from swing counts", """
SELECT r.batch, count(c.identity) AS conflicted_identities
FROM (SELECT DISTINCT batch FROM raw) r LEFT JOIN conflicts c ON c.batch = r.batch
GROUP BY r.batch ORDER BY r.batch"""),
		("Cross-batch identity and exact-payload overlap", """
SELECT a.batch, b.batch, count(*) AS overlapping_identities,
 sum(a.payload = b.payload) AS identical_payloads
FROM events a JOIN events b ON a.identity = b.identity AND a.batch < b.batch
GROUP BY a.batch, b.batch"""),
		("Core fingerprint match candidates (not automatically deduplicated across batches)", """
SELECT a.batch, a.parent_id, b.batch, b.parent_id,
 a.event_type, a.rel_time, a.event_index, a.capture_revision, b.capture_revision,
 sa.start_time, sb.start_time, sa.mapname, sb.mapname
FROM events a JOIN events b ON a.batch < b.batch AND a.observation = b.observation
LEFT JOIN sessions sa ON sa.batch = a.batch AND sa.source_context = a.source_context
 AND sa.record_id = a.parent_id
LEFT JOIN sessions sb ON sb.batch = b.batch AND sb.source_context = b.source_context
 AND sb.record_id = b.parent_id
ORDER BY a.batch, a.parent_id, a.event_index"""),
		("Cross-batch core observations ignoring export record/parent IDs", """
SELECT a.batch, b.batch, count(*) AS matching_observation_fingerprints
FROM (SELECT DISTINCT batch, observation FROM events) a
JOIN (SELECT DISTINCT batch, observation FROM events) b
 ON a.batch < b.batch AND a.observation = b.observation
GROUP BY a.batch, b.batch"""),
		("Capture provenance and animation coverage", """
SELECT batch, capture_version, capture_revision, count(*) AS events,
 sum(torso_timer != '' AND torso_anim != '' AND CAST(torso_timer AS INTEGER) >= 0
 AND CAST(torso_anim AS INTEGER) >= 0) AS animation_rows
FROM events GROUP BY 1,2,3 ORDER BY 1,4 DESC"""),
		("Human basic swing starts (observed, not demonstrated combos)", """
SELECT batch, saber_stance, saber_move, count(*) AS starts
FROM events WHERE participant_kind IN ('1','2') AND event_type = 'swing_start'
 AND CAST(saber_move AS INTEGER) BETWEEN 4 AND 10
GROUP BY 1,2,3 ORDER BY 1,4 DESC"""),
		("Accepted basic attack mix by participant kind (frequency, not effectiveness)", """
SELECT batch, participant_kind, count(*) AS basic_starts,
 sum(saber_move IN ('5','8')) AS horizontal_starts,
 sum(saber_move = '10') AS vertical_starts,
 sum(saber_move IN ('4','6','7','9')) AS diagonal_starts
FROM events WHERE event_type = 'swing_start' AND note = 'accepted'
 AND CAST(saber_move AS INTEGER) BETWEEN 4 AND 10
GROUP BY 1,2 ORDER BY 1,2"""),
		("Human adjacent accepted starts within 1000ms (and explicit accepted_link annotations)", """
WITH attacks AS (
 SELECT *, lag(saber_move) OVER w AS previous_move,
 lag(rel_time) OVER w AS previous_time, lag(saber_stance) OVER w AS previous_stance,
 lag(opponent_key) OVER w AS previous_opponent
 FROM events WHERE event_type = 'swing_start' AND participant_kind IN ('1','2')
 WINDOW w AS (PARTITION BY batch,source_context,parent_id,participant_key
 ORDER BY CAST(rel_time AS INTEGER),CAST(event_index AS INTEGER))
)
SELECT a.batch, previous_move || '>' || a.saber_move AS pair, count(*) AS adjacent_starts,
 sum(EXISTS(SELECT 1 FROM events link WHERE link.batch = a.batch
  AND link.source_context = a.source_context AND link.parent_id = a.parent_id
  AND link.participant_key = a.participant_key AND link.rel_time = a.rel_time
  AND link.saber_move = a.saber_move AND link.event_type = 'attack_chain'
  AND link.note = 'accepted_link')) AS explicit_accepted_links
FROM attacks a WHERE note = 'accepted'
 AND CAST(rel_time AS INTEGER) - CAST(previous_time AS INTEGER) BETWEEN 1 AND 1000
 AND saber_stance = previous_stance AND opponent_key = previous_opponent
 AND CAST(previous_move AS INTEGER) BETWEEN 4 AND 10
 AND CAST(saber_move AS INTEGER) BETWEEN 4 AND 10
GROUP BY 1,2 ORDER BY 1,3 DESC"""),
		("Human accepted three-swing windows (sliding observations, not causal combo outcomes)", """
WITH linked AS (
 SELECT a.*, EXISTS(SELECT 1 FROM events link WHERE link.batch = a.batch
  AND link.source_context = a.source_context AND link.parent_id = a.parent_id
  AND link.participant_key = a.participant_key AND link.rel_time = a.rel_time
  AND link.saber_move = a.saber_move AND link.event_type = 'attack_chain'
  AND link.note = 'accepted_link') AS accepted_link
 FROM events a WHERE event_type = 'swing_start' AND participant_kind IN ('1','2')
), triples AS (
 SELECT *, lag(saber_move, 2) OVER w AS first_move, lag(saber_move) OVER w AS second_move,
 lag(rel_time, 2) OVER w AS first_time, lag(rel_time) OVER w AS second_time,
 lag(saber_stance, 2) OVER w AS first_stance, lag(saber_stance) OVER w AS second_stance,
 lag(opponent_key, 2) OVER w AS first_opponent, lag(opponent_key) OVER w AS second_opponent,
 lag(note, 2) OVER w AS first_note, lag(note) OVER w AS second_note,
 lag(accepted_link) OVER w AS second_link
 FROM linked WINDOW w AS (PARTITION BY batch,source_context,parent_id,participant_key
 ORDER BY CAST(rel_time AS INTEGER),CAST(event_index AS INTEGER))
)
SELECT batch, saber_stance, first_move || '>' || second_move || '>' || saber_move AS sequence,
 count(*) AS observed_triples, sum(second_link AND accepted_link) AS explicit_linked_triples
FROM triples WHERE note = 'accepted' AND first_note = 'accepted' AND second_note = 'accepted'
 AND CAST(second_time AS INTEGER) - CAST(first_time AS INTEGER) BETWEEN 1 AND 1000
 AND CAST(rel_time AS INTEGER) - CAST(second_time AS INTEGER) BETWEEN 1 AND 1000
 AND saber_stance = first_stance AND saber_stance = second_stance
 AND opponent_key = first_opponent AND opponent_key = second_opponent
 AND CAST(first_move AS INTEGER) BETWEEN 4 AND 10
 AND CAST(second_move AS INTEGER) BETWEEN 4 AND 10 AND CAST(saber_move AS INTEGER) BETWEEN 4 AND 10
GROUP BY 1,2,3 ORDER BY 1,4 DESC,2,3"""),
		("Human accepted-start annotation coverage", """
SELECT batch, note, count(*) AS starts FROM events
WHERE participant_kind IN ('1','2') AND event_type = 'swing_start'
GROUP BY 1,2 ORDER BY 1,3 DESC"""),
		("Learned action-token compatibility, kept separate by batch/source/mode", """
SELECT batch, source, (ctx_key >> 16) & 3 AS mode,
 (ctx_key & 1610612736) = 1610612736 AS current_format,
 count(*) AS rows, sum(samples) AS samples
FROM learned GROUP BY 1,2,3,4 ORDER BY 1,2,3,4"""),
		("Learned aggregate snapshot overlap (never sum overlapping exports)", """
SELECT a.batch, b.batch, count(*) AS overlapping_sequence_keys,
 sum(a.samples = b.samples AND a.wins = b.wins AND a.net_damage = b.net_damage) AS identical_aggregates,
 sum(a.samples != b.samples OR a.wins != b.wins OR a.net_damage != b.net_damage) AS changed_aggregates
FROM learned a JOIN learned b ON a.batch < b.batch AND a.source = b.source
 AND a.skill_band = b.skill_band AND a.ctx_key = b.ctx_key AND a.stimulus = b.stimulus
 AND a.response = b.response AND a.follow1 = b.follow1 AND a.follow2 = b.follow2
GROUP BY 1,2"""),
	]
	for title, query in queries:
		print("\n## " + title + "\n```sql\n" + query.strip() + "\n```")
		cursor = con.execute(query)
		print(json.dumps([d[0] for d in cursor.description]))
		for row in cursor:
			print(json.dumps(row))
	print("\nDeduplication: newest export schema is selected only for agreeing core observations. "
		"Core conflicts and differing payloads within one schema are explicitly flagged and "
		"excluded from swing counts, not silently resolved. All raw payloads and distinct core "
		"variants remain in raw/event_variants. Schema-upgrade defaults are not extra observations.\n"
		"Gaps: accepted pairs/triples are overlapping observational windows, not proof of an "
		"effective or intentional combo. No player/chat identities are printed. "
		"Zero exact session-row overlap does not rule out the same duel with changed metadata. "
		"Session IDs are reused across captures: all event/session joins and deduplication "
		"are scoped to source batch, never global record_id or parent_id. "
		"Route and learned aggregates are historical snapshots, never independent batches to sum. "
		"legacy missing animation fields cannot establish active-window timing. Net yaw_sweep "
		"is not oscillation evidence. No identity overlap does not exclude replay under new IDs. "
		"Learned action tokens do not identify saber moves or techniques. "
		"Confirmed directional hits are joined by captured start timestamp, actual move and opponent. "
		"No confirmed hit is not proof of a miss: interrupted/partial capture is possible. "
		"These individual attack outcomes do not demonstrate complete recipe effectiveness.")


def throw_evidence(con):
	"""Measure flight and sampled inputs separately; never infer button-release times."""
	con.execute("""CREATE INDEX throw_event_lookup ON events
		(batch,source_context,parent_id,participant_key,opponent_key,event_type,rel_time)""")
	con.execute("""CREATE TABLE throw_conflicts AS
		SELECT DISTINCT v.batch,v.source_context,v.parent_id,v.participant_key,v.opponent_key,
		 CAST(v.rel_time AS INTEGER) AS at FROM event_variants v
		JOIN conflicts c ON c.batch=v.batch AND c.identity=v.identity
		WHERE v.event_type IN ('throw_start','throw_end')""")
	con.execute("""CREATE INDEX throw_conflict_lookup ON throw_conflicts
		(batch,source_context,parent_id,participant_key,opponent_key,at)""")
	con.execute("""CREATE TABLE flights AS
		WITH boundaries AS (
		 SELECT *, lead(event_type) OVER w AS next_type,
		  lead(CAST(rel_time AS INTEGER)) OVER w AS ended,
		  lead(CAST(amount AS INTEGER)) OVER w AS damage
		 FROM events WHERE event_type IN ('throw_start','throw_end')
		 WINDOW w AS (PARTITION BY batch,source_context,parent_id,participant_key,opponent_key
		  ORDER BY CAST(rel_time AS INTEGER),CAST(event_index AS INTEGER)))
		SELECT *, CAST(rel_time AS INTEGER) AS started,
		 ended - CAST(rel_time AS INTEGER) AS flight_ms FROM boundaries
		WHERE participant_kind IN ('1','2') AND event_type = 'throw_start'
		 AND next_type = 'throw_end' AND ended > CAST(rel_time AS INTEGER)
		 AND NOT EXISTS (SELECT 1 FROM throw_conflicts c WHERE c.batch=boundaries.batch
		  AND c.source_context=boundaries.source_context AND c.parent_id=boundaries.parent_id
		  AND c.participant_key=boundaries.participant_key AND c.opponent_key=boundaries.opponent_key
		  AND c.at BETWEEN CAST(boundaries.rel_time AS INTEGER) AND boundaries.ended)""")
	con.execute("""CREATE TABLE geometry AS SELECT DISTINCT * FROM geometry_raw
		GROUP BY batch,source_context,parent_id,participant_key,opponent_key,rel_time
		HAVING count(DISTINCT self_yaw || ':' || distance) = 1""")
	con.execute("""CREATE INDEX throw_geometry_lookup ON geometry
		(batch,source_context,parent_id,participant_key,opponent_key,rel_time)""")
	queries = [
		("Human capture coverage (not all starts have a recorded end)", """
SELECT batch, event_type, count(*) AS observations FROM events
WHERE participant_kind IN ('1','2')
 AND event_type IN ('throw_start','throw_end','input_start','input_end','attack_sample')
GROUP BY 1,2 ORDER BY 1,2"""),
		("Human complete throw airtime, not alt-input duration", """
SELECT batch, count(*) AS complete_flights, round(avg(flight_ms),1) AS mean_airtime_ms,
 min(flight_ms) AS min_ms, max(flight_ms) AS max_ms,
 sum(flight_ms >= 1500) AS flights_ge_1500ms,
 sum(flight_ms >= 3000) AS flights_ge_3000ms,
 sum(flight_ms >= 3000 AND damage > 0) AS damaging_flights_ge_3000ms
FROM flights GROUP BY 1"""),
		("Sampled alt input during complete human flights (128 = BUTTON_ALT_ATTACK)", """
WITH sampled AS (
 SELECT f.batch,f.identity,f.flight_ms,
  count(e.identity) AS samples,
  sum((CAST(e.buttons AS INTEGER) & 128) != 0) AS held_samples,
  min(CASE WHEN (CAST(e.buttons AS INTEGER) & 128) != 0
   THEN CAST(e.rel_time AS INTEGER) END) AS first_held,
  max(CASE WHEN (CAST(e.buttons AS INTEGER) & 128) != 0
   THEN CAST(e.rel_time AS INTEGER) END) AS last_held,
  sum((CAST(e.buttons AS INTEGER) & 128) != 0 AND CAST(e.forwardmove AS INTEGER) > 0) AS forward_held
 FROM flights f LEFT JOIN events e ON e.batch=f.batch AND e.source_context=f.source_context
  AND e.parent_id=f.parent_id AND e.participant_key=f.participant_key
  AND e.opponent_key=f.opponent_key AND e.event_type='attack_sample'
  AND CAST(e.rel_time AS INTEGER) BETWEEN f.started AND f.ended
 GROUP BY f.batch,f.identity)
SELECT batch,sum(samples > 0) AS sampled_flights,sum(samples) AS samples,
 sum(held_samples) AS held_samples,sum(forward_held) AS forward_held_samples,
 sum(last_held-first_held >= 1500) AS held_observations_span_ge_1500ms,
 sum(last_held-first_held >= 3000) AS held_observations_span_ge_3000ms
FROM sampled GROUP BY 1"""),
		("Human turning/closing and opponent drain during flight (observational, not a motor recipe)", """
WITH steps AS (
 SELECT f.batch,f.identity,f.started,f.ended,g.rel_time,g.self_yaw,g.distance,
  lag(g.self_yaw) OVER w AS previous_yaw,
  first_value(g.distance) OVER w AS first_distance,
  last_value(g.distance) OVER (PARTITION BY f.batch,f.identity ORDER BY g.rel_time
   ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING) AS last_distance
 FROM flights f JOIN geometry g ON g.batch=f.batch AND g.source_context=f.source_context
  AND g.parent_id=f.parent_id AND g.participant_key=f.participant_key
  AND g.opponent_key=f.opponent_key AND g.rel_time BETWEEN f.started AND f.ended
 WINDOW w AS (PARTITION BY f.batch,f.identity ORDER BY g.rel_time)),
 turns AS (
 SELECT batch,identity,count(*) AS samples,
  sum(abs((self_yaw-previous_yaw+540) % 360 - 180)) AS cumulative_turn,
  max(first_distance-last_distance) AS closed_distance FROM steps GROUP BY 1,2),
 context AS (
 SELECT f.batch,f.identity,
  EXISTS(SELECT 1 FROM events e WHERE e.batch=f.batch AND e.source_context=f.source_context
   AND e.parent_id=f.parent_id AND e.participant_key=f.opponent_key
   AND e.opponent_key=f.participant_key AND e.event_type='force' AND e.power='3'
   AND CAST(e.rel_time AS INTEGER) BETWEEN f.started AND f.ended) AS opponent_drain,
  EXISTS(SELECT 1 FROM events e WHERE e.batch=f.batch AND e.source_context=f.source_context
   AND e.parent_id=f.parent_id AND e.participant_key=f.participant_key
   AND e.opponent_key=f.opponent_key AND e.event_type='damage_dealt' AND e.damage_source='2'
   AND CAST(e.rel_time AS INTEGER) BETWEEN f.started AND f.ended+1000) AS nearby_melee
 FROM flights f)
SELECT t.batch,count(*) AS geometry_flights,
 sum(cumulative_turn >= 360) AS accumulated_turn_ge_360deg,
 sum(opponent_drain) AS opponent_drain_flights,
 sum(opponent_drain AND closed_distance > 32) AS drain_flights_closing_gt_32u,
 sum(opponent_drain AND cumulative_turn >= 360 AND closed_distance > 32) AS turning_closing_drain,
 sum(opponent_drain AND cumulative_turn >= 360 AND closed_distance > 32 AND nearby_melee)
  AS turning_closing_drain_near_melee
FROM turns t JOIN context c ON c.batch=t.batch AND c.identity=t.identity GROUP BY 1"""),
	]
	for title, query in queries:
		print("\n## " + title + "\n```sql\n" + query.strip() + "\n```")
		cursor = con.execute(query)
		print(json.dumps([d[0] for d in cursor.description]))
		for row in cursor:
			print(json.dumps(row))
	print("\nLimits: batches remain separate; conflicting event identities are excluded. "
		"Only adjacent start/end boundaries with the same duel, actor and opponent are paired. "
		"No input_end is captured: held-sample spans do not establish continuous alt holds, "
		"and airtime includes the saber return. Geometry yaw steps are shortest-path sampled "
		"turns, not proof of a deliberate circle or continuous spinning. Closing uses the first "
		"and last available geometry samples, not exact release/return positions. MOD_MELEE "
		"does not distinguish flipkicks. Opponent drain spend is not proof of drain denial. "
		"These observations cannot establish a causal spin/hold/kick technique or its efficacy.")


QUERIES = [
	("Swing starts and roll-stab use (saber_move 14 = LS_ROLL_STAB) by participant kind", """
SELECT participant_kind,
	count(*) AS swing_starts,
	count(*) FILTER (WHERE saber_move = '14') AS roll_stabs
FROM events WHERE event_type = 'swing_start' {rev}
GROUP BY 1 ORDER BY 1"""),
	("Roll-stab rows (own or enemy), without player identities", """
SELECT participant_kind, opponent_kind, event_type, quality, note, self_force, amount
FROM events WHERE (saber_move = '14' OR enemy_saber_move = '14')
	AND event_type NOT IN ('regen', 'range', 'air', 'chat') {rev}
ORDER BY participant_kind, TRY_CAST(rel_time AS INTEGER)"""),
	("Close-range (<128u) swing-start footing input", """
SELECT participant_kind,
	count(*) AS n,
	count(*) FILTER (WHERE TRY_CAST(forwardmove AS INTEGER) < 0) AS back_input,
	count(*) FILTER (WHERE TRY_CAST(forwardmove AS INTEGER) > 0) AS forward_input,
	round(avg(TRY_CAST(radial_speed AS DOUBLE)), 1) AS avg_radial_speed
FROM events
WHERE event_type = 'swing_start' AND range_bucket = '1' AND forwardmove IS NOT NULL {rev}
GROUP BY 1 ORDER BY 1"""),
	("Close-range self_footing share (events that record footing)", """
SELECT participant_kind, self_footing, count(*) AS n,
	round(100.0 * count(*) / sum(count(*)) OVER (PARTITION BY participant_kind), 1) AS pct
FROM events
WHERE range_bucket = '1' AND self_footing IN ('0', '1', '2', '3') {rev}
GROUP BY 1, 2 ORDER BY 1, 2"""),
	("Attack chains per close-range swing end, and accepted links", """
SELECT participant_kind,
	count(*) FILTER (WHERE event_type = 'swing_end') AS swing_ends,
	count(*) FILTER (WHERE event_type = 'attack_chain') AS chains,
	round(count(*) FILTER (WHERE event_type = 'attack_chain') * 1.0 /
		nullif(count(*) FILTER (WHERE event_type = 'swing_end'), 0), 2) AS chains_per_swing,
	count(*) FILTER (WHERE event_type = 'attack_chain' AND note = 'accepted_link') AS accepted_links
FROM events WHERE range_bucket = '1' {rev}
GROUP BY 1 ORDER BY 1"""),
	("Bot back-input swing starts by controller family (5 = BURST)", """
SELECT controller_owns_inputs, controller_family, count(*) AS back_input_swings
FROM events
WHERE event_type = 'swing_start' AND participant_kind = '0' AND range_bucket = '1'
	AND TRY_CAST(forwardmove AS INTEGER) < 0 AND controller_family IS NOT NULL {rev}
GROUP BY 1, 2 ORDER BY 3 DESC"""),
	("Capture revisions present (bots)", """
SELECT capture_revision, count(*) AS events
FROM events WHERE participant_kind = '0' AND capture_revision IS NOT NULL
GROUP BY 1 ORDER BY 2 DESC"""),
	("Top current-format learned sequences by source and mode (samples >= 20)", """
SELECT source, (CAST(ctx_key AS BIGINT) >> 16) & 3 AS mode,
	stimulus || '>' || response || '>' || follow1 || '>' || follow2 AS sequence,
	sum(CAST(samples AS INTEGER)) AS samples,
	round(100.0 * sum(CAST(wins AS INTEGER)) / sum(CAST(samples AS INTEGER)), 0) AS win_pct,
	round(sum(CAST(net_damage AS DOUBLE)) / sum(CAST(samples AS INTEGER)), 1) AS net_dmg_per_sample
FROM learned
WHERE (CAST(ctx_key AS BIGINT) & 1610612736) = 1610612736
GROUP BY 1, 2, 3 HAVING sum(CAST(samples AS INTEGER)) >= 20
ORDER BY 1, 2, 4 DESC LIMIT 40"""),
	("Learned samples per token (current format)", """
SELECT source, tok, sum(CAST(samples AS INTEGER)) AS samples
FROM (SELECT source, samples, ctx_key, unnest([stimulus, response, follow1, follow2]) AS tok FROM learned)
WHERE (CAST(ctx_key AS BIGINT) & 1610612736) = 1610612736
GROUP BY 1, 2 ORDER BY 1, 3 DESC"""),
]


def to_utf8(paths, outdir):
	converted = []
	for path in paths:
		target = os.path.join(outdir, os.path.basename(path))
		with open(path, "rb") as src:
			data = src.read()
		try:
			data.decode("utf-8")
		except UnicodeDecodeError:
			data = data.decode("latin-1").encode("utf-8")
		with open(target, "wb") as dst:
			dst.write(data)
		converted.append(target)
	return converted


def load(con, name, paths):
	if not paths:
		return False
	files = ", ".join("'" + p.replace("'", "''") + "'" for p in paths)
	con.execute(f"""CREATE TABLE {name} AS SELECT DISTINCT * FROM read_csv([{files}],
		union_by_name = true, all_varchar = true, header = true)""")
	return True


def main():
	parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
	parser.add_argument("csv_dir", nargs="?", default=".")
	parser.add_argument("--revision", help="only events with this capture_revision")
	parser.add_argument("--saber-evidence", action="store_true",
		help="stdlib CSV/SQLite batch provenance and saber evidence (prints exact SQL)")
	parser.add_argument("--throw-evidence", action="store_true",
		help="stdlib CSV/SQLite human throw airtime, sampled inputs and movement (prints exact SQL)")
	parser.add_argument("--release-dir", help="separately downloaded Dueltracks1 CSV directory")
	args = parser.parse_args()

	if args.saber_evidence or args.throw_evidence:
		if args.saber_evidence and args.throw_evidence:
			parser.error("choose either --saber-evidence or --throw-evidence")
		if args.revision:
			parser.error("--revision is for the general report; evidence reports preserve every capture revision")
		batches = [("repository", args.csv_dir)]
		if args.release_dir:
			batches.append(("Dueltracks1", args.release_dir))
		saber_evidence(batches, throws_only=args.throw_evidence)
		return
	try:
		import duckdb
	except ImportError:
		sys.exit("duckdb is required for the general report: pip install duckdb")
	con = duckdb.connect()
	with tempfile.TemporaryDirectory() as tmp:
		events = to_utf8(sorted(glob.glob(os.path.join(args.csv_dir, "dueltrack_events*.csv"))), tmp)
		learned = to_utf8(sorted(glob.glob(os.path.join(args.csv_dir, "dueltrack_learned.csv"))), tmp)
		if not load(con, "events", events):
			sys.exit(f"no dueltrack_events*.csv in {args.csv_dir}")
		have_learned = load(con, "learned", learned)

	rev = ""
	if args.revision:
		rev = "AND capture_revision = '" + args.revision.replace("'", "''") + "'"

	for title, sql in QUERIES:
		if "FROM learned" in sql and not have_learned:
			continue
		query = sql.format(rev=rev)
		print("## " + title)
		print("```sql" + query + "\n```")
		print(con.sql(query))
		print()


if __name__ == "__main__":
	main()
