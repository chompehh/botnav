#include "g_duel_tutorial.h"
#include "g_duel_tutorial_sequences.h"
#include "g_duel_tutorial_chains.h"
#include <boost/test/unit_test.hpp>
#include <set>
#include <string>

BOOST_AUTO_TEST_SUITE(duel_tutorial)

BOOST_AUTO_TEST_CASE(skill_signals_and_results_choose_beginner_advice)
{
	duel_tutorial_session_t session = {};
	BOOST_CHECK(G_DuelTutorialPreferBasics(&session, 3, 3, 0, 0));
	session.flipkick = 1;
	BOOST_CHECK(!G_DuelTutorialPreferBasics(&session, 3, 3, 0, 0));
	BOOST_CHECK(G_DuelTutorialPreferBasics(&session, 2, 3, 0, 0));
	BOOST_CHECK(G_DuelTutorialPreferBasics(&session, 3, 2, 0, 0));
	BOOST_CHECK(G_DuelTutorialPreferBasics(&session, 3, 3, 3, 0));
	BOOST_CHECK(G_DuelTutorialPreferBasics(&session, 3, 3, 0, 2));
	session.forceWins = 3;
	BOOST_CHECK(!G_DuelTutorialPreferBasics(&session, 3, 3, 3, 0));
	BOOST_CHECK(G_DuelTutorialPreferBasics(&session, 2, 3, 3, 0));
	session.forceLosses = 4;
	BOOST_CHECK(G_DuelTutorialPreferBasics(&session, 3, 3, 0, 0));
}

BOOST_AUTO_TEST_CASE(welcome_is_for_beginner_signals_and_only_the_unheard_opening)
{
	duel_tutorial_session_t session = {};
	BOOST_CHECK(G_DuelTutorialWelcomeEligible(&session, 3, 3, 0, 0, 0));
	BOOST_CHECK(G_DuelTutorialWelcomeEligible(&session, 3, 3, 0, 0, 1));
	BOOST_CHECK(!G_DuelTutorialWelcomeEligible(&session, 3, 3, 0, 0, 2));
	session.flipkick = 1;
	BOOST_CHECK(!G_DuelTutorialWelcomeEligible(&session, 3, 3, 0, 0, 0));
	BOOST_CHECK(G_DuelTutorialWelcomeEligible(&session, 2, 3, 0, 0, 0));
	BOOST_CHECK(G_DuelTutorialWelcomeEligible(&session, 3, 2, 0, 0, 0));
	BOOST_CHECK(G_DuelTutorialWelcomeEligible(&session, 3, 3, 3, 0, 0));
	BOOST_CHECK(G_DuelTutorialWelcomeEligible(&session, 3, 3, 0, 2, 0));
	session.welcomeComplete = 1;
	BOOST_CHECK(!G_DuelTutorialWelcomeEligible(&session, 2, 3, 0, 0, 0));
}

BOOST_AUTO_TEST_CASE(first_force_loss_requests_advice_but_other_outcomes_do_not)
{
	duel_tutorial_session_t session = {};
	G_DuelTutorialSessionFinish(&session, 0, 0, 0);
	G_DuelTutorialSessionFinish(&session, 1, 0, 1);
	G_DuelTutorialSessionFinish(&session, 1, 1, 0);
	BOOST_CHECK_EQUAL(session.pendingForceLoss, 0);
	G_DuelTutorialSessionFinish(&session, 1, 0, 0);
	BOOST_CHECK_EQUAL(session.pendingForceLoss, 1);
	BOOST_CHECK_EQUAL(session.forceLosses, 1);
}

BOOST_AUTO_TEST_CASE(beginner_cursor_persists_per_identity)
{
	sqlite3 *db = NULL;
	int cursor = -1;
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialBasicCursor(db, "a", 1, 0, &cursor), SQLITE_OK);
	BOOST_CHECK_EQUAL(cursor, 0);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialBasicCursor(db, "a", 1, 42, &cursor), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialBasicCursor(db, "a", 1, 10, &cursor), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialBasicCursor(db, "a", 1, 0, &cursor), SQLITE_OK);
	BOOST_CHECK_EQUAL(cursor, 42);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialBasicCursor(db, "b", 1, 0, &cursor), SQLITE_OK);
	BOOST_CHECK_EQUAL(cursor, 0);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(session_counts_only_duels_and_offers_saber_reminder_once)
{
	duel_tutorial_session_t session = {};
	BOOST_CHECK(!G_DuelTutorialSessionFinish(&session, 1, 1, 0));
	BOOST_CHECK(!G_DuelTutorialSessionFinish(&session, 1, 0, 0));
	BOOST_CHECK(!G_DuelTutorialSessionFinish(&session, 1, 0, 1));
	BOOST_CHECK(!G_DuelTutorialSessionFinish(&session, 2, 0, 0));
	BOOST_CHECK_EQUAL(session.forceWins, 1);
	BOOST_CHECK_EQUAL(session.forceLosses, 1);
	BOOST_CHECK_EQUAL(session.forceDuels, 3);
	for (int i = 0; i < 9; ++i)
		BOOST_CHECK(!G_DuelTutorialSessionFinish(&session, 0, 1, 0));
	BOOST_CHECK(G_DuelTutorialSessionFinish(&session, 0, 0, 1));
	session.saberReminderSent = 1;
	BOOST_CHECK(!G_DuelTutorialSessionFinish(&session, 0, 1, 0));
	BOOST_CHECK_EQUAL(session.saberDuels, 11);
}

