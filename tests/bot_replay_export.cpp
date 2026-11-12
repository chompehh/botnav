#include <cstring>
#include <limits>
#include <vector>

#include "ai_strafejump.h"
#include "g_account.h"

#include <boost/test/unit_test.hpp>

namespace
{
std::vector<bot_sfj_replay_sample_t> replay(int count)
{
	std::vector<bot_sfj_replay_sample_t> samples(count);
	for (int i = 0; i < count; i++)
	{
		std::memset(&samples[i], 0, sizeof(samples[i]));
		samples[i].timeMs = i * 8;
		samples[i].origin[0] = i * 2.0f;
		samples[i].forward = 64;
		samples[i].speed = 250.0f;
	}
	return samples;
}

int groundIntervals(const std::vector<bot_sfj_replay_sample_t>& samples)
{
	int intervals = 0;
	bool grounded = false;
	for (const auto& sample : samples)
	{
		const bool next = (sample.flags & BOT_SFJ_REPLAY_FLAG_GROUNDED) != 0;
		if (next && !grounded)
			intervals++;
		grounded = next;
	}
	return intervals;
}

strafetrack_export_coverage_t coverage(float start, float end, int force = 0, int style = 0)
{
	strafetrack_export_coverage_t route = {};
	route.start[0] = start;
	route.end[0] = end;
	route.haveEnd = 1;
	route.forceJump = force;
	route.moveStyle = style;
	return route;
}
}

BOOST_AUTO_TEST_SUITE(bot_replay_export)

BOOST_AUTO_TEST_CASE(lossless_transitions_exceed_legacy_budget)
{
	auto samples = replay(4096);
	for (int i = 0; i < static_cast<int>(samples.size()); i++)
	{
		samples[i].flags = BOT_SFJ_REPLAY_FLAG_WALLKNOWN;
		if (i % 9 < 2)
			samples[i].flags |= BOT_SFJ_REPLAY_FLAG_GROUNDED;
		if (i % 11 < 3)
			samples[i].flags |= BOT_SFJ_REPLAY_FLAG_JUMP;
		if (i % 19 < 4)
			samples[i].flags |= BOT_SFJ_REPLAY_FLAG_FORCEJUMP;
		if (i % 23 < 5)
			samples[i].flags |= BOT_SFJ_REPLAY_FLAG_WALLRUN;
		samples[i].forward = i % 17 < 8 ? 32 : 64;
		samples[i].right = i % 13 < 4 ? -32 : 0;
		samples[i].up = i % 11 < 3 ? 32 : 0;
		samples[i].node = i / 128;
	}
	const int needed = BotSFJ_ReplayDecimate(samples.data(), samples.size(), nullptr, 0);
	BOOST_REQUIRE_GT(needed, 1362);
	BOOST_REQUIRE(BotSFJ_ReplayFitsBudget(0, needed));
	std::vector<bot_sfj_replay_sample_t> output(needed);
	BOOST_REQUIRE_EQUAL(BotSFJ_ReplayDecimate(samples.data(), samples.size(), output.data(),
		output.size()), needed);
	int cursor = 0;
	for (int i = 0; i < static_cast<int>(samples.size()); i++)
	{
		if (!BotSFJ_ReplayEssential(samples.data(), samples.size(), i))
			continue;
		while (cursor < needed && output[cursor].timeMs < samples[i].timeMs)
			cursor++;
		BOOST_REQUIRE_LT(cursor, needed);
		BOOST_CHECK_EQUAL(std::memcmp(&samples[i], &output[cursor], sizeof(samples[i])), 0);
	}
	BOOST_CHECK_EQUAL(groundIntervals(output), groundIntervals(samples));
}

BOOST_AUTO_TEST_CASE(undersized_output_is_not_partially_written)
{
	auto samples = replay(800);
	for (int i = 0; i < static_cast<int>(samples.size()); i++)
		samples[i].right = i % 2 ? 32 : 64;
	auto output = replay(BOT_SFJ_REPLAY_MAX_SAMPLES);
	const auto before = output;
	BOOST_CHECK_EQUAL(BotSFJ_ReplayDecimate(samples.data(), samples.size(),
		output.data(), output.size()), 800);
	BOOST_CHECK_EQUAL(std::memcmp(output.data(), before.data(), output.size() * sizeof(output[0])), 0);
}

