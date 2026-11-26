#ifndef G_DUEL_TUTORIAL_SEQUENCES_H
#define G_DUEL_TUTORIAL_SEQUENCES_H

#include <string.h>
#include "sqlite3.h"

enum {
	DTS_NONE, DTS_THROW, DTS_PULL, DTS_GRIP, DTS_DRAIN, DTS_PUSH,
	DTS_KICK, DTS_SWING
};
enum {
	DTS_THROW_PULL_KICK, DTS_GRIP_KICK, DTS_PULL_KICK, DTS_DRAIN_KICK,
	DTS_DRAIN_PULL, DTS_DRAIN_SWING, DTS_DRAIN_ALONE, DTS_PULL_SWING,
	DTS_GRIP_SWING, DTS_PUSH_SWING, DTS_ANTI_DRAIN_PULL,
	DTS_ANTI_DRAIN_PUSH, DTS_COUNT
};

typedef struct {
	int time, action, selfHealth, selfArmor, selfForce;
	int enemyHealth, enemyArmor, enemyForce, dealt, taken, enemyDraining;
} duel_sequence_event_t;

typedef struct {
	int attempts, losses, noDamageFp, poorForce, selfHp, selfAp, selfFp;
	int enemyHp, enemyAp, enemyFp, dealt, taken;
	int sampleSelfHp, sampleSelfAp, sampleSelfFp;
	int sampleEnemyHp, sampleEnemyAp, sampleEnemyFp, sampleDealt, sampleTaken;
	int sampleLoss, sampleNoDamageFp, samplePoorForce;
} duel_sequence_stat_t;

typedef struct {
	duel_sequence_stat_t stat[DTS_COUNT];
	duel_sequence_event_t previous, start;
	int havePrevious, active, opener, follow, third, deadline, dealt, taken, enemyDraining;
} duel_sequence_observation_t;

typedef struct {
	duel_sequence_stat_t stat[DTS_COUNT];
	int duels[DTS_COUNT], lastRotation[DTS_COUNT];
} duel_sequence_history_t;

static inline int G_DuelSequencePowerAction(int beforePowers, int powers,
	int beforeHand, int hand, int pullHand, int pushHand, int gripBit, int drainBit)
{
	if (hand != beforeHand && hand == pullHand) return DTS_PULL;
	if (hand != beforeHand && hand == pushHand) return DTS_PUSH;
	if ((powers & gripBit) && !(beforePowers & gripBit)) return DTS_GRIP;
	if ((powers & drainBit) && !(beforePowers & drainBit)) return DTS_DRAIN;
	return DTS_NONE;
}

static inline int G_DuelSequenceKickEdge(int kicking, int wasKicking,
	int target, int previousTarget, int opponent)
{
	return (kicking && !wasKicking) ||
		(opponent >= 0 && target == opponent && previousTarget != opponent);
}

static inline int G_DuelSequenceKind(int opener, int follow, int third)
{
	if (opener == DTS_THROW && follow == DTS_PULL && third == DTS_KICK) return DTS_THROW_PULL_KICK;
	if (opener == DTS_GRIP && follow == DTS_KICK) return DTS_GRIP_KICK;
	if (opener == DTS_PULL && follow == DTS_KICK) return DTS_PULL_KICK;
	if (opener == DTS_DRAIN && follow == DTS_KICK) return DTS_DRAIN_KICK;
	if (opener == DTS_DRAIN && follow == DTS_PULL) return DTS_DRAIN_PULL;
	if (opener == DTS_DRAIN && follow == DTS_SWING) return DTS_DRAIN_SWING;
	if (opener == DTS_DRAIN && !follow) return DTS_DRAIN_ALONE;
	if (opener == DTS_PULL && follow == DTS_SWING) return DTS_PULL_SWING;
	if (opener == DTS_GRIP && follow == DTS_SWING) return DTS_GRIP_SWING;
	if (opener == DTS_PUSH && follow == DTS_SWING) return DTS_PUSH_SWING;
	return -1;
}

