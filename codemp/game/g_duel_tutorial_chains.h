#ifndef G_DUEL_TUTORIAL_CHAINS_H
#define G_DUEL_TUTORIAL_CHAINS_H

#include <stdio.h>
#include <string.h>
#include "g_duel_tutorial_sequences.h"
#include "g_duel_tutorial_chain_messages.h"

#define DTC_MAX_ACTIONS 8
#define DTC_MAX_WINDOWS 32
#define DTC_ACTION_HEAL 8

typedef struct {
	duel_sequence_event_t state;
	int actor, action, recoveryReady;
} duel_chain_event_t;

typedef struct {
	int time, actor, action;
} duel_chain_action_t;

typedef struct {
	char signature[64], label[40];
	unsigned int themes;
	int dealt, taken, selfHp, enemyHp, selfFp, enemyFp;
	int lowHp, otherHp, deficitFp, otherFp;
} duel_chain_window_t;

typedef struct {
	duel_chain_window_t windows[DTC_MAX_WINDOWS];
	duel_chain_action_t actions[DTC_MAX_ACTIONS];
	duel_sequence_event_t previous, baseline, start;
	int count, actionCount, active, havePrevious, haveBaseline, deadline, lastAction;
	int ownAttacks, enemyAttacks, dealt, taken, recovery, lowHp, otherHp;
	int deficitFp, otherFp, drainSamples, lastDrain, failed;
} duel_chain_observation_t;

typedef struct {
	duel_chain_window_t sample;
	int theme, matches, attempts, duels;
} duel_chain_advice_t;

static inline int G_DuelChainCombo(const duel_chain_action_t *actions, int count, int actor)
{
	int a[DTC_MAX_ACTIONS], time[DTC_MAX_ACTIONS], n = 0, i;
	for (i = 0; i < count; ++i)
		if (actions[i].actor == actor)
		{
			a[n] = actions[i].action;
			time[n++] = actions[i].time;
		}
	for (i = 0; i < n; ++i)
	{
		if (i + 2 < n && a[i] == DTS_THROW && a[i + 1] == DTS_PULL &&
			a[i + 2] == DTS_KICK && time[i + 2] - time[i] <= 1500)
			return DTC_VS_PTK;
		if (i + 1 < n && a[i] == DTS_PULL && a[i + 1] == DTS_KICK &&
			time[i + 1] - time[i] <= 1500)
			return DTC_VS_PK;
		if (i + 1 < n && a[i] == DTS_GRIP && a[i + 1] == DTS_KICK &&
			time[i + 1] - time[i] <= 1500)
			return DTC_VS_GK;
	}
	return -1;
}

static inline const char *G_DuelChainActionName(int action)
{
	static const char *const names[] = {
		"none", "Throw", "Pull", "Grip", "Drain", "Push", "Kick", "Saber", "Heal"
	};
	return action >= DTS_THROW && action <= DTC_ACTION_HEAL ? names[action] : "none";
}

static inline void G_DuelChainLabel(const duel_chain_action_t *actions, int count, char *label)
{
	char side[2][15];
	int actor, i, first, last, combo;
	for (actor = 0; actor < 2; ++actor)
	{
		first = last = 0;
		for (i = 0; i < count; ++i)
			if (actions[i].actor == actor)
			{
				if (!first) first = actions[i].action;
				last = actions[i].action;
			}
		combo = G_DuelChainCombo(actions, count, actor);
		if (combo >= 0)
			snprintf(side[actor], sizeof(side[actor]), "%s..%s",
				combo == DTC_VS_PTK ? "PTK" : combo == DTC_VS_PK ? "PK" : "GK",
				G_DuelChainActionName(last));
		else
			snprintf(side[actor], sizeof(side[actor]), "%s..%s",
				G_DuelChainActionName(first), G_DuelChainActionName(last));
	}
	snprintf(label, 40, "You:%s / Me:%s", side[0], side[1]);
}