BOOST_AUTO_TEST_CASE(one_command_contacts_and_wall_metadata_keep_both_edges)
{
	auto samples = replay(15);
	for (auto& sample : samples)
		sample.origin[0] = 0;
	samples[3].flags = BOT_SFJ_REPLAY_FLAG_GROUNDED;
	samples[6].flags = BOT_SFJ_REPLAY_FLAG_WALLKNOWN;
	samples[9].flags = BOT_SFJ_REPLAY_FLAG_FORCEJUMP;
	samples[12].node = 1;
	const int needed = BotSFJ_ReplayDecimate(samples.data(), samples.size(), nullptr, 0);
	std::vector<bot_sfj_replay_sample_t> output(needed);
	BotSFJ_ReplayDecimate(samples.data(), samples.size(), output.data(), needed);
	for (int index : {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13})
	{
		bool found = false;
		for (const auto& sample : output)
			found |= sample.timeMs == samples[index].timeMs;
		BOOST_CHECK(found);
	}
	BOOST_CHECK_EQUAL(groundIntervals(output), 1);
}

BOOST_AUTO_TEST_CASE(smooth_motion_still_decimates_and_preserves_endpoints)
{
	auto samples = replay(1000);
	const int needed = BotSFJ_ReplayDecimate(samples.data(), samples.size(), nullptr, 0);
	BOOST_REQUIRE_LT(needed, 256);
	std::vector<bot_sfj_replay_sample_t> output(needed);
	BotSFJ_ReplayDecimate(samples.data(), samples.size(), output.data(), needed);
	BOOST_CHECK_EQUAL(output.front().timeMs, samples.front().timeMs);
	BOOST_CHECK_EQUAL(output.back().timeMs, samples.back().timeMs);
	BOOST_CHECK_EQUAL(BotSFJ_ReplayDecimate(nullptr, 0, nullptr, 0), 0);
}

BOOST_AUTO_TEST_CASE(budgets_allow_large_routes_but_never_partial_replays)
{
	BOOST_CHECK(BotSFJ_ReplayFitsBudget(0, 1362));
	BOOST_CHECK(BotSFJ_ReplayFitsBudget(0, BOT_SFJ_REPLAY_ROUTE_SAMPLES));
	BOOST_CHECK(BotSFJ_ReplayFitsBudget(BOT_SFJ_REPLAY_TOTAL_SAMPLES - 4096, 4096));
	BOOST_CHECK(!BotSFJ_ReplayFitsBudget(0, 4097));
	BOOST_CHECK(!BotSFJ_ReplayFitsBudget(BOT_SFJ_REPLAY_TOTAL_SAMPLES - 4095, 4096));
	BOOST_CHECK(!BotSFJ_ReplayFitsBudget(BOT_SFJ_REPLAY_TOTAL_SAMPLES + 1, 2));
	BOOST_CHECK(!BotSFJ_ReplayFitsBudget(-1, 2));
	BOOST_CHECK(!BotSFJ_ReplayFitsBudget(0, 1));
	BOOST_CHECK(!BotSFJ_ReplayFitsBudget(std::numeric_limits<int>::max(), 2));
}

BOOST_AUTO_TEST_CASE(export_coverage_reserves_starts_and_distinct_destinations)
{
	const strafetrack_export_coverage_t routes[] = {
		coverage(0, 1000), coverage(1, 1001), coverage(0, -1000),
		coverage(0, 3000), coverage(5000, 6000), coverage(5001, 6001)
	};
	unsigned char picked[6] = {};
	BOOST_CHECK_EQUAL(G_StrafeTrackSelectExportCoverage(routes, 6, 4, picked), 4);
	BOOST_CHECK(picked[0]);
	BOOST_CHECK(!picked[1]);
	BOOST_CHECK(picked[2]);
	BOOST_CHECK(picked[3]);
	BOOST_CHECK(picked[4]);
	BOOST_CHECK(!picked[5]);
}

