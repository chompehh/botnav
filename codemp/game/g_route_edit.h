#ifndef G_ROUTE_EDIT_H
#define G_ROUTE_EDIT_H

#include "sqlite3.h"
#include <float.h>
#include <stdlib.h>

typedef enum {
	ROUTE_EDIT_ID,
	ROUTE_EDIT_MAP,
	ROUTE_EDIT_LAST,
	ROUTE_EDIT_SESSION,
	ROUTE_EDIT_RADIUS,
	ROUTE_EDIT_SERVER_LAST,
	ROUTE_EDIT_SERVER_SESSION
} route_edit_mode_t;

static inline int G_RouteEditFinite(double value)
{
	return value == value && value >= -DBL_MAX && value <= DBL_MAX;
}

static inline int G_RouteEditStartNear(const double *point, const double *start, double radius)
{
	double d, distance = 0.0;
	int i;

	if (!G_RouteEditFinite(radius) || radius <= 0.0)
		return 0;
	for (i = 0; i < 3; i++)
	{
		if (!G_RouteEditFinite(point[i]) || !G_RouteEditFinite(start[i]))
			return 0;
		d = point[i] - start[i];
		distance += d * d;
	}
	return distance <= radius * radius;
}

/* Owns the transaction, including selection, child rows and optional legacy CTF
 * references. Callers must stop pending writers before invoking this helper. */
