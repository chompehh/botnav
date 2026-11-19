#ifndef G_SPAWNPOINT_H
#define G_SPAWNPOINT_H

#include <string.h>

/* Input is lower-cased by the command handler; values match team_t. */
static inline int G_SpawnPointTeam(const char *arg)
{
	if (!arg[0] || !strcmp(arg, "0") || !strcmp(arg, "free"))
		return 0;
	if (!strcmp(arg, "1") || !strcmp(arg, "red"))
		return 1;
	if (!strcmp(arg, "2") || !strcmp(arg, "blue"))
		return 2;
	return -1;
}

static inline void G_SpawnPointPose(const float *origin, const float *viewangles,
	int following, float *spawnOrigin, float *spawnAngles)
{
	spawnOrigin[0] = origin[0];
	spawnOrigin[1] = origin[1];
	/* Match amtelemark's follow offset, then undo the spawn selectors' +9. */
	spawnOrigin[2] = origin[2] + (following ? 58.0f : 0.0f) - 9.0f;
	spawnAngles[0] = viewangles[0];
	spawnAngles[1] = viewangles[1];
	spawnAngles[2] = 0.0f;
}

#endif
