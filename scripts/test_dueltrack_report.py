import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path

from dueltrack_report import export_family, print_batch_inventory


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


if __name__ == "__main__":
	unittest.main()
