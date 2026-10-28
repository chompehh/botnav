#include "g_ctf_tracking.h"
#include "g_account.h"
#include "sqlite3.h"
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <sstream>
#include <string>

BOOST_AUTO_TEST_SUITE(ctf_tracking)

BOOST_AUTO_TEST_CASE(only_explicit_player_returns_and_captures_are_success)
{
	for (int team = -1; team <= 3; ++team)
	{
		BOOST_CHECK(!G_CTFTrackSuccess(CTF_TRACK_PICKUP, team));
		BOOST_CHECK(!G_CTFTrackSuccess(CTF_TRACK_DROP, team));
		BOOST_CHECK_EQUAL(G_CTFTrackSuccess(CTF_TRACK_RETURN, team), team == 1 || team == 2);
		BOOST_CHECK_EQUAL(G_CTFTrackSuccess(CTF_TRACK_CAPTURE, team), team == 1 || team == 2);
	}
	BOOST_CHECK(!G_CTFTrackSuccess(0, 1));
	BOOST_CHECK(!G_CTFTrackSuccess(99, 2));
}

BOOST_AUTO_TEST_CASE(objective_boundaries_close_journeys_but_pickups_preserve_approach_routes)
{
	BOOST_CHECK(!G_CTFTrackClosesJourney(CTF_TRACK_PICKUP));
	BOOST_CHECK(G_CTFTrackClosesJourney(CTF_TRACK_DROP));
	BOOST_CHECK(G_CTFTrackClosesJourney(CTF_TRACK_RETURN));
	BOOST_CHECK(G_CTFTrackClosesJourney(CTF_TRACK_CAPTURE));
	BOOST_CHECK(!G_CTFTrackClosesJourney(0));
}

static std::string AccountSQL(const std::string &marker)
{
	std::ifstream file(BOTNAV_SOURCE_DIR "/codemp/game/g_account.c");
	std::ostringstream contents;
	contents << file.rdbuf();
	const std::string source = contents.str();
	std::size_t pos = source.find("\"" + marker);
	BOOST_REQUIRE(pos != std::string::npos);
	std::string sql;
	do
	{
		++pos;
		while (pos < source.size() && source[pos] != '"')
		{
			if (source[pos] == '\\')
				++pos;
			sql += source[pos++];
		}
		++pos;
		pos = source.find_first_not_of(" \t\r\n", pos);
	} while (pos != std::string::npos && source[pos] == '"');
	return sql;
}

