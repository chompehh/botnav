#include "g_duel_tutorial.h"
#include "g_duel_tutorial_sequences.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(duel_tutorial)

BOOST_AUTO_TEST_CASE(low_health_attacks_require_evidence_and_available_heal)
{
	duel_tutorial_observation_t observation;
	duel_tutorial_patterns_t patterns = {};
	duel_tutorial_event_t event = {};
	G_DuelTutorialObserveInit(&observation);
	event.type = DUEL_TUTORIAL_EVENT_ATTACK;
	event.confirmed = 1;
	event.time = 100;
	event.selfHealth = 40;
	event.enemyHealth = 80;
	G_DuelTutorialObserve(&observation, &event, 3);
	BOOST_CHECK_EQUAL(observation.count[DUEL_TUTORIAL_LOW_HEALTH_ATTACK], 0);
	event.healAvailable = 1;
	G_DuelTutorialObserve(&observation, &event, 3);
	event.time = 200;
	G_DuelTutorialObserve(&observation, &event, 3);
	BOOST_CHECK_EQUAL(observation.count[DUEL_TUTORIAL_LOW_HEALTH_ATTACK], 1);
	event.time = 1200;
	event.enemyHealth = 59;
	G_DuelTutorialObserve(&observation, &event, 3);
	BOOST_CHECK_EQUAL(observation.count[DUEL_TUTORIAL_LOW_HEALTH_ATTACK], 1);
	event.enemyHealth = 80;
	G_DuelTutorialObserve(&observation, &event, 3);
	BOOST_CHECK_EQUAL(observation.count[DUEL_TUTORIAL_LOW_HEALTH_ATTACK], 2);
	patterns.lastRotation[DUEL_TUTORIAL_LOW_HEALTH_ATTACK] = -1000;
	patterns.duels[DUEL_TUTORIAL_LOW_HEALTH_ATTACK] = 1;
	BOOST_CHECK_EQUAL(G_DuelTutorialSelectPattern(3, 30, &observation, &patterns), -1);
	patterns.duels[DUEL_TUTORIAL_LOW_HEALTH_ATTACK] = 2;
	BOOST_CHECK_EQUAL(G_DuelTutorialSelectPattern(3, 30, &observation, &patterns), DUEL_TUTORIAL_LOW_HEALTH_ATTACK);
	BOOST_CHECK_EQUAL(observation.sampleSelfHealth, 40);
	BOOST_CHECK_EQUAL(observation.sampleEnemyHealth, 80);
	patterns.lastRotation[DUEL_TUTORIAL_LOW_HEALTH_ATTACK] = 28;
	BOOST_CHECK_EQUAL(G_DuelTutorialSelectPattern(3, 30, &observation, &patterns), -1);
}

BOOST_AUTO_TEST_CASE(introduction_cursor_persists_independently_for_each_identity)
{
	sqlite3 *db = NULL;
	int cursor = -1;
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialIntroCursor(db, "iphash:a", 2, 0, &cursor), SQLITE_OK);
	BOOST_CHECK_EQUAL(cursor, 0);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialIntroCursor(db, "iphash:a", 2, 2, &cursor), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialIntroCursor(db, "iphash:a", 2, 1, &cursor), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialIntroCursor(db, "iphash:a", 2, 0, &cursor), SQLITE_OK);
	BOOST_CHECK_EQUAL(cursor, 2);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialIntroCursor(db, "iphash:a", 1, 0, &cursor), SQLITE_OK);
	BOOST_CHECK_EQUAL(cursor, 0);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialIntroCursor(db, "iphash:b", 2, 0, &cursor), SQLITE_OK);
	BOOST_CHECK_EQUAL(cursor, 0);
	BOOST_REQUIRE_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(power_actions_require_hand_or_active_edges_not_force_loss)
{
	BOOST_CHECK_EQUAL(G_DuelSequencePowerAction(4, 4, 0, 0, 1, 2, 4, 8), DTS_NONE);
	BOOST_CHECK_EQUAL(G_DuelSequencePowerAction(0, 0, 0, 1, 1, 2, 4, 8), DTS_PULL);
	BOOST_CHECK_EQUAL(G_DuelSequencePowerAction(0, 8, 0, 0, 1, 2, 4, 8), DTS_DRAIN);
	BOOST_CHECK_EQUAL(G_DuelSequencePowerAction(8, 8, 0, 0, 1, 2, 4, 8), DTS_NONE);
}

