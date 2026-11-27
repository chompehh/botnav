#ifndef G_DUEL_ELO_LIMITS_H
#define G_DUEL_ELO_LIMITS_H

#include "sqlite3.h"
#include <stdio.h>

/* Count only rated bot duels in the UTC day; -1 means the query failed. */
static inline int G_DuelRatedBotDuelsToday(sqlite3 *db, const char *identity,
	const char *legacyIdentityPattern, sqlite3_int64 timestamp)
{
	static const char sql[] =
		"SELECT COUNT(*) FROM LocalDuel WHERE end_time >= ?1 AND end_time < ?2 "
		"AND winner_elo > -998 AND loser_elo > -998 AND "
		"(((winner = ?3 OR winner LIKE ?4) AND (loser GLOB 'botlvl[1-9]*' OR loser LIKE '% [bot L%]')) OR "
		"((loser = ?3 OR loser LIKE ?4) AND (winner GLOB 'botlvl[1-9]*' OR winner LIKE '% [bot L%]')))";
	sqlite3_stmt *stmt = NULL;
	sqlite3_int64 start;
	int result, count = -1;

	if (!db || !identity || !identity[0])
		return -1;
	start = timestamp - timestamp % 86400;
	result = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (result == SQLITE_OK)
		result = sqlite3_bind_int64(stmt, 1, start);
	if (result == SQLITE_OK)
		result = sqlite3_bind_int64(stmt, 2, start + 86400);
	if (result == SQLITE_OK)
		result = sqlite3_bind_text(stmt, 3, identity, -1, SQLITE_TRANSIENT);
	if (result == SQLITE_OK)
		result = legacyIdentityPattern ?
			sqlite3_bind_text(stmt, 4, legacyIdentityPattern, -1, SQLITE_TRANSIENT) :
			sqlite3_bind_null(stmt, 4);
	if (result == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW)
		count = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return count;
}

/* Returns 1 if rated, 0 if capped, -1 if counting failed. */
static inline int G_DuelBotRatingAllowed(sqlite3 *db, const char *winner,
	const char *loser, int winnerLevel, int loserLevel, int humanLimit,
	int botLimit, sqlite3_int64 timestamp)
{
	int count, limit;
	char pattern[32];

	if (!winnerLevel && !loserLevel)
		return 1;
	limit = winnerLevel && loserLevel ? botLimit : humanLimit;
	if (limit <= 0)
		return 1;
	if (!winnerLevel || !loserLevel)
	{
		count = G_DuelRatedBotDuelsToday(db, winnerLevel ? loser : winner, NULL, timestamp);
		return count < 0 ? -1 : count < limit;
	}
	snprintf(pattern, sizeof(pattern), "%% [bot L%i]", winnerLevel);
	count = G_DuelRatedBotDuelsToday(db, winner, pattern, timestamp);
	if (count < 0)
		return -1;
	if (count >= limit)
		return 0;
	if (winnerLevel == loserLevel)
		return 1;
	snprintf(pattern, sizeof(pattern), "%% [bot L%i]", loserLevel);
	count = G_DuelRatedBotDuelsToday(db, loser, pattern, timestamp);
	return count < 0 ? -1 : count < limit;
}

#endif