static inline void G_DuelChainFinish(duel_chain_observation_t *o,
	const duel_sequence_event_t *end, int wonAgainstBot)
{
	duel_chain_window_t *w;
	int i, enemyCombo, ownSwings = 0, enemySwings = 0, loss, poor;
	if (end->time - o->deadline > 250 || o->actionCount < 4 ||
		o->ownAttacks < 2 || o->enemyAttacks < 2)
		return;
	if (o->count >= DTC_MAX_WINDOWS)
	{
		o->failed = 1;
		return;
	}
	w = &o->windows[o->count++];
	memset(w, 0, sizeof(*w));
	for (i = 0; i < o->actionCount; ++i)
	{
		w->signature[i * 3] = o->actions[i].actor ? 'e' : 'y';
		w->signature[i * 3 + 1] = '0' + o->actions[i].action;
		w->signature[i * 3 + 2] = '/';
		if (o->actions[i].action == DTS_SWING)
		{
			if (o->actions[i].actor) ++enemySwings;
			else ++ownSwings;
		}
	}
	G_DuelChainLabel(o->actions, o->actionCount, w->label);
	w->dealt = o->dealt; w->taken = o->taken;
	w->selfHp = end->selfHealth - o->start.selfHealth;
	w->enemyHp = end->enemyHealth - o->start.enemyHealth;
	w->selfFp = end->selfForce - o->start.selfForce;
	w->enemyFp = end->enemyForce - o->start.enemyForce;
	loss = w->selfHp + end->selfArmor - o->start.selfArmor <
		w->enemyHp + end->enemyArmor - o->start.enemyArmor;
	poor = !o->dealt && w->selfFp < 0 && w->selfHp + end->selfArmor - o->start.selfArmor <= 0 &&
		w->selfHp + end->selfArmor - o->start.selfArmor <=
			w->enemyHp + end->enemyArmor - o->start.enemyArmor && w->selfFp < w->enemyFp;
	enemyCombo = G_DuelChainCombo(o->actions, o->actionCount, 1);
	if (poor) w->themes |= 1u << DTC_POOR_FORCE;
	if (o->drainSamples >= 2 && end->selfForce < 20 && w->selfFp < 0)
		w->themes |= 1u << DTC_DRAIN_EXPOSURE;
	if (o->lowHp > 0 && !o->recovery)
	{
		w->themes |= 1u << DTC_LOW_HEALTH;
		w->lowHp = o->lowHp; w->otherHp = o->otherHp;
	}
	if (o->otherFp >= o->deficitFp + 20)
	{
		w->themes |= 1u << DTC_FORCE_DEFICIT;
		w->deficitFp = o->deficitFp; w->otherFp = o->otherFp;
	}
	if (loss && enemyCombo >= 0) w->themes |= 1u << enemyCombo;
	if (loss && enemySwings >= 2) w->themes |= 1u << DTC_VS_SABER;
	if (!loss && w->selfHp > w->enemyHp && ownSwings >= 2 &&
		o->dealt > o->taken && w->selfFp >= w->enemyFp &&
		(enemyCombo >= 0 || enemySwings >= 2))
		w->themes |= 1u << DTC_SABER_REPLY;
	if (wonAgainstBot && !loss && o->dealt > o->taken)
		w->themes |= 1u << DTC_BOT_SUCCESS;
}

