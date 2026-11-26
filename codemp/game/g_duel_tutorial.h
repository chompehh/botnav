#ifndef G_DUEL_TUTORIAL_H
#define G_DUEL_TUTORIAL_H

#include <stddef.h>
#include "sqlite3.h"
#include "g_duel_tutorial_sequences.h"

typedef struct
{
	int duels;
	int modeDuels;
	int recoveryDuels;
	int rotation;
} duel_tutorial_history_t;

typedef struct
{
	int forceDuels, forceWins, forceLosses;
	int saberDuels, saberReminderSent;
	int flipkick;
	int opponent;
	int pendingForceLoss, welcomeComplete, nextWelcomeCheck;
} duel_tutorial_session_t;

static inline int G_DuelTutorialPreferBasics(const duel_tutorial_session_t *session,
	int push, int pull, int lightning, int mindtrick)
{
	int signals = (push < 3) + (pull < 3) + (lightning >= 3) +
		(mindtrick >= 2) + !session->flipkick;
	if (session->forceLosses > session->forceWins)
		return 1;
	if (session->forceWins > session->forceLosses)
		return signals >= 2;
	return signals > 0;
}

static inline int G_DuelTutorialWelcomeEligible(const duel_tutorial_session_t *session,
	int push, int pull, int lightning, int mindtrick, int introCursor)
{
	return !session->welcomeComplete && introCursor < 2 &&
		G_DuelTutorialPreferBasics(session, push, pull, lightning, mindtrick);
}

static inline int G_DuelTutorialSessionFinish(duel_tutorial_session_t *session,
	int duelType, int won, int draw)
{
	if (duelType == 1)
	{
		session->forceDuels++;
		if (!draw)
		{
			if (won)
				session->forceWins++;
			else
			{
				session->forceLosses++;
				session->pendingForceLoss = 1;
			}
		}
	}
	else if (duelType == 0)
	{
		session->saberDuels++;
		return session->saberDuels >= 10 && !session->saberReminderSent;
	}
	return 0;
}

static inline int G_DuelTutorialMessageDelay(int baseMs, size_t previousLength, size_t nextLength)
{
	size_t length = previousLength > nextLength ? previousLength : nextLength;
	if (length > 149)
		length = 149;
	return baseMs + (int)length * 30;
}

static inline int G_DuelTutorialInformative(int events, int duration, int complete)
{
	return complete && events >= 10 && duration >= 12000;
}

static inline int G_DuelTutorialCanTeach(int informative, int duels)
{
	return informative && duels >= 3;
}

static inline int G_DuelTutorialCanDescribePattern(int informative, int duels, int comparableDuels)
{
	return G_DuelTutorialCanTeach(informative, duels) && comparableDuels >= 2;
}

enum {
	DUEL_TUTORIAL_LOW_FORCE, DUEL_TUTORIAL_DRAIN, DUEL_TUTORIAL_KNOCKDOWN,
	DUEL_TUTORIAL_STRAIGHT, DUEL_TUTORIAL_RECOVERY, DUEL_TUTORIAL_LOW_HEALTH_ATTACK,
	DUEL_TUTORIAL_PATTERN_COUNT
};

enum { DUEL_TUTORIAL_EVENT_SPEND, DUEL_TUTORIAL_EVENT_KNOCKDOWN, DUEL_TUTORIAL_EVENT_HIT,
	DUEL_TUTORIAL_EVENT_ATTACK };

static inline int G_DuelTutorialPatternAllowedInMode(int pattern, int fullForce)
{
	return !fullForce || (pattern != DUEL_TUTORIAL_STRAIGHT &&
		pattern != DUEL_TUTORIAL_RECOVERY);
}

typedef struct
{
	int type, time, power, confirmed, force, straight, recovery;
	int selfHealth, enemyHealth, healAvailable;
} duel_tutorial_event_t;

typedef struct
{
	int count[DUEL_TUTORIAL_PATTERN_COUNT];
	int lastSpend, lastPower, lastDrain, lastKnockdown, lastHit, lastLowHealthAttack;
	int samplePower, sampleForce, sampleTime[DUEL_TUTORIAL_PATTERN_COUNT];
	int sampleSelfHealth, sampleEnemyHealth;
} duel_tutorial_observation_t;