BOOST_AUTO_TEST_CASE(export_coverage_keeps_movement_types_and_fills_spare_slots)
{
	const strafetrack_export_coverage_t routes[] = {
		coverage(0, 1000), coverage(1, 1001), coverage(0, 1000, 1), coverage(0, 1000, 0, 1)
	};
	unsigned char picked[4] = {};
	BOOST_CHECK_EQUAL(G_StrafeTrackSelectExportCoverage(routes, 4, 3, picked), 3);
	BOOST_CHECK(picked[0]);
	BOOST_CHECK(!picked[1]);
	BOOST_CHECK(picked[2]);
	BOOST_CHECK(picked[3]);
	BOOST_CHECK_EQUAL(G_StrafeTrackSelectExportCoverage(routes, 4, 8, picked), 4);
	BOOST_CHECK_EQUAL(G_StrafeTrackSelectExportCoverage(routes, 4, 0, picked), 0);
}

BOOST_AUTO_TEST_CASE(export_reserves_quick_short_connectors_over_long_duplicates)
{
	strafetrack_export_coverage_t routes[] = {
		coverage(0, 2000), coverage(1, 2001), coverage(3000, 5000),
		coverage(6000, 8000), coverage(10, 266), coverage(10, 266), coverage(10, 394)
	};
	routes[4].connectorScore = BotSFJ_TrackConnectorScore(256, 1200, 1.0f);
	routes[5].connectorScore = BotSFJ_TrackConnectorScore(256, 600, 1.5f);
	routes[6].connectorScore = BotSFJ_TrackConnectorScore(384, 900, 1.0f);
	unsigned char picked[7] = {};
	BOOST_CHECK_EQUAL(G_StrafeTrackSelectExportCoverage(routes, 7, 4, picked), 4);
	BOOST_CHECK(picked[5]);
	BOOST_CHECK(!picked[4]);
	BOOST_CHECK(!picked[1]);
	BOOST_CHECK(!G_StrafeTrackExportEquivalent(&routes[5], &routes[6]));
	BOOST_CHECK(G_StrafeTrackExportEquivalent(&routes[4], &routes[5]));
	BOOST_CHECK_EQUAL(G_StrafeTrackSelectExportCoverage(routes, 7, 8, picked), 7);
	BOOST_CHECK_EQUAL(G_StrafeTrackSelectExportCoverage(routes, 7, 0, picked), 0);
	BOOST_CHECK_EQUAL(G_StrafeTrackSelectExportCoverage(routes, 0, 8, picked), 0);
}

BOOST_AUTO_TEST_CASE(short_force_starts_survive_faster_flat_connectors)
{
	strafetrack_export_coverage_t routes[] = {
		coverage(0, 2000), coverage(3000, 5000), coverage(6000, 8000),
		coverage(9000, 9256), coverage(12000, 12256, 1)
	};
	routes[3].connectorScore = BotSFJ_TrackConnectorScore(256, 500, 1.0f);
	routes[4].connectorScore = BotSFJ_TrackConnectorScore(256, 1800, 1.0f);
	routes[4].shortForceJump = 1;
	unsigned char picked[5] = {};
	BOOST_REQUIRE_EQUAL(G_StrafeTrackSelectExportCoverage(routes, 5, 4, picked), 4);
	BOOST_CHECK(picked[4]);
	BOOST_CHECK(!picked[3]);
	/* An invalid or unscored Force route must not displace a usable connector. */
	routes[4].connectorScore = 0;
	BOOST_REQUIRE_EQUAL(G_StrafeTrackSelectExportCoverage(routes, 5, 4, picked), 4);
	BOOST_CHECK(picked[3]);
}

