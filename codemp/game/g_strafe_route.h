#ifndef G_STRAFE_ROUTE_H
#define G_STRAFE_ROUTE_H

#include <math.h>

#define STRAFE_ROUTE_ENTRY_NORMAL 0
#define STRAFE_ROUTE_ENTRY_ROLL 1
#define STRAFE_ROUTE_ENTRY_WALLRUN 2
#define STRAFE_ROUTE_ENTRY_PREROLL_MS 1000
#define STRAFE_ROUTE_ROLL_RUNUP_MS 250
#define STRAFE_ROUTE_REPLAY_FLAG_CROUCH 32
#define STRAFE_ROUTE_REPLAY_FLAG_ROLL 64

static inline int G_StrafeRouteReplayEntryFlags(int upmove, int rolling)
{
	return (upmove < 0 ? STRAFE_ROUTE_REPLAY_FLAG_CROUCH : 0) |
		(rolling ? STRAFE_ROUTE_REPLAY_FLAG_ROLL : 0);
}

static inline int G_StrafeRouteEntryKind(int wasGrounded, int wasRolling,
	int rolling, int wasWallRun, int wallRun)
{
	if (wallRun && !wasWallRun)
		return STRAFE_ROUTE_ENTRY_WALLRUN;
	if (rolling && !wasRolling && wasGrounded)
		return STRAFE_ROUTE_ENTRY_ROLL;
	return STRAFE_ROUTE_ENTRY_NORMAL;
}

static inline int G_StrafeRouteEntryAnchor(int grounded, int commandTime, int now)
{
	return grounded && now >= commandTime &&
		now - commandTime <= STRAFE_ROUTE_ENTRY_PREROLL_MS;
}

static inline int G_StrafeRouteEntryAnchorPreferred(int entryKind, int age, int selectedAge)
{
	if (age < 0 || age > STRAFE_ROUTE_ENTRY_PREROLL_MS)
		return 0;
	if (selectedAge < 0)
		return 1;
	return entryKind == STRAFE_ROUTE_ENTRY_ROLL && age <= STRAFE_ROUTE_ROLL_RUNUP_MS &&
		age > selectedAge;
}

static inline float G_StrafeRouteTravelDistance(int entryKind, float dx, float dy, float dz)
{
	if (entryKind == STRAFE_ROUTE_ENTRY_NORMAL)
		dz = 0.0f;
	return sqrtf(dx * dx + dy * dy + dz * dz);
}

/* A pure drop or vertical wall climb can have little horizontal travel. The
 * real airborne->ground landing is still mandatory; a ground roll is not one. */
static inline int G_StrafeRouteSpecialKeep(int entryKind, int nodes, float pathDistance,
	float heightChange, int touchedHazard)
{
	if (touchedHazard || nodes < 1)
		return 0;
	if (entryKind == STRAFE_ROUTE_ENTRY_ROLL || entryKind == STRAFE_ROUTE_ENTRY_WALLRUN)
		return pathDistance >= 32.0f || fabsf(heightChange) >= 32.0f;
	return 0;
}

#endif