static inline void G_DuelSequenceFinish(duel_sequence_observation_t *o,
	const duel_sequence_event_t *end)
{
	int kind = G_DuelSequenceKind(o->opener, o->follow, o->third);
	duel_sequence_stat_t *s;
	if (o->enemyDraining && !o->follow && o->opener == DTS_PULL) kind = DTS_ANTI_DRAIN_PULL;
	if (o->enemyDraining && !o->follow && o->opener == DTS_PUSH) kind = DTS_ANTI_DRAIN_PUSH;
	if (kind < 0 || end->time - o->deadline > 250)
		return;
	s = &o->stat[kind];
	s->sampleSelfHp = end->selfHealth - o->start.selfHealth;
	s->sampleSelfAp = end->selfArmor - o->start.selfArmor;
	s->sampleSelfFp = end->selfForce - o->start.selfForce;
	s->sampleEnemyHp = end->enemyHealth - o->start.enemyHealth;
	s->sampleEnemyAp = end->enemyArmor - o->start.enemyArmor;
	s->sampleEnemyFp = end->enemyForce - o->start.enemyForce;
	s->sampleDealt = o->dealt;
	s->sampleTaken = o->taken;
	s->sampleLoss = s->sampleSelfHp + s->sampleSelfAp <
		s->sampleEnemyHp + s->sampleEnemyAp;
	s->sampleNoDamageFp = !o->dealt && s->sampleSelfFp < 0;
	s->samplePoorForce = s->sampleNoDamageFp &&
		s->sampleSelfHp + s->sampleSelfAp <= 0 &&
		s->sampleSelfHp + s->sampleSelfAp <= s->sampleEnemyHp + s->sampleEnemyAp &&
		s->sampleSelfFp < s->sampleEnemyFp;
	s->attempts++;
	s->losses += s->sampleLoss;
	s->noDamageFp += s->sampleNoDamageFp;
	s->poorForce += s->samplePoorForce;
	s->selfHp += s->sampleSelfHp; s->selfAp += s->sampleSelfAp; s->selfFp += s->sampleSelfFp;
	s->enemyHp += s->sampleEnemyHp; s->enemyAp += s->sampleEnemyAp; s->enemyFp += s->sampleEnemyFp;
	s->dealt += o->dealt; s->taken += o->taken;
}

/* The preceding snapshot must be close to the opener; a late end is unknown,
 * not a zero-damage outcome. Windows never overlap. */
static inline void G_DuelSequenceObserve(duel_sequence_observation_t *o,
	const duel_sequence_event_t *e)
{
	int wasActive = o->active;
	if (o->active && e->time >= o->deadline)
	{
		if (e->time == o->deadline)
		{
			o->dealt += e->dealt;
			o->taken += e->taken;
		}
		G_DuelSequenceFinish(o, e);
		o->active = 0;
	}
	else if (o->active)
	{
		o->dealt += e->dealt;
		o->taken += e->taken;
		if (e->action && e->time - o->start.time <= 1000)
		{
			if (!o->follow && e->action != o->opener)
			{
				o->follow = e->action;
				o->deadline = e->time + 2000;
			}
			else if (o->opener == DTS_THROW && o->follow == DTS_PULL &&
				!o->third && e->action == DTS_KICK)
			{
				o->third = e->action;
				o->deadline = e->time + 2000;
			}
		}
	}
	if (!wasActive && e->action >= DTS_THROW && e->action <= DTS_PUSH &&
		o->havePrevious && e->time >= o->previous.time &&
		e->time - o->previous.time <= 250)
	{
		o->active = 1;
		o->opener = e->action; o->follow = o->third = 0;
		o->enemyDraining = e->enemyDraining;
		o->start = o->previous;
		o->start.time = e->time;
		o->deadline = e->time + 2000;
		o->dealt = e->dealt; o->taken = e->taken;
	}
	o->previous = *e;
	o->havePrevious = 1;
}