BOOST_AUTO_TEST_CASE(actual_schema_preserves_unknown_legacy_and_links_only_observed_objectives)
{
	sqlite3 *db = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	const std::string schema = AccountSQL("CREATE TABLE IF NOT EXISTS LocalCTFTrackSession");
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, schema.c_str(), NULL, NULL, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, AccountSQL("CREATE TABLE IF NOT EXISTS LocalStrafeRoute(id").c_str(),
		NULL, NULL, NULL), SQLITE_OK);
	const char *columns[] = {
		"bot", "pmove_fixed", "pmove_msec", "move_style", "per_command", "speed_active", "speed_level",
		"session_id", "journey_id", "objective_success", "start_ms", "end_ms"
	};
	for (const char *column : columns)
	{
		const std::string migration = AccountSQL(std::string("ALTER TABLE LocalStrafeRoute ADD COLUMN ") + column + " ");
		BOOST_REQUIRE_EQUAL(sqlite3_exec(db, migration.c_str(), NULL, NULL, NULL), SQLITE_OK);
	}
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db,
		"INSERT INTO LocalStrafeRoute(mapname) VALUES('mp/ctf4');"
		"INSERT INTO LocalCTFObjectiveEvent(journey_id,event,team) VALUES(7,1,1),(8,4,2),(9,3,-1),(10,3,1)",
		NULL, NULL, NULL), SQLITE_OK);
	sqlite3_stmt *insert = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db, AccountSQL("INSERT INTO LocalStrafeRoute(created,").c_str(),
		-1, &insert, NULL), SQLITE_OK);
	BOOST_CHECK_EQUAL(sqlite3_bind_parameter_count(insert), 24);
	for (int journey = 7; journey <= 10; ++journey)
	{
		sqlite3_reset(insert);
		sqlite3_bind_int64(insert, 19, 1);
		sqlite3_bind_int64(insert, 20, journey);
		sqlite3_bind_int64(insert, 21, journey);
		sqlite3_bind_int64(insert, 22, journey);
		BOOST_REQUIRE_EQUAL(sqlite3_step(insert), SQLITE_DONE);
	}
	sqlite3_finalize(insert);
	sqlite3_stmt *rows = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db,
		"SELECT objective_success FROM LocalStrafeRoute ORDER BY id", -1, &rows, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_type(rows, 0), SQLITE_NULL);
	const int expected[] = {0, 1, 0, 1};
	for (int success : expected)
	{
		BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
		BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 0), success);
	}
	sqlite3_finalize(rows);
	/* A capture joins all pause-separated routes in the same journey only. */
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db,
		"INSERT INTO LocalStrafeRoute(journey_id,objective_success) VALUES(7,0),(11,0);"
		"INSERT INTO LocalCTFObjectiveEvent(journey_id,event,team) VALUES(7,4,1)",
		NULL, NULL, NULL), SQLITE_OK);
	sqlite3_stmt *update = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db,
		AccountSQL("UPDATE LocalStrafeRoute SET objective_success=1 WHERE journey_id=?").c_str(),
		-1, &update, NULL), SQLITE_OK);
	sqlite3_bind_int64(update, 1, 7);
	BOOST_REQUIRE_EQUAL(sqlite3_step(update), SQLITE_DONE);
	BOOST_CHECK_EQUAL(sqlite3_changes(db), 2);
	sqlite3_finalize(update);
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db,
		"SELECT objective_success FROM LocalStrafeRoute WHERE journey_id=11 OR journey_id IS NULL ORDER BY id",
		-1, &rows, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_type(rows, 0), SQLITE_NULL);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 0), 0);
	sqlite3_finalize(rows);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(takeoff_schema_and_insert_keep_missing_measurements_unknown)
{
	sqlite3 *db = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, AccountSQL("CREATE TABLE IF NOT EXISTS LocalStrafeRoute(id").c_str(),
		NULL, NULL, NULL), SQLITE_OK);
	const char *columns[] = {
		"side_only_pct", "switch_pct", "first_side", "takeoff_yaw_rate", "air_yaw_rate", "force_jump",
		"force_height", "speed_active", "speed_level", "takeoff_x", "takeoff_y", "takeoff_z", "ground_ms"
	};
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "INSERT INTO LocalStrafeRouteNode(route_id,node_index) VALUES(1,0)",
		NULL, NULL, NULL), SQLITE_OK);
	for (const char *column : columns)
	{
		const std::string migration = AccountSQL(std::string("ALTER TABLE LocalStrafeRouteNode ADD COLUMN ") + column + " ");
		BOOST_REQUIRE_EQUAL(sqlite3_exec(db, migration.c_str(), NULL, NULL, NULL), SQLITE_OK);
	}
	sqlite3_stmt *insert = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db, AccountSQL("INSERT INTO LocalStrafeRouteNode(route_id,").c_str(),
		-1, &insert, NULL), SQLITE_OK);
	BOOST_CHECK_EQUAL(sqlite3_bind_parameter_count(insert), 22);
	sqlite3_bind_int64(insert, 1, 1);
	sqlite3_bind_int(insert, 2, 1);
	sqlite3_bind_double(insert, 19, 100);
	sqlite3_bind_double(insert, 20, 200);
	sqlite3_bind_double(insert, 21, 300);
	sqlite3_bind_int(insert, 22, 400);
	BOOST_REQUIRE_EQUAL(sqlite3_step(insert), SQLITE_DONE);
	sqlite3_finalize(insert);
	sqlite3_stmt *rows = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db,
		"SELECT takeoff_x,takeoff_y,takeoff_z,ground_ms FROM LocalStrafeRouteNode ORDER BY node_index",
		-1, &rows, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	for (int column = 0; column < 4; ++column)
		BOOST_CHECK_EQUAL(sqlite3_column_type(rows, column), SQLITE_NULL);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	for (int column = 0; column < 4; ++column)
		BOOST_CHECK_EQUAL(sqlite3_column_int(rows, column), (column + 1) * 100);
	sqlite3_finalize(rows);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

