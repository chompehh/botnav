#include "ai_combat_tuning.h"
#include "g_bot_learning.h"
#include "g_bot_counter_store.h"
#include <boost/test/unit_test.hpp>
#include <limits>

BOOST_AUTO_TEST_SUITE(bot_combat_trials)

BOOST_AUTO_TEST_CASE(throw_live_cost_and_safety)
{
	BOOST_CHECK(!NewBotAI_ThrowLiveDrainThreat(1, 1, 0, 0, 60, 45, 10));
	BOOST_CHECK(NewBotAI_ThrowLiveDrainThreat(1, 1, 0, 0, 60, 19, 10));
	BOOST_CHECK(NewBotAI_ThrowLiveDrainThreat(1, 1, 0, 1, 60, 39, 10));
	BOOST_CHECK(!NewBotAI_ThrowLiveDrainThreat(1, 1, 1, 1, 60, 15, 10));
	BOOST_CHECK(!NewBotAI_ThrowLiveDrainThreat(1, 0, 0, 1, 60, 15, 10));
}

BOOST_AUTO_TEST_CASE(throw_progress_is_bounded_and_advantage_relative)
{
	BOOST_CHECK(NewBotAI_ThrowProgressHold(3, 3000, 0, 1, 1, -30, -30, 20, -5));
	BOOST_CHECK(!NewBotAI_ThrowProgressHold(3, 3000, 1, 1, 1, -30, -30, 20, -5));
	BOOST_CHECK(!NewBotAI_ThrowProgressHold(3, 3000, 0, 1, 0, -30, -30, 20, -5));
	BOOST_CHECK(!NewBotAI_ThrowProgressHold(3, 3000, 0, 1, 1, -30, -51, 20, -5));
	BOOST_CHECK(!NewBotAI_ThrowProgressHold(3, 3000, 0, 1, 1, -30, -30, 20, -11));
	BOOST_CHECK(!NewBotAI_ThrowProgressHold(3, 5200, 0, 1, 1, 0, 0, 0, 0));
	BOOST_CHECK(!NewBotAI_ThrowProgressHold(1, 1000, 0, 1, 1, 0, 0, 0, 0));
}

