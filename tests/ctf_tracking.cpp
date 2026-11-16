#include "g_ctf_tracking.h"
#include "g_account.h"
#include "g_route_edit.h"
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
		"session_id", "journey_id", "objective_success", "start_ms", "end_ms", "editor_session_id", "route_priority",
		"recording_session_id", "start_kind"
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
	BOOST_CHECK_EQUAL(sqlite3_bind_parameter_count(insert), 28);
	for (int journey = 7; journey <= 10; ++journey)
	{
		sqlite3_reset(insert);
		sqlite3_bind_int64(insert, 19, 1);
		sqlite3_bind_int64(insert, 20, journey);
		sqlite3_bind_int64(insert, 21, journey);
		sqlite3_bind_int64(insert, 22, journey);
		sqlite3_bind_int64(insert, 25, 123);
		sqlite3_bind_int(insert, 26, journey == 8 ? 1 : 0);
		sqlite3_bind_int64(insert, 27, 456);
		sqlite3_bind_int(insert, 28, journey == 8 ? 2 : (journey == 9 ? 1 : 0));
		BOOST_REQUIRE_EQUAL(sqlite3_step(insert), SQLITE_DONE);
	}
	sqlite3_finalize(insert);
	sqlite3_stmt *rows = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db,
		"SELECT objective_success,editor_session_id,route_priority,start_kind FROM LocalStrafeRoute ORDER BY id",
		-1, &rows, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_type(rows, 0), SQLITE_NULL);
	BOOST_CHECK_EQUAL(sqlite3_column_type(rows, 1), SQLITE_NULL);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 2), 0);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 3), 0);
	const int expected[] = {0, 1, 0, 1};
	const int expectedKinds[] = {0, 2, 1, 0};
	int route = 0;
	for (int success : expected)
	{
		BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
		BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 0), success);
		BOOST_CHECK_EQUAL(sqlite3_column_int64(rows, 1), 123);
		BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 2), route == 1 ? 1 : 0);
		BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 3), expectedKinds[route++]);
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

BOOST_AUTO_TEST_CASE(roll_input_schema_preserves_unknown_legacy_and_actual_crouch_commands)
{
	sqlite3 *db = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, AccountSQL("CREATE TABLE IF NOT EXISTS LocalStrafeRoute(id").c_str(),
		NULL, NULL, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "INSERT INTO LocalStrafeRouteInput(route_id,frame) VALUES(1,0)",
		NULL, NULL, NULL), SQLITE_OK);
	const char *columns[] = {
		"cmd_time", "pitch", "vel_x", "vel_y", "jump_held", "force_jump", "speed_active", "speed_level", "wall_run", "rolling"
	};
	for (const char *column : columns)
	{
		const std::string migration = AccountSQL(std::string("ALTER TABLE LocalStrafeRouteInput ADD COLUMN ") + column + " ");
		BOOST_REQUIRE_EQUAL(sqlite3_exec(db, migration.c_str(), NULL, NULL, NULL), SQLITE_OK);
	}
	sqlite3_stmt *insert = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db, AccountSQL("INSERT INTO LocalStrafeRouteInput(route_id,").c_str(),
		-1, &insert, NULL), SQLITE_OK);
	sqlite3_bind_int(insert, 1, 1);
	sqlite3_bind_int(insert, 2, 1);
	sqlite3_bind_int(insert, 5, 127);
	sqlite3_bind_int(insert, 7, -127);
	sqlite3_bind_int(insert, 24, 1);
	BOOST_REQUIRE_EQUAL(sqlite3_step(insert), SQLITE_DONE);
	sqlite3_finalize(insert);
	sqlite3_stmt *rows = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db,
		"SELECT forwardmove,upmove,rolling FROM LocalStrafeRouteInput ORDER BY frame", -1, &rows, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_type(rows, 2), SQLITE_NULL);
	BOOST_REQUIRE_EQUAL(sqlite3_step(rows), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 0), 127);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 1), -127);
	BOOST_CHECK_EQUAL(sqlite3_column_int(rows, 2), 1);
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

