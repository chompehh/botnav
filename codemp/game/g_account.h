#ifndef G_ACCOUNT_H
#define G_ACCOUNT_H

#include "sqlite3.h"
#include "g_duel_command_capture.h"
#include <stdlib.h>
#include <string.h>

static inline void G_ReleaseTrackedPersistSnapshots(void *job, size_t jobSize,
	void *winnerEvents, void *loserEvents)
{
	free(winnerEvents);
	free(loserEvents);
	memset(job, 0, jobSize);
}

static inline int G_TrackedPersistQueueSlot(int head, int count, int capacity)
{
	if (count < 0 || count >= capacity || head < 0 || head >= capacity)
		return -1;
	return (head + count) % capacity;
}

/* A blocked step must yield immediately, never spin or release the queued job. */
static inline int G_DrainTrackedPersistJob(void *job, int (*step)(void *),
	int *stage, int *cursor, sqlite3_int64 *summaryId, int maxSteps,
	int (*milliseconds)(void), int budgetMs)
{
	const int start = milliseconds ? milliseconds() : 0;
	int i;
	for (i = 0; i < maxSteps; i++)
	{
		const int savedStage = *stage;
		const int savedCursor = *cursor;
		const sqlite3_int64 savedSummaryId = *summaryId;
		if (milliseconds && milliseconds() - start >= budgetMs)
			return 0;
		if (!step(job))
			return 1;
		if (*stage == savedStage && *cursor == savedCursor && *summaryId == savedSummaryId)
			return 0;
	}
	return 0;
}

static inline int G_TrackedPersistRetryable(int status)
{
	const int primary = status & 0xff;
	return primary == SQLITE_BUSY || primary == SQLITE_LOCKED;
}

/* Keep the highest-ranked selected routes when a caller requires a hard cap. */
static inline int G_StrafeTrackResetApplyHardBudget(const int *rankedIndices,
	int candidateCount, const unsigned char *selected, int budget,
	unsigned char *bounded)
{
	int i, kept = 0;

	if (candidateCount < 0 || budget < 0 ||
		(candidateCount > 0 && (!rankedIndices || !selected || !bounded)))
		return -1;
	if (candidateCount > 0)
		memset(bounded, 0, (size_t)candidateCount);
	for (i = 0; i < candidateCount && kept < budget; i++)
	{
		const int index = rankedIndices[i];
		if (index < 0 || index >= candidateCount)
			return -1;
		if (selected[index])
		{
			bounded[index] = 1;
			kept++;
		}
	}
	return kept;
}

static inline int G_BeginTrackedPersistTransaction(sqlite3 *db)
{
	if (!sqlite3_get_autocommit(db))
		return SQLITE_BUSY;
	return sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
}

static inline int G_FinishTrackedPersistTransaction(sqlite3 *db, int status,
	int *stage, int *cursor, sqlite3_int64 *summaryId,
	int savedStage, int savedCursor, sqlite3_int64 savedSummaryId)
{
	if (status == SQLITE_OK)
		status = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	if (status != SQLITE_OK)
	{
		const int rollback = sqlite3_get_autocommit(db) ? SQLITE_OK :
			sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		*stage = savedStage;
		*cursor = savedCursor;
		*summaryId = savedSummaryId;
		if (rollback != SQLITE_OK)
			return SQLITE_ERROR;
	}
	return status;
}

static const char *const g_trackedDuelTableNames[] = {
	"LocalDuelTrackSummary",
	"LocalDuelTrackParticipant",
	"LocalDuelTrackEvent",
	"LocalDuelTrackGeometry",
	"LocalDuelTrackAggregate"
};

static const char g_botLearnMotorSchema[] =
	"CREATE TABLE IF NOT EXISTS LocalBotLearnedMotor("
	"session_id TEXT NOT NULL, duel_start INTEGER NOT NULL, summary_id INTEGER, "
	"participant_key TEXT NOT NULL, span_id INTEGER NOT NULL, kind INTEGER NOT NULL, "
	"ctx_key INTEGER, stance INTEGER, throw_level INTEGER, hold_ms INTEGER, "
	"yaw_rate REAL, pitch_rate REAL, net_damage INTEGER, capture_version INTEGER, "
	"PRIMARY KEY(session_id,duel_start,participant_key,span_id,kind))";

/* ROLLBACK resets sqlite3_errmsg() to "not an error"; keep the real cause for the caller. */
static char g_learnedMotorLastError[256];

static inline const char *G_ExtractLearnedMotorError(void)
{
	return g_learnedMotorLastError;
}