BOOST_AUTO_TEST_CASE(counter_keys_timings_and_evidence)
{
	int samples[] = {4, 4, 4}, net[] = {-40, 80, -20};
	const int left = BotLearn_CounterKey(1, 2, 3, 2, 70, 30);
	const int right = BotLearn_CounterKey(4, 2, 3, 2, 70, 30);
	BOOST_CHECK_NE(left, right);
	BOOST_CHECK_NE(left, BotLearn_CounterKey(1, 2, 3, 2, 70, -30));
	BOOST_CHECK_NE(left, BotLearn_CounterKey(1, 2, 3, 3, 70, 30));
	BOOST_CHECK_EQUAL(BotLearn_CounterKey(-1, 2, 3, 2, 70, 30), -1);
	BOOST_CHECK_EQUAL(BotLearn_CounterSelect(samples, net, 5), 0);
	BOOST_CHECK_EQUAL(BotLearn_CounterSelect(samples, net, 15), 1);
	BOOST_CHECK_EQUAL(BotLearn_CounterSelect(samples, net, 25), 2);
	BOOST_CHECK_EQUAL(BotLearn_CounterSelect(samples, net, 50), 1);
	samples[1] = 3;
	BOOST_CHECK_EQUAL(BotLearn_CounterSelect(samples, net, 50), 2);
	BOOST_CHECK(!BotLearn_CounterWindow(74, 1));
	BOOST_CHECK(BotLearn_CounterWindow(75, 1));
	BOOST_CHECK(BotLearn_CounterWindow(175, 1));
	BOOST_CHECK(!BotLearn_CounterWindow(176, 1));
	BOOST_CHECK_NE(BotLearn_CounterKeyV2(1, 2, 3, 2, 70, 30, 300),
		BotLearn_CounterKeyV2(1, 2, 3, 2, 70, 30, 900));
	BOOST_CHECK_EQUAL(BotLearn_CounterKeyV2(1, 2, 3, 2, 70, 30, 0), -1);
	BOOST_CHECK_EQUAL(BotLearn_CounterPhaseDelay(2, 300, 1), 75);
	BOOST_CHECK_EQUAL(BotLearn_CounterPhaseDelay(2, 900, 1), 225);
	BOOST_CHECK_EQUAL(BotLearn_CounterPhaseDelay(2, 900, 2), 630);
	BOOST_CHECK(!BotLearn_CounterPhaseWindow(629, 2, 900, 2));
	BOOST_CHECK(BotLearn_CounterPhaseWindow(630, 2, 900, 2));
	BOOST_CHECK_NE(BotLearn_CounterKeyV3(1, 2, 3, 2, 70, 30, 600, 0),
		BotLearn_CounterKeyV3(1, 2, 3, 2, 70, 30, 600, 2));
	BOOST_CHECK_EQUAL(BotLearn_CounterKey(1, 2, 3, 2,
		std::numeric_limits<float>::quiet_NaN(), 30), -1);
	BOOST_CHECK_EQUAL(BotLearn_CounterKey(1, 2, 3, 2, 70,
		std::numeric_limits<float>::infinity()), -1);
	{
		int motorSamples[] = {4, 4};
		int motorNet[] = {-40, -80};
		BOOST_CHECK_EQUAL(BotLearn_CounterMotorSelect(motorSamples, motorNet, 50), 0);
		BOOST_CHECK_EQUAL(BotLearn_CounterMotorSelect(motorSamples, motorNet, 15), 1);
		motorSamples[0] = motorSamples[1] = 0;
		BOOST_CHECK_EQUAL(BotLearn_CounterMotorSelect(motorSamples, motorNet, 50), 1);
		BOOST_CHECK_NE(BotLearn_CounterComboChoice(1, 0, 0),
			BotLearn_CounterComboChoice(2, 0, 0));
		BOOST_CHECK_NE(BotLearn_CounterComboChoice(1, 0, 0),
			BotLearn_CounterComboChoice(1, 0, 1));
		BOOST_CHECK_EQUAL(BotLearn_CounterRecipeBonus(4, -80), -15);
	}
}