static inline void G_DuelChainObserve(duel_chain_observation_t *o,
	const duel_chain_event_t *event, int wonAgainstBot)
{
	const duel_sequence_event_t *e = &event->state;
	int wasActive = o->active, attack;
	if (o->failed) return;
	if (event->actor < 0 || event->actor > 1 || event->action < 0 ||
		event->action > DTC_ACTION_HEAL || e->time < 0)
	{
		o->failed = 1;
		return;
	}
	if (o->havePrevious && e->time < o->previous.time)
	{
		o->failed = 1;
		return;
	}
	if (o->havePrevious && e->time > o->previous.time)
	{
		o->baseline = o->previous;
		o->haveBaseline = 1;
	}
	if (o->active && o->havePrevious && e->time - o->previous.time > 500)
		o->active = 0;
	if (e->selfHealth <= 0 || e->enemyHealth <= 0 ||
		e->selfArmor < 0 || e->enemyArmor < 0 || e->selfForce < 0 || e->enemyForce < 0)
	{
		o->active = o->havePrevious = o->haveBaseline = 0;
		return;
	}
	if (o->active && e->time >= o->deadline)
	{
		if (e->time == o->deadline)
		{
			o->dealt += e->dealt;
			o->taken += e->taken;
		}
		G_DuelChainFinish(o, e, wonAgainstBot);
		o->active = 0;
	}
	if (!wasActive && event->action && o->haveBaseline &&
		e->time - o->baseline.time <= 250)
	{
		o->active = 1;
		o->start = o->baseline;
		o->start.time = e->time;
		o->actionCount = o->ownAttacks = o->enemyAttacks = 0;
		o->dealt = o->taken = o->recovery = o->lowHp = o->otherHp = 0;
		o->deficitFp = o->otherFp = o->drainSamples = 0;
		o->lastDrain = -1;
		o->deadline = e->time + 2000;
	}
	if (o->active)
	{
		if (event->action && o->actionCount >= DTC_MAX_ACTIONS)
		{
			o->active = 0;
			o->previous = *e;
			o->havePrevious = 1;
			return;
		}
		o->dealt += e->dealt; o->taken += e->taken;
		attack = event->action && event->action != DTS_DRAIN && event->action != DTC_ACTION_HEAL;
		if (event->action && o->actionCount < DTC_MAX_ACTIONS)
		{
			duel_chain_action_t *a = &o->actions[o->actionCount++];
			a->time = e->time; a->actor = event->actor; a->action = event->action;
			o->lastAction = e->time;
			o->deadline = e->time + 2000;
			if (o->deadline > o->start.time + 12000) o->deadline = o->start.time + 12000;
			if (attack)
			{
				if (event->actor) ++o->enemyAttacks;
				else ++o->ownAttacks;
			}
			if (!event->actor && attack && o->ownAttacks >= 2)
			{
				if (event->recoveryReady && e->selfHealth > 0 && e->selfHealth <= 60 &&
					e->enemyHealth >= e->selfHealth + 20)
				{
					o->lowHp = e->selfHealth; o->otherHp = e->enemyHealth;
					o->recovery = 0;
				}
				if (e->enemyForce >= e->selfForce + 20)
				{
					o->deficitFp = e->selfForce; o->otherFp = e->enemyForce;
				}
			}
			if (!event->actor && (event->action == DTS_DRAIN || event->action == DTC_ACTION_HEAL))
				o->recovery = 1;
		}
		if (e->enemyDraining && e->selfForce < o->start.selfForce &&
			(o->lastDrain < 0 || e->time - o->lastDrain >= 100))
		{
			++o->drainSamples;
			o->lastDrain = e->time;
		}
	}
	o->previous = *e;
	o->havePrevious = 1;
}

static inline int G_DuelChainRender(const duel_chain_advice_t *advice, int rotation,
	char *text, size_t size)
{
	int a, b, n, index;
	if (!text || !size) return 0;
	text[0] = '\0';
	if (advice->theme < 0 || advice->theme >= DTC_THEME_COUNT) return 0;
	a = advice->matches; b = advice->attempts;
	if (advice->theme == DTC_LOW_HEALTH)
	{
		a = advice->sample.lowHp; b = advice->sample.otherHp;
	}
	else if (advice->theme == DTC_FORCE_DEFICIT)
	{
		a = advice->sample.deficitFp; b = advice->sample.otherFp;
	}
	else if (advice->theme == DTC_SABER_REPLY)
	{
		a = advice->sample.dealt; b = advice->sample.taken;
	}
	if (a < 0 || b < 0 || a > 999999 || b > 999999) return 0;
	index = advice->theme * 10 + (rotation >= 0 ? rotation : 0) % 10;
	if (index >= (int)DUEL_CHAIN_MESSAGE_COUNT || g_duelChainMessages[index].theme != advice->theme) return 0;
	n = snprintf(text, size, g_duelChainMessages[index].text, advice->sample.label, a, b);
	if (n < 0 || n >= 150 || (size_t)n >= size)
	{
		text[0] = '\0';
		return 0;
	}
	return 1;
}

