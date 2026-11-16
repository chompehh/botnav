import tempfile
import unittest
from pathlib import Path

from analyze_strafe_decisions import learning_priors, rows, summarize


class StrafeDecisionTests(unittest.TestCase):
    def samples(self, parent="1"):
        events, geometry = [], []
        for time in (0, 50, 100, 150):
            common = {"source_context": "duel", "parent_id": parent,
                      "participant_key": "test", "capture_revision": "3",
                      "rel_time": str(time)}
            events.append(dict(common, participant_kind="1", event_type="command_sample",
                               grounded="1" if time == 150 else "0",
                               forwardmove="127", rightmove="127", self_hp="50"))
            geometry.append(dict(common, self_x="0", self_y="0", enemy_x="1000",
                                 enemy_y="0", self_vx="500", self_vy="0",
                                 enemy_vx="450", enemy_vy="0"))
        return events, geometry

    def test_motion_requires_same_duel_and_sustained_diagonal_airborne_input(self):
        events, geometry = self.samples()
        result = summarize(events, geometry)
        self.assertEqual(result["chase"]["episodes"], 1)
        self.assertEqual(result["chase"]["enemy_outward_speed"]["median"], 450)
        self.assertEqual(summarize(events, self.samples("2")[1])["geometry_matches"], 0)
        events[1]["rightmove"] = "0"
        events[2]["rightmove"] = "0"
        self.assertEqual(summarize(events, geometry)["chase"]["episodes"], 0)

    def test_geometry_gap_and_bot_commands_do_not_supply_human_evidence(self):
        events, geometry = self.samples()
        for sample in geometry:
            sample["rel_time"] = str(int(sample["rel_time"]) + 1000)
        self.assertEqual(summarize(events, geometry)["geometry_matches"], 0)
        for event in events:
            event["participant_kind"] = "0"
        self.assertEqual(summarize(events, self.samples()[1])["human_commands"], 0)

    def test_priors_exclude_bots_and_old_contexts_and_compare_with_source_baseline(self):
        base = {"source": "human", "ctx_key": str(0x60010000), "stimulus": "jump",
                "response": "jump", "samples": "10", "wins": "8", "net_damage": "100"}
        learned = [base, dict(base, response="swing", wins="2"),
                   dict(base, source="bot", samples="1000"),
                   dict(base, ctx_key="0", samples="1000")]
        self.assertEqual(learning_priors(learned), [{
            "mode": 1, "stimulus": "jump", "samples": 10, "wins": 8,
            "excess_wins": 3, "net_damage": 100}])

    def test_partial_sample_is_reported_instead_of_silently_discarded(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.csv"
            path.write_text("a,b\n1,2\n3\n", encoding="latin1")
            with self.assertWarnsRegex(RuntimeWarning, "skipped 1 incomplete"):
                self.assertEqual(list(rows(path)), [{"a": "1", "b": "2"}])


if __name__ == "__main__":
    unittest.main()