static sqlite3 *RouteEditDatabase()
{
	sqlite3 *db = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db,
		"CREATE TABLE LocalStrafeRoute(id INTEGER PRIMARY KEY,mapname TEXT,editor_session_id INTEGER,recording_session_id INTEGER,"
		"start_x REAL,start_y REAL,start_z REAL);"
		"CREATE TABLE LocalStrafeRouteNode(route_id INTEGER,node_index INTEGER,x REAL,y REAL,z REAL);"
		"CREATE TABLE LocalStrafeRouteInput(route_id INTEGER,frame INTEGER,x REAL,y REAL,z REAL);"
		"CREATE TABLE LocalCTFObjectiveEvent(id INTEGER PRIMARY KEY,route_id INTEGER);"
		"CREATE TABLE LocalDuelResult(id INTEGER);INSERT INTO LocalDuelResult VALUES(1);"
		"INSERT INTO LocalStrafeRoute VALUES(1,'mp/ffa3',10,100,10,0,0),(2,'MP/FFA3',10,100,0,0,50),"
		"(3,'mp/ffa3',20,100,-100,0,0),(4,'mp/ffa1',10,100,0,0,0);"
		"INSERT INTO LocalStrafeRouteNode VALUES(1,0,100,0,0),(2,0,100,0,50),"
		"(3,0,0,0,0),(4,0,100,0,0);"
		"INSERT INTO LocalStrafeRouteInput VALUES(1,0,100,0,0),(2,0,100,0,50),"
		"(3,0,-100,100,0),(3,1,100,100,0),(3,2,0,0,0),(4,0,100,0,0);"
		"INSERT INTO LocalCTFObjectiveEvent VALUES(1,1),(2,2),(3,4);",
		NULL, NULL, NULL), SQLITE_OK);
	return db;
}

static int RouteEditCount(sqlite3 *db, const char *sql)
{
	sqlite3_stmt *stmt = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_step(stmt), SQLITE_ROW);
	const int result = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return result;
}