BOOST_AUTO_TEST_CASE(flipkick_requires_confirmed_opponent_target)
{
	BOOST_CHECK(G_DuelSequenceKickEdge(0, 0, 7, -1, 7));
	BOOST_CHECK(!G_DuelSequenceKickEdge(0, 0, 7, 7, 7));
	BOOST_CHECK(!G_DuelSequenceKickEdge(0, 0, -1, -1, 7));
	BOOST_CHECK(!G_DuelSequenceKickEdge(0, 0, 8, -1, 7));
	BOOST_CHECK(!G_DuelSequenceKickEdge(0, 0, -1, 7, -1));
	BOOST_CHECK(G_DuelSequenceKickEdge(1, 0, -1, -1, 7));
	BOOST_CHECK(!G_DuelSequenceKickEdge(1, 1, -1, -1, 7));
}

BOOST_AUTO_TEST_CASE(sequence_windows_measure_net_resources_and_do_not_overlap)
{
	duel_sequence_observation_t o = {};
	duel_sequence_event_t e = {};
	e.selfHealth = e.enemyHealth = 100;
	e.selfForce = e.enemyForce = 100;
	e.time = 900; G_DuelSequenceObserve(&o, &e);
	e.time = 1000; e.action = DTS_THROW; e.selfForce = 95; G_DuelSequenceObserve(&o, &e);
	e.time = 1100; e.action = DTS_PULL; e.selfForce = 85; G_DuelSequenceObserve(&o, &e);
	e.time = 1200; e.action = DTS_KICK; G_DuelSequenceObserve(&o, &e);
	e.time = 2000; e.action = DTS_PULL; e.taken = 20; e.selfHealth = 80; G_DuelSequenceObserve(&o, &e);
	e.time = 3200; e.action = DTS_DRAIN; e.taken = 0; e.selfForce = 90; e.enemyForce = 80; e.enemyHealth = 95;
	G_DuelSequenceObserve(&o, &e);
	const duel_sequence_stat_t &s = o.stat[DTS_THROW_PULL_KICK];
	BOOST_CHECK_EQUAL(s.attempts, 1);
	BOOST_CHECK_EQUAL(s.losses, 1);
	BOOST_CHECK_EQUAL(s.sampleSelfHp, -20);
	BOOST_CHECK_EQUAL(s.sampleSelfFp, -10);
	BOOST_CHECK_EQUAL(s.sampleEnemyFp, -20);
	BOOST_CHECK_EQUAL(s.sampleTaken, 20);
	BOOST_CHECK_EQUAL(o.stat[DTS_PULL_KICK].attempts, 0);
	BOOST_CHECK(!o.active);
}

BOOST_AUTO_TEST_CASE(sequence_good_damage_heal_and_missing_samples_are_not_bad)
{
	duel_sequence_observation_t o = {};
	duel_sequence_event_t e = {};
	e.selfHealth = 80; e.enemyHealth = 100; e.selfForce = 100;
	e.time = 1; G_DuelSequenceObserve(&o, &e);
	e.time = 100; e.action = DTS_DRAIN; e.selfForce = 90; G_DuelSequenceObserve(&o, &e);
	e.time = 2100; e.action = 0; e.selfHealth = 95; e.enemyHealth = 80; e.dealt = 20;
	G_DuelSequenceObserve(&o, &e);
	BOOST_CHECK_EQUAL(o.stat[DTS_DRAIN_ALONE].losses, 0);
	BOOST_CHECK_EQUAL(o.stat[DTS_DRAIN_ALONE].noDamageFp, 0);
	BOOST_CHECK_EQUAL(o.stat[DTS_DRAIN_ALONE].sampleSelfHp, 15);
	e.time = 2200; e.action = DTS_DRAIN; e.dealt = 0; G_DuelSequenceObserve(&o, &e);
	e.time = 5000; e.action = 0; G_DuelSequenceObserve(&o, &e);
	BOOST_CHECK_EQUAL(o.stat[DTS_DRAIN_ALONE].attempts, 1);
}