typedef struct
{
	int count[DUEL_TUTORIAL_PATTERN_COUNT];
	int duels[DUEL_TUTORIAL_PATTERN_COUNT];
	int lastRotation[DUEL_TUTORIAL_PATTERN_COUNT];
} duel_tutorial_patterns_t;

static inline void G_DuelTutorialObserveInit(duel_tutorial_observation_t *o)
{
	int i;
	for (i = 0; i < DUEL_TUTORIAL_PATTERN_COUNT; i++)
	{
		o->count[i] = 0;
		o->sampleTime[i] = -1;
	}
	o->lastSpend = o->lastDrain = o->lastKnockdown = o->lastHit = -1;
	o->lastLowHealthAttack = -1;
	o->lastPower = o->samplePower = -1;
	o->sampleForce = -1;
	o->sampleSelfHealth = o->sampleEnemyHealth = -1;
}

static inline void G_DuelTutorialObserve(duel_tutorial_observation_t *o,
	const duel_tutorial_event_t *event, int drainPower)
{
	int delta;
	if (event->type == DUEL_TUTORIAL_EVENT_ATTACK && event->confirmed &&
		event->healAvailable && event->selfHealth > 0 && event->selfHealth <= 60 &&
		event->enemyHealth >= event->selfHealth + 20 &&
		(o->lastLowHealthAttack < 0 || event->time - o->lastLowHealthAttack >= 1000))
	{
		o->lastLowHealthAttack = event->time;
		o->count[DUEL_TUTORIAL_LOW_HEALTH_ATTACK]++;
		o->sampleSelfHealth = event->selfHealth;
		o->sampleEnemyHealth = event->enemyHealth;
		o->sampleTime[DUEL_TUTORIAL_LOW_HEALTH_ATTACK] = event->time;
	}
	else if (event->type == DUEL_TUTORIAL_EVENT_SPEND && event->confirmed)
	{
		o->lastSpend = event->time;
		o->lastPower = event->power;
		if (event->power == drainPower)
			o->lastDrain = event->time;
	}
	else if (event->type == DUEL_TUTORIAL_EVENT_KNOCKDOWN && event->confirmed)
		o->lastKnockdown = event->time;
	else if (event->type == DUEL_TUTORIAL_EVENT_HIT && event->confirmed)
	{
		if (o->lastHit >= 0 && event->time - o->lastHit < 1000)
			return;
		o->lastHit = event->time;
		delta = event->time - o->lastSpend;
		if (o->lastSpend >= 0 && delta >= 0 && delta <= 3000 && event->force >= 0 && event->force <= 25)
		{
			o->count[DUEL_TUTORIAL_LOW_FORCE]++;
			o->samplePower = o->lastPower;
			o->sampleForce = event->force;
			o->sampleTime[DUEL_TUTORIAL_LOW_FORCE] = event->time;
		}
		delta = event->time - o->lastDrain;
		if (o->lastDrain >= 0 && delta >= 0 && delta <= 3000)
		{
			o->count[DUEL_TUTORIAL_DRAIN]++;
			o->sampleTime[DUEL_TUTORIAL_DRAIN] = event->time;
		}
		delta = event->time - o->lastKnockdown;
		if (o->lastKnockdown >= 0 && delta >= 0 && delta <= 3000)
		{
			o->count[DUEL_TUTORIAL_KNOCKDOWN]++;
			o->sampleTime[DUEL_TUTORIAL_KNOCKDOWN] = event->time;
		}
		if (event->straight)
		{
			o->count[DUEL_TUTORIAL_STRAIGHT]++;
			o->sampleTime[DUEL_TUTORIAL_STRAIGHT] = event->time;
		}
		if (event->recovery)
		{
			o->count[DUEL_TUTORIAL_RECOVERY]++;
			o->sampleTime[DUEL_TUTORIAL_RECOVERY] = event->time;
		}
		o->lastSpend = o->lastDrain = o->lastKnockdown = -1;
	}
}