static inline int G_ExtractLearnedMotor(sqlite3 *db, int limit,
	int attackMask, int altAttackMask, int saberDamageMod, int *loaded)
{
	const char *sql =
		"SELECT e.summary_id,e.participant_key,e.event_type,e.command_time,e.command_msec,"
		"e.buttons,e.command_yaw,e.command_pitch,e.span_id,e.span_kind,e.learned_context,"
		"e.saber_stance,e.amount,e.damage_source,e.damage_attacker_key,e.opponent_key,e.span_outcome,"
		"e.session_id,e.duel_start_time "
		"FROM LocalDuelTrackEvent e JOIN LocalDuelTrackParticipant p "
		"ON p.summary_id=e.summary_id AND p.participant_key=e.participant_key "
		"WHERE p.participant_kind IN (1,2) AND p.event_dropped=0 AND p.motor_extracted=0 "
		"AND e.event_capture_version>=14 "
		"AND e.summary_id IN (SELECT id FROM LocalDuelTrackSummary "
		"WHERE capture_version>=14 AND EXISTS (SELECT 1 FROM LocalDuelTrackParticipant pending "
		"WHERE pending.summary_id=LocalDuelTrackSummary.id AND pending.participant_kind IN (1,2) "
		"AND pending.event_dropped=0 AND pending.motor_extracted=0) ORDER BY id DESC LIMIT ?1) "
		"AND e.event_type IN ('command_sample','span_end','damage_dealt','damage') "
		"AND (e.event_type<>'command_sample' OR e.command_source=1) "
		"ORDER BY e.summary_id,e.participant_key,e.event_index";
	const char *insertSql = "INSERT OR IGNORE INTO LocalBotLearnedMotor("
		"session_id,duel_start,summary_id,participant_key,span_id,kind,ctx_key,stance,"
		"throw_level,hold_ms,yaw_rate,pitch_rate,net_damage,capture_version) "
		"VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,14)";
	sqlite3_stmt *stmt = NULL, *insert = NULL;
	sqlite3_int64 summary = -1;
	char participant[64] = "";
	botlearn_command_sample_t previous;
	botlearn_motor_hold_t holds[2];
	int rc, seen = 0;

	*loaded = 0;
	g_learnedMotorLastError[0] = '\0';
	if (!sqlite3_get_autocommit(db))
	{
		sqlite3_snprintf(sizeof(g_learnedMotorLastError), g_learnedMotorLastError,
			"a transaction is already open");
		return SQLITE_BUSY;
	}
	memset(holds, 0, sizeof(holds));
	memset(&previous, 0, sizeof(previous));
	rc = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
	if (rc != SQLITE_OK)
	{
		sqlite3_snprintf(sizeof(g_learnedMotorLastError), g_learnedMotorLastError, "%s", sqlite3_errmsg(db));
		return rc;
	}
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc == SQLITE_OK)
		rc = sqlite3_prepare_v2(db, insertSql, -1, &insert, NULL);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_int(stmt, 1, limit);
		while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
		{
			const sqlite3_int64 nextSummary = sqlite3_column_int64(stmt, 0);
			const char *key = (const char *)sqlite3_column_text(stmt, 1);
			const char *type = (const char *)sqlite3_column_text(stmt, 2);
			const int span = sqlite3_column_int(stmt, 8);
			int k;

			if (!key || !type)
				continue;
			if (summary != nextSummary || strcmp(key, participant))
			{
				summary = nextSummary;
				sqlite3_snprintf(sizeof(participant), participant, "%s", key);
				memset(holds, 0, sizeof(holds));
				memset(&previous, 0, sizeof(previous));
				seen = 0;
			}
			if (!strcmp(type, "command_sample"))
			{
				botlearn_command_sample_t sample;
				sample.time = sqlite3_column_int(stmt, 3);
				sample.msec = sqlite3_column_int(stmt, 4);
				sample.buttons = sqlite3_column_int(stmt, 5);
				sample.yaw = (float)sqlite3_column_double(stmt, 6);
				sample.pitch = (float)sqlite3_column_double(stmt, 7);
				sample.span = span;
				sample.kind = sqlite3_column_int(stmt, 9);
				sample.context = sqlite3_column_int(stmt, 10);
				sample.stance = sqlite3_column_int(stmt, 11);
				sample.throwLevel = sqlite3_column_int(stmt, 12);
				BotLearn_MotorCommand(&holds[0], &sample, seen ? &previous : NULL,
					attackMask, BOTLEARN_SPAN_KIND_SABER);
				BotLearn_MotorCommand(&holds[1], &sample, seen ? &previous : NULL,
					altAttackMask, BOTLEARN_SPAN_KIND_THROW);
				previous = sample;
				seen = 1;
				continue;
			}
			for (k = 0; k < 2; k++)
			{
				botlearn_motor_hold_t *hold = &holds[k];
				const int kind = k ? BOTLEARN_SPAN_KIND_THROW : BOTLEARN_SPAN_KIND_SABER;
				if (!hold->active || !span || hold->span != span)
					continue;
				if (!strcmp(type, "damage_dealt") && sqlite3_column_int(stmt, 13) == saberDamageMod)
					hold->netDamage += sqlite3_column_int(stmt, 12);
				else if (!strcmp(type, "damage"))
				{
					const char *attacker = (const char *)sqlite3_column_text(stmt, 14);
					const char *opponent = (const char *)sqlite3_column_text(stmt, 15);
					if (attacker && opponent && !strcmp(attacker, opponent))
						hold->netDamage -= sqlite3_column_int(stmt, 12);
				}
				else if (!strcmp(type, "span_end"))
				{
					const int duration = k ? BotLearn_ThrowHoldEvidence(hold->start, hold->end,
						hold->commands, hold->eligible, hold->released, BOTLEARN_COMMAND_SOURCE_HUMAN) :
						BotLearn_MotorHoldEvidence(hold->start, hold->end, hold->commands,
							hold->eligible, hold->released, BOTLEARN_COMMAND_SOURCE_HUMAN);
					if (duration && hold->aimValid && hold->aimMsec > 0 &&
						sqlite3_column_int(stmt, 16) != BOTLEARN_SPAN_OUTCOME_UNKNOWN)
					{
						const char *session = (const char *)sqlite3_column_text(stmt, 17);
						if (session && session[0])
						{
							sqlite3_reset(insert);
							sqlite3_bind_text(insert, 1, session, -1, SQLITE_TRANSIENT);
							sqlite3_bind_int(insert, 2, sqlite3_column_int(stmt, 18));
							sqlite3_bind_int64(insert, 3, summary);
							sqlite3_bind_text(insert, 4, participant, -1, SQLITE_TRANSIENT);
							sqlite3_bind_int(insert, 5, span);
							sqlite3_bind_int(insert, 6, kind);
							sqlite3_bind_int(insert, 7, hold->context);
							sqlite3_bind_int(insert, 8, hold->stance);
							sqlite3_bind_int(insert, 9, k ? hold->throwLevel : 0);
							sqlite3_bind_int(insert, 10, duration);
							sqlite3_bind_double(insert, 11, hold->yawSum * 1000.0f / hold->aimMsec);
							sqlite3_bind_double(insert, 12, hold->pitchSum * 1000.0f / hold->aimMsec);
							sqlite3_bind_int(insert, 13, hold->netDamage);
							rc = sqlite3_step(insert);
							if (rc != SQLITE_DONE)
								break;
							*loaded += sqlite3_changes(db);
						}
					}
					memset(hold, 0, sizeof(*hold));
				}
			}
			if (rc != SQLITE_ROW && rc != SQLITE_DONE)
				break;
		}
	}
	sqlite3_finalize(stmt);
	sqlite3_finalize(insert);
	stmt = NULL;
	if (rc == SQLITE_DONE)
		rc = sqlite3_prepare_v2(db, "UPDATE LocalDuelTrackParticipant SET motor_extracted=1 "
			"WHERE participant_kind IN (1,2) AND event_dropped=0 AND summary_id IN "
			"(SELECT id FROM LocalDuelTrackSummary WHERE capture_version>=14 AND EXISTS "
			"(SELECT 1 FROM LocalDuelTrackParticipant pending WHERE pending.summary_id=LocalDuelTrackSummary.id "
			"AND pending.participant_kind IN (1,2) AND pending.event_dropped=0 AND pending.motor_extracted=0) "
			"ORDER BY id DESC LIMIT ?1)", -1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_int(stmt, 1, limit);
		rc = sqlite3_step(stmt);
		if (rc == SQLITE_DONE)
			rc = SQLITE_OK;
	}
	sqlite3_finalize(stmt);
	if (rc == SQLITE_OK)
		rc = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	if (rc != SQLITE_OK)
	{
		sqlite3_snprintf(sizeof(g_learnedMotorLastError), g_learnedMotorLastError, "%s", sqlite3_errmsg(db));
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		*loaded = 0;
	}
	return rc;
}