BOOST_AUTO_TEST_CASE(counter_trials_commit_reload_and_retry)
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;
	botlearn_counter_cache_t cache = {}, loaded = {};
	const int key = BotLearn_CounterKeyV3(1, 2, 3, 2, 70, 30, 600, 2);
	const int family = BotLearn_CounterFamilyChoice(1, 6, 0);
	const int combo = BotLearn_CounterComboChoice(1, 0, 0);
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "CREATE TABLE LocalBotCounterV1("
		"key INTEGER,choice INTEGER,samples INTEGER,net INTEGER,PRIMARY KEY(key,choice))",
		NULL, NULL, NULL), SQLITE_OK);
	BotCounter_Record(&cache, key, 1, 35);
	BotCounter_Record(&cache, key, 1, -20);
	BotCounter_Record(&cache, key, family, 30);
	BotCounter_Record(&cache, key, combo, 45);
	BOOST_REQUIRE(BotCounter_Flush(&cache, db));
	BOOST_CHECK(BotCounter_Flush(&cache, db)); // no duplicate pending increments
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db,
		"SELECT key,choice,samples,net FROM LocalBotCounterV1 WHERE choice=1", -1, &stmt, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_step(stmt), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_int(stmt, 2), 2);
	BOOST_CHECK_EQUAL(sqlite3_column_int(stmt, 3), 15);
	BotCounter_AddLoaded(&loaded, sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1),
		sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3));
	sqlite3_finalize(stmt);
	BOOST_REQUIRE(BotCounter_Find(&loaded, key, 1, 0));
	BOOST_CHECK_EQUAL(BotCounter_Find(&loaded, key, 1, 0)->samples, 2);
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db,
		"SELECT key,choice,samples,net FROM LocalBotCounterV1 WHERE choice<>1", -1, &stmt, NULL), SQLITE_OK);
	while (sqlite3_step(stmt) == SQLITE_ROW)
		BotCounter_AddLoaded(&loaded, sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1),
			sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3));
	sqlite3_finalize(stmt);
	BOOST_REQUIRE(BotCounter_Find(&loaded, key, family, 0));
	BOOST_CHECK_EQUAL(BotCounter_Find(&loaded, key, combo, 0)->net, 45);
	BotCounter_Record(&loaded, key, 1, -10);
	BOOST_REQUIRE(BotCounter_Load(&loaded, &cache, db));
	BOOST_CHECK_EQUAL(BotCounter_Find(&loaded, key, 1, 0)->samples, 3);
	BOOST_CHECK_EQUAL(BotCounter_Find(&loaded, key, 1, 0)->net, 5);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "DROP TABLE LocalBotCounterV1", NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK(!BotCounter_Load(&loaded, &cache, db));
	BOOST_CHECK(!BotCounter_Flush(&loaded, db));
	BOOST_CHECK_EQUAL(BotCounter_Find(&loaded, key, 1, 0)->pendingSamples, 1);
	BOOST_CHECK_EQUAL(BotCounter_Find(&loaded, key, 1, 0)->net, 5);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "CREATE TABLE LocalBotCounterV1("
		"key INTEGER,choice INTEGER,samples INTEGER,net INTEGER,PRIMARY KEY(key,choice))",
		NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK(BotCounter_Flush(&loaded, db));
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db,
		"SELECT samples,net FROM LocalBotCounterV1", -1, &stmt, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_step(stmt), SQLITE_ROW);
	BOOST_CHECK_EQUAL(sqlite3_column_int(stmt, 0), 1);
	BOOST_CHECK_EQUAL(sqlite3_column_int(stmt, 1), -10);
	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

BOOST_AUTO_TEST_CASE(counter_full_cache_reports_failure)
{
	botlearn_counter_cache_t cache = {}, staging = {};
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;
	const int first = 1 << 17;
	int i;
	for (i = 0; i < BOTLEARN_COUNTER_CACHE_SIZE; i++)
		if (!BotCounter_Record(&cache, first + i, 0, 1))
			break;
	BOOST_REQUIRE_EQUAL(i, BOTLEARN_COUNTER_CACHE_SIZE);
	BOOST_CHECK(!BotCounter_Record(&cache, first + BOTLEARN_COUNTER_CACHE_SIZE, 0, 1));
	BOOST_CHECK(BotCounter_Record(&cache, first, 0, -1));
	botlearn_counter_entry_t *entry = BotCounter_Find(&cache, first, 0, 0);
	BOOST_REQUIRE(entry);
	BOOST_CHECK_EQUAL(entry->pendingSamples, 2);
	entry->samples = 1000000;
	BOOST_CHECK(BotCounter_Record(&cache, first, 0, 10));
	BOOST_CHECK_EQUAL(entry->pendingSamples, 2);
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "CREATE TABLE LocalBotCounterV1("
		"key INTEGER,choice INTEGER,samples INTEGER,net INTEGER)", NULL, NULL, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db,
		"INSERT INTO LocalBotCounterV1 VALUES(?,0,1,1)", -1, &stmt, NULL), SQLITE_OK);
	sqlite3_bind_int(stmt, 1, first + BOTLEARN_COUNTER_CACHE_SIZE);
	BOOST_REQUIRE_EQUAL(sqlite3_step(stmt), SQLITE_DONE);
	sqlite3_finalize(stmt);
	BOOST_CHECK(!BotCounter_Load(&cache, &staging, db));
	BOOST_CHECK_EQUAL(BotCounter_Find(&cache, first, 0, 0)->pendingSamples, 2);
	sqlite3_close(db);
}

BOOST_AUTO_TEST_SUITE_END()