static inline int G_DuelChainRecord(sqlite3 *db, const char *identity, int kind,
	int opponentBot, const char *duel, const duel_chain_observation_t *o,
	int rotation, int humanWon, duel_chain_advice_t *advice)
{
	sqlite3_stmt *st = NULL;
	int rc, fresh = 0, i, theme, match;
	memset(advice, 0, sizeof(*advice));
	advice->theme = -1;
	if (o->failed) return SQLITE_OK;
	rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS LocalBotTutorialChainV1("
		"identity TEXT NOT NULL,kind INTEGER NOT NULL,bot INTEGER NOT NULL,signature TEXT NOT NULL,theme INTEGER NOT NULL,"
		"attempts INTEGER NOT NULL DEFAULT 0,matches INTEGER NOT NULL DEFAULT 0,duels INTEGER NOT NULL DEFAULT 0,"
		"matched_duels INTEGER NOT NULL DEFAULT 0,last_duel TEXT NOT NULL DEFAULT '',last_match TEXT NOT NULL DEFAULT '',"
		"last_rotation INTEGER NOT NULL DEFAULT -1000,PRIMARY KEY(identity,kind,bot,signature,theme));"
		"CREATE TABLE IF NOT EXISTS LocalBotTutorialChainReceiptV1("
		"identity TEXT NOT NULL,kind INTEGER NOT NULL,duel TEXT NOT NULL,PRIMARY KEY(identity,kind,duel))",
		NULL, NULL, NULL);
	if (rc != SQLITE_OK) return rc;
	rc = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
	if (rc != SQLITE_OK) return rc;
#define DTC_BIND(call) do { if (rc == SQLITE_OK) rc = (call); } while (0)
	rc = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO LocalBotTutorialChainReceiptV1 VALUES(?,?,?)", -1, &st, NULL);
	DTC_BIND(sqlite3_bind_text(st, 1, identity, -1, SQLITE_TRANSIENT));
	DTC_BIND(sqlite3_bind_int(st, 2, kind));
	DTC_BIND(sqlite3_bind_text(st, 3, duel, -1, SQLITE_TRANSIENT));
	if (rc == SQLITE_OK)
	{
		rc = sqlite3_step(st);
		if (rc == SQLITE_DONE) { fresh = sqlite3_changes(db); rc = SQLITE_OK; }
	}
	sqlite3_finalize(st); st = NULL;
	for (i = 0; fresh && i < o->count && rc == SQLITE_OK; ++i)
		for (theme = 0; theme < DTC_THEME_COUNT && rc == SQLITE_OK; ++theme)
		{
			const duel_chain_window_t *w = &o->windows[i];
			match = (w->themes & (1u << theme)) != 0;
			rc = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO LocalBotTutorialChainV1(identity,kind,bot,signature,theme) VALUES(?,?,?,?,?)", -1, &st, NULL);
			DTC_BIND(sqlite3_bind_text(st, 1, identity, -1, SQLITE_TRANSIENT));
			DTC_BIND(sqlite3_bind_int(st, 2, kind)); DTC_BIND(sqlite3_bind_int(st, 3, opponentBot));
			DTC_BIND(sqlite3_bind_text(st, 4, w->signature, -1, SQLITE_TRANSIENT));
			DTC_BIND(sqlite3_bind_int(st, 5, theme));
			if (rc == SQLITE_OK) { rc = sqlite3_step(st); if (rc == SQLITE_DONE) rc = SQLITE_OK; }
			sqlite3_finalize(st); st = NULL;
			if (rc == SQLITE_OK)
				rc = sqlite3_prepare_v2(db, "UPDATE LocalBotTutorialChainV1 SET attempts=attempts+1,matches=matches+?,"
					"duels=duels+(last_duel<>?),matched_duels=matched_duels+(? AND last_match<>?),last_duel=?,"
					"last_match=CASE WHEN ? THEN ? ELSE last_match END WHERE identity=? AND kind=? AND bot=? AND signature=? AND theme=?",
					-1, &st, NULL);
			DTC_BIND(sqlite3_bind_int(st, 1, match));
			DTC_BIND(sqlite3_bind_text(st, 2, duel, -1, SQLITE_TRANSIENT));
			DTC_BIND(sqlite3_bind_int(st, 3, match));
			DTC_BIND(sqlite3_bind_text(st, 4, duel, -1, SQLITE_TRANSIENT));
			DTC_BIND(sqlite3_bind_text(st, 5, duel, -1, SQLITE_TRANSIENT));
			DTC_BIND(sqlite3_bind_int(st, 6, match));
			DTC_BIND(sqlite3_bind_text(st, 7, duel, -1, SQLITE_TRANSIENT));
			DTC_BIND(sqlite3_bind_text(st, 8, identity, -1, SQLITE_TRANSIENT));
			DTC_BIND(sqlite3_bind_int(st, 9, kind)); DTC_BIND(sqlite3_bind_int(st, 10, opponentBot));
			DTC_BIND(sqlite3_bind_text(st, 11, w->signature, -1, SQLITE_TRANSIENT));
			DTC_BIND(sqlite3_bind_int(st, 12, theme));
			if (rc == SQLITE_OK) { rc = sqlite3_step(st); if (rc == SQLITE_DONE) rc = SQLITE_OK; }
			sqlite3_finalize(st); st = NULL;
		}
	for (i = 0; i < o->count && rc == SQLITE_OK; ++i)
	{
		const duel_chain_window_t *w = &o->windows[i];
		rc = sqlite3_prepare_v2(db, "SELECT theme,matches,attempts,matched_duels,last_rotation FROM LocalBotTutorialChainV1 "
			"WHERE identity=? AND kind=? AND bot=? AND signature=?", -1, &st, NULL);
		DTC_BIND(sqlite3_bind_text(st, 1, identity, -1, SQLITE_TRANSIENT));
		DTC_BIND(sqlite3_bind_int(st, 2, kind)); DTC_BIND(sqlite3_bind_int(st, 3, opponentBot));
		DTC_BIND(sqlite3_bind_text(st, 4, w->signature, -1, SQLITE_TRANSIENT));
		if (rc == SQLITE_OK)
		{
			while ((rc = sqlite3_step(st)) == SQLITE_ROW)
			{
				theme = sqlite3_column_int(st, 0);
				if (theme < 0 || theme >= DTC_THEME_COUNT || !(w->themes & (1u << theme)) ||
					(humanWon && theme != DTC_BOT_SUCCESS) || (!humanWon && theme == DTC_BOT_SUCCESS) ||
					sqlite3_column_int(st, 1) < 2 || sqlite3_column_int(st, 2) < 3 ||
					sqlite3_column_int(st, 3) < 2 || rotation - sqlite3_column_int(st, 4) < 4)
					continue;
				if (advice->theme < 0 || theme == DTC_LOW_HEALTH ||
					(advice->theme != DTC_LOW_HEALTH && theme < advice->theme))
				{
					advice->sample = *w; advice->theme = theme;
					advice->matches = sqlite3_column_int(st, 1);
					advice->attempts = sqlite3_column_int(st, 2);
					advice->duels = sqlite3_column_int(st, 3);
				}
			}
			if (rc == SQLITE_DONE) rc = SQLITE_OK;
		}
		sqlite3_finalize(st); st = NULL;
	}
	if (rc == SQLITE_OK) rc = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	if (rc != SQLITE_OK)
	{
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		advice->theme = -1;
	}
#undef DTC_BIND
	return rc;
}