/* Receipt and all aggregates commit together, so a failed/retried duel cannot
 * count twice. The caller supplies its existing opaque session + start key. */
static inline int G_DuelTutorialSequenceRecord(sqlite3 *db, const char *identity,
	int kind, int mode, const char *duel, const duel_sequence_observation_t *o,
	duel_sequence_history_t *h)
{
	sqlite3_stmt *st = NULL;
	int rc, i, fresh = 0;
	memset(h, 0, sizeof(*h));
	for (i = 0; i < DTS_COUNT; i++) h->lastRotation[i] = -1000;
	rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS LocalBotTutorialSequenceV3("
		"identity TEXT NOT NULL,kind INTEGER NOT NULL,mode INTEGER NOT NULL,seq INTEGER NOT NULL,"
		"attempts INTEGER NOT NULL DEFAULT 0,duels INTEGER NOT NULL DEFAULT 0,"
		"losses INTEGER NOT NULL DEFAULT 0,nodamagefp INTEGER NOT NULL DEFAULT 0,poorforce INTEGER NOT NULL DEFAULT 0,"
		"sh INTEGER NOT NULL DEFAULT 0,sa INTEGER NOT NULL DEFAULT 0,sf INTEGER NOT NULL DEFAULT 0,"
		"eh INTEGER NOT NULL DEFAULT 0,ea INTEGER NOT NULL DEFAULT 0,ef INTEGER NOT NULL DEFAULT 0,"
		"dealt INTEGER NOT NULL DEFAULT 0,taken INTEGER NOT NULL DEFAULT 0,"
		"sample_sh INTEGER NOT NULL DEFAULT 0,sample_sa INTEGER NOT NULL DEFAULT 0,sample_sf INTEGER NOT NULL DEFAULT 0,"
		"sample_eh INTEGER NOT NULL DEFAULT 0,sample_ea INTEGER NOT NULL DEFAULT 0,sample_ef INTEGER NOT NULL DEFAULT 0,"
		"sample_dealt INTEGER NOT NULL DEFAULT 0,sample_taken INTEGER NOT NULL DEFAULT 0,"
		"last_rotation INTEGER NOT NULL DEFAULT -1000,PRIMARY KEY(identity,kind,mode,seq));"
		"CREATE TABLE IF NOT EXISTS LocalBotTutorialSequenceReceiptV3("
		"identity TEXT NOT NULL,kind INTEGER NOT NULL,mode INTEGER NOT NULL,duel TEXT NOT NULL,"
		"PRIMARY KEY(identity,kind,mode,duel))", NULL, NULL, NULL);
	if (rc != SQLITE_OK) return rc;
	rc = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
	if (rc != SQLITE_OK) return rc;
#define DTS_BIND(call) do { if (rc == SQLITE_OK) rc = (call); } while (0)
	rc = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO LocalBotTutorialSequenceReceiptV3 VALUES(?,?,?,?)", -1, &st, NULL);
	DTS_BIND(sqlite3_bind_text(st, 1, identity, -1, SQLITE_TRANSIENT));
	DTS_BIND(sqlite3_bind_int(st, 2, kind)); DTS_BIND(sqlite3_bind_int(st, 3, mode));
	DTS_BIND(sqlite3_bind_text(st, 4, duel, -1, SQLITE_TRANSIENT));
	if (rc == SQLITE_OK) { rc = sqlite3_step(st); if (rc == SQLITE_DONE) { fresh = sqlite3_changes(db); rc = SQLITE_OK; } }
	sqlite3_finalize(st); st = NULL;
	for (i = 0; fresh && i < DTS_COUNT && rc == SQLITE_OK; i++)
	{
		const duel_sequence_stat_t *s = &o->stat[i];
		if (!s->attempts) continue;
		rc = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO LocalBotTutorialSequenceV3(identity,kind,mode,seq) VALUES(?,?,?,?)", -1, &st, NULL);
		DTS_BIND(sqlite3_bind_text(st, 1, identity, -1, SQLITE_TRANSIENT));
		DTS_BIND(sqlite3_bind_int(st, 2, kind)); DTS_BIND(sqlite3_bind_int(st, 3, mode)); DTS_BIND(sqlite3_bind_int(st, 4, i));
		if (rc == SQLITE_OK) { rc = sqlite3_step(st); if (rc == SQLITE_DONE) rc = SQLITE_OK; }
		sqlite3_finalize(st); st = NULL;
		if (rc == SQLITE_OK) rc = sqlite3_prepare_v2(db, "UPDATE LocalBotTutorialSequenceV3 SET attempts=attempts+?,duels=duels+1,losses=losses+?,nodamagefp=nodamagefp+?,sh=sh+?,sa=sa+?,sf=sf+?,eh=eh+?,ea=ea+?,ef=ef+?,dealt=dealt+?,taken=taken+?,sample_sh=?,sample_sa=?,sample_sf=?,sample_eh=?,sample_ea=?,sample_ef=?,sample_dealt=?,sample_taken=?,poorforce=poorforce+? WHERE identity=? AND kind=? AND mode=? AND seq=?", -1, &st, NULL);
		DTS_BIND(sqlite3_bind_int(st, 1, s->attempts)); DTS_BIND(sqlite3_bind_int(st, 2, s->losses)); DTS_BIND(sqlite3_bind_int(st, 3, s->noDamageFp));
		DTS_BIND(sqlite3_bind_int(st, 4, s->selfHp)); DTS_BIND(sqlite3_bind_int(st, 5, s->selfAp)); DTS_BIND(sqlite3_bind_int(st, 6, s->selfFp));
		DTS_BIND(sqlite3_bind_int(st, 7, s->enemyHp)); DTS_BIND(sqlite3_bind_int(st, 8, s->enemyAp)); DTS_BIND(sqlite3_bind_int(st, 9, s->enemyFp));
		DTS_BIND(sqlite3_bind_int(st, 10, s->dealt)); DTS_BIND(sqlite3_bind_int(st, 11, s->taken));
		DTS_BIND(sqlite3_bind_int(st, 12, s->sampleSelfHp)); DTS_BIND(sqlite3_bind_int(st, 13, s->sampleSelfAp)); DTS_BIND(sqlite3_bind_int(st, 14, s->sampleSelfFp));
		DTS_BIND(sqlite3_bind_int(st, 15, s->sampleEnemyHp)); DTS_BIND(sqlite3_bind_int(st, 16, s->sampleEnemyAp)); DTS_BIND(sqlite3_bind_int(st, 17, s->sampleEnemyFp));
		DTS_BIND(sqlite3_bind_int(st, 18, s->sampleDealt)); DTS_BIND(sqlite3_bind_int(st, 19, s->sampleTaken));
		DTS_BIND(sqlite3_bind_int(st, 20, s->poorForce));
		DTS_BIND(sqlite3_bind_text(st, 21, identity, -1, SQLITE_TRANSIENT)); DTS_BIND(sqlite3_bind_int(st, 22, kind));
		DTS_BIND(sqlite3_bind_int(st, 23, mode)); DTS_BIND(sqlite3_bind_int(st, 24, i));
		if (rc == SQLITE_OK) { rc = sqlite3_step(st); if (rc == SQLITE_DONE) rc = SQLITE_OK; }
		sqlite3_finalize(st); st = NULL;
	}
	if (rc == SQLITE_OK) rc = sqlite3_prepare_v2(db, "SELECT seq,attempts,duels,losses,nodamagefp,sh,sa,sf,eh,ea,ef,dealt,taken,last_rotation,poorforce FROM LocalBotTutorialSequenceV3 WHERE identity=? AND kind=? AND mode=?", -1, &st, NULL);
	DTS_BIND(sqlite3_bind_text(st, 1, identity, -1, SQLITE_TRANSIENT)); DTS_BIND(sqlite3_bind_int(st, 2, kind)); DTS_BIND(sqlite3_bind_int(st, 3, mode));
	if (rc == SQLITE_OK)
	{
		while ((rc = sqlite3_step(st)) == SQLITE_ROW)
		{
			duel_sequence_stat_t *s;
			i = sqlite3_column_int(st, 0);
			if (i < 0 || i >= DTS_COUNT) continue;
			s = &h->stat[i];
			s->attempts = sqlite3_column_int(st, 1); h->duels[i] = sqlite3_column_int(st, 2);
			s->losses = sqlite3_column_int(st, 3); s->noDamageFp = sqlite3_column_int(st, 4);
			s->selfHp = sqlite3_column_int(st, 5); s->selfAp = sqlite3_column_int(st, 6); s->selfFp = sqlite3_column_int(st, 7);
			s->enemyHp = sqlite3_column_int(st, 8); s->enemyAp = sqlite3_column_int(st, 9); s->enemyFp = sqlite3_column_int(st, 10);
			s->dealt = sqlite3_column_int(st, 11); s->taken = sqlite3_column_int(st, 12); h->lastRotation[i] = sqlite3_column_int(st, 13);
			s->poorForce = sqlite3_column_int(st, 14);
		}
		if (rc == SQLITE_DONE) rc = SQLITE_OK;
	}
	sqlite3_finalize(st);
	if (rc == SQLITE_OK) rc = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	if (rc != SQLITE_OK) sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
#undef DTS_BIND
	return rc;
}