BOOST_AUTO_TEST_CASE(tutorial_message_spacing_grows_with_length)
{
	BOOST_CHECK_EQUAL(G_DuelTutorialMessageDelay(7000, 20, 40), 8200);
	BOOST_CHECK_EQUAL(G_DuelTutorialMessageDelay(7000, 120, 40), 10600);
	BOOST_CHECK_EQUAL(G_DuelTutorialMessageDelay(7000, 1000, 0), 11470);
}

static duel_chain_observation_t observe_long_chain(bool recovery, bool gap, bool won)
{
	duel_chain_observation_t o = {};
	duel_chain_event_t e = {};
	e.state.selfHealth = 40; e.state.enemyHealth = 80;
	e.state.selfForce = 80; e.state.enemyForce = 100;
	G_DuelChainObserve(&o, &e, won);
	const int actions[] = {DTS_THROW, DTS_THROW, DTS_PULL, DTS_PULL, DTS_KICK, DTS_KICK};
	for (int t = 100; t <= (recovery ? 2700 : 2600); t += 100)
	{
		if (gap && t == 1000) t += 600;
		e.state.time = t; e.action = 0; e.actor = 0;
		if (t <= 600) { e.action = actions[t / 100 - 1]; e.actor = (t / 100 - 1) % 2; }
		if (t == 700 && recovery) e.action = DTC_ACTION_HEAL;
		e.recoveryReady = 1;
		e.state.selfForce = t < 700 ? 80 : 10;
		e.state.enemyDraining = t >= 700;
		e.state.selfHealth = won ? 40 : (t < 700 ? 40 : 20);
		e.state.enemyHealth = won ? (t < 700 ? 80 : 20) : 80;
		e.state.dealt = won && t == 700 ? 60 : 0;
		e.state.taken = !won && t == 700 ? 20 : 0;
		G_DuelChainObserve(&o, &e, won);
	}
	return o;
}

BOOST_AUTO_TEST_CASE(long_chains_require_both_sides_and_measure_recovery_opportunities)
{
	duel_chain_observation_t o = observe_long_chain(false, false, false);
	BOOST_REQUIRE_EQUAL(o.count, 1);
	const duel_chain_window_t &w = o.windows[0];
	BOOST_CHECK(w.themes & (1u << DTC_LOW_HEALTH));
	BOOST_CHECK(w.themes & (1u << DTC_DRAIN_EXPOSURE));
	BOOST_CHECK(w.themes & (1u << DTC_FORCE_DEFICIT));
	BOOST_CHECK(w.themes & (1u << DTC_VS_PTK));
	BOOST_CHECK_EQUAL(w.lowHp, 40);
	BOOST_CHECK_EQUAL(w.otherHp, 80);
	BOOST_CHECK_EQUAL(w.selfFp, -70);
	BOOST_CHECK_EQUAL(w.taken, 20);
	BOOST_CHECK(std::string(w.label).find("PTK") != std::string::npos);
	o = observe_long_chain(true, false, false);
	BOOST_REQUIRE_EQUAL(o.count, 1);
	BOOST_CHECK(!(o.windows[0].themes & (1u << DTC_LOW_HEALTH)));
	o = observe_long_chain(false, true, false);
	BOOST_CHECK_EQUAL(o.count, 0);
	o = observe_long_chain(false, false, true);
	BOOST_REQUIRE_EQUAL(o.count, 1);
	BOOST_CHECK(o.windows[0].themes & (1u << DTC_BOT_SUCCESS));
	BOOST_CHECK(!(o.windows[0].themes & (1u << DTC_VS_PTK)));
}