BOOST_AUTO_TEST_CASE(route_edit_targeted_delete_is_transactional_and_clears_only_matching_references)
{
	sqlite3 *db = RouteEditDatabase();
	sqlite3_int64 *ids = NULL;
	int count = 0;
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "CREATE TRIGGER reject_route BEFORE DELETE ON LocalStrafeRoute "
		"BEGIN SELECT RAISE(ABORT,'test failure'); END", NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK_NE(G_DeleteRecordedRoutes(db, "mp/ffa3", ROUTE_EDIT_ID, 1, NULL, 0, &ids, &count), SQLITE_OK);
	BOOST_CHECK(ids == NULL);
	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalStrafeRouteInput"), 6);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalStrafeRouteNode"), 4);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT route_id FROM LocalCTFObjectiveEvent WHERE id=1"), 1);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "DROP TRIGGER reject_route", NULL, NULL, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DeleteRecordedRoutes(db, "MP/FFA3", ROUTE_EDIT_ID, 1, NULL, 0, &ids, &count), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(count, 1);
	BOOST_CHECK_EQUAL(ids[0], 1);
	free(ids);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalStrafeRouteInput"), 5);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalStrafeRouteNode"), 3);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalCTFObjectiveEvent WHERE route_id IS NULL"), 1);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalDuelResult"), 1);
	BOOST_CHECK(sqlite3_get_autocommit(db));
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(route_edit_last_and_session_use_explicit_recorder_session_and_current_map)
{
	sqlite3 *db = RouteEditDatabase();
	sqlite3_int64 *ids = NULL;
	int count = 0;
	BOOST_REQUIRE_EQUAL(G_DeleteRecordedRoutes(db, "mp/ffa3", ROUTE_EDIT_LAST, 10, NULL, 0, &ids, &count), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(count, 1);
	BOOST_CHECK_EQUAL(ids[0], 2);
	free(ids);
	BOOST_REQUIRE_EQUAL(G_DeleteRecordedRoutes(db, "mp/ffa3", ROUTE_EDIT_SESSION, 10, NULL, 0, &ids, &count), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(count, 1);
	BOOST_CHECK_EQUAL(ids[0], 1);
	free(ids);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalStrafeRoute WHERE id IN (3,4)"), 2);
	BOOST_REQUIRE_EQUAL(G_DeleteRecordedRoutes(db, "mp/ffa3", ROUTE_EDIT_SESSION, 0, NULL, 0, &ids, &count), SQLITE_OK);
	BOOST_CHECK_EQUAL(count, 0);
	free(ids);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(route_edit_radius_matches_only_starts_in_3d_not_intersecting_paths_or_endpoints)
{
	sqlite3 *db = RouteEditDatabase();
	sqlite3_int64 *ids = NULL;
	int count = 0;
	const double point[3] = {0, 0, 0};
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "INSERT INTO LocalStrafeRoute VALUES(5,'mp/ffa3',10,100,NULL,NULL,NULL)",
		NULL, NULL, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DeleteRecordedRoutes(db, "mp/ffa3", ROUTE_EDIT_RADIUS, 0, point, 10, &ids, &count), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(count, 1);
	BOOST_CHECK_EQUAL(ids[0], 1);
	free(ids);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalStrafeRoute WHERE id IN (2,3,4)"), 3);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalStrafeRoute WHERE id=5"), 1);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "DELETE FROM LocalStrafeRouteInput WHERE route_id=3", NULL, NULL, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DeleteRecordedRoutes(db, "mp/ffa3", ROUTE_EDIT_RADIUS, 0, point, 10, &ids, &count), SQLITE_OK);
	BOOST_CHECK_EQUAL(count, 0);
	free(ids);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalStrafeRoute WHERE id=3"), 1);
	BOOST_CHECK_EQUAL(G_DeleteRecordedRoutes(db, "mp/ffa3", ROUTE_EDIT_RADIUS, 0, point, -1, &ids, &count), SQLITE_MISUSE);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(route_edit_server_session_spans_recorders_but_not_earlier_sessions_or_other_maps)
{
	sqlite3 *db = RouteEditDatabase();
	sqlite3_int64 *ids = NULL;
	int count = 0;
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "INSERT INTO LocalStrafeRoute VALUES(5,'mp/ffa3',30,99,0,0,0)",
		NULL, NULL, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DeleteRecordedRoutes(db, "mp/ffa3", ROUTE_EDIT_SERVER_LAST, 100, NULL, 0, &ids, &count), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(count, 1);
	BOOST_CHECK_EQUAL(ids[0], 3);
	free(ids);
	BOOST_REQUIRE_EQUAL(G_DeleteRecordedRoutes(db, "mp/ffa3", ROUTE_EDIT_SERVER_SESSION, 100, NULL, 0, &ids, &count), SQLITE_OK);
	BOOST_CHECK_EQUAL(count, 2);
	free(ids);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalStrafeRoute WHERE id IN (4,5)"), 2);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(route_edit_map_reset_is_isolated_and_handles_schemas_without_ctf_route_references)
{
	sqlite3 *db = RouteEditDatabase();
	sqlite3_int64 *ids = NULL;
	int count = 0;
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "DROP TABLE LocalCTFObjectiveEvent;"
		"CREATE TABLE LocalCTFObjectiveEvent(id INTEGER PRIMARY KEY)", NULL, NULL, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DeleteRecordedRoutes(db, "MP/FFA3", ROUTE_EDIT_MAP, 0, NULL, 0, &ids, &count), SQLITE_OK);
	BOOST_CHECK_EQUAL(count, 3);
	free(ids);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalStrafeRoute WHERE id=4"), 1);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalStrafeRouteNode"), 1);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalStrafeRouteInput"), 1);
	BOOST_CHECK_EQUAL(RouteEditCount(db, "SELECT COUNT(*) FROM LocalDuelResult"), 1);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_SUITE_END()
