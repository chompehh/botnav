#!/usr/bin/env python3
"""Measure human fast airborne chase/escape episodes from Dueltrack CSVs.

Geometry is associated by duel, actor, capture revision and nearest timestamp
(at most 50 ms), never by row number. Output contains no player identifiers.
"""

import argparse
import bisect
import csv
import json
import math
import warnings
from collections import defaultdict


def rows(path):
    incomplete = 0
    with open(path, encoding="latin1", newline="") as source:
        for row in csv.DictReader(source):
            if None not in row and all(value is not None for value in row.values()):
                yield row
            else:
                incomplete += 1
    if incomplete:
        warnings.warn(f"{path}: skipped {incomplete} incomplete CSV rows", RuntimeWarning)


def identity(row):
    return row["source_context"], row["parent_id"], row["participant_key"], row["capture_revision"]


def quantiles(values):
    ordered = sorted(values)
    if not ordered:
        return None
    return {name: round(ordered[round((len(ordered) - 1) * fraction)], 1)
            for name, fraction in (("p25", .25), ("median", .5), ("p75", .75))}


def summarize(events, geometry):
    tracks = defaultdict(list)
    for row in geometry:
        tracks[identity(row)].append(row)
    times = {}
    for key, track in tracks.items():
        track.sort(key=lambda row: int(row["rel_time"]))
        times[key] = [int(row["rel_time"]) for row in track]
    commands = defaultdict(dict)
    total = matched = 0
    for event in events:
        if event["participant_kind"] not in ("1", "2") or event["event_type"] != "command_sample":
            continue
        if event.get("grounded") not in ("0", "1"):
            continue
        total += 1
        key, now = identity(event), int(event["rel_time"])
        if key not in times:
            continue
        index = bisect.bisect_left(times[key], now)
        candidates = [i for i in (index - 1, index) if 0 <= i < len(times[key])]
        nearest = min(candidates, key=lambda i: abs(times[key][i] - now))
        if abs(times[key][nearest] - now) > 50:
            continue
        matched += 1
        commands[key][now] = event, tracks[key][nearest]
    episodes = defaultdict(list)
    for samples in commands.values():
        previous = None
        active = []
        for now, pair in sorted(samples.items()):
            event, geom = pair
            if previous is None or now - previous > 150 or event["grounded"] == "1":
                if active:
                    note_episode(active, episodes)
                active = []
            if event["grounded"] == "0":
                active.append(pair)
            previous = now
        if active:
            note_episode(active, episodes)
    result = {"human_commands": total, "geometry_matches": matched,
              "duel_actors": len(commands), "max_geometry_gap_ms": 50}
    for kind in ("chase", "escape", "lateral"):
        sample = episodes[kind]
        result[kind] = {
            "episodes": len(sample),
            "distance": quantiles([item[0] for item in sample]),
            "self_radial_speed": quantiles([item[1] for item in sample]),
            "enemy_outward_speed": quantiles([item[2] for item in sample]),
            "hp": quantiles([item[3] for item in sample]),
        }
    return result


def note_episode(samples, episodes):
    if len(samples) < 2:
        return
    if int(samples[-1][0]["rel_time"]) - int(samples[0][0]["rel_time"]) < 100:
        return
    diagonal = sum(abs(float(e["forwardmove"])) > 0 and abs(float(e["rightmove"])) > 0
                   for e, _ in samples)
    if diagonal < 2:
        return
    speeds = [math.hypot(float(g["self_vx"]), float(g["self_vy"])) for _, g in samples]
    if max(speeds) < 400:
        return
    event, geom = samples[speeds.index(max(speeds))]
    dx = float(geom["enemy_x"]) - float(geom["self_x"])
    dy = float(geom["enemy_y"]) - float(geom["self_y"])
    distance = math.hypot(dx, dy)
    if distance < 1:
        return
    toward = (float(geom["self_vx"]) * dx + float(geom["self_vy"]) * dy) / distance
    outward = (float(geom["enemy_vx"]) * dx + float(geom["enemy_vy"]) * dy) / distance
    kind = "chase" if toward >= 100 else "escape" if toward <= -100 else "lateral"
    episodes[kind].append((distance, toward, outward, float(event["self_hp"])))


def learning_priors(learned):
    baselines = defaultdict(lambda: [0., 0.])
    groups = defaultdict(lambda: [0., 0., 0.])
    for row in learned:
        context = int(row["ctx_key"])
        if row["source"] != "human" or context & 0x60000000 != 0x60000000:
            continue
        mode = (context >> 16) & 3
        if mode not in (0, 1):
            continue
        samples, wins = float(row["samples"]), float(row["wins"])
        baselines[mode][0] += samples
        baselines[mode][1] += wins
        if row["response"] == "jump":
            group = groups[mode, row["stimulus"]]
            group[0] += samples
            group[1] += wins
            group[2] += float(row["net_damage"])
    return [{"mode": mode, "stimulus": stimulus, "samples": n, "wins": w,
             "excess_wins": round(w - n * baselines[mode][1] / baselines[mode][0], 2),
             "net_damage": damage}
            for (mode, stimulus), (n, w, damage) in sorted(groups.items())]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--events", nargs="+")
    parser.add_argument("--geometry", nargs="+")
    parser.add_argument("--learned")
    args = parser.parse_args()
    if bool(args.events) != bool(args.geometry) or not (args.events or args.learned):
        parser.error("supply both --events and --geometry, or --learned")
    result = summarize((row for path in args.events for row in rows(path)),
                       (row for path in args.geometry for row in rows(path))) if args.events else {}
    if args.learned:
        result["human_jump_priors"] = learning_priors(rows(args.learned))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