BOOST_AUTO_TEST_CASE(final_links_onto_a_flag_are_reserved_on_top_of_coverage)
{
	strafetrack_export_coverage_t routes[] = {
		coverage(0, 2000), coverage(3000, 5000), coverage(6000, 8000),
		coverage(9000, 9300), coverage(9010, 9310), coverage(9100, 9300),
		coverage(12000, 14000), coverage(15000, 17000), coverage(18000, 20000),
		coverage(21000, 23000), coverage(24000, 26000), coverage(27000, 29000)
	};
	unsigned char picked[12] = {};
	routes[3].objectiveLink = G_StrafeTrackObjectiveLinkScore(40.0f, 2, 1, 1.0f, 1.0f);
	routes[4].objectiveLink = G_StrafeTrackObjectiveLinkScore(40.0f, 2, 0, 1.0f, 1.0f);
	routes[5].objectiveLink = G_StrafeTrackObjectiveLinkScore(300.0f, 6, 0, 1.0f, 1.0f);
	/* An eighth of 9 is two final links; near-duplicates (64u) collapse to the stronger one. */
	BOOST_REQUIRE_EQUAL(G_StrafeTrackSelectExportCoverage(routes, 12, 9, picked), 9);
	BOOST_CHECK(picked[3]);
	BOOST_CHECK(!picked[4]);
	BOOST_CHECK(picked[5]);
	/* The reservation comes on top of the general start coverage. */
	BOOST_CHECK(picked[0]);
	BOOST_CHECK(picked[6]);
	BOOST_CHECK_GT(G_StrafeTrackObjectiveLinkScore(40.0f, 2, 1, 1.0f, 1.0f),
		G_StrafeTrackObjectiveLinkScore(40.0f, 2, 0, 1.0f, 1.0f));
	BOOST_CHECK_EQUAL(G_StrafeTrackObjectiveLinkScore(500.0f, 2, 1, 1.0f, 1.0f), 0.0f);
	BOOST_CHECK_EQUAL(G_StrafeTrackObjectiveLinkScore(-1.0f, 2, 1, 1.0f, 1.0f), 0.0f);
	BOOST_CHECK_GT(G_StrafeTrackRecencyFactor(200, 100, 200), G_StrafeTrackRecencyFactor(100, 100, 200));
	BOOST_CHECK_EQUAL(G_StrafeTrackRecencyFactor(150, 100, 100), 1.0f);
}

BOOST_AUTO_TEST_CASE(unknown_takeoff_duration_does_not_invalidate_human_replay)
{
	const float takeoff[] = {-1033.8f, 8583.0f, 216.1f};
	BOOST_CHECK(BotSFJ_TrackTakeoffValid(takeoff, -1));
	BOOST_CHECK(BotSFJ_TrackTakeoffValid(takeoff, 0));
	BOOST_CHECK(!BotSFJ_TrackTakeoffValid(takeoff, -2));
	const float invalid[] = {0, std::numeric_limits<float>::quiet_NaN(), 0};
	BOOST_CHECK(!BotSFJ_TrackTakeoffValid(invalid, -1));
	BOOST_CHECK(!BotSFJ_HopNeedsApproach(1, 0, -1, 128.0f));
}

BOOST_AUTO_TEST_CASE(cooling_suffix_hands_off_only_after_grounded_prefix)
{
	const int available[] = {1, 1, 0, 1};
	BOOST_CHECK_EQUAL(BotSFJ_AvailablePrefixEnd(available, 4, 0), 2);
	BOOST_CHECK(BotSFJ_UsablePrefixFinished(2, 4, 1, 0, available[2]));
	BOOST_CHECK(!BotSFJ_UsablePrefixFinished(2, 4, 0, 1, available[2]));
	BOOST_CHECK(!BotSFJ_UsablePrefixFinished(2, 4, 1, 1, available[2]));
	BOOST_CHECK(!BotSFJ_UsablePrefixFinished(0, 4, 1, 0, 0));
	BOOST_CHECK(!BotSFJ_UsablePrefixFinished(2, 4, 1, 0, 1));
	BOOST_CHECK(BotSFJ_UsablePrefixFinished(4, 4, 1, 0, 1));
	/* A cooldown expiring en route must not extend the already selected prefix. */
	BOOST_CHECK(BotSFJ_UsablePrefixFinished(2, 2, 1, 0, 1));
}