BOOST_AUTO_TEST_CASE(no_damage_force_decline_is_not_a_gross_cost_or_a_loss)
{
	duel_sequence_observation_t o = {};
	duel_sequence_event_t e = {};
	e.selfHealth = e.enemyHealth = 100;
	e.selfForce = e.enemyForce = 100;
	e.time = 10; G_DuelSequenceObserve(&o, &e);
	e.time = 20; e.action = DTS_PULL; e.selfForce = 90; G_DuelSequenceObserve(&o, &e);
	e.time = 30; e.action = DTS_KICK; G_DuelSequenceObserve(&o, &e);
	e.time = 2030; e.action = 0; e.selfForce = 95; e.enemyForce = 75;
	G_DuelSequenceObserve(&o, &e);
	const duel_sequence_stat_t &s = o.stat[DTS_PULL_KICK];
	BOOST_CHECK_EQUAL(s.noDamageFp, 1);
	BOOST_CHECK_EQUAL(s.poorForce, 0);
	BOOST_CHECK_EQUAL(s.losses, 0);
	BOOST_CHECK_EQUAL(s.sampleSelfFp, -5);
	BOOST_CHECK_EQUAL(s.sampleEnemyFp, -25);
}

BOOST_AUTO_TEST_CASE(resource_inefficiency_requires_no_health_benefit_and_worse_fp_trade)
{
	const auto sample = [](int action, int selfFp, int enemyFp, int selfHp, int enemyHp, int draining) {
		duel_sequence_observation_t o = {};
		duel_sequence_event_t e = {};
		e.selfHealth = e.enemyHealth = e.selfForce = e.enemyForce = 100;
		e.time = 10; G_DuelSequenceObserve(&o, &e);
		e.time = 20; e.action = action; e.enemyDraining = draining;
		G_DuelSequenceObserve(&o, &e);
		e.time = 2020; e.action = 0;
		e.selfForce = selfFp; e.enemyForce = enemyFp;
		e.selfHealth = selfHp; e.enemyHealth = enemyHp;
		G_DuelSequenceObserve(&o, &e);
		return o.stat[draining ? DTS_ANTI_DRAIN_PULL : DTS_DRAIN_ALONE];
	};
	duel_sequence_stat_t s = sample(DTS_DRAIN, 90, 50, 100, 100, 0);
	BOOST_CHECK_EQUAL(s.noDamageFp, 1);
	BOOST_CHECK_EQUAL(s.poorForce, 0);
	s = sample(DTS_PULL, 80, 95, 100, 100, 1);
	BOOST_CHECK_EQUAL(s.poorForce, 1);
	s = sample(DTS_DRAIN, 80, 95, 105, 100, 0);
	BOOST_CHECK_EQUAL(s.noDamageFp, 1);
	BOOST_CHECK_EQUAL(s.poorForce, 0);
	s = sample(DTS_DRAIN, 80, 95, 100, 90, 0);
	BOOST_CHECK_EQUAL(s.poorForce, 0);
	s = sample(DTS_DRAIN, 80, 80, 100, 100, 0);
	BOOST_CHECK_EQUAL(s.poorForce, 0);
}

BOOST_AUTO_TEST_CASE(damage_after_deadline_is_not_attributed_to_window)
{
	const auto sample = [](int endTime) {
		duel_sequence_observation_t o = {};
		duel_sequence_event_t e = {};
		e.time = 1; G_DuelSequenceObserve(&o, &e);
		e.time = 10; e.action = DTS_DRAIN; G_DuelSequenceObserve(&o, &e);
		e.time = endTime; e.action = 0; e.dealt = 12; e.taken = 7;
		G_DuelSequenceObserve(&o, &e);
		return o.stat[DTS_DRAIN_ALONE];
	};
	duel_sequence_stat_t atDeadline = sample(2010);
	duel_sequence_stat_t afterDeadline = sample(2011);
	BOOST_CHECK_EQUAL(atDeadline.sampleDealt, 12);
	BOOST_CHECK_EQUAL(atDeadline.sampleTaken, 7);
	BOOST_CHECK_EQUAL(afterDeadline.attempts, 1);
	BOOST_CHECK_EQUAL(afterDeadline.sampleDealt, 0);
	BOOST_CHECK_EQUAL(afterDeadline.sampleTaken, 0);
}