static inline int G_BackfillLearningEvidenceSessions(sqlite3 *db)
{
	return sqlite3_exec(db,
		"UPDATE LocalBotLearnedEvidence SET session_id = COALESCE("
		"(SELECT NULLIF(e.session_id, '') FROM LocalDuelTrackEvent e "
		"WHERE e.summary_id = LocalBotLearnedEvidence.summary_id "
		"AND e.participant_key = LocalBotLearnedEvidence.participant_key "
		"AND e.session_id IS NOT NULL AND e.session_id <> '' LIMIT 1), "
		"'legacy:' || summary_id) WHERE session_id IS NULL OR session_id = ''",
		NULL, NULL, NULL);
}

static inline int G_AccountTableExists(sqlite3 *db, const char *name)
{
	sqlite3_stmt *stmt = NULL;
	int found = 0;
	if (sqlite3_prepare_v2(db, "SELECT 1 FROM main.sqlite_master WHERE type='table' AND name=?",
		-1, &stmt, NULL) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
		found = sqlite3_step(stmt) == SQLITE_ROW;
	}
	sqlite3_finalize(stmt);
	return found;
}

/* Keep the reset whitelist and its transaction shared with the SQLite regression tests. */
static inline int G_ResetTrackingTables(sqlite3 *db, int learning, int *clearedRows)
{
	static const char *const learnedTables[] = {
		"LocalBotLearnedSequence", "LocalBotLearnedEvidence", "LocalBotLearnedMotor"
	};
	const char *const *tables = learning ? learnedTables : g_trackedDuelTableNames;
	const int count = learning ? (int)(sizeof(learnedTables) / sizeof(learnedTables[0])) :
		(int)(sizeof(g_trackedDuelTableNames) / sizeof(g_trackedDuelTableNames[0]));
	char sql[160];
	int result, i, rows = 0;
	*clearedRows = 0;
	if (!sqlite3_get_autocommit(db))
		return SQLITE_BUSY;
	result = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
	if (result != SQLITE_OK)
		return result;
	/* Preserve provenance before summary/event IDs can be reused by a duel reset. */
	if (!learning)
		result = G_BackfillLearningEvidenceSessions(db);
	else if (G_AccountTableExists(db, "LocalBotLearnedMotor"))
		result = sqlite3_exec(db, "UPDATE LocalDuelTrackParticipant SET motor_extracted=1",
			NULL, NULL, NULL);
	for (i = 0; result == SQLITE_OK && i < count; i++)
	{
		if (learning && i == 2 && !G_AccountTableExists(db, tables[i]))
			continue;
		sqlite3_snprintf(sizeof(sql), sql, "DELETE FROM main.%s", tables[i]);
		result = sqlite3_exec(db, sql, NULL, NULL, NULL);
		if (result == SQLITE_OK)
			rows += sqlite3_changes(db);
	}
	if (result == SQLITE_OK && !learning && G_AccountTableExists(db, "LocalDuelTrackImport"))
		result = sqlite3_exec(db, "DELETE FROM main.LocalDuelTrackImport", NULL, NULL, NULL);
	if (result == SQLITE_OK && G_AccountTableExists(db, "sqlite_sequence"))
	{
		for (i = 0; result == SQLITE_OK && i < count; i++)
		{
			sqlite3_snprintf(sizeof(sql), sql, "DELETE FROM main.sqlite_sequence WHERE name='%s'", tables[i]);
			result = sqlite3_exec(db, sql, NULL, NULL, NULL);
		}
	}
	if (result == SQLITE_OK)
		result = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	if (result != SQLITE_OK)
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
	else
		*clearedRows = rows;
	return result;
}