static inline int G_DuelTutorialSelectPattern(int duels, int rotation,
	const duel_tutorial_observation_t *o, const duel_tutorial_patterns_t *p)
{
	int i;
	i = DUEL_TUTORIAL_LOW_HEALTH_ATTACK;
	if (o->count[i] && G_DuelTutorialCanDescribePattern(1, duels, p->duels[i]) &&
		rotation - p->lastRotation[i] >= 4)
		return i;
	for (i = 0; i < DUEL_TUTORIAL_PATTERN_COUNT; i++)
		if (o->count[i] && G_DuelTutorialCanDescribePattern(1, duels, p->duels[i]) &&
			rotation - p->lastRotation[i] >= 4)
			return i;
	return -1;
}

/* Aggregate each observation once per finished duel, never once per event. */
static inline int G_DuelTutorialRecordPatterns(sqlite3 *db, const char *identity, int kind,
	int mode, const duel_tutorial_observation_t *o, duel_tutorial_patterns_t *p)
{
	sqlite3_stmt *stmt = NULL;
	int rc, i;
	rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS LocalBotTutorialPatternV1("
		"identity TEXT NOT NULL,kind INTEGER NOT NULL,mode INTEGER NOT NULL,pattern INTEGER NOT NULL,"
		"occurrences INTEGER NOT NULL DEFAULT 0,duels INTEGER NOT NULL DEFAULT 0,"
		"sample_power INTEGER NOT NULL DEFAULT -1,sample_force INTEGER NOT NULL DEFAULT -1,sample_time INTEGER NOT NULL DEFAULT -1,"
		"last_rotation INTEGER NOT NULL DEFAULT -1000,PRIMARY KEY(identity,kind,mode,pattern))",
		NULL, NULL, NULL);
	if (rc != SQLITE_OK)
		return rc;
	for (i = 0; i < DUEL_TUTORIAL_PATTERN_COUNT; i++)
	{
		p->count[i] = p->duels[i] = 0;
		p->lastRotation[i] = -1000;
	}
	rc = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
	if (rc != SQLITE_OK)
		return rc;
	for (i = 0; i < DUEL_TUTORIAL_PATTERN_COUNT && rc == SQLITE_OK; i++)
	{
		if (!o->count[i])
			continue;
		rc = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO LocalBotTutorialPatternV1(identity,kind,mode,pattern) VALUES(?,?,?,?)", -1, &stmt, NULL);
		if (rc == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, identity, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 2, kind);
			sqlite3_bind_int(stmt, 3, mode);
			sqlite3_bind_int(stmt, 4, i);
			rc = sqlite3_step(stmt) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(db);
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
		if (rc == SQLITE_OK)
			rc = sqlite3_prepare_v2(db, "UPDATE LocalBotTutorialPatternV1 SET occurrences=occurrences+?,duels=duels+1,"
				"sample_power=?,sample_force=?,sample_time=? WHERE identity=? AND kind=? AND mode=? AND pattern=?", -1, &stmt, NULL);
		if (rc == SQLITE_OK)
		{
			sqlite3_bind_int(stmt, 1, o->count[i]);
			sqlite3_bind_int(stmt, 2, i == DUEL_TUTORIAL_LOW_FORCE ? o->samplePower : -1);
			sqlite3_bind_int(stmt, 3, i == DUEL_TUTORIAL_LOW_FORCE ? o->sampleForce : -1);
			sqlite3_bind_int(stmt, 4, o->sampleTime[i]);
			sqlite3_bind_text(stmt, 5, identity, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 6, kind);
			sqlite3_bind_int(stmt, 7, mode);
			sqlite3_bind_int(stmt, 8, i);
			rc = sqlite3_step(stmt) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(db);
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if (rc == SQLITE_OK)
		rc = sqlite3_prepare_v2(db, "SELECT pattern,occurrences,duels,last_rotation FROM LocalBotTutorialPatternV1 WHERE identity=? AND kind=? AND mode=?", -1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, identity, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, kind);
		sqlite3_bind_int(stmt, 3, mode);
		while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
		{
			i = sqlite3_column_int(stmt, 0);
			if (i >= 0 && i < DUEL_TUTORIAL_PATTERN_COUNT)
			{
				p->count[i] = sqlite3_column_int(stmt, 1);
				p->duels[i] = sqlite3_column_int(stmt, 2);
				p->lastRotation[i] = sqlite3_column_int(stmt, 3);
			}
		}
		if (rc == SQLITE_DONE)
			rc = SQLITE_OK;
	}
	sqlite3_finalize(stmt);
	if (rc == SQLITE_OK)
		rc = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	if (rc != SQLITE_OK)
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
	return rc;
}

static inline int G_DuelTutorialMarkPattern(sqlite3 *db, const char *identity, int kind, int mode, int pattern, int rotation)
{
	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare_v2(db, "UPDATE LocalBotTutorialPatternV1 SET last_rotation=? WHERE identity=? AND kind=? AND mode=? AND pattern=?", -1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_int(stmt, 1, rotation);
		sqlite3_bind_text(stmt, 2, identity, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 3, kind);
		sqlite3_bind_int(stmt, 4, mode);
		sqlite3_bind_int(stmt, 5, pattern);
		rc = sqlite3_step(stmt) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(db);
	}
	sqlite3_finalize(stmt);
	return rc;
}

static inline int G_DuelTutorialIntroCursor(sqlite3 *db, const char *identity, int kind, int advance, int *cursor)
{
	sqlite3_stmt *stmt = NULL;
	int rc;
	rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS LocalBotTutorialIntroV1("
		"identity TEXT NOT NULL,kind INTEGER NOT NULL,cursor INTEGER NOT NULL DEFAULT 0,"
		"PRIMARY KEY(identity,kind))", NULL, NULL, NULL);
	if (rc != SQLITE_OK)
		return rc;
	if (advance)
	{
		rc = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO LocalBotTutorialIntroV1(identity,kind) VALUES(?,?)", -1, &stmt, NULL);
		if (rc == SQLITE_OK) rc = sqlite3_bind_text(stmt, 1, identity, -1, SQLITE_TRANSIENT);
		if (rc == SQLITE_OK) rc = sqlite3_bind_int(stmt, 2, kind);
		if (rc == SQLITE_OK)
		{
			rc = sqlite3_step(stmt);
			if (rc == SQLITE_DONE) rc = SQLITE_OK;
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
		if (rc != SQLITE_OK)
			return rc;
	}
	rc = sqlite3_prepare_v2(db, advance ?
		"UPDATE LocalBotTutorialIntroV1 SET cursor=MAX(cursor,?) WHERE identity=? AND kind=?" :
		"SELECT cursor FROM LocalBotTutorialIntroV1 WHERE identity=? AND kind=?", -1, &stmt, NULL);
	if (advance && rc == SQLITE_OK) rc = sqlite3_bind_int(stmt, 1, advance);
	if (rc == SQLITE_OK) rc = sqlite3_bind_text(stmt, advance ? 2 : 1, identity, -1, SQLITE_TRANSIENT);
	if (rc == SQLITE_OK) rc = sqlite3_bind_int(stmt, advance ? 3 : 2, kind);
	if (rc == SQLITE_OK)
	{
		rc = sqlite3_step(stmt);
		if (!advance && rc == SQLITE_ROW)
		{
			*cursor = sqlite3_column_int(stmt, 0);
			rc = SQLITE_OK;
		}
		else if (rc == SQLITE_DONE)
		{
			if (!advance) *cursor = 0;
			rc = SQLITE_OK;
		}
	}
	sqlite3_finalize(stmt);
	return rc;
}

static inline int G_DuelTutorialBasicCursor(sqlite3 *db, const char *identity, int kind, int advance, int *cursor)
{
	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS LocalBotTutorialBasicV1("
		"identity TEXT NOT NULL,kind INTEGER NOT NULL,cursor INTEGER NOT NULL DEFAULT 0,"
		"PRIMARY KEY(identity,kind))", NULL, NULL, NULL);
	if (rc != SQLITE_OK)
		return rc;
	rc = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO LocalBotTutorialBasicV1(identity,kind) VALUES(?,?)", -1, &stmt, NULL);
	if (rc == SQLITE_OK) rc = sqlite3_bind_text(stmt, 1, identity, -1, SQLITE_TRANSIENT);
	if (rc == SQLITE_OK) rc = sqlite3_bind_int(stmt, 2, kind);
	if (rc == SQLITE_OK) rc = sqlite3_step(stmt) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(db);
	sqlite3_finalize(stmt);
	stmt = NULL;
	if (rc != SQLITE_OK)
		return rc;
	rc = sqlite3_prepare_v2(db, advance ?
		"UPDATE LocalBotTutorialBasicV1 SET cursor=MAX(cursor,?) WHERE identity=? AND kind=?" :
		"SELECT cursor FROM LocalBotTutorialBasicV1 WHERE identity=? AND kind=?", -1, &stmt, NULL);
	if (advance && rc == SQLITE_OK) rc = sqlite3_bind_int(stmt, 1, advance);
	if (rc == SQLITE_OK) rc = sqlite3_bind_text(stmt, advance ? 2 : 1, identity, -1, SQLITE_TRANSIENT);
	if (rc == SQLITE_OK) rc = sqlite3_bind_int(stmt, advance ? 3 : 2, kind);
	if (rc == SQLITE_OK)
	{
		rc = sqlite3_step(stmt);
		if (!advance && rc == SQLITE_ROW)
		{
			*cursor = sqlite3_column_int(stmt, 0);
			rc = SQLITE_OK;
		}
		else if (advance && rc == SQLITE_DONE)
			rc = SQLITE_OK;
	}
	sqlite3_finalize(stmt);
	return rc;
}

/* Import only a new identity's proven-complete versioned raw captures. The
 * caller invokes this once, immediately after its first V1 record. */
static inline int G_DuelTutorialImportComplete(sqlite3 *db, const char *identity, int kind,
	int mode, duel_tutorial_history_t *history)
{
	const char *sql = "SELECT type,COUNT(*) FROM (SELECT DISTINCT s.id,s.type FROM LocalDuelTrackParticipant p "
		"JOIN LocalDuelTrackSummary s ON s.id=p.summary_id WHERE p.participant_key=? AND p.participant_kind=? "
		"AND p.id>COALESCE((SELECT MAX(id)-20000 FROM LocalDuelTrackParticipant),0) "
		"AND s.source_context='duel' AND s.result IN ('duel_complete','draw') AND s.type IN (0,1) AND s.capture_version>=14 "
		"AND p.event_total>=10 AND p.event_retained=p.event_total AND p.event_dropped=0 "
		"AND p.event_critical_dropped=0 AND p.event_allocation_failures=0 "
		"AND (SELECT COUNT(*) FROM LocalDuelTrackEvent e WHERE e.summary_id=s.id AND "
		"e.participant_key=p.participant_key AND e.event_type<>'chat')=p.event_retained "
		"AND EXISTS(SELECT 1 FROM LocalDuelTrackEvent e WHERE e.summary_id=s.id AND "
		"e.participant_key=p.participant_key AND e.event_type<>'chat' AND e.rel_time>=12000) "
		"ORDER BY s.id DESC LIMIT 100) GROUP BY type";
	sqlite3_stmt *stmt = NULL;
	int rc, counts[2] = {0, 0}, i;
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, identity, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, kind);
		while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
		{
			i = sqlite3_column_int(stmt, 0);
			if (i >= 0 && i < 2)
				counts[i] = sqlite3_column_int(stmt, 1);
		}
		if (rc == SQLITE_DONE)
			rc = SQLITE_OK;
	}
	sqlite3_finalize(stmt);
	stmt = NULL;
	if (rc != SQLITE_OK)
		return rc;
	rc = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
	for (i = 0; i < 2 && rc == SQLITE_OK; i++)
	{
		if (!counts[i])
			continue;
		rc = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO LocalBotTutorialV1(identity,kind,mode) VALUES(?,?,?)", -1, &stmt, NULL);
		if (rc == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, identity, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 2, kind);
			sqlite3_bind_int(stmt, 3, i);
			rc = sqlite3_step(stmt) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(db);
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
		if (rc == SQLITE_OK)
			rc = sqlite3_prepare_v2(db, "UPDATE LocalBotTutorialV1 SET duels=duels+? WHERE identity=? AND kind=? AND mode=?", -1, &stmt, NULL);
		if (rc == SQLITE_OK)
		{
			sqlite3_bind_int(stmt, 1, counts[i]);
			sqlite3_bind_text(stmt, 2, identity, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 3, kind);
			sqlite3_bind_int(stmt, 4, i);
			rc = sqlite3_step(stmt) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(db);
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if (rc == SQLITE_OK)
		rc = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	if (rc != SQLITE_OK)
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
	else
	{
		history->duels += counts[0] + counts[1];
		if (mode >= 0 && mode < 2)
			history->modeDuels += counts[mode];
	}
	return rc;
}

static inline int G_DuelTutorialAdvanceRotation(sqlite3 *db, const char *identity, int kind, int mode)
{
	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare_v2(db, "UPDATE LocalBotTutorialV1 SET rotation=rotation+1 WHERE identity=? AND kind=? AND mode=?", -1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, identity, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, kind);
		sqlite3_bind_int(stmt, 3, mode);
		rc = sqlite3_step(stmt) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(db);
	}
	sqlite3_finalize(stmt);
	return rc;
}

/* This table is deliberately independent of raw capture and its asynchronous saves.
 * Old summaries do not establish completeness and are not imported as evidence. */
static inline int G_DuelTutorialRecord(sqlite3 *db, const char *identity, int kind,
	int mode, int recovery, int rotate, duel_tutorial_history_t *history)
{
	sqlite3_stmt *stmt = NULL;
	int rc;
	if (!db || !identity || !identity[0] || !history)
		return SQLITE_MISUSE;
	rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS LocalBotTutorialV1("
		"identity TEXT NOT NULL, kind INTEGER NOT NULL, mode INTEGER NOT NULL,"
		"duels INTEGER NOT NULL DEFAULT 0, recovery_duels INTEGER NOT NULL DEFAULT 0,"
		"rotation INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(identity,kind,mode))",
		NULL, NULL, NULL);
	if (rc != SQLITE_OK)
		return rc;
	rc = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
	if (rc != SQLITE_OK)
		return rc;
	rc = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO LocalBotTutorialV1(identity,kind,mode) VALUES(?,?,?)", -1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, identity, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, kind);
		sqlite3_bind_int(stmt, 3, mode);
		rc = sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	stmt = NULL;
	if (rc == SQLITE_DONE)
		rc = sqlite3_prepare_v2(db, "UPDATE LocalBotTutorialV1 SET duels=duels+1,"
			"recovery_duels=recovery_duels+?, rotation=rotation+CASE WHEN ? AND "
			"(CASE WHEN mode=2 THEN duels ELSE "
			"(SELECT SUM(duels) FROM LocalBotTutorialV1 WHERE identity=? AND kind=?) END)>=2 "
			"THEN 1 ELSE 0 END WHERE identity=? AND kind=? AND mode=?",
			-1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_int(stmt, 1, recovery ? 1 : 0);
		sqlite3_bind_int(stmt, 2, rotate ? 1 : 0);
		sqlite3_bind_text(stmt, 3, identity, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 4, kind);
		sqlite3_bind_text(stmt, 5, identity, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 6, kind);
		sqlite3_bind_int(stmt, 7, mode);
		rc = sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	stmt = NULL;
	if (rc == SQLITE_DONE)
		rc = sqlite3_prepare_v2(db, "SELECT (SELECT SUM(duels) FROM LocalBotTutorialV1 "
			"WHERE identity=? AND kind=?), duels, recovery_duels, rotation FROM LocalBotTutorialV1 "
			"WHERE identity=? AND kind=? AND mode=?", -1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, identity, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, kind);
		sqlite3_bind_text(stmt, 3, identity, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 4, kind);
		sqlite3_bind_int(stmt, 5, mode);
		rc = sqlite3_step(stmt);
		if (rc == SQLITE_ROW)
		{
			history->duels = sqlite3_column_int(stmt, 0);
			history->modeDuels = sqlite3_column_int(stmt, 1);
			history->recoveryDuels = sqlite3_column_int(stmt, 2);
			history->rotation = sqlite3_column_int(stmt, 3);
			rc = SQLITE_OK;
		}
	}
	sqlite3_finalize(stmt);
	if (rc == SQLITE_OK)
		rc = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	if (rc != SQLITE_OK)
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
	return rc;
}

#endif
