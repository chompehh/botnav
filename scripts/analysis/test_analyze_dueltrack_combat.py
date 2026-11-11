import unittest

from analyze_dueltrack_combat import flight_features, n, next_paired_end


class CombatAnalysisTests(unittest.TestCase):
	def test_missing_measurements_are_unknown_and_invalid_numbers_fail(self):
		self.assertIsNone(n({}, "buttons"))
		self.assertIsNone(n({"buttons": ""}, "buttons"))
		for value in ("invalid", "nan", "inf"):
			with self.assertRaises(ValueError):
				n({"buttons": value}, "buttons")

	def test_flight_features_follow_button_release_and_off_axis_reacquisition(self):
		flight = [
			{"rel_time": str(t), "buttons": str(buttons), "command_yaw": str(yaw),
			 "command_pitch": "-10", "saber_position_valid": "1",
			 "saber_x": "50", "saber_y": "0", "saber_z": "120"}
			for t, buttons, yaw in ((0, 128, 180), (50, 128, 0), (100, 0, 0))
		]
		geometry = [
			{"rel_time": str(t), "self_x": "0", "self_y": "0",
			 "enemy_x": "100", "enemy_y": "0", "enemy_z": "0", "enemy_yaw": "0"}
			for t in (0, 50, 100)
		]
		result = flight_features(flight, geometry, 0, 100)
		self.assertEqual(result["alt_observed_prefix_ms"], 50)
		self.assertEqual(result["first_observed_alt_off_ms"], 100)
		self.assertEqual(result["longest_observed_alt_run_ms"], 50)
		self.assertTrue(result["full_button_window"])
		self.assertEqual(result["isolation_reacquire_ms"], [50])
		self.assertTrue(result["early_pitch_above_horizon"])
		self.assertTrue(result["saber_rear_24u_seen"])
		self.assertTrue(result["saber_above_96u_seen"])

	def test_capture_gaps_do_not_establish_a_button_release_or_geometry(self):
		flight = [
			{"rel_time": "0", "buttons": "128", "command_yaw": "0"},
			{"rel_time": "200", "buttons": "0", "command_yaw": "180"},
		]
		result = flight_features(flight, [], 0, 200)
		self.assertEqual(result["alt_observed_prefix_ms"], 0)
		self.assertIsNone(result["first_observed_alt_off_ms"])
		self.assertFalse(result["full_button_window"])
		self.assertIsNone(result["max_view_bearing_error"])
		self.assertIsNone(result["early_pitch_above_horizon"])
		self.assertIsNone(result["saber_rear_24u_seen"])

	def test_pairing_does_not_assign_a_later_throws_end_to_a_missing_end(self):
		starts = [{"event_index": "10"}, {"event_index": "20"}]
		ends = [{"event_index": "25"}]
		self.assertIsNone(next_paired_end(starts[0], starts, ends))
		self.assertEqual(next_paired_end(starts[1], starts, ends), ends[0])


if __name__ == "__main__":
	unittest.main()