/*
 * compactdueltrack: drop raw per-frame events and geometry of all but the
 * keepRecent newest duels. Summaries, participants, aggregates and all learned
 * tables stay, so Elo, duel history and bot learning are unchanged.
 */
static inline int G_CompactDuelTrackTables(sqlite3 *db, int keepRecent, int preview,
	int *deletedEvents, int *deletedGeometry)
{
	static const char *const tables[] = { "LocalDuelTrackEvent", "LocalDuelTrackGeometry" };
	int *deleted[2];
	char sql[256];
	int result = SQLITE_OK, i;

	deleted[0] = deletedEvents;
	deleted[1] = deletedGeometry;
	if (!db || !deletedEvents || !deletedGeometry || keepRecent < 0)
		return SQLITE_MISUSE;
	*deletedEvents = *deletedGeometry = 0;
	if (!sqlite3_get_autocommit(db))
		return SQLITE_BUSY;
	if (!preview)
	{
		result = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
		/* Keep learned evidence provenance once its source events are gone. */
		if (result == SQLITE_OK)
			result = G_BackfillLearningEvidenceSessions(db);
	}
	for (i = 0; result == SQLITE_OK && i < 2; i++)
	{
		sqlite3_stmt *stmt = NULL;

		sqlite3_snprintf(sizeof(sql), sql, preview ?
			"SELECT COUNT(*) FROM main.%s WHERE summary_id IS NULL OR summary_id NOT IN "
			"(SELECT id FROM main.LocalDuelTrackSummary ORDER BY id DESC LIMIT %d)" :
			"DELETE FROM main.%s WHERE summary_id IS NULL OR summary_id NOT IN "
			"(SELECT id FROM main.LocalDuelTrackSummary ORDER BY id DESC LIMIT %d)",
			tables[i], keepRecent);
		result = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
		if (result == SQLITE_OK)
		{
			result = sqlite3_step(stmt);
			if (preview && result == SQLITE_ROW)
			{
				*deleted[i] = sqlite3_column_int(stmt, 0);
				result = SQLITE_OK;
			}
			else if (!preview && result == SQLITE_DONE)
			{
				*deleted[i] = sqlite3_changes(db);
				result = SQLITE_OK;
			}
		}
		sqlite3_finalize(stmt);
	}
	if (preview)
		return result;
	if (result == SQLITE_OK)
		result = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	if (result != SQLITE_OK)
	{
		if (!sqlite3_get_autocommit(db))
			sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		*deletedEvents = *deletedGeometry = 0;
	}
	return result;
}

