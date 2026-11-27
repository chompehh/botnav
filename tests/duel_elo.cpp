#include "g_duel_elo.h"
#include "g_duel_elo_limits.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE( duel_elo )

BOOST_AUTO_TEST_CASE(daily_bot_duel_counts_cover_levels_and_ignore_unranked)
{
	sqlite3 *db = NULL;
	BOOST_REQUIRE_EQUAL(sqlite3_open(":memory:", &db), SQLITE_OK);
	BOOST_REQUIRE_EQUAL(sqlite3_exec(db,
		"CREATE TABLE LocalDuel(winner TEXT, loser TEXT, end_time INTEGER, winner_elo REAL, loser_elo REAL);"
		"INSERT INTO LocalDuel VALUES"
		"('alice','botlvl1',86400,1000,1000),"
		"('botlvl2','alice',86401,1000,1000),"
		"('alice','botlvl3',86402,-999,-999),"
		"('alice','botlvl1',172800,1000,1000),"
		"('botlvl1','botlvl2',86403,1000,1000),"
		"('botlvl3','botlvl1',86404,1000,1000),"
		"('legacy [bot L1]','botlvl3',86405,1000,1000),"
		"('alice','bob',86406,1000,1000);",
		NULL, NULL, NULL), SQLITE_OK);
	BOOST_CHECK_EQUAL(G_DuelRatedBotDuelsToday(db, "alice", NULL, 86420), 2);
	BOOST_CHECK_EQUAL(G_DuelRatedBotDuelsToday(db, "alice", NULL, 172800), 1);
	BOOST_CHECK_EQUAL(G_DuelRatedBotDuelsToday(db, "botlvl1", "% [bot L1]", 86420), 3);
	BOOST_CHECK_EQUAL(G_DuelRatedBotDuelsToday(db, "botlvl2", "% [bot L2]", 86420), 1);
	BOOST_CHECK_EQUAL(G_DuelRatedBotDuelsToday(db, "bob", NULL, 86420), 0);
	BOOST_CHECK_EQUAL(G_DuelBotRatingAllowed(db, "alice", "botlvl3", 0, 3, 2, 5, 86420), 0);
	BOOST_CHECK_EQUAL(G_DuelBotRatingAllowed(db, "botlvl3", "alice", 3, 0, 3, 5, 86420), 1);
	BOOST_CHECK_EQUAL(G_DuelBotRatingAllowed(db, "alice", "botlvl3", 0, 3, 2, 5, 172800), 1);
	BOOST_CHECK_EQUAL(G_DuelBotRatingAllowed(db, "botlvl1", "botlvl2", 1, 2, 0, 3, 86420), 0);
	BOOST_CHECK_EQUAL(G_DuelBotRatingAllowed(db, "botlvl2", "botlvl1", 2, 1, 0, 3, 86420), 0);
	BOOST_CHECK_EQUAL(G_DuelBotRatingAllowed(db, "botlvl1", "botlvl3", 1, 3, 0, 0, 86420), 1);
	BOOST_CHECK_EQUAL(G_DuelBotRatingAllowed(db, "alice", "bob", 0, 0, 1, 1, 86420), 1);
	BOOST_CHECK_EQUAL(G_DuelBotRatingAllowed(NULL, "alice", "botlvl1", 0, 1, 1, 1, 86420), -1);
	BOOST_CHECK_EQUAL(sqlite3_close(db), SQLITE_OK);
}

BOOST_AUTO_TEST_CASE( human_wins_against_each_bot_seed_gain_rating )
{
	for (int level = 1; level <= 10; ++level)
	{
		const float botElo = 800.0f + (level - 1) * 300.0f / 9.0f;
		float humanAfter, botAfter;
		const float odds = G_CalculateDuelElo(1000.0f, botElo, 25.0f, 25.0f, &humanAfter, &botAfter);

		BOOST_CHECK_GT(humanAfter, 1000.0f);
		BOOST_CHECK_LT(botAfter, botElo);
		BOOST_CHECK_GT(odds, 0.0f);
		BOOST_CHECK_LT(odds, 1.0f);

		float humanAgain, botAgain;
		G_CalculateDuelElo(humanAfter, botAfter, 25.0f, 25.0f, &humanAgain, &botAgain);
		BOOST_CHECK_GT(humanAgain, humanAfter);
		BOOST_CHECK_LT(botAgain, botAfter);
	}
}

BOOST_AUTO_TEST_CASE( fractional_provisional_k_does_not_erase_gains )
{
	float winnerAfter, loserAfter;
	G_CalculateDuelElo(1000.0f, 1000.0f, 1.0f * 0.1f, 1.0f * 0.1f, &winnerAfter, &loserAfter);
	BOOST_CHECK_CLOSE(winnerAfter, 1000.05f, 0.0001f);
	BOOST_CHECK_CLOSE(loserAfter, 999.95f, 0.0001f);
}

BOOST_AUTO_TEST_CASE( equal_ratings_and_upsets_keep_standard_elo_behavior )
{
	float winnerAfter, loserAfter;
	const float odds = G_CalculateDuelElo(1000.0f, 1000.0f, 25.0f, 25.0f, &winnerAfter, &loserAfter);
	BOOST_CHECK_EQUAL(odds, 0.5f);
	BOOST_CHECK_EQUAL(winnerAfter, 1012.5f);
	BOOST_CHECK_EQUAL(loserAfter, 987.5f);

	G_CalculateDuelElo(800.0f, 1200.0f, 25.0f, 25.0f, &winnerAfter, &loserAfter);
	BOOST_CHECK_CLOSE(winnerAfter, 822.7273f, 0.001f);
	BOOST_CHECK_CLOSE(loserAfter, 1177.2727f, 0.001f);
}

BOOST_AUTO_TEST_CASE( high_ratings_do_not_overflow_expected_scores )
{
	float winnerAfter, loserAfter;
	const float odds = G_CalculateDuelElo(20000.0f, 20000.0f, 25.0f, 25.0f, &winnerAfter, &loserAfter);
	BOOST_CHECK_EQUAL(odds, 0.5f);
	BOOST_CHECK_EQUAL(winnerAfter, 20012.5f);
	BOOST_CHECK_EQUAL(loserAfter, 19987.5f);
}

BOOST_AUTO_TEST_SUITE_END()
