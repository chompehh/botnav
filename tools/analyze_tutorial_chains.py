#!/usr/bin/env python3
"""Reproduce bounded, identity-free Dueltracks analysis from local CSVs.

Usage: python3 tools/analyze_tutorial_chains.py INPUT_DIRECTORY
See docs/tutorial-sequence-evidence.md for provenance and capture limitations.
"""
import collections
import csv
import pathlib
import statistics
import sys

if len(sys.argv) != 2:
    raise SystemExit("Usage: analyze_tutorial_chains.py INPUT_DIRECTORY")
ROOT = pathlib.Path(sys.argv[1])
FILES = ("events-prefix.csv", "events-part2-prefix.csv", "events-part3-prefix.csv")
POWER = {"0": "push", "1": "pull", "2": "grip", "3": "drain",
         "4": "rage", "5": "absorb", "6": "protect", "7": "heal",
         "8": "speed", "9": "seeing"}
STATS = ("self_hp", "self_armor", "self_force",
         "enemy_hp", "enemy_armor", "enemy_force")


def read(name):
    with (ROOT / name).open(newline="", encoding="utf-8-sig") as file:
        yield from csv.DictReader(file)


def nums(row):
    try:
        vals = tuple(int(row[k]) for k in STATS)
        return vals if all(v >= 0 for v in vals) else None
    except ValueError:
        return None


def aligned(row, actor):
    values = nums(row)
    if values is None:
        return None
    return values if row["participant_key"] == actor else values[3:] + values[:3]


def median(values):
    return round(statistics.median(values), 1)


sessions = {r["record_id"]: r for r in read("sessions.csv")}
participants = collections.defaultdict(list)
for row in read("participants.csv"):
    participants[row["summary_id"]].append(row)

events = collections.defaultdict(list)
versions = collections.Counter()
rows_total = 0
mispaired = 0
for filename in FILES:
    for row in read(filename):
        rows_total += 1
        versions[(row["export_format_version"], row["capture_version"],
                  row["capture_revision"])] += 1
        if row["source_context"] != "duel":
            continue
        sid = row["parent_id"]
        if sid not in sessions or not any(
                row["participant_key"] == p["participant_key"]
                and row["opponent_key"] == p["opponent_key"]
                for p in participants[sid]):
            mispaired += 1
            continue
        events[sid].append(row)

complete = []
for sid, rows in events.items():
    pp = participants[sid]
    counts = collections.Counter(r["participant_key"] for r in rows)
    if (len(pp) == 2 and pp[0]["participant_key"] == pp[1]["opponent_key"]
            and pp[1]["participant_key"] == pp[0]["opponent_key"]
            and all(counts[p["participant_key"]] == int(p["event_retained"] or 0)
                    and int(p["event_dropped"] or 0) == 0
                    and int(p["event_critical_dropped"] or 0) == 0
                    for p in pp)):
        complete.append(sid)
full = [sid for sid in complete if sessions[sid]["type"] == "1"]
print("parts", len(FILES), "rows", rows_total, "versions", dict(versions))
print("sample duel sessions", len(events), "complete-undropped", len(complete),
      "full-force-complete", len(full), "mispaired rows", mispaired)
print("full-force participant kinds", dict(collections.Counter(
    "bot" if p["participant_kind"] == "0" else "human" if p["participant_kind"] in ("1", "2") else "unknown"
    for sid in full for p in participants[sid])))
print("full-force duel matchups", dict(collections.Counter(
    "human-human" if all(p["participant_kind"] in ("1", "2") for p in participants[sid])
    else "human-bot" if any(p["participant_kind"] == "0" for p in participants[sid])
    and any(p["participant_kind"] in ("1", "2") for p in participants[sid])
    else "bot-bot-or-unknown" for sid in full)))

