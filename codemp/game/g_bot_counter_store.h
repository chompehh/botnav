#ifndef G_BOT_COUNTER_STORE_H
#define G_BOT_COUNTER_STORE_H

#include "sqlite3.h"
#include <string.h>

#define BOTLEARN_COUNTER_CACHE_SIZE 4096
typedef struct
{
	int used, key, choice, samples, net, pendingSamples, pendingNet;
} botlearn_counter_entry_t;

typedef struct
{
	botlearn_counter_entry_t entries[BOTLEARN_COUNTER_CACHE_SIZE];
} botlearn_counter_cache_t;

static inline botlearn_counter_entry_t *BotCounter_Find(botlearn_counter_cache_t *cache,
	int key, int choice, int create)
{
	unsigned int slot;
	int i;
	if (key < 0 || key >= 1048576 || choice < 0 ||
		choice >= BOTLEARN_COUNTER_STORE_CHOICES)
		return NULL;
	slot = ((unsigned int)key * 2654435761u + choice * 131u) & (BOTLEARN_COUNTER_CACHE_SIZE - 1);
	for (i = 0; i < BOTLEARN_COUNTER_CACHE_SIZE; i++)
	{
		botlearn_counter_entry_t *e = &cache->entries[slot];
		if (!e->used)
		{
			if (!create) return NULL;
			e->used = 1; e->key = key; e->choice = choice;
			return e;
		}
		if (e->key == key && e->choice == choice) return e;
		slot = (slot + 1) & (BOTLEARN_COUNTER_CACHE_SIZE - 1);
	}
	return NULL;
}

static inline int BotCounter_AddLoaded(botlearn_counter_cache_t *cache,
	int key, int choice, int samples, int net)
{
	botlearn_counter_entry_t *e;
	if (!(key & (1 << 17)) || samples <= 0 || samples > 1000000 ||
		net < -100 * samples || net > 100 * samples)
		return 1;
	e = BotCounter_Find(cache, key, choice, 1);
	if (!e) return 0;
	e->samples = samples + e->pendingSamples;
	e->net = net + e->pendingNet;
	return 1;
}

static inline int BotCounter_Record(botlearn_counter_cache_t *cache, int key, int choice, int net)
{
	botlearn_counter_entry_t *e = BotCounter_Find(cache, key, choice, 1);
	if (!e) return 0;
	if (e->samples >= 1000000) return 1;
	if (net < -100) net = -100;
	if (net > 100) net = 100;
	e->samples++; e->net += net; e->pendingSamples++; e->pendingNet += net;
	return 1;
}

/* Stage the entire load before replacing live totals. Pending local increments
 * survive failed loads and are merged with successfully reloaded DB totals. */
static inline int BotCounter_Load(botlearn_counter_cache_t *cache,
	botlearn_counter_cache_t *staging, sqlite3 *db)
{
	sqlite3_stmt *stmt = NULL;
	int i, rc;
	if (!db || cache == staging) return 0;
	memset(staging, 0, sizeof(*staging));
	for (i = 0; i < BOTLEARN_COUNTER_CACHE_SIZE; i++)
	{
		botlearn_counter_entry_t *old = &cache->entries[i], *e;
		if (!old->pendingSamples) continue;
		e = BotCounter_Find(staging, old->key, old->choice, 1);
		if (!e) return 0;
		e->samples = e->pendingSamples = old->pendingSamples;
		e->net = e->pendingNet = old->pendingNet;
	}
	rc = sqlite3_prepare_v2(db, "SELECT key,choice,samples,net FROM LocalBotCounterV1",
		-1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
		{
			if (!BotCounter_AddLoaded(staging, sqlite3_column_int(stmt, 0),
				sqlite3_column_int(stmt, 1), sqlite3_column_int(stmt, 2),
				sqlite3_column_int(stmt, 3)))
			{
				sqlite3_finalize(stmt);
				return 0;
			}
		}
	}
	sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE) return 0;
	*cache = *staging;
	return 1;
}

static inline int BotCounter_Flush(botlearn_counter_cache_t *cache, sqlite3 *db)
{
	sqlite3_stmt *insert = NULL, *update = NULL;
	int i, rc;
	if (!db || sqlite3_exec(db, "BEGIN", NULL, NULL, NULL) != SQLITE_OK)
		return 0;
	rc = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO LocalBotCounterV1"
		"(key,choice,samples,net) VALUES(?,?,0,0)", -1, &insert, NULL);
	if (rc == SQLITE_OK)
		rc = sqlite3_prepare_v2(db, "UPDATE LocalBotCounterV1 SET samples=samples+?,"
			"net=net+? WHERE key=? AND choice=?", -1, &update, NULL);
	for (i = 0; rc == SQLITE_OK && i < BOTLEARN_COUNTER_CACHE_SIZE; i++)
	{
		botlearn_counter_entry_t *e = &cache->entries[i];
		if (!e->pendingSamples) continue;
		sqlite3_bind_int(insert, 1, e->key);
		sqlite3_bind_int(insert, 2, e->choice);
		rc = sqlite3_step(insert) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
		sqlite3_reset(insert);
		sqlite3_clear_bindings(insert);
		if (rc != SQLITE_OK) break;
		sqlite3_bind_int(update, 1, e->pendingSamples);
		sqlite3_bind_int(update, 2, e->pendingNet);
		sqlite3_bind_int(update, 3, e->key);
		sqlite3_bind_int(update, 4, e->choice);
		rc = sqlite3_step(update) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
		sqlite3_reset(update);
		sqlite3_clear_bindings(update);
	}
	sqlite3_finalize(insert);
	sqlite3_finalize(update);
	if (rc == SQLITE_OK) rc = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	if (rc != SQLITE_OK)
	{
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		return 0;
	}
	for (i = 0; i < BOTLEARN_COUNTER_CACHE_SIZE; i++)
		cache->entries[i].pendingSamples = cache->entries[i].pendingNet = 0;
	return 1;
}

#endif