static inline int G_StrafeTrackResetEquivalent(const float *start, const float *end,
	const float *otherStart, const float *otherEnd, int speedGroup, int otherSpeedGroup,
	int forceJump, int otherForceJump, int moveStyle, int otherMoveStyle)
{
	float startDistance = 0.0f, endDistance = 0.0f;
	int i;

	if (speedGroup != otherSpeedGroup || forceJump != otherForceJump ||
		moveStyle != otherMoveStyle)
		return 0;
	for (i = 0; i < 3; i++)
	{
		const float ds = start[i] - otherStart[i];
		const float de = end[i] - otherEnd[i];
		startDistance += ds * ds;
		endDistance += de * de;
	}
	return startDistance <= 192.0f * 192.0f && endDistance <= 192.0f * 192.0f;
}

typedef struct
{
	float start[3], end[3];
	int forceJump, moveStyle, haveEnd, shortForceJump;
	float connectorScore;
	float objectiveLink;	/* > 0: lands near a flag stand (final link toward a flag/capture) */
} strafetrack_export_coverage_t;

/* Newer recordings rank up to 15% higher and the oldest 10% lower, so improved samples replace old ones. */
static inline float G_StrafeTrackRecencyFactor(double created, double oldest, double newest)
{
	if (!(newest > oldest) || created != created)
		return 1.0f;
	if (created <= oldest)
		return 0.9f;
	if (created >= newest)
		return 1.15f;
	return 0.9f + 0.25f * (float)((created - oldest) / (newest - oldest));
}

/* Final links worth reserving: ends near a flag; short links and capture-journey routes rank first. */
static inline float G_StrafeTrackObjectiveLinkScore(float endToFlag, int hops, int objectiveSuccess,
	float recency, float evidence)
{
	if (!(endToFlag >= 0.0f) || endToFlag > 384.0f || hops <= 0 || !(evidence > 0.0f))
		return 0.0f;
	return (hops <= 3 ? 2.0f : 1.0f) * (objectiveSuccess ? 1.5f : 1.0f) *
		(recency > 0.0f ? recency : 1.0f) * evidence * (1.0f + (384.0f - endToFlag) / 384.0f);
}

static inline int G_StrafeTrackExportEquivalent(const strafetrack_export_coverage_t *a,
	const strafetrack_export_coverage_t *b)
{
	int axis;
	float start = 0.0f, end = 0.0f;
	const float radius = a->connectorScore > 0.0f || b->connectorScore > 0.0f ? 64.0f : 192.0f;

	if (!a->haveEnd || !b->haveEnd || a->forceJump != b->forceJump || a->moveStyle != b->moveStyle)
		return 0;
	for (axis = 0; axis < 3; axis++)
	{
		const float ds = a->start[axis] - b->start[axis];
		const float de = a->end[axis] - b->end[axis];
		start += ds * ds;
		end += de * de;
	}
	return start <= radius * radius && end <= radius * radius;
}

static inline int G_StrafeTrackExportBranch(const strafetrack_export_coverage_t *a,
	const strafetrack_export_coverage_t *b)
{
	int axis;
	float start = 0.0f, end = 0.0f;

	if (!a->haveEnd || !b->haveEnd)
		return 0;
	for (axis = 0; axis < 3; axis++)
	{
		const float ds = a->start[axis] - b->start[axis];
		const float de = a->end[axis] - b->end[axis];

		start += ds * ds;
		end += de * de;
	}
	return start <= 192.0f * 192.0f && end > 384.0f * 384.0f;
}

/*
 * Candidates are quality-sorted and in the same speed group. Reserve an eighth
 * for final links onto a flag (best objective links first, distinct to 64u), an eighth
 * for short Force-jump starts, a quarter for all short connectors, then half for
 * start coverage (including those connectors), then distinct endpoints/movement
 * before quality fills.
 */
