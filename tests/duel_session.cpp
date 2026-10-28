#include "g_duel_session.h"
#include "g_account.h"

#include <boost/test/unit_test.hpp>
#include <cstring>

BOOST_AUTO_TEST_SUITE( duel_session )

BOOST_AUTO_TEST_CASE( nonce_initializes_bundled_sqlite_on_cold_start )
{
	BOOST_REQUIRE_EQUAL(sqlite3_shutdown(), SQLITE_OK);
	unsigned char nonce[16];
	BOOST_REQUIRE_EQUAL(G_GenerateLearningSessionNonce(nonce), SQLITE_OK);
	sqlite3 *db = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_CHECK_EQUAL(sqlite3_exec(db, "SELECT 1", NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
	BOOST_CHECK_EQUAL(sqlite3_shutdown(), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE( repeated_map_sessions_keep_sqlite_initialized )
{
	BOOST_REQUIRE_EQUAL(sqlite3_shutdown(), SQLITE_OK);
	unsigned char nonce[16];
	for (int map = 0; map < 3; ++map)
		BOOST_REQUIRE_EQUAL(G_GenerateLearningSessionNonce(nonce), SQLITE_OK);
	BOOST_CHECK_EQUAL(sqlite3_shutdown(), SQLITE_OK);
}

static int FailSQLiteAllocatorInitialization(void *)
{
	return SQLITE_NOMEM;
}

BOOST_AUTO_TEST_CASE( initialization_failure_does_not_enter_prng )
{
	BOOST_REQUIRE_EQUAL(sqlite3_shutdown(), SQLITE_OK);
	sqlite3_mem_methods original;
	BOOST_REQUIRE_EQUAL(sqlite3_config(SQLITE_CONFIG_GETMALLOC, &original), SQLITE_OK);
	sqlite3_mem_methods failing = original;
	failing.xInit = FailSQLiteAllocatorInitialization;
	BOOST_REQUIRE_EQUAL(sqlite3_config(SQLITE_CONFIG_MALLOC, &failing), SQLITE_OK);
	unsigned char nonce[16];
	std::memset(nonce, 0xA5, sizeof(nonce));
	const int result = G_GenerateLearningSessionNonce(nonce);
	BOOST_REQUIRE_EQUAL(sqlite3_shutdown(), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_config(SQLITE_CONFIG_MALLOC, &original), SQLITE_OK);
	BOOST_CHECK_EQUAL(result, SQLITE_NOMEM);
	for (unsigned char byte : nonce)
		BOOST_CHECK_EQUAL(byte, 0xA5);
}

static int StrafeResetCount(sqlite3 *db, const char *sql)
{
	sqlite3_stmt *stmt = NULL;
	int count = -1;

	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
		return -1;
	if (sqlite3_step(stmt) != SQLITE_ROW)
	{
		sqlite3_finalize(stmt);
		return -1;
	}
	count = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return count;
}

static void CreateStrafeResetFixture(sqlite3 *db)
{
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db,
		"CREATE TABLE LocalStrafeRoute(id INTEGER PRIMARY KEY, mapname TEXT);"
		"CREATE TABLE LocalStrafeRouteNode(route_id INTEGER, node_index INTEGER);"
		"CREATE TABLE LocalStrafeRouteInput(route_id INTEGER, frame INTEGER);"
		"CREATE TABLE LocalBotLearnedSequence(sequence TEXT);"
		"CREATE TABLE LocalBotLearnedEvidence(sequence TEXT);"
		"CREATE TABLE LocalCTFObjectiveEvent(route_id INTEGER);"
		"INSERT INTO LocalStrafeRoute VALUES (1, 'map_a'), (2, 'map_a'), (3, 'map_b');"
		"INSERT INTO LocalStrafeRouteNode VALUES (1, 0), (1, 1), (2, 0), (3, 0);"
		"INSERT INTO LocalStrafeRouteInput VALUES (1, 0), (1, 1), (2, 0), (3, 0);"
		"INSERT INTO LocalBotLearnedSequence VALUES ('learned');"
		"INSERT INTO LocalBotLearnedEvidence VALUES ('evidence');"
		"INSERT INTO LocalCTFObjectiveEvent VALUES (2)",
		NULL, NULL, NULL), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE( strafe_track_reset_preview_and_commit_keep_selected_raw_demonstrations )
{
	sqlite3 *db = NULL;
	sqlite3_int64 keep[] = { 1, 3 };
	int routes, nodes, inputs;

	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	CreateStrafeResetFixture(db);
	BOOST_CHECK_EQUAL(G_ResetStrafeTrackData(db, keep, 2, 1, &routes, &nodes, &inputs), SQLITE_OK);
	BOOST_CHECK_EQUAL(routes, 1);
	BOOST_CHECK_EQUAL(nodes, 1);
	BOOST_CHECK_EQUAL(inputs, 1);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalStrafeRoute"), 3);

	BOOST_CHECK_EQUAL(G_ResetStrafeTrackData(db, keep, 2, 0, &routes, &nodes, &inputs), SQLITE_OK);
	BOOST_CHECK_EQUAL(routes, 1);
	BOOST_CHECK_EQUAL(nodes, 1);
	BOOST_CHECK_EQUAL(inputs, 1);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalStrafeRoute"), 2);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalStrafeRouteNode"), 3);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalStrafeRouteInput"), 3);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalBotLearnedSequence"), 1);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalBotLearnedEvidence"), 1);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalCTFObjectiveEvent"), 1);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE( strafe_track_reset_distinguishes_sole_connectors_and_force_speed_types )
{
	const float start[] = { 0, 0, 0 };
	const float end[] = { 1000, 0, 0 };
	const float nearStart[] = { 20, 0, 0 };
	const float nearEnd[] = { 1020, 0, 0 };
	const float otherEnd[] = { 0, 1000, 0 };
	const float otherStart[] = { 400, 0, 0 };

	BOOST_CHECK(G_StrafeTrackResetEquivalent(start, end, nearStart, nearEnd, 0, 0, 0, 0, 1, 1));
	BOOST_CHECK(!G_StrafeTrackResetEquivalent(start, end, start, otherEnd, 0, 0, 0, 0, 1, 1));
	BOOST_CHECK(!G_StrafeTrackResetEquivalent(start, end, otherStart, end, 0, 0, 0, 0, 1, 1));
	BOOST_CHECK(!G_StrafeTrackResetEquivalent(start, end, start, end, 0, 3, 0, 0, 1, 1));
	BOOST_CHECK(!G_StrafeTrackResetEquivalent(start, end, start, end, 0, 0, 0, 1, 1, 1));
	BOOST_CHECK(!G_StrafeTrackResetEquivalent(start, end, start, end, 0, 0, -1, 0, 1, 1));
	BOOST_CHECK(!G_StrafeTrackResetEquivalent(start, end, start, end, 0, 0, 0, 0, 1, 2));
}