static sqlite3 *TakeoffDatabase()
{
	sqlite3 *db = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db,
		"CREATE TABLE LocalStrafeRouteInput(route_id,frame,time_ms,msec,grounded,x,y,z);"
		"CREATE TABLE LocalStrafeRouteNode(route_id,time_ms,takeoff_x,takeoff_y,takeoff_z,ground_ms);"
		"INSERT INTO LocalStrafeRouteNode(route_id,time_ms) VALUES(1,30),(1,360),(1,390),(2,30);"
		"INSERT INTO LocalStrafeRouteInput VALUES"
		"(1,0,0,10,1,1,0,0),(1,1,10,10,0,2,0,0),(1,2,20,10,0,3,0,0),"
		"(1,3,30,10,1,4,0,0),(1,4,130,100,1,5,0,0),(1,5,330,200,1,6,0,0),"
		"(1,6,340,10,0,7,0,0),(1,7,350,10,0,8,0,0),(1,8,360,10,1,9,0,0),"
		"(1,9,370,10,1,10,0,0),(1,10,380,10,0,11,0,0),(1,11,390,10,1,12,0,0)",
		NULL, NULL, NULL), SQLITE_OK);
	return db;
}

BOOST_AUTO_TEST_CASE(takeoff_backfill_commits_multiple_hops_and_leaves_no_transaction_open)
{
	sqlite3 *db = TakeoffDatabase();
	BOOST_REQUIRE_EQUAL(G_StrafeTrackBackfillTakeoffsDB(db, 1), SQLITE_OK);
	BOOST_CHECK(sqlite3_get_autocommit(db));
	sqlite3_stmt *rows = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db,
		"SELECT takeoff_x,ground_ms FROM LocalStrafeRouteNode WHERE route_id=1 ORDER BY time_ms",
		-1, &rows, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 0), 1);
	BOOST_CHECK_EQUAL(sqlite3_column_type(rows, 1), SQLITE_NULL);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 0), 6);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 1), 310);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 0), 10);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 1), 20);
	sqlite3_finalize(rows);
	BOOST_CHECK_EQUAL(G_StrafeTrackBackfillTakeoffsDB(db, 2), SQLITE_OK);
	BOOST_CHECK(sqlite3_get_autocommit(db));
	BOOST_CHECK_EQUAL(G_StrafeTrackBackfillTakeoffsDB(db, 1), SQLITE_OK);
	BOOST_CHECK(sqlite3_get_autocommit(db));
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(takeoff_backfill_rolls_back_all_hops_and_respects_caller_transactions)
{
	sqlite3 *db = TakeoffDatabase();
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db,
		"CREATE TRIGGER reject_hop BEFORE UPDATE ON LocalStrafeRouteNode "
		"WHEN NEW.time_ms=360 BEGIN SELECT RAISE(ABORT,'test failure'); END",
		NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK_NE(G_StrafeTrackBackfillTakeoffsDB(db, 1), SQLITE_OK);
	BOOST_CHECK(sqlite3_get_autocommit(db));
	sqlite3_stmt *rows = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db,
		"SELECT COUNT(*) FROM LocalStrafeRouteNode WHERE takeoff_x IS NOT NULL", -1, &rows, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 0), 0);
	sqlite3_finalize(rows);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "BEGIN", NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK_EQUAL(G_StrafeTrackBackfillTakeoffsDB(db, 1), SQLITE_BUSY);
	BOOST_CHECK(!sqlite3_get_autocommit(db));
	BOOST_CHECK_EQUAL(sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(takeoff_backfill_does_not_infer_across_input_gaps_or_incomplete_hops)
{
	sqlite3 *db = TakeoffDatabase();
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db,
		"UPDATE LocalStrafeRouteInput SET msec=9 WHERE route_id=1 AND frame=2;"
		"INSERT INTO LocalStrafeRouteInput VALUES(2,0,0,10,1,1,0,0),(2,1,10,10,0,2,0,0)",
		NULL, NULL, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_StrafeTrackBackfillTakeoffsDB(db, 1), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_StrafeTrackBackfillTakeoffsDB(db, 2), SQLITE_OK);
	BOOST_CHECK(sqlite3_get_autocommit(db));
	sqlite3_stmt *rows = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db,
		"SELECT takeoff_x FROM LocalStrafeRouteNode ORDER BY route_id,time_ms",
		-1, &rows, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_type(rows, 0), SQLITE_NULL);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 0), 6);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 0), 10);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_type(rows, 0), SQLITE_NULL);
	sqlite3_finalize(rows);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_SUITE_END()