static inline int G_StrafeTrackSelectExportCoverage(const strafetrack_export_coverage_t *routes,
	int count, int maximum, unsigned char *picked)
{
	int pass, i, j, selected = 0, reserved;

	memset(picked, 0, count);
	while (selected < (maximum + 7) / 8)
	{
		int best = -1;

		for (i = 0; i < count; i++)
		{
			int represented = 0;

			if (picked[i] || !routes[i].haveEnd || !(routes[i].objectiveLink > 0.0f) ||
				(best >= 0 && routes[i].objectiveLink <= routes[best].objectiveLink))
				continue;
			for (j = 0; j < count && !represented; j++)
			{
				float start = 0.0f, end = 0.0f;
				int axis;

				if (!picked[j] || routes[j].forceJump != routes[i].forceJump ||
					routes[j].moveStyle != routes[i].moveStyle)
					continue;
				for (axis = 0; axis < 3; axis++)
				{
					const float ds = routes[i].start[axis] - routes[j].start[axis];
					const float de = routes[i].end[axis] - routes[j].end[axis];
					start += ds * ds;
					end += de * de;
				}
				represented = start <= 64.0f * 64.0f && end <= 64.0f * 64.0f;
			}
			if (!represented)
				best = i;
		}
		if (best < 0)
			break;
		picked[best] = 1;
		selected++;
	}
	reserved = selected;
	for (pass = 0; pass < 2; pass++)
	{
		const int limit = reserved + (pass ? maximum / 4 : (maximum + 7) / 8);
		while (selected < limit && selected < maximum)
		{
			int best = -1;

			for (i = 0; i < count; i++)
			{
				int represented = 0;

				if (picked[i] || !routes[i].haveEnd || routes[i].connectorScore <= 0.0f ||
					(!pass && !routes[i].shortForceJump) ||
					(best >= 0 && routes[i].connectorScore <= routes[best].connectorScore))
					continue;
				for (j = 0; j < count && !represented; j++)
					if (picked[j])
						represented = G_StrafeTrackExportEquivalent(&routes[i], &routes[j]);
				if (!represented)
					best = i;
			}
			if (best < 0)
				break;
			picked[best] = 1;
			selected++;
		}
	}
	for (pass = 0; pass < 3; pass++)
	{
		const int limit = pass ? maximum : reserved + (maximum + 1) / 2;
		/* Preserve different exits from already-covered starts before quality fills. */
		if (pass == 1)
		{
			const int branchLimit = selected + (maximum + 7) / 8;

			for (i = 0; i < count && selected < branchLimit && selected < maximum; i++)
			{
				int branch = 0, represented = 0;

				if (picked[i])
					continue;
				for (j = 0; j < count; j++)
					if (picked[j])
					{
						branch |= G_StrafeTrackExportBranch(&routes[i], &routes[j]);
						represented |= G_StrafeTrackExportEquivalent(&routes[i], &routes[j]);
					}
				if (branch && !represented)
				{
					picked[i] = 1;
					selected++;
				}
			}
		}
		for (i = 0; i < count && selected < limit && selected < maximum; i++)
		{
			int represented = 0;

			if (picked[i])
				continue;
			for (j = 0; pass < 2 && j < count && !represented; j++)
			{
				if (!picked[j])
					continue;
				if (!pass)
				{
					float distance = 0.0f;
					int axis;
					for (axis = 0; axis < 3; axis++)
					{
						const float delta = routes[i].start[axis] - routes[j].start[axis];
						distance += delta * delta;
					}
					represented = distance < 192.0f * 192.0f;
				}
				else
					represented = G_StrafeTrackExportEquivalent(&routes[i], &routes[j]);
			}
			if (!represented)
			{
				picked[i] = 1;
				selected++;
			}
		}
	}
	return selected;
}

static inline int G_StrafeTrackBackfillTakeoffsDB(sqlite3 *db, sqlite3_int64 routeId)
{
	sqlite3_stmt *inputs = NULL, *update = NULL;
	double previous[3], takeoff[3];
	int previousTime = 0, previousGrounded = 0, groundSince = 0, groundMs = -1;
	int result, i, started = 0;
	int havePrevious = 0, haveTakeoff = 0, haveGroundSince = 0;

	if (!sqlite3_get_autocommit(db))
		return SQLITE_BUSY;
	result = sqlite3_prepare_v2(db, "SELECT 1 FROM LocalStrafeRouteNode WHERE route_id=? "
		"AND takeoff_x IS NULL LIMIT 1", -1, &inputs, NULL);
	if (result != SQLITE_OK)
		goto error;
	sqlite3_bind_int64(inputs, 1, routeId);
	result = sqlite3_step(inputs);
	sqlite3_finalize(inputs);
	inputs = NULL;
	if (result == SQLITE_DONE)
		return SQLITE_OK;
	if (result != SQLITE_ROW)
		goto error;
	result = sqlite3_prepare_v2(db, "SELECT time_ms,msec,grounded,x,y,z FROM LocalStrafeRouteInput "
		"WHERE route_id=? ORDER BY frame", -1, &inputs, NULL);
	if (result != SQLITE_OK)
		goto error;
	result = sqlite3_prepare_v2(db, "UPDATE LocalStrafeRouteNode SET takeoff_x=?,takeoff_y=?,takeoff_z=?,"
		"ground_ms=? WHERE route_id=? AND time_ms=? AND takeoff_x IS NULL", -1, &update, NULL);
	if (result != SQLITE_OK)
		goto error;
	result = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
	if (result != SQLITE_OK)
		goto error;
	started = 1;
	sqlite3_bind_int64(inputs, 1, routeId);
	while ((result = sqlite3_step(inputs)) == SQLITE_ROW)
	{
		const int timeMs = sqlite3_column_int(inputs, 0);
		const int msec = sqlite3_column_int(inputs, 1);
		const int grounded = sqlite3_column_int(inputs, 2);

		if (!havePrevious || msec <= 0 || msec > 250 || timeMs - previousTime != msec)
		{
			haveTakeoff = 0;
			haveGroundSince = 0;
		}
		else if (previousGrounded && !grounded)
		{
			for (i = 0; i < 3; i++)
				takeoff[i] = previous[i];
			haveTakeoff = 1;
			groundMs = haveGroundSince ? timeMs - groundSince : -1;
		}
		else if (!previousGrounded && grounded)
		{
			if (haveTakeoff)
			{
				sqlite3_reset(update);
				for (i = 0; i < 3; i++)
					sqlite3_bind_double(update, i + 1, takeoff[i]);
				if (groundMs >= 0)
					sqlite3_bind_int(update, 4, groundMs);
				else
					sqlite3_bind_null(update, 4);
				sqlite3_bind_int64(update, 5, routeId);
				sqlite3_bind_int(update, 6, timeMs);
				result = sqlite3_step(update);
				if (result != SQLITE_DONE)
					goto error;
			}
			groundSince = timeMs;
			haveGroundSince = 1;
			haveTakeoff = 0;
		}
		previousTime = timeMs;
		previousGrounded = grounded;
		for (i = 0; i < 3; i++)
			previous[i] = sqlite3_column_double(inputs, i + 3);
		havePrevious = 1;
	}
	if (result == SQLITE_DONE)
		result = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
error:
	sqlite3_finalize(inputs);
	sqlite3_finalize(update);
	if (result != SQLITE_OK && started && !sqlite3_get_autocommit(db))
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
	return result;
}