BOOST_AUTO_TEST_CASE(terminal_duel_before_deadline_leaves_outcome_unknown)
{
	duel_sequence_observation_t o = {};
	duel_sequence_event_t e = {};
	e.time = 10; G_DuelSequenceObserve(&o, &e);
	e.time = 20; e.action = DTS_DRAIN; G_DuelSequenceObserve(&o, &e);
	e.time = 2000; e.action = 0; e.taken = 20;
	G_DuelSequenceObserve(&o, &e);
	BOOST_CHECK_EQUAL(o.stat[DTS_DRAIN_ALONE].attempts, 0);
	BOOST_CHECK(o.active);
}

BOOST_AUTO_TEST_CASE(sequence_message_rotation_uses_only_applicable_evidence)
{
	duel_sequence_stat_t total = {}, current = {};
	duel_sequence_observation_t observation = {};
	duel_sequence_history_t history = {};
	observation.stat[DTS_DRAIN_ALONE].attempts = 1;
	observation.stat[DTS_DRAIN_ALONE].sampleNoDamageFp = 1;
	history.stat[DTS_DRAIN_ALONE].attempts = 3;
	history.stat[DTS_DRAIN_ALONE].noDamageFp = 3;
	history.duels[DTS_DRAIN_ALONE] = 2;
	history.lastRotation[DTS_DRAIN_ALONE] = -1000;
	BOOST_CHECK_EQUAL(G_DuelSequenceSelect(3, 1, &observation, &history), -1);
	total.attempts = 3; total.losses = 2; current.sampleLoss = 1;
	for (int rotation = 1; rotation <= 4; ++rotation)
		BOOST_CHECK_EQUAL(G_DuelSequenceMessageKind(rotation, &total, &current), DTS_MESSAGE_LOSS);
	total.poorForce = 2; current.samplePoorForce = 1;
	BOOST_CHECK_EQUAL(G_DuelSequenceMessageKind(1, &total, &current), DTS_MESSAGE_LOSS);
	BOOST_CHECK_EQUAL(G_DuelSequenceMessageKind(2, &total, &current), DTS_MESSAGE_POOR_FORCE);
	current.sampleLoss = 0; current.samplePoorForce = 0;
	BOOST_CHECK_EQUAL(G_DuelSequenceMessageKind(3, &total, &current), -1);
	total.attempts = 4;
	BOOST_CHECK_EQUAL(G_DuelSequenceMessageKind(3, &total, &current), DTS_MESSAGE_REPEAT);
	BOOST_CHECK(!G_DuelTutorialPatternAllowedInMode(DUEL_TUTORIAL_STRAIGHT, 1));
	BOOST_CHECK(!G_DuelTutorialPatternAllowedInMode(DUEL_TUTORIAL_RECOVERY, 1));
	BOOST_CHECK(G_DuelTutorialPatternAllowedInMode(DUEL_TUTORIAL_STRAIGHT, 0));
	BOOST_CHECK(G_DuelTutorialPatternAllowedInMode(DUEL_TUTORIAL_DRAIN, 1));
}