windows = []
actions_count = collections.Counter()
force_fallback = 0
for sid in full:
    ordered = sorted(events[sid], key=lambda r: (int(r["rel_time"]), int(r["record_id"])))
    kinds = {p["participant_key"]: ("bot" if p["participant_kind"] == "0"
                                      else "human" if p["participant_kind"] in ("1", "2")
                                      else "unknown") for p in participants[sid]}
    actions = []
    last_force = {}
    for pos, r in enumerate(ordered):
        t = int(r["rel_time"])
        label = None
        if r["event_type"] == "force":
            if r["note"] == "selected_fallback":
                force_fallback += 1
            elif r["note"] == "" and r["power"] in POWER:
                # State-supported bursts are not counted power activations.
                key = (r["participant_key"], r["power"])
                previous = last_force.get(key)
                last_force[key] = t
                if previous is None or t - previous >= 800:
                    label = POWER[r["power"]]
        elif r["event_type"] == "swing_start" and r["note"] == "accepted":
            label = "swing"
        elif r["event_type"] == "throw_start" and r["note"] == "release":
            label = "throw"
        if label:
            actions_count[label] += 1
            actions.append((t, pos, r["participant_key"], label))

    # Disjoint action-index windows, capped at eight actions and twelve seconds.
    i = 0
    while i < len(actions):
        stop = i
        while (stop < len(actions) and stop - i < 8
               and actions[stop][0] - actions[i][0] <= 12000):
            stop += 1
        chunk = actions[i:stop]
        actor = chunk[0][2]
        if len(chunk) < 4 or sum(a[2] == actor for a in chunk) < 2 or sum(a[2] != actor for a in chunk) < 2:
            i += 1
            continue
        first_t, first_pos = chunk[0][:2]
        before = None
        for r in reversed(ordered[:first_pos]):
            dt = first_t - int(r["rel_time"])
            if dt > 1000:
                break
            if dt > 0 and aligned(r, actor) is not None:
                before = aligned(r, actor)
                break
        after = aligned(ordered[chunk[-1][1]], actor)
        if before is not None and after is not None:
            seq = tuple(("A:" if a[2] == actor else "O:") + a[3] for a in chunk)
            own = [a[3] for a in chunk if a[2] == actor]
            opp = [a[3] for a in chunk if a[2] != actor]
            delta = tuple(b - a for a, b in zip(before, after))
            windows.append((sid, kinds[actor], seq, own, opp, before, delta,
                            chunk[-1][0] - first_t, first_t))
        i = stop

print("retained action bursts", dict(actions_count), "fallback force excluded", force_fallback)
print("eligible nonoverlap windows with baseline", len(windows),
      "unique duels", len({w[0] for w in windows}),
      "human/bot", dict(collections.Counter(w[1] for w in windows)))
print("window action counts", dict(collections.Counter(len(w[2]) for w in windows)))


def report(name, selected):
    if not selected:
        return
    print("\n", name, "windows", len(selected), "duels", len({w[0] for w in selected}),
          "actor kind", dict(collections.Counter(w[1] for w in selected)),
          "median delta actor HP/AP/FP, opponent HP/AP/FP",
          tuple(median([w[6][i] for w in selected]) for i in range(6)))
    print("  actor HP delta negative/zero/positive",
          tuple(sum((w[6][0] < 0, w[6][0] == 0, w[6][0] > 0)[i] for w in selected)
                for i in range(3)),
          "actor FP delta negative/zero/positive",
          tuple(sum((w[6][2] < 0, w[6][2] == 0, w[6][2] > 0)[i] for w in selected)
                for i in range(3)))


def later(own, a, b):
    return a in own and b in own[own.index(a) + 1:]


report("all", windows)
report("actor pull then throw", [w for w in windows if later(w[3], "pull", "throw")])
report("actor pull then swing", [w for w in windows if later(w[3], "pull", "swing")])
report("actor grip then swing or throw", [w for w in windows
       if later(w[3], "grip", "swing") or later(w[3], "grip", "throw")])
report("actor swing plus pull", [w for w in windows if "swing" in w[3] and "pull" in w[3]])
report("actor speed then throw with opponent drain", [w for w in windows
       if later(w[3], "speed", "throw") and "drain" in w[4]])
report("actor baseline FP below 20 and opponent drain", [w for w in windows
       if w[5][2] < 20 and "drain" in w[4]])
report("late (second half) actor baseline FP below 20 and opponent drain", [w for w in windows
       if w[8] >= int(sessions[w[0]]["duration"]) * 500
       and w[5][2] < 20 and "drain" in w[4]])
report("actor starts behind HP and has >=2 swing/throw/push/pull/grip bursts",
       [w for w in windows if w[5][0] < w[5][3]
        and sum(a in ("swing", "throw", "push", "pull", "grip") for a in w[3]) >= 2])
report("actor drain, opponent drain", [w for w in windows if "drain" in w[3] and "drain" in w[4]])
report("actor pull or grip and opponent drain", [w for w in windows
       if ("pull" in w[3] or "grip" in w[3]) and "drain" in w[4]])

templates = collections.Counter(w[2] for w in windows)
print("\nrepeated exact action templates, first 20 (A=actor, O=opponent)")
for seq, n in templates.most_common(20):
    if n < 2:
        break
    distinct = len({w[0] for w in windows if w[2] == seq})
    selected = [w for w in windows if w[2] == seq]
    print(n, "duels", distinct, " ".join(seq), "median deltas",
          tuple(median([w[6][i] for w in selected]) for i in range(6)))