BOOST_AUTO_TEST_CASE(long_chain_counter_themes_follow_observed_opponent_actions_not_damage_labels)
{
	const int enemyActions[][3] = {
		{DTS_THROW, DTS_PULL, DTS_KICK},
		{DTS_PULL, DTS_KICK, DTS_SWING},
		{DTS_GRIP, DTS_KICK, DTS_SWING},
		{DTS_SWING, DTS_SWING, DTS_SWING}
	};
	const int themes[] = {DTC_VS_PTK, DTC_VS_PK, DTC_VS_GK, DTC_VS_SABER};
	for (int combo = 0; combo < 4; ++combo)
		for (int positive = 0; positive <= 1; ++positive)
		{
			duel_chain_observation_t o = {};
			duel_chain_event_t e = {};
			e.state.selfHealth = e.state.enemyHealth = 100;
			e.state.selfForce = e.state.enemyForce = 100;
			G_DuelChainObserve(&o, &e, 0);
			for (int t = 100; t <= 2600; t += 100)
			{
				e.state.time = t; e.action = 0; e.actor = 0;
				if (t <= 600)
				{
					e.actor = (t / 100 - 1) % 2;
					e.action = e.actor ? enemyActions[combo][t / 200 - 1] : DTS_SWING;
				}
				if (t >= 700)
				{
					e.state.selfHealth = positive ? 80 : 60;
					e.state.enemyHealth = positive ? 40 : 80;
					e.state.selfForce = positive ? 90 : 40;
					e.state.enemyForce = positive ? 40 : 90;
				}
				e.state.dealt = t == 700 ? (positive ? 60 : 20) : 0;
				e.state.taken = t == 700 ? (positive ? 20 : 40) : 0;
				G_DuelChainObserve(&o, &e, 0);
			}
			BOOST_REQUIRE_EQUAL(o.count, 1);
			if (positive)
			{
				BOOST_CHECK(o.windows[0].themes & (1u << DTC_SABER_REPLY));
				BOOST_CHECK(!(o.windows[0].themes & (1u << themes[combo])));
			}
			else
			{
				BOOST_CHECK(o.windows[0].themes & (1u << themes[combo]));
				BOOST_CHECK(!(o.windows[0].themes & (1u << DTC_SABER_REPLY)));
			}
		}
}

BOOST_AUTO_TEST_CASE(long_chain_start_uses_preceding_timestamp_not_same_frame_after_state)
{
	duel_chain_observation_t o = {};
	duel_chain_event_t e = {};
	e.state.selfHealth = e.state.enemyHealth = 100;
	e.state.selfForce = e.state.enemyForce = 100;
	G_DuelChainObserve(&o, &e, 0);
	e.state.time = 100; e.state.selfForce = 80;
	G_DuelChainObserve(&o, &e, 0);
	e.action = DTS_THROW;
	G_DuelChainObserve(&o, &e, 0);
	BOOST_CHECK_EQUAL(o.start.selfForce, 100);
	BOOST_CHECK_EQUAL(o.start.time, 100);
	e.state.time = 50;
	G_DuelChainObserve(&o, &e, 0);
	BOOST_CHECK(o.failed);
}

BOOST_AUTO_TEST_CASE(long_chains_reject_truncation_terminal_snapshots_and_invalid_actors)
{
	duel_chain_observation_t o = {};
	duel_chain_event_t e = {};
	e.state.selfHealth = e.state.enemyHealth = e.state.selfForce = e.state.enemyForce = 100;
	G_DuelChainObserve(&o, &e, 0);
	for (int i = 1; i <= 9; ++i)
	{
		e.state.time = i * 100; e.actor = i % 2; e.action = DTS_SWING;
		G_DuelChainObserve(&o, &e, 0);
	}
	e.action = 0;
	for (int t = 1000; t <= 3000; t += 100)
	{
		e.state.time = t;
		G_DuelChainObserve(&o, &e, 0);
	}
	BOOST_CHECK_EQUAL(o.count, 0);
	o = {};
	e.state.time = 0;
	G_DuelChainObserve(&o, &e, 0);
	for (int i = 1; i <= 4; ++i)
	{
		e.state.time = i * 100; e.actor = i % 2; e.action = DTS_SWING;
		G_DuelChainObserve(&o, &e, 0);
	}
	e.action = 0;
	e.state.selfHealth = 0; e.state.time = 500;
	G_DuelChainObserve(&o, &e, 0);
	e.state.selfHealth = 100; e.state.time = 2400;
	G_DuelChainObserve(&o, &e, 0);
	BOOST_CHECK_EQUAL(o.count, 0);
	e.actor = 2;
	G_DuelChainObserve(&o, &e, 0);
	BOOST_CHECK(o.failed);
}

