import contextlib
import csv
import io
import json
import tempfile
import unittest
from pathlib import Path

from dueltrack_report import export_family, print_batch_inventory
from strafetrack_report import report


class ExportNamesTest(unittest.TestCase):
	def test_release_and_browser_duplicate_names_keep_the_family(self):
		for name in ("dueltrack_geometry.csv", "dueltrack_geometry.2.csv",
			"dueltrack_geometry (2).csv", "dueltrack_geometry_part4.1.csv",
			"dueltrack_geometry_part4 (2).csv"):
			self.assertEqual(export_family(name), "dueltrack_geometry.csv")
		self.assertEqual(export_family("dueltrack_learned_evidence.3.csv"),
			"dueltrack_learned_evidence.csv")

	def test_inventory_recognizes_renamed_release_parts_and_support(self):
		with tempfile.TemporaryDirectory() as directory:
			for name in ("dueltrack_events.2.csv", "dueltrack_events_part2.2.csv",
				"dueltrack_sessions.2.csv"):
				Path(directory, name).touch()
			output = io.StringIO()
			with contextlib.redirect_stdout(output):
				print_batch_inventory("release", directory)
			rows = [json.loads(line) for line in output.getvalue().splitlines()]
			events = next(row for row in rows if row.get("split_family") == "dueltrack_events")
			self.assertEqual(events["present_parts"], [1, 2])
			self.assertEqual(events["missing_internal_parts"], [])
			self.assertNotIn("dueltrack_sessions.csv", rows[-1]["missing_support_files"])

	def test_strafe_metadata_reports_unknown_takeoff_without_input_files(self):
		with tempfile.TemporaryDirectory() as directory:
			route = dict(id="1", mapname="MP/CTF4", bot="0", start_speed="250",
				max_speed="400", end_speed="300", distance="256", duration_ms="1200", node_count="1")
			node = dict(route_id="1", node_index="0", side_only_pct="", switch_pct="",
				air_yaw_rate="", force_jump="1", takeoff_x="0", takeoff_y="0",
				takeoff_z="0", ground_ms="")
			for name, row in (("routes", route), ("nodes", node)):
				with Path(directory, f"strafetrack_{name}.csv").open("w", newline="") as stream:
					writer = csv.DictWriter(stream, row.keys())
					writer.writeheader()
					writer.writerow(row)
			output = io.StringIO()
			with contextlib.redirect_stdout(output):
				report(Path(directory))
			text = output.getvalue()
			self.assertIn('["mp/ctf4", 1, 1, 1]', text)
			self.assertIn('["mp/ctf4", 1, 1]', text)
			self.assertIn("ground_ms IS NULL OR ground_ms=-1", text)
			self.assertIn('"input_rows_read": 0', text)
			self.assertIn("do not contain .botroute_stats", text)


if __name__ == "__main__":
	unittest.main()