static inline int G_DuelSequenceSelect(int duels, int rotation,
	const duel_sequence_observation_t *o, const duel_sequence_history_t *h)
{
	int i, best = -1;
	if (duels < 3) return -1;
	for (i = 0; i < DTS_COUNT; i++)
	{
		const duel_sequence_stat_t *s = &h->stat[i], *current = &o->stat[i];
		if (!current->attempts || h->duels[i] < 2 || s->attempts < 3 ||
			rotation - h->lastRotation[i] < 4) continue;
		if ((current->sampleLoss && s->losses >= 2) ||
			(current->samplePoorForce && s->poorForce >= 2))
			return i;
		if (s->attempts >= 4 && best < 0) best = i;
	}
	return best;
}

enum { DTS_MESSAGE_LOSS, DTS_MESSAGE_POOR_FORCE, DTS_MESSAGE_REPEAT };

static inline int G_DuelSequenceMessageKind(int rotation,
	const duel_sequence_stat_t *total, const duel_sequence_stat_t *sample)
{
	int available[3], count = 0;
	if (sample->sampleLoss && total->losses >= 2) available[count++] = DTS_MESSAGE_LOSS;
	if (sample->samplePoorForce && total->poorForce >= 2) available[count++] = DTS_MESSAGE_POOR_FORCE;
	if (total->attempts >= 4) available[count++] = DTS_MESSAGE_REPEAT;
	return count ? available[(rotation > 0 ? rotation - 1 : 0) % count] : -1;
}

static inline int G_DuelSequenceMark(sqlite3 *db, const char *identity, int kind,
	int mode, int seq, int rotation)
{
	sqlite3_stmt *st = NULL;
	int rc = sqlite3_prepare_v2(db, "UPDATE LocalBotTutorialSequenceV3 SET last_rotation=? WHERE identity=? AND kind=? AND mode=? AND seq=?", -1, &st, NULL);
	if (rc == SQLITE_OK) rc = sqlite3_bind_int(st, 1, rotation);
	if (rc == SQLITE_OK) rc = sqlite3_bind_text(st, 2, identity, -1, SQLITE_TRANSIENT);
	if (rc == SQLITE_OK) rc = sqlite3_bind_int(st, 3, kind);
	if (rc == SQLITE_OK) rc = sqlite3_bind_int(st, 4, mode);
	if (rc == SQLITE_OK) rc = sqlite3_bind_int(st, 5, seq);
	if (rc == SQLITE_OK) { rc = sqlite3_step(st); if (rc == SQLITE_DONE) rc = SQLITE_OK; }
	sqlite3_finalize(st);
	return rc;
}
#endif
