#!/usr/bin/env python3
"""Report one downloaded strafe capture, printing every SQL query with its result."""

import argparse
import csv
import json
import math
import sqlite3
from pathlib import Path

from dueltrack_report import csv_provenance


def report(directory):
	con = sqlite3.connect(":memory:")
	con.execute("""CREATE TABLE routes(id INTEGER PRIMARY KEY, mapname TEXT, bot INTEGER,
		start_speed REAL, max_speed REAL, end_speed REAL, distance REAL, duration_ms INTEGER,
		node_count INTEGER)""")
	con.execute("""CREATE TABLE nodes(route_id INTEGER, node_index INTEGER, side_only_pct REAL,
		switch_pct REAL, air_yaw_rate REAL, force_jump INTEGER, takeoff_known INTEGER,
		ground_ms INTEGER, PRIMARY KEY(route_id,node_index))""")
	con.execute("""CREATE TABLE inputs(route_id INTEGER, frame INTEGER, time_ms INTEGER,
		grounded INTEGER, speed REAL, yaw_rate REAL, wish_offset REAL, velocity_source TEXT,
		style TEXT, PRIMARY KEY(route_id,frame))""")

	def rows(path):
		digest, encoding = csv_provenance(str(path))
		print(json.dumps({"file": path.name, "sha256": digest, "encoding": encoding}))
		with path.open(encoding=encoding, newline="") as src:
			yield from csv.DictReader(src)

	humans = set()
	for row in rows(directory / "strafetrack_routes.csv"):
		route_id = int(row["id"])
		bot = int(row["bot"])
		if not bot:
			humans.add(route_id)
		con.execute("INSERT INTO routes VALUES (?,?,?,?,?,?,?,?,?)", (
			route_id, row["mapname"], bot, float(row["start_speed"]),
			float(row["max_speed"]), float(row["end_speed"]),
			float(row["distance"]), int(row["duration_ms"]), int(row["node_count"])))
	for row in rows(directory / "strafetrack_nodes.csv"):
		con.execute("INSERT INTO nodes VALUES (?,?,?,?,?,?,?,?)", (
			int(row["route_id"]), int(row["node_index"]),
			float(row["side_only_pct"]) if row["side_only_pct"] else None,
			float(row["switch_pct"]) if row["switch_pct"] else None,
			float(row["air_yaw_rate"]) if row["air_yaw_rate"] else None,
			int(row["force_jump"]) if row.get("force_jump") else None,
			int(all(row.get("takeoff_" + axis) for axis in "xyz")),
			int(row["ground_ms"]) if row.get("ground_ms") else None))
	previous = {}
	total = 0
	for path in sorted(directory.glob("strafetrack_inputs*.csv")):
		batch = []
		for row in rows(path):
			total += 1
			route_id, frame, time = int(row["route_id"]), int(row["frame"]), int(row["time_ms"])
			x, y = float(row["x"]), float(row["y"])
			prev = previous.get(route_id)
			previous[route_id] = frame, time, x, y
			msec = int(row["msec"])
			forward, right = int(row["forwardmove"]), int(row["rightmove"])
			if route_id not in humans or not (forward or right) or not 0 < msec <= 250:
				continue
			if row.get("vel_x") and row.get("vel_y"):
				vx, vy = float(row["vel_x"]), float(row["vel_y"])
				source = "recorded_velocity"
			elif prev and prev[0] + 1 == frame and 0 < time - prev[1] <= 250:
				vx, vy = (x - prev[2]) * 1000 / (time - prev[1]), (y - prev[3]) * 1000 / (time - prev[1])
				source = "displacement_estimate"
			else:
				continue
			if math.hypot(vx, vy) < 100:
				continue
			# JKA's right axis is clockwise. These are command/post-pmove samples.
			wish = float(row["yaw"]) - math.degrees(math.atan2(right, forward))
			offset = abs((wish - math.degrees(math.atan2(vy, vx)) + 180) % 360 - 180)
			style = "inverse" if forward < 0 else "side_only" if not forward else "forward_only" if not right else "full"
			batch.append((route_id, frame, time, int(row["grounded"]), float(row["speed"]),
				abs(float(row["yaw_delta"])) * 1000 / msec, offset, source, style))
			if len(batch) == 2048:
				con.executemany("INSERT INTO inputs VALUES (?,?,?,?,?,?,?,?,?)", batch)
				batch.clear()
		if batch:
			con.executemany("INSERT INTO inputs VALUES (?,?,?,?,?,?,?,?,?)", batch)
	print(json.dumps({"input_rows_read": total,
		"limits": "One capture only. Missing velocity is explicitly estimated from consecutive positions. "
		"Post-pmove offsets are not pre-acceleration angles; key styles do not prove a named technique. "
		"Route CSVs do not contain .botroute_stats failure/cooldown evidence."}))
	queries = [
		"""SELECT lower(mapname) mapname, count(*) human_routes,
			sum(node_count BETWEEN 1 AND 2) short_routes,
			sum(node_count BETWEEN 1 AND 2 AND EXISTS (
				SELECT 1 FROM nodes WHERE route_id=routes.id AND node_index=0 AND force_jump=1))
				short_force_starts FROM routes WHERE bot=0 GROUP BY 1 ORDER BY 1""",
		"""SELECT lower(mapname) mapname, count(DISTINCT routes.id) affected_human_routes,
			count(*) takeoffs_with_unknown_duration FROM routes JOIN nodes ON routes.id=nodes.route_id
			WHERE routes.bot=0 AND takeoff_known=1 AND (ground_ms IS NULL OR ground_ms=-1)
			GROUP BY 1 ORDER BY 1""",
		"""SELECT count(*) routes, count(DISTINCT mapname) maps,
			round(avg(start_speed),1) mean_start, round(avg(max_speed),1) mean_peak,
			round(avg(end_speed),1) mean_end, sum(max_speed > start_speed) peak_exceeds_start
			FROM routes WHERE bot=0""",
		"""SELECT CASE WHEN side_only_pct IS NULL THEN 'unknown'
			WHEN side_only_pct < 10 THEN '0..9' WHEN side_only_pct < 60 THEN '10..59'
			WHEN side_only_pct < 90 THEN '60..89' ELSE '90..100' END side_only_pct_bin,
			count(*) nodes FROM nodes JOIN routes ON routes.id=nodes.route_id
			WHERE routes.bot=0 GROUP BY 1 ORDER BY 1""",
		"""SELECT velocity_source, style, count(*) samples FROM inputs
			WHERE grounded=0 GROUP BY 1,2 ORDER BY 1,2""",
		"""SELECT velocity_source, count(*) fast_turn_samples,
			round(100.0*sum(wish_offset>74)/count(*),1) pct_offset_over_74,
			round(avg(wish_offset),1) mean_offset FROM inputs
			WHERE grounded=0 AND yaw_rate>=45 GROUP BY 1""",
		"""SELECT velocity_source, wish_offset p90_fast_turn_offset FROM (
			SELECT velocity_source, wish_offset,
			row_number() OVER (PARTITION BY velocity_source ORDER BY wish_offset) n,
			count(*) OVER (PARTITION BY velocity_source) total FROM inputs
			WHERE grounded=0 AND yaw_rate>=45)
			WHERE n=CAST((total-1)*0.9 AS INTEGER)+1""",
	]
	for query in queries:
		print("\n```sql\n" + query + "\n```")
		result = con.execute(query)
		print(json.dumps([column[0] for column in result.description]))
		for row in result:
			print(json.dumps(row))
	con.close()


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("csv_dir", type=Path)
	report(parser.parse_args().csv_dir)