static inline int G_DuelChainMark(sqlite3 *db, const char *identity, int kind,
	int opponentBot, const duel_chain_advice_t *advice, int rotation)
{
	sqlite3_stmt *st = NULL;
	int rc = sqlite3_prepare_v2(db, "UPDATE LocalBotTutorialChainV1 SET last_rotation=? "
		"WHERE identity=? AND kind=? AND bot=? AND signature=? AND theme=?", -1, &st, NULL);
#define DTC_BIND(call) do { if (rc == SQLITE_OK) rc = (call); } while (0)
	DTC_BIND(sqlite3_bind_int(st, 1, rotation));
	DTC_BIND(sqlite3_bind_text(st, 2, identity, -1, SQLITE_TRANSIENT));
	DTC_BIND(sqlite3_bind_int(st, 3, kind)); DTC_BIND(sqlite3_bind_int(st, 4, opponentBot));
	DTC_BIND(sqlite3_bind_text(st, 5, advice->sample.signature, -1, SQLITE_TRANSIENT));
	DTC_BIND(sqlite3_bind_int(st, 6, advice->theme));
	if (rc == SQLITE_OK) { rc = sqlite3_step(st); if (rc == SQLITE_DONE) rc = SQLITE_OK; }
	sqlite3_finalize(st);
#undef DTC_BIND
	return rc;
}

#endif