static inline int G_DeleteRecordedRoutes(sqlite3 *db, const char *map,
	route_edit_mode_t mode, sqlite3_int64 identity, const double *point, double radius,
	sqlite3_int64 **deletedIds, int *deletedCount)
{
	static const char *const ctfTables[] = {
		"LocalCTFObjectiveEvent", "LocalCTFTrackJourney", "LocalCTFTrackSession"
	};
	static const char *const deleteSQL[] = {
		"DELETE FROM LocalStrafeRouteInput WHERE route_id IN (SELECT id FROM temp.RouteEditDelete)",
		"DELETE FROM LocalStrafeRouteNode WHERE route_id IN (SELECT id FROM temp.RouteEditDelete)",
		"DELETE FROM LocalStrafeRoute WHERE id IN (SELECT id FROM temp.RouteEditDelete)"
	};
	sqlite3_stmt *routes = NULL, *insert = NULL, *columns = NULL;
	sqlite3_int64 *ids = NULL, *grown, id;
	char sql[256];
	double start[3];
	int result, count = 0, capacity = 0, i, hasReference;
	const char *query;

	if (!db || !map || !map[0] || !deletedIds || !deletedCount ||
		mode < ROUTE_EDIT_ID || mode > ROUTE_EDIT_SERVER_SESSION ||
		(mode == ROUTE_EDIT_RADIUS && (!point || !G_RouteEditFinite(radius) || radius <= 0.0)))
		return SQLITE_MISUSE;
	*deletedIds = NULL;
	*deletedCount = 0;
	if (!sqlite3_get_autocommit(db))
		return SQLITE_BUSY;
	result = sqlite3_exec(db, "BEGIN IMMEDIATE;"
		"DROP TABLE IF EXISTS temp.RouteEditDelete;"
		"CREATE TEMP TABLE RouteEditDelete(id INTEGER PRIMARY KEY)", NULL, NULL, NULL);
	if (result != SQLITE_OK)
		goto failed;
	if (mode == ROUTE_EDIT_ID)
		query = "SELECT id,start_x,start_y,start_z FROM LocalStrafeRoute WHERE lower(mapname)=lower(?1) AND id=?2";
	else if (mode == ROUTE_EDIT_SESSION || mode == ROUTE_EDIT_LAST)
		query = "SELECT id,start_x,start_y,start_z FROM LocalStrafeRoute WHERE lower(mapname)=lower(?1) "
			"AND editor_session_id=?2 ORDER BY id DESC LIMIT ?3";
	else if (mode == ROUTE_EDIT_SERVER_SESSION || mode == ROUTE_EDIT_SERVER_LAST)
		query = "SELECT id,start_x,start_y,start_z FROM LocalStrafeRoute WHERE lower(mapname)=lower(?1) "
			"AND recording_session_id=?2 ORDER BY id DESC LIMIT ?3";
	else
		query = "SELECT id,start_x,start_y,start_z FROM LocalStrafeRoute WHERE lower(mapname)=lower(?1)";
	result = sqlite3_prepare_v2(db, query, -1, &routes, NULL);
	if (result != SQLITE_OK)
		goto failed;
	sqlite3_bind_text(routes, 1, map, -1, SQLITE_TRANSIENT);
	if (mode == ROUTE_EDIT_ID || mode == ROUTE_EDIT_SESSION || mode == ROUTE_EDIT_LAST ||
		mode == ROUTE_EDIT_SERVER_SESSION || mode == ROUTE_EDIT_SERVER_LAST)
		sqlite3_bind_int64(routes, 2, identity);
	if (mode == ROUTE_EDIT_SESSION || mode == ROUTE_EDIT_LAST ||
		mode == ROUTE_EDIT_SERVER_SESSION || mode == ROUTE_EDIT_SERVER_LAST)
		sqlite3_bind_int(routes, 3, mode == ROUTE_EDIT_LAST || mode == ROUTE_EDIT_SERVER_LAST ? 1 : -1);
	result = sqlite3_prepare_v2(db, "INSERT INTO temp.RouteEditDelete VALUES (?)", -1, &insert, NULL);
	if (result != SQLITE_OK)
		goto failed;
	while ((result = sqlite3_step(routes)) == SQLITE_ROW)
	{
		id = sqlite3_column_int64(routes, 0);
		if (mode == ROUTE_EDIT_RADIUS)
		{
			if (sqlite3_column_type(routes, 1) == SQLITE_NULL ||
				sqlite3_column_type(routes, 2) == SQLITE_NULL ||
				sqlite3_column_type(routes, 3) == SQLITE_NULL)
				continue;
			for (i = 0; i < 3; i++)
				start[i] = sqlite3_column_double(routes, i + 1);
			if (!G_RouteEditStartNear(point, start, radius))
				continue;
		}
		if (count == capacity)
		{
			if (capacity > 0x3fffffff / (int)sizeof(*ids))
			{
				result = SQLITE_TOOBIG;
				goto failed;
			}
			capacity = capacity ? capacity * 2 : 32;
			grown = (sqlite3_int64 *)realloc(ids, (size_t)capacity * sizeof(*ids));
			if (!grown)
			{
				result = SQLITE_NOMEM;
				goto failed;
			}
			ids = grown;
		}
		ids[count++] = id;
		sqlite3_reset(insert);
		sqlite3_bind_int64(insert, 1, id);
		result = sqlite3_step(insert);
		if (result != SQLITE_DONE)
			goto failed;
	}
	if (result != SQLITE_DONE)
		goto failed;
	sqlite3_finalize(routes);
	routes = NULL;
	sqlite3_finalize(insert);
	insert = NULL;
	for (i = 0; i < (int)(sizeof(ctfTables) / sizeof(ctfTables[0])); i++)
	{
		sqlite3_snprintf(sizeof(sql), sql, "PRAGMA table_info(%s)", ctfTables[i]);
		result = sqlite3_prepare_v2(db, sql, -1, &columns, NULL);
		if (result != SQLITE_OK)
			goto failed;
		hasReference = 0;
		while ((result = sqlite3_step(columns)) == SQLITE_ROW)
			if (!sqlite3_stricmp((const char *)sqlite3_column_text(columns, 1), "route_id"))
				hasReference = 1;
		sqlite3_finalize(columns);
		columns = NULL;
		if (result != SQLITE_DONE)
			goto failed;
		if (hasReference)
		{
			sqlite3_snprintf(sizeof(sql), sql, "UPDATE %s SET route_id=NULL WHERE route_id IN "
				"(SELECT id FROM temp.RouteEditDelete)", ctfTables[i]);
			result = sqlite3_exec(db, sql, NULL, NULL, NULL);
			if (result != SQLITE_OK)
				goto failed;
		}
	}
	for (i = 0; i < 3; i++)
	{
		result = sqlite3_exec(db, deleteSQL[i], NULL, NULL, NULL);
		if (result != SQLITE_OK)
			goto failed;
	}
	result = sqlite3_exec(db, "DROP TABLE temp.RouteEditDelete;COMMIT", NULL, NULL, NULL);
	if (result != SQLITE_OK)
		goto failed;
	*deletedIds = ids;
	*deletedCount = count;
	return SQLITE_OK;

failed:
	sqlite3_finalize(routes);
	sqlite3_finalize(insert);
	sqlite3_finalize(columns);
	if (!sqlite3_get_autocommit(db))
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
	free(ids);
	return result;
}

#endif