static inline int G_StrafeTrackResetCount(sqlite3 *db, const char *table,
	int routeIdColumn, const sqlite3_int64 *keepRouteIds, int keepRouteCount,
	int *count)
{
	sqlite3_stmt *stmt = NULL;
	char sql[160];
	int result, i, j, retained = 0;

	sqlite3_snprintf(sizeof(sql), sql,
		routeIdColumn ? "SELECT COUNT(*) FROM main.%s WHERE route_id = ?" :
		"SELECT COUNT(*) FROM main.%s WHERE id = ?", table);
	result = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (result != SQLITE_OK)
		return result;
	for (i = 0; i < keepRouteCount; i++)
	{
		for (j = 0; j < i; j++)
			if (keepRouteIds[j] == keepRouteIds[i])
				break;
		if (j < i)
			continue;
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		sqlite3_bind_int64(stmt, 1, keepRouteIds[i]);
		result = sqlite3_step(stmt);
		if (result == SQLITE_ROW)
		{
			retained += sqlite3_column_int(stmt, 0);
			result = SQLITE_OK;
		}
		else
			break;
	}
	sqlite3_finalize(stmt);
	if (result != SQLITE_OK)
		return result;
	sqlite3_snprintf(sizeof(sql), sql, "SELECT COUNT(*) FROM main.%s", table);
	result = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (result != SQLITE_OK)
		return result;
	result = sqlite3_step(stmt);
	if (result == SQLITE_ROW)
	{
		*count = sqlite3_column_int(stmt, 0) - retained;
		result = SQLITE_OK;
	}
	sqlite3_finalize(stmt);
	return result;
}

/* Preserve only explicitly selected human demonstrations; bot comparisons and
 * their raw node/input data are removed atomically. Learning and CTF tables are
 * deliberately outside this whitelist. */