BOOST_AUTO_TEST_CASE(long_chain_history_is_receipted_and_separates_bot_evidence)
{
	sqlite3 *db = NULL;
	duel_chain_observation_t o = observe_long_chain(false, false, false);
	duel_chain_advice_t advice = {};
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelChainRecord(db, "a", 2, 1, "one", &o, 40, 0, &advice), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelChainRecord(db, "a", 2, 1, "one", &o, 40, 0, &advice), SQLITE_OK);
	BOOST_CHECK_EQUAL(advice.theme, -1);
	o.windows[1] = o.windows[0]; o.count = 2;
	BOOST_REQUIRE_EQUAL(G_DuelChainRecord(db, "a", 2, 1, "two", &o, 40, 0, &advice), SQLITE_OK);
	BOOST_REQUIRE_GE(advice.theme, 0);
	BOOST_CHECK_EQUAL(advice.theme, DTC_LOW_HEALTH);
	BOOST_CHECK_EQUAL(advice.matches, 3);
	BOOST_CHECK_EQUAL(advice.attempts, 3);
	BOOST_CHECK_EQUAL(advice.duels, 2);
	BOOST_REQUIRE_EQUAL(G_DuelChainMark(db, "a", 2, 1, &advice, 40), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelChainRecord(db, "a", 2, 0, "human", &o, 41, 0, &advice), SQLITE_OK);
	BOOST_CHECK_EQUAL(advice.theme, -1);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "CREATE TRIGGER fail_chain BEFORE UPDATE ON LocalBotTutorialChainV1 "
		"BEGIN SELECT RAISE(FAIL,'test'); END", NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK_NE(G_DuelChainRecord(db, "a", 2, 1, "three", &o, 45, 0, &advice), SQLITE_OK);
	BOOST_CHECK_EQUAL(advice.theme, -1);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db, "DROP TRIGGER fail_chain", NULL, NULL, NULL), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelChainRecord(db, "a", 2, 1, "three", &o, 45, 0, &advice), SQLITE_OK);
	BOOST_CHECK_EQUAL(advice.attempts, 5);
	BOOST_CHECK_EQUAL(advice.duels, 3);
	BOOST_REQUIRE_EQUAL(G_DuelChainRecord(db, "a", 2, 1, "three", &o, 45, 1, &advice), SQLITE_OK);
	BOOST_CHECK_EQUAL(advice.theme, -1);
	BOOST_REQUIRE_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(bot_win_acknowledgments_require_repeated_positive_bot_windows)
{
	sqlite3 *db = NULL;
	duel_chain_observation_t o = observe_long_chain(false, false, true);
	duel_chain_advice_t advice = {};
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelChainRecord(db, "a", 2, 1, "one", &o, 40, 1, &advice), SQLITE_OK);
	BOOST_CHECK_EQUAL(advice.theme, -1);
	o.windows[1] = o.windows[0]; o.count = 2;
	BOOST_REQUIRE_EQUAL(G_DuelChainRecord(db, "a", 2, 1, "two", &o, 40, 1, &advice), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(advice.theme, DTC_BOT_SUCCESS);
	BOOST_REQUIRE_EQUAL(G_DuelChainMark(db, "a", 2, 1, &advice, 40), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(G_DuelChainRecord(db, "a", 2, 1, "three", &o, 41, 1, &advice), SQLITE_OK);
	BOOST_CHECK_EQUAL(advice.theme, -1);
	BOOST_REQUIRE_EQUAL(G_DuelChainRecord(db, "a", 2, 1, "four", &o, 44, 1, &advice), SQLITE_OK);
	BOOST_CHECK_EQUAL(advice.theme, DTC_BOT_SUCCESS);
	BOOST_REQUIRE_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE(long_chain_catalog_has_one_hundred_evidence_scoped_messages)
{
	std::set<std::string> messages;
	duel_chain_advice_t advice = {};
	char text[150];
	BOOST_REQUIRE_EQUAL(DUEL_CHAIN_MESSAGE_COUNT, 100u);
	for (int theme = 0; theme < DTC_THEME_COUNT; ++theme)
		for (int i = 0; i < 10; ++i)
		{
			const duel_chain_message_t &lesson = g_duelChainMessages[theme * 10 + i];
			BOOST_CHECK_EQUAL(lesson.theme, theme);
			BOOST_CHECK(messages.insert(lesson.text).second);
			advice.theme = theme;
			advice.matches = advice.attempts = 999999;
			advice.sample.lowHp = advice.sample.otherHp = 999999;
			advice.sample.deficitFp = advice.sample.otherFp = 999999;
			advice.sample.dealt = advice.sample.taken = 999999;
			std::memset(advice.sample.label, 'x', sizeof(advice.sample.label) - 1);
			advice.sample.label[sizeof(advice.sample.label) - 1] = '\0';
			BOOST_REQUIRE(G_DuelChainRender(&advice, i, text, sizeof(text)));
			BOOST_CHECK_LT(std::strlen(text), 150u);
		}
}

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
	BOOST_REQUIRE_EQUAL(G_DuelTutorialImportComplete(db, "account", 1, 1, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels, 2);
	BOOST_CHECK_EQUAL(h.modeDuels, 2);
	BOOST_REQUIRE_EQUAL(G_DuelTutorialRecord(db, "account", 1, 1, 0, 0, &h), SQLITE_OK);
	BOOST_CHECK_EQUAL(h.duels, 3);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_SUITE_END()