BOOST_AUTO_TEST_CASE(timed_match_waits_through_pause_without_later_loop_skip)
{
	auto samples = replay(130);
	for (int i = 0; i < 130; i++)
	{
		samples[i].origin[0] = 0.0f;
		samples[i].forward = i % 2 ? 127 : 64;
		samples[i].right = i % 2 ? -127 : 127;
		samples[i].timeMs = i;
	}
	for (int i = 80; i < 100; i++)
	{
		samples[i].origin[0] = (i - 79) * 10.0f;
		samples[i].timeMs = 80 + (i - 80) * 10;
	}
	for (int i = 100; i < 130; i++)
	{
		samples[i].origin[0] = (130 - i) * 5.0f;
		samples[i].timeMs = 280 + (i - 100) * 10;
	}
	samples[129].origin[0] = 0.0f;
	const float origin[3] = {0.0f, 0.0f, 0.0f};
	float deviation = -1.0f, cross = 0.0f, fraction = -1.0f;
	BOOST_CHECK_EQUAL(BotSFJ_ReplayMatch(samples.data(), samples.size(), 0, origin,
		&deviation, &cross, &fraction), 0);
	BOOST_CHECK_EQUAL(BotSFJ_ReplayMatchTimed(samples.data(), samples.size(), 0, origin, 0, 20,
		&deviation, &cross, &fraction), 20);
	BOOST_CHECK_EQUAL(BotSFJ_ReplayMatchTimed(samples.data(), samples.size(), 0, origin, 0, -1,
		&deviation, &cross, &fraction), 0);
	BOOST_CHECK_EQUAL(BotSFJ_ReplayMatchTimed(samples.data(), samples.size(), 0, origin, 0, 100,
		&deviation, &cross, &fraction), 79);
	BOOST_CHECK_EQUAL(BotSFJ_ReplayMatchTimed(samples.data(), samples.size(), 79, origin, 0, 10000,
		&deviation, &cross, &fraction), 79);
	BOOST_CHECK_SMALL(deviation, 0.001f);
	BOOST_CHECK_GT(samples[80].origin[0], samples[79].origin[0]);
	BOOST_CHECK_EQUAL(BotSFJ_ReplayMatchTimed(samples.data(), samples.size(), 50, origin, 0, 0,
		&deviation, &cross, &fraction), 50);
}

BOOST_AUTO_TEST_CASE(timed_match_stays_inside_upcoming_landing_node)
{
	auto samples = replay(12);
	for (int i = 0; i < 12; i++)
	{
		samples[i].origin[0] = i * 10.0f;
		samples[i].node = i < 4 ? 0 : (i < 8 ? 1 : 2);
	}
	const float early[3] = {10.0f, 0.0f, 0.0f};
	const float late[3] = {100.0f, 0.0f, 0.0f};
	float deviation = 0.0f;
	BOOST_CHECK_EQUAL(BotSFJ_ReplayMatchTimed(samples.data(), samples.size(), 0, early, 1, -1,
		&deviation, nullptr, nullptr), 4);
	BOOST_CHECK_EQUAL(BotSFJ_ReplayMatchTimed(samples.data(), samples.size(), 0, late, 1, -1,
		&deviation, nullptr, nullptr), 7);
	BOOST_CHECK_EQUAL(BotSFJ_ReplayMatchTimed(samples.data(), samples.size(), 9, early, 1, -1,
		&deviation, nullptr, nullptr), -1);
	BOOST_CHECK_EQUAL(BotSFJ_ReplayMatchTimed(samples.data(), samples.size(), 0, early, 3, -1,
		&deviation, nullptr, nullptr), -1);
	BOOST_CHECK_EQUAL(BotSFJ_ReplayMatchTimed(nullptr, 0, 0, early, 0, 0,
		&deviation, nullptr, nullptr), -1);
}

BOOST_AUTO_TEST_CASE(lookahead_is_time_bounded_and_stops_before_transitions)
{
	auto samples = replay(20);
	for (int i = 0; i < 20; i++)
		samples[i].timeMs = i * 10;
	BOOST_CHECK_EQUAL(BotSFJ_ReplayLookAhead(samples.data(), samples.size(), 2,
		BOT_SFJ_REPLAY_LOOKAHEAD_MS), 7);
	samples[5].flags = BOT_SFJ_REPLAY_FLAG_GROUNDED;
	BOOST_CHECK_EQUAL(BotSFJ_ReplayLookAhead(samples.data(), samples.size(), 2, 50), 4);
	samples[3].right = 127;
	BOOST_CHECK_EQUAL(BotSFJ_ReplayLookAhead(samples.data(), samples.size(), 2, 50), 2);
	samples[11].node = 1;
	BOOST_CHECK_EQUAL(BotSFJ_ReplayLookAhead(samples.data(), samples.size(), 8, 50), 10);
	BOOST_CHECK_EQUAL(BotSFJ_ReplayLookAhead(samples.data(), samples.size(), 19, 50), 19);
	BOOST_CHECK_EQUAL(BotSFJ_ReplayLookAhead(samples.data(), samples.size(), 20, 50), -1);
}

BOOST_AUTO_TEST_SUITE_END()