static inline int G_ResetStrafeTrackData(sqlite3 *db,
	const sqlite3_int64 *keepRouteIds, int keepRouteCount, int preview,
	int *deletedRoutes, int *deletedNodes, int *deletedInputs)
{
	static const char *const tables[] = {
		"LocalStrafeRoute", "LocalStrafeRouteNode", "LocalStrafeRouteInput"
	};
	sqlite3_stmt *stmt = NULL;
	int result = SQLITE_OK, i;
	int *deleted[] = { deletedRoutes, deletedNodes, deletedInputs };

	if (!db || !deletedRoutes || !deletedNodes || !deletedInputs ||
		keepRouteCount < 0 || (keepRouteCount > 0 && !keepRouteIds))
		return SQLITE_MISUSE;
	*deletedRoutes = *deletedNodes = *deletedInputs = 0;
	if (sqlite3_get_autocommit(db) == 0)
		return SQLITE_BUSY;
	if (preview)
	{
		for (i = 0; i < 3 && result == SQLITE_OK; i++)
			result = G_StrafeTrackResetCount(db, tables[i], i != 0,
				keepRouteIds, keepRouteCount, deleted[i]);
		return result;
	}

	result = sqlite3_exec(db, "BEGIN IMMEDIATE;"
		"DROP TABLE IF EXISTS temp.G_StrafeTrackResetKeep;"
		"CREATE TEMP TABLE G_StrafeTrackResetKeep(route_id INTEGER PRIMARY KEY)",
		NULL, NULL, NULL);
	if (result == SQLITE_OK && sqlite3_prepare_v2(db,
		"INSERT OR IGNORE INTO temp.G_StrafeTrackResetKeep(route_id) VALUES (?)",
		-1, &stmt, NULL) != SQLITE_OK)
		result = sqlite3_errcode(db);
	for (i = 0; result == SQLITE_OK && i < keepRouteCount; i++)
	{
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		sqlite3_bind_int64(stmt, 1, keepRouteIds[i]);
		result = sqlite3_step(stmt);
		if (result == SQLITE_DONE)
			result = SQLITE_OK;
	}
	sqlite3_finalize(stmt);
	stmt = NULL;
	if (result == SQLITE_OK)
	{
		result = sqlite3_exec(db, "DELETE FROM main.LocalStrafeRouteInput WHERE route_id NOT IN "
			"(SELECT route_id FROM temp.G_StrafeTrackResetKeep)", NULL, NULL, NULL);
		if (result == SQLITE_OK)
			*deletedInputs = sqlite3_changes(db);
	}
	if (result == SQLITE_OK)
	{
		result = sqlite3_exec(db, "DELETE FROM main.LocalStrafeRouteNode WHERE route_id NOT IN "
			"(SELECT route_id FROM temp.G_StrafeTrackResetKeep)", NULL, NULL, NULL);
		if (result == SQLITE_OK)
			*deletedNodes = sqlite3_changes(db);
	}
	if (result == SQLITE_OK)
	{
		result = sqlite3_exec(db, "DELETE FROM main.LocalStrafeRoute WHERE id NOT IN "
			"(SELECT route_id FROM temp.G_StrafeTrackResetKeep)", NULL, NULL, NULL);
		if (result == SQLITE_OK)
			*deletedRoutes = sqlite3_changes(db);
	}
	if (result == SQLITE_OK)
	{
		result = sqlite3_exec(db, "DROP TABLE temp.G_StrafeTrackResetKeep;COMMIT",
			NULL, NULL, NULL);
	}
	if (result != SQLITE_OK)
	{
		if (!sqlite3_get_autocommit(db))
			sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		*deletedRoutes = *deletedNodes = *deletedInputs = 0;
		return result;
	}
	return SQLITE_OK;
}

static inline int G_ResetAllStrafeTrackData(sqlite3 *db, int preview,
	int *deletedRoutes, int *deletedNodes, int *deletedInputs,
	int *deletedObjectives, int *deletedJourneys, int *deletedSessions)
{
	static const char *const tables[] = {
		"LocalStrafeRoute", "LocalStrafeRouteNode", "LocalStrafeRouteInput",
		"LocalCTFObjectiveEvent", "LocalCTFTrackJourney", "LocalCTFTrackSession"
	};
	static const char *const deleteTables[] = {
		"LocalStrafeRouteInput", "LocalStrafeRouteNode", "LocalStrafeRoute",
		"LocalCTFObjectiveEvent", "LocalCTFTrackJourney", "LocalCTFTrackSession"
	};
	const int tableCount = (int)(sizeof(tables) / sizeof(tables[0]));
	int *deleted[] = {
		deletedRoutes, deletedNodes, deletedInputs,
		deletedObjectives, deletedJourneys, deletedSessions
	};
	int result = SQLITE_OK, i;

	if (!db || !deletedRoutes || !deletedNodes || !deletedInputs ||
		!deletedObjectives || !deletedJourneys || !deletedSessions)
		return SQLITE_MISUSE;
	for (i = 0; i < tableCount; i++)
		*deleted[i] = 0;
	if (sqlite3_get_autocommit(db) == 0)
		return SQLITE_BUSY;
	if (preview)
	{
		for (i = 0; i < tableCount && result == SQLITE_OK; i++)
			result = G_StrafeTrackResetCount(db, tables[i], i == 1 || i == 2,
				NULL, 0, deleted[i]);
		return result;
	}

	result = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
	for (i = 0; result == SQLITE_OK && i < tableCount; i++)
	{
		char sql[160];

		sqlite3_snprintf(sizeof(sql), sql, "DELETE FROM main.%s", deleteTables[i]);
		result = sqlite3_exec(db, sql, NULL, NULL, NULL);
		if (result == SQLITE_OK)
		{
			if (i == 0)
				*deletedInputs = sqlite3_changes(db);
			else if (i == 1)
				*deletedNodes = sqlite3_changes(db);
			else if (i == 2)
				*deletedRoutes = sqlite3_changes(db);
			else if (i == 3)
				*deletedObjectives = sqlite3_changes(db);
			else if (i == 4)
				*deletedJourneys = sqlite3_changes(db);
			else
				*deletedSessions = sqlite3_changes(db);
		}
	}
	if (result == SQLITE_OK)
		result = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	if (result != SQLITE_OK)
	{
		if (!sqlite3_get_autocommit(db))
			sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		for (i = 0; i < tableCount; i++)
			*deleted[i] = 0;
	}
	return result;
}

#endif