BOOST_AUTO_TEST_CASE( strafe_track_reset_refuses_outer_transaction_and_can_clear_all_raw_data )
{
	sqlite3 *db = NULL;
	int routes, nodes, inputs;

	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	CreateStrafeResetFixture(db);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "BEGIN", NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK_EQUAL(G_ResetStrafeTrackData(db, NULL, 0, 0, &routes, &nodes, &inputs), SQLITE_BUSY);
	BOOST_CHECK_EQUAL(sqlite3_get_autocommit(db), 0);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK_EQUAL(G_ResetStrafeTrackData(db, NULL, 0, 0, &routes, &nodes, &inputs), SQLITE_OK);
	BOOST_CHECK_EQUAL(routes, 3);
	BOOST_CHECK_EQUAL(nodes, 4);
	BOOST_CHECK_EQUAL(inputs, 4);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalBotLearnedSequence"), 1);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalCTFObjectiveEvent"), 1);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE( strafe_track_reset_rolls_back_all_route_data_on_delete_error )
{
	sqlite3 *db = NULL;
	sqlite3_int64 keep[] = { 1 };
	int routes, nodes, inputs;

	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	CreateStrafeResetFixture(db);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db,
		"CREATE TRIGGER reject_strafe_node_delete BEFORE DELETE ON LocalStrafeRouteNode "
		"BEGIN SELECT RAISE(ABORT, 'test failure'); END",
		NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK_NE(G_ResetStrafeTrackData(db, keep, 1, 0, &routes, &nodes, &inputs), SQLITE_OK);
	BOOST_CHECK_EQUAL(routes, 0);
	BOOST_CHECK_EQUAL(nodes, 0);
	BOOST_CHECK_EQUAL(inputs, 0);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalStrafeRoute"), 3);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalStrafeRouteNode"), 4);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalStrafeRouteInput"), 4);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalBotLearnedSequence"), 1);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalBotLearnedEvidence"), 1);
	BOOST_CHECK_EQUAL(StrafeResetCount(db, "SELECT COUNT(*) FROM LocalCTFObjectiveEvent"), 1);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_SUITE_END()