BOOST_AUTO_TEST_CASE(sequence_history_is_atomic_idempotent_and_mode_separated)
{
	sqlite3 *db = NULL;
	duel_sequence_observation_t o = {};
	duel_sequence_history_t h = {};
	o.stat[DTS_PULL_KICK].attempts = 2;
	o.stat[DTS_PULL_KICK].losses = 2;
	o.stat[DTS_PULL_KICK].noDamageFp = 2;
	o.stat[DTS_PULL_KICK].poorForce = 1;
	o.stat[DTS_PULL_KICK].sampleLoss = 1;
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialSequenceRecord(db, "iphash:x", 2, 1, "duel1", &o, &h), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialSequenceRecord(db, "iphash:x", 2, 1, "duel1", &o, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.stat[DTS_PULL_KICK].attempts, 2);
	BOOST_CHECK_EQUAL(h.duels[DTS_PULL_KICK], 1);
	BOOST_CHECK_EQUAL(G_DuelSequenceSelect(3, 1, &o, &h), -1);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialSequenceRecord(db, "iphash:x", 2, 0, "duel2", &o, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels[DTS_PULL_KICK], 1);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialSequenceRecord(db, "iphash:x", 2, 1, "duel2", &o, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels[DTS_PULL_KICK], 2);
	BOOST_CHECK_EQUAL(h.stat[DTS_PULL_KICK].noDamageFp, 4);
	BOOST_CHECK_EQUAL(h.stat[DTS_PULL_KICK].poorForce, 2);
	BOOST_CHECK_EQUAL(G_DuelSequenceSelect(3, 1, &o, &h), DTS_PULL_KICK);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "CREATE TRIGGER fail_sequence BEFORE UPDATE ON LocalBotTutorialSequenceV3 BEGIN SELECT RAISE(FAIL,'test'); END", NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK_NE(G_DuelTutorialSequenceRecord(db, "iphash:x", 2, 1, "duel3", &o, &h), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "DROP TRIGGER fail_sequence", NULL, NULL, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialSequenceRecord(db, "iphash:x", 2, 1, "duel3", &o, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels[DTS_PULL_KICK], 3);
	BOOST_CHECK_EQUAL(h.stat[DTS_PULL_KICK].attempts, 6);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(gates_reject_short_incomplete_and_insufficient_evidence)
{
	BOOST_CHECK(!G_DuelTutorialInformative(20, 2000, 1));
	BOOST_CHECK(!G_DuelTutorialInformative(9, 20000, 1));
	BOOST_CHECK(!G_DuelTutorialInformative(20, 20000, 0));
	BOOST_CHECK(!G_DuelTutorialCanTeach(1, 2));
	BOOST_CHECK(!G_DuelTutorialCanTeach(0, 99));
	BOOST_CHECK(G_DuelTutorialCanTeach(1, 3));
	BOOST_CHECK(!G_DuelTutorialCanArcadeTeach(1, 2));
	BOOST_CHECK(!G_DuelTutorialCanArcadeTeach(0, 3));
	BOOST_CHECK(G_DuelTutorialCanArcadeTeach(1, 3));
	BOOST_CHECK(!G_DuelTutorialCanDescribePattern(1, 3, 1));
	BOOST_CHECK(!G_DuelTutorialCanDescribePattern(0, 9, 9));
	BOOST_CHECK(G_DuelTutorialCanDescribePattern(1, 3, 2));
}

BOOST_AUTO_TEST_CASE(history_is_persistent_separate_by_identity_and_mode)
{
	sqlite3 *db = NULL;
	sqlite3 *reconnected = NULL;
	duel_tutorial_history_t h = {};
	BOOST_REQUIRE_EQUAL(sqlite3_open_v2("file:duel_tutorial_test?mode=memory&cache=shared", &db,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecord(db, "iphash:123", 2, 0, 1, 0, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels, 1);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecord(db, "iphash:123", 2, 1, 1, 1, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels, 2);
	BOOST_CHECK_EQUAL(h.recoveryDuels, 1);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecord(db, "iphash:123", 2, 1, 1, 1, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels, 3);
	BOOST_CHECK_EQUAL(h.recoveryDuels, 2);
	BOOST_CHECK_EQUAL(h.rotation, 1);
	BOOST_REQUIRE_EQUAL(sqlite3_open_v2("file:duel_tutorial_test?mode=memory&cache=shared", &reconnected,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecord(reconnected, "iphash:123", 2, 1, 0, 0, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels, 4);
	BOOST_CHECK_EQUAL(h.recoveryDuels, 2);
	BOOST_CHECK_EQUAL(h.rotation, 1);
	BOOST_CHECK_EQUAL(sqlite3_close(reconnected), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecord(db, "iphash:123", 2, 0, 0, 0, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels, 5);
	BOOST_CHECK_EQUAL(h.recoveryDuels, 1);
	BOOST_CHECK_EQUAL(h.rotation, 0);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecord(db, "account:123", 1, 1, 0, 0, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels, 1);
	BOOST_CHECK_EQUAL(h.recoveryDuels, 0);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(database_failure_does_not_invent_history)
{
	sqlite3 *db = NULL;
	duel_tutorial_history_t h = {};
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "CREATE TABLE LocalBotTutorialV1(broken INTEGER)", NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK_NE(G_DuelTutorialRecord(db, "player", 1, 1, 1, 1, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels, 0);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(arcade_rotation_waits_for_comparable_rounds)
{
	sqlite3 *db = NULL;
	duel_tutorial_history_t h = {};
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	for (int i = 0; i < 3; ++i)
		BOOST_REQUIRE_EQUAL(G_DuelTutorialRecord(db, "player", 1, 1, 0, 0, &h), SQLITE_OK);
	for (int i = 0; i < 2; ++i)
	{
		BOOST_REQUIRE_EQUAL(G_DuelTutorialRecord(db, "player", 1, 2, 0, 1, &h), SQLITE_OK);
		BOOST_CHECK_EQUAL(h.rotation, 0);
	}
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecord(db, "player", 1, 2, 0, 1, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.modeDuels, 3);
	BOOST_CHECK_EQUAL(h.rotation, 1);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(patterns_require_confirmed_order_and_distinct_duels)
{
	sqlite3 *db = NULL;
	duel_tutorial_observation_t o;
	duel_tutorial_patterns_t p;
	duel_tutorial_event_t e = {};
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	G_DuelTutorialObserveInit(&o);
	e.type = DUEL_TUTORIAL_EVENT_SPEND; e.time = 1000; e.power = 3; e.confirmed = 0;
	G_DuelTutorialObserve(&o, &e, 3);
	e.type = DUEL_TUTORIAL_EVENT_HIT; e.time = 1500; e.confirmed = 1; e.force = 10;
	G_DuelTutorialObserve(&o, &e, 3);
	BOOST_CHECK_EQUAL(o.count[DUEL_TUTORIAL_LOW_FORCE], 0);
	e.type = DUEL_TUTORIAL_EVENT_SPEND; e.time = 3000; e.power = 3; e.confirmed = 1;
	G_DuelTutorialObserve(&o, &e, 3);
	e.type = DUEL_TUTORIAL_EVENT_HIT; e.time = 4000; e.force = 12; e.straight = 1; e.recovery = 1;
	G_DuelTutorialObserve(&o, &e, 3);
	BOOST_CHECK_EQUAL(o.count[DUEL_TUTORIAL_LOW_FORCE], 1);
	BOOST_CHECK_EQUAL(o.count[DUEL_TUTORIAL_DRAIN], 1);
	BOOST_CHECK_EQUAL(o.sampleForce, 12);
	e.type = DUEL_TUTORIAL_EVENT_SPEND; e.time = 6000; e.power = 3; e.confirmed = 1;
	G_DuelTutorialObserve(&o, &e, 3);
	e.type = DUEL_TUTORIAL_EVENT_HIT; e.time = 7000; e.force = 11;
	G_DuelTutorialObserve(&o, &e, 3);
	BOOST_CHECK_EQUAL(o.count[DUEL_TUTORIAL_LOW_FORCE], 2);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecordPatterns(db, "iphash:123", 2, 1, &o, &p), SQLITE_OK);
	BOOST_CHECK_EQUAL(p.duels[DUEL_TUTORIAL_LOW_FORCE], 1);
	BOOST_CHECK_EQUAL(p.count[DUEL_TUTORIAL_LOW_FORCE], 2);
	{
		sqlite3_stmt *stmt = NULL;
		BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db, "SELECT sample_power,sample_force,sample_time FROM LocalBotTutorialPatternV1 WHERE pattern=0", -1, &stmt, NULL), SQLITE_OK);
		BOOST_REQUIRE_EQUAL(sqlite3_step(stmt), SQLITE_ROW);
		BOOST_CHECK_EQUAL(sqlite3_column_int(stmt, 0), 3);
		BOOST_CHECK_EQUAL(sqlite3_column_int(stmt, 1), 11);
		BOOST_CHECK_EQUAL(sqlite3_column_int(stmt, 2), 7000);
		sqlite3_finalize(stmt);
	}
	BOOST_CHECK_EQUAL(G_DuelTutorialSelectPattern(3, 1, &o, &p), -1);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecordPatterns(db, "iphash:123", 2, 0, &o, &p), SQLITE_OK);
	BOOST_CHECK_EQUAL(p.duels[DUEL_TUTORIAL_LOW_FORCE], 1);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecordPatterns(db, "iphash:123", 2, 1, &o, &p), SQLITE_OK);
	BOOST_CHECK_EQUAL(p.duels[DUEL_TUTORIAL_LOW_FORCE], 2);
	BOOST_CHECK_EQUAL(p.count[DUEL_TUTORIAL_LOW_FORCE], 4);
	BOOST_CHECK_EQUAL(G_DuelTutorialSelectPattern(3, 2, &o, &p), DUEL_TUTORIAL_LOW_FORCE);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialMarkPattern(db, "iphash:123", 2, 1, DUEL_TUTORIAL_LOW_FORCE, 2), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecordPatterns(db, "iphash:123", 2, 1, &o, &p), SQLITE_OK);
	BOOST_CHECK_EQUAL(G_DuelTutorialSelectPattern(4, 3, &o, &p), DUEL_TUTORIAL_DRAIN);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialMarkPattern(db, "iphash:123", 2, 1, DUEL_TUTORIAL_DRAIN, 3), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecordPatterns(db, "iphash:123", 2, 1, &o, &p), SQLITE_OK);
	BOOST_CHECK_EQUAL(G_DuelTutorialSelectPattern(5, 4, &o, &p), DUEL_TUTORIAL_STRAIGHT);
	BOOST_CHECK_EQUAL(G_DuelTutorialSelectPattern(7, 6, &o, &p), DUEL_TUTORIAL_LOW_FORCE);
	G_DuelTutorialObserveInit(&o);
	e.type = DUEL_TUTORIAL_EVENT_KNOCKDOWN; e.time = 1000; e.confirmed = 1;
	G_DuelTutorialObserve(&o, &e, 3);
	e.type = DUEL_TUTORIAL_EVENT_HIT; e.time = 4501; e.confirmed = 1;
	G_DuelTutorialObserve(&o, &e, 3);
	BOOST_CHECK_EQUAL(o.count[DUEL_TUTORIAL_KNOCKDOWN], 0);
	e.type = DUEL_TUTORIAL_EVENT_KNOCKDOWN; e.time = 6000; e.confirmed = 0;
	G_DuelTutorialObserve(&o, &e, 3);
	e.type = DUEL_TUTORIAL_EVENT_HIT; e.time = 7000; e.confirmed = 1;
	G_DuelTutorialObserve(&o, &e, 3);
	BOOST_CHECK_EQUAL(o.count[DUEL_TUTORIAL_KNOCKDOWN], 0);
	e.type = DUEL_TUTORIAL_EVENT_KNOCKDOWN; e.time = 8000; e.confirmed = 1;
	G_DuelTutorialObserve(&o, &e, 3);
	e.type = DUEL_TUTORIAL_EVENT_HIT; e.time = 9000; e.confirmed = 1;
	G_DuelTutorialObserve(&o, &e, 3);
	BOOST_CHECK_EQUAL(o.count[DUEL_TUTORIAL_KNOCKDOWN], 1);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(imports_only_complete_versioned_unimported_captures)
{
	sqlite3 *db = NULL;
	duel_tutorial_history_t h = {};
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db,
		"CREATE TABLE LocalDuelTrackSummary(id INTEGER,type INTEGER,source_context TEXT,capture_version INTEGER,result TEXT);"
		"CREATE TABLE LocalDuelTrackParticipant(id INTEGER PRIMARY KEY,summary_id INTEGER,participant_key TEXT,participant_kind INTEGER,event_total INTEGER,event_retained INTEGER,event_dropped INTEGER,event_critical_dropped INTEGER,event_allocation_failures INTEGER);"
		"CREATE TABLE LocalDuelTrackEvent(summary_id INTEGER,participant_key TEXT,event_type TEXT,rel_time INTEGER);"
		"INSERT INTO LocalDuelTrackSummary VALUES(1,1,'duel',14,'duel_complete'),(2,1,'duel',14,'duel_complete'),(3,1,'duel',13,'duel_complete');"
		"INSERT INTO LocalDuelTrackParticipant VALUES(1,1,'account',1,10,10,0,0,0),(2,2,'account',1,10,10,1,0,0),(3,3,'account',1,10,10,0,0,0);",
		NULL, NULL, NULL), SQLITE_OK);
	for (int duel = 1; duel <= 3; ++duel)
		for (int i = 0; i < 10; ++i)
		{
			sqlite3_stmt *stmt = NULL;
			BOOST_REQUIRE_EQUAL(sqlite3_prepare_v2(db, "INSERT INTO LocalDuelTrackEvent VALUES(?,'account','damage',?)", -1, &stmt, NULL), SQLITE_OK);
			sqlite3_bind_int(stmt, 1, duel);
			sqlite3_bind_int(stmt, 2, i == 9 ? 12000 : i * 100);
			BOOST_REQUIRE_EQUAL(sqlite3_step(stmt), SQLITE_DONE);
			sqlite3_finalize(stmt);
		}
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecord(db, "account", 1, 1, 0, 0, &h), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialImportComplete(db, "account", 1, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels, 2);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecord(db, "account", 1, 1, 0, 0, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels, 3);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_SUITE_END()
