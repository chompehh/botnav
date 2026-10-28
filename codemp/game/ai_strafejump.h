#ifndef AI_STRAFEJUMP_H
#define AI_STRAFEJUMP_H

#include <math.h>

#define BOT_SFJ_INTENT_MAX_AGE_MS 250
#define BOT_SFJ_ABORT_COOLDOWN_MS 500
#define BOT_SFJ_TRACK_APPROACH_RADIUS 3072.0f
#define BOT_SFJ_TRACK_APPROACH_TOUCH 48.0f
#define BOT_SFJ_TRACK_APPROACH_TIMEOUT_MS 8000
#define BOT_SFJ_TRACK_RETRY_MS 5000
#define BOT_SFJ_MAX_TRACK_ROUTES 256
#define BOT_SFJ_ROLL_PAUSE_MAX_MS 1500
#define BOT_SFJ_PURSUIT_START_DISTANCE 512.0f
#define BOT_SFJ_PURSUIT_STOP_DISTANCE 384.0f
#define BOT_SFJ_PURSUIT_START_RATE 80.0f
#define BOT_SFJ_PURSUIT_STOP_RATE 20.0f
#define BOT_SFJ_NAV_ENEMY_GUARD 384.0f
#define BOT_SFJ_JUMP_VELOCITY 225.0f
/* Held force jumps need a longer horizon than ordinary hops. */
#define BOT_SFJ_MAX_ARC_STEPS 4096
#define BOT_SFJ_MAX_CORRIDOR_POINTS 513

static inline float BotSFJ_ClampSetting(float value, float minimum, float maximum)
{
	if (!isfinite(value))
		return minimum;
	return value < minimum ? minimum : (value > maximum ? maximum : value);
}

static inline int BotSFJ_HeightAllows(float delta, float maxRise, float maxDrop)
{
	return isfinite(delta) && delta <= maxRise && delta >= -maxDrop;
}

static inline int BotSFJ_TrackEntryAllows(float distance, float height,
	int circleStart, float pickupRadius)
{
	return isfinite(distance) && distance >= 0.0f && isfinite(height) &&
		fabsf(height) <= (circleStart ? 64.0f : 96.0f) &&
		distance <= (circleStart ? BOT_SFJ_TRACK_APPROACH_RADIUS : pickupRadius);
}

static inline int BotSFJ_TrackIgnoresNavigation(int route, int requiredInteraction)
{
	return route > 0 && !requiredInteraction;
}

static inline const char *BotSFJ_SkipStatsRecordWhitespace(const char *text)
{
	while (text && *text && (unsigned char)*text <= ' ')
		text++;
	return text;
}

static inline void BotSFJ_ClearTrackRetries(int *retryUntil)
{
	int i;

	for (i = 0; i < BOT_SFJ_MAX_TRACK_ROUTES; i++)
		retryUntil[i] = 0;
}

static inline void BotSFJ_DelayTrackRetry(int route, int *retryUntil, int now)
{
	if (route >= 0 && route < BOT_SFJ_MAX_TRACK_ROUTES)
		retryUntil[route] = now + BOT_SFJ_TRACK_RETRY_MS;
}

static inline int BotSFJ_TrackRetryAllows(int route, const int *retryUntil, int now)
{
	return route >= 0 && route < BOT_SFJ_MAX_TRACK_ROUTES && now >= retryUntil[route];
}

static inline int BotSFJ_TrackRecoveryAllows(int loosen, int route, int grounded,
	int nodeElapsedMs, int nodeTimeoutMs)
{
	return loosen && route > 0 && grounded &&
		nodeElapsedMs >= 0 && nodeElapsedMs < nodeTimeoutMs;
}

static inline int BotSFJ_TrackSampleAllows(int elapsedMs, float distance, float speed)
{
	return elapsedMs > 0 && elapsedMs <= 1000 && isfinite(distance) &&
		distance >= 0.0f && isfinite(speed) && speed >= 0.0f &&
		distance <= (speed + 400.0f) * (float)elapsedMs / 1000.0f + 64.0f;
}

static inline int BotSFJ_TrackMeasuredDuration(int approaching, int started, int now)
{
	return !approaching && now > started ? now - started : 0;
}

static inline int BotSFJ_TrackBlockedFailure(int arcSafe, int traceBlocked)
{
	return !arcSafe && traceBlocked;
}

static inline int BotSFJ_RollPauseAllows(int loosen, int rolling, int running,
	int hardConflict, int elapsedMs)
{
	return loosen && rolling && running && !hardConflict &&
		elapsedMs >= 0 && elapsedMs <= BOT_SFJ_ROLL_PAUSE_MAX_MS;
}

static inline int BotSFJ_TrackResolvedAttempts(int attempts, int completions, int interrupted)
{
	if (interrupted > 0)
		attempts -= interrupted;
	return attempts > completions ? attempts : completions;
}

static inline float BotSFJ_TrackStallFactor(int stalls)
{
	float factor;

	if (stalls <= 0)
		return 1.0f;
	factor = powf(0.65f, (float)stalls);
	return factor < 0.1f ? 0.1f : factor;
}

static inline int BotSFJ_TrackForceJumpChargeAllows(int route, int forceJumpHop,
	int routeApproach)
{
	return route && forceJumpHop && !routeApproach;
}

static inline int BotSFJ_SpeedContext(int active, int level)
{
	return active ? (level < 1 ? 1 : (level > 3 ? 3 : level)) : 0;
}

static inline int BotSFJ_TrackSpeedGroup(int active, int level)
{
	return active == 0 ? 0 : (active == 1 && level >= 1 && level <= 3 ? level : 4);
}

static inline void BotSFJ_TrackCoverageQuotas(const int counts[5], int maximum,
	int quotas[5])
{
	int i, remaining = maximum;
	int changed;

	for (i = 0; i < 5; i++)
		quotas[i] = 0;
	do
	{
		changed = 0;
		for (i = 0; i < 5 && remaining > 0; i++)
			if (quotas[i] < counts[i])
			{
				quotas[i]++;
				remaining--;
				changed = 1;
			}
	} while (changed && remaining > 0);
}

static inline int BotSFJ_TrackSpeedAllows(int recordedActive, int recordedLevel,
	int active, int level)
{
	return recordedActive <= 0 ||
		(active && (recordedLevel < 0 || level >= recordedLevel));
}

/*
 * Force Speed for a recorded route comes only from a human recording that used
 * it. Unknown (-1) never authorizes Speed. The bot must be able to activate it
 * now at no less than the recorded level.
 */
static inline int BotSFJ_TrackSpeedActivatable(int recordedActive, int recordedLevel,
	int powersAllowed, int known, int level, int forcePower, int needed)
{
	if (recordedActive != 1 || !powersAllowed || !known || level < 1 || level > 3)
		return 0;
	if (recordedLevel > level)
		return 0;
	return forcePower >= needed;
}

/* A route hop that needs Speed is usable now, or once the bot activates Speed. */
static inline int BotSFJ_TrackSpeedUsable(int recordedActive, int recordedLevel,
	int active, int level, int activatable)
{
	return BotSFJ_TrackSpeedAllows(recordedActive, recordedLevel, active, level) ||
		(recordedActive == 1 && !active && activatable);
}

/* Bot-held Speed that did not come from a Speed route is turned off on a normal route. */
static inline int BotSFJ_TrackSpeedShouldDrop(int routeRecordedActive, int speedActive,
	int speedFromRoute, int enemyVisible)
{
	return routeRecordedActive != 1 && speedActive && !speedFromRoute && !enemyVisible;
}

/*
 * Human routes are trusted over the bot's own arc prediction (walls, ledges,
 * off-corridor landings). Trust is lost only after a death on the route, or
 * after repeated real failures to carry speed through the hop that outnumber
 * its successful passes. Hazards are never trusted away.
 */
#define BOT_SFJ_TRACK_TRUST_FAILURES 3

static inline int BotSFJ_TrackHopTrusted(int deaths, int failures, int passes)
{
	return deaths <= 0 &&
		(failures < BOT_SFJ_TRACK_TRUST_FAILURES || failures <= passes);
}

/*
 * Recorded takeoffs are within about 12u of the stored start and run along the
 * start->first-landing heading. The circle jump starts only once the bot is on
 * that line at the start, moving along it; a moving bot may trigger a little
 * early by the distance it covers before leaving the ground.
 */
#define BOT_SFJ_TRACK_START_TOUCH 16.0f
#define BOT_SFJ_TRACK_START_HEADING 25.0f
#define BOT_SFJ_TRACK_START_RUNUP 96.0f
#define BOT_SFJ_TRACK_START_CONE 20.0f

static inline float BotSFJ_TrackStartLead(float speed, int frameMs)
{
	float seconds;

	if (!isfinite(speed) || speed <= 0.0f)
		return 0.0f;
	seconds = (float)(frameMs > 0 ? frameMs : 0) / 1000.0f;
	/* Slow starts spend up to 120 ms swinging the view before takeoff. */
	if (speed < 250.0f)
		seconds += 0.12f;
	return BotSFJ_ClampSetting(speed * seconds, 0.0f, 48.0f);
}

/* (hx, hy) is the unit start->first-landing heading. */
static inline int BotSFJ_TrackStartReady(float ox, float oy, float sx, float sy,
	float hx, float hy, float vx, float vy, int frameMs, int relaxedHeading)
{
	const float rx = ox - sx;
	const float ry = oy - sy;
	const float along = rx * hx + ry * hy;
	const float lateral = fabsf(rx * hy - ry * hx);
	const float speed = sqrtf(vx * vx + vy * vy);
	const float lead = BotSFJ_TrackStartLead(speed, frameMs);

	if (!isfinite(along) || !isfinite(lateral) || !isfinite(speed))
		return 0;
	if (relaxedHeading)
		return sqrtf(rx * rx + ry * ry) <= BOT_SFJ_TRACK_START_TOUCH + lead;
	if (lateral > BOT_SFJ_TRACK_START_TOUCH ||
		along > BOT_SFJ_TRACK_START_TOUCH ||
		along < -(BOT_SFJ_TRACK_START_TOUCH + lead))
		return 0;
	if (speed < 32.0f)
		return 1;
	return (vx * hx + vy * hy) / speed >=
		cosf(BOT_SFJ_TRACK_START_HEADING * 0.017453292519943295f);
}

/*
 * Where to walk while approaching a circle start: straight through the start
 * from behind it along the route heading, otherwise to a run-up point behind
 * the start first. Returns 1 when aiming at the start itself.
 */
static inline int BotSFJ_TrackStartAim(float ox, float oy, float sx, float sy,
	float hx, float hy, float runup, float *tx, float *ty)
{
	const float rx = ox - sx;
	const float ry = oy - sy;
	const float along = rx * hx + ry * hy;
	const float lateral = fabsf(rx * hy - ry * hx);
	const float cone = tanf(BOT_SFJ_TRACK_START_CONE * 0.017453292519943295f);

	if (runup <= 0.0f || (along <= 0.0f &&
		lateral <= (BOT_SFJ_TRACK_START_TOUCH > -along * cone ?
			BOT_SFJ_TRACK_START_TOUCH : -along * cone)))
	{
		*tx = sx;
		*ty = sy;
		return 1;
	}
	*tx = sx - hx * runup;
	*ty = sy - hy * runup;
	return 0;
}

/*
 * A nearby route that is more successful, longer or faster is worth a short
 * extra walk compared with the nearest start. The credit is bounded, so far
 * routes do not displace a close start by much.
 */
#define BOT_SFJ_TRACK_QUALITY_WALK 384.0f

static inline float BotSFJ_TrackQualityCredit(float remaining, float averageSpeed,
	int attempts, int completions, float reliability)
{
	float success, lengthFactor, speedFactor;

	if (!isfinite(remaining) || remaining <= 0.0f)
		return 0.0f;
	if (attempts < 0)
		attempts = 0;
	if (completions < 0)
		completions = 0;
	if (completions > attempts)
		completions = attempts;
	success = ((float)completions + 1.0f) / ((float)attempts + 2.0f);
	lengthFactor = BotSFJ_ClampSetting(remaining / 2048.0f, 0.0f, 1.0f);
	speedFactor = BotSFJ_ClampSetting((averageSpeed - 250.0f) / 350.0f, 0.0f, 1.0f);
	return BOT_SFJ_TRACK_QUALITY_WALK * success *
		(0.5f * lengthFactor + 0.5f * speedFactor) *
		BotSFJ_ClampSetting(reliability, 0.0f, 1.0f);
}

static inline int BotSFJ_HopRetryDelay(int failures, int lethal)
{
	int delay = lethal ? 60000 : 15000;
	int i;

	for (i = 1; i < failures && delay < 300000; i++)
		delay *= 2;
	return delay > 300000 ? 300000 : delay;
}

static inline float BotSFJ_HopReliability(int failures, int passes)
{
	return ((float)passes + 1.0f) / ((float)passes + 1.0f + 4.0f * failures);
}

static inline int BotSFJ_MomentumCollision(float before, float after,
	int elapsed, int launched, int wallContact, int externallyForced)
{
	return launched && wallContact && !externallyForced &&
		elapsed > 0 && elapsed <= 150 && isfinite(before) && isfinite(after) &&
		before >= 300.0f && after >= 0.0f &&
		before - after >= 200.0f && after <= before * 0.45f;
}

static inline int BotSFJ_GroundRestart(int groundMs, float takeoffDistance)
{
	return groundMs >= 250 || takeoffDistance > 48.0f;
}

static inline float BotSFJ_GoalRouteScore(float before, float after, float cost)
{
	if (!isfinite(before) || !isfinite(after) || !isfinite(cost) ||
		before < 0.0f || after < 0.0f || cost < 0.0f)
		return -1.0f;
	return before - after - cost * 0.15f;
}

#define BOT_SFJ_TRACK_FLEE_GAIN 128.0f

static inline float BotSFJ_EscapeRouteScore(float before, float after, float cost)
{
	if (!isfinite(before) || !isfinite(after) || !isfinite(cost) ||
		before < 0.0f || after < before + BOT_SFJ_TRACK_FLEE_GAIN || cost < 0.0f)
		return -1.0f;
	return after - before - cost * 0.15f;
}

static inline int BotSFJ_EscapeEntryAway(const float *position, const float *entry,
	const float *enemy)
{
	const float moveX = entry[0] - position[0], moveY = entry[1] - position[1];
	const float awayX = position[0] - enemy[0], awayY = position[1] - enemy[1];
	const float lengths = (moveX * moveX + moveY * moveY) * (awayX * awayX + awayY * awayY);

	return isfinite(lengths) && lengths > 0.0f &&
		moveX * awayX + moveY * awayY >= 0.25f * sqrtf(lengths);
}

static inline int BotSFJ_DefensiveRouteEscape(float aggression, int enemy,
	int carryingFlag, float distance, float startDistance)
{
	return enemy && !carryingFlag && isfinite(aggression) && aggression < 0.0f &&
		distance >= startDistance;
}

static inline int BotSFJ_BackwardRouteEscape(int retreat, int saberOnly, int enabled)
{
	return retreat && !saberOnly && enabled;
}

/* Multi-source Dijkstra: seeds are safe approaches, edges are safe connectors.
 * A negative cost means unreachable; first[] keeps the acquisition route. */
static inline void BotSFJ_RoutePathsWithParents(int count, const float *edges,
	const float *seed, float *cost, int *first, int *parent)
{
	int visited[BOT_SFJ_MAX_TRACK_ROUTES];
	int i, j, step, best;

	if (count < 0 || count > BOT_SFJ_MAX_TRACK_ROUTES)
		return;
	for (i = 0; i < count; i++)
	{
		visited[i] = 0;
		cost[i] = seed[i];
		first[i] = seed[i] >= 0.0f ? i : -1;
		if (parent)
			parent[i] = -1;
	}
	for (step = 0; step < count; step++)
	{
		best = -1;
		for (i = 0; i < count; i++)
			if (!visited[i] && cost[i] >= 0.0f &&
				(best < 0 || cost[i] < cost[best]))
				best = i;
		if (best < 0)
			break;
		visited[best] = 1;
		for (j = 0; j < count; j++)
		{
			const float edge = edges[best * count + j];
			const float next = cost[best] + edge;

			if (!visited[j] && edge >= 0.0f &&
				(cost[j] < 0.0f || next < cost[j]))
			{
				cost[j] = next;
				first[j] = first[best];
				if (parent)
					parent[j] = best;
			}
		}
	}
}

static inline void BotSFJ_RoutePaths(int count, const float *edges,
	const float *seed, float *cost, int *first)
{
	BotSFJ_RoutePathsWithParents(count, edges, seed, cost, first, NULL);
}

/* Interrupted attempts are not collision failures. Unknown measurements stay neutral. */
static inline float BotSFJ_TrackPerformance(float distance, float durationMs,
	float speedIntegral, float referenceIntegral)
{
	float speed, retention;

	if (!(distance > 0.0f) || !(durationMs > 0.0f) ||
		!isfinite(distance) || !isfinite(durationMs))
		return 1.0f;
	speed = BotSFJ_ClampSetting(distance * 1000.0f / durationMs / 400.0f, 0.25f, 2.5f);
	retention = referenceIntegral > 0.0f ?
		BotSFJ_ClampSetting(speedIntegral / referenceIntegral, 0.25f, 1.5f) : 1.0f;
	return sqrtf(speed * retention);
}

/* A landing node is a progress marker, not an exact touchdown requirement. */
static inline int BotSFJ_TrackNodePassed(const float *origin, const float *from,
	const float *node, int grounded, float tolerance, float heightTolerance)
{
	const float x = origin[0] - node[0];
	const float y = origin[1] - node[1];
	const float dx = node[0] - from[0];
	const float dy = node[1] - from[1];
	const float length = sqrtf(dx * dx + dy * dy);
	float lateral;

	if (grounded && fabsf(origin[2] - node[2]) > heightTolerance)
		return 0;
	if (x * x + y * y <= tolerance * tolerance)
		return 1;
	if (length <= 1.0f || x * dx + y * dy < 0.0f)
		return 0;
	lateral = (x * dy - y * dx) / length;
	return fabsf(lateral) <= tolerance;
}

/* Recorded routes come from humans at a different frame rate, so a bot rarely lands on
 * the recorded spot. Long hops get proportionally more slack (30% of the gap). */
#define BOT_SFJ_TRACK_LOOSE_MIN_TOLERANCE 80.0f
#define BOT_SFJ_TRACK_LOOSE_MAX_TOLERANCE 256.0f
#define BOT_SFJ_TRACK_LOOSE_GAP_FRACTION 0.3f
#define BOT_SFJ_TRACK_LOOSE_HEIGHT 96.0f
#define BOT_SFJ_TRACK_LOOSE_PICKUP_RADIUS 192.0f

static inline float BotSFJ_TrackNodeTolerance(float baseTolerance, float gapLength, int loosen)
{
	float t;

	if (!loosen)
		return baseTolerance;
	t = gapLength * BOT_SFJ_TRACK_LOOSE_GAP_FRACTION;
	if (t < baseTolerance)
		t = baseTolerance;
	if (t < BOT_SFJ_TRACK_LOOSE_MIN_TOLERANCE)
		t = BOT_SFJ_TRACK_LOOSE_MIN_TOLERANCE;
	if (t > BOT_SFJ_TRACK_LOOSE_MAX_TOLERANCE)
		t = BOT_SFJ_TRACK_LOOSE_MAX_TOLERANCE;
	return t;
}

/* Node timeout: the configured value, or longer for hops the human took long to fly. */
static inline int BotSFJ_TrackNodeTimeout(int configuredMs, int recordedAirMs, int loosen)
{
	int t = configuredMs;

	if (loosen && recordedAirMs > 0)
	{
		const int recordedTimeout = recordedAirMs >= 6750 ? 15000 : recordedAirMs * 2 + 1500;
		if (recordedTimeout > t)
			t = recordedTimeout;
	}
	return t > 15000 ? 15000 : t;
}

static inline int BotSFJ_TrackAirTimeout(int route, int configuredMs, int recordedAirMs, int loosen)
{
	const int timeout = route && loosen ?
		BotSFJ_TrackNodeTimeout(configuredMs, recordedAirMs, loosen) : 1500;

	return timeout < 1500 ? 1500 : timeout;
}

/* Horizontal distance squared from origin to node. */
static inline float BotSFJ_Dist2DSq(const float *a, const float *b)
{
	const float x = a[0] - b[0];
	const float y = a[1] - b[1];

	return x * x + y * y;
}

/* How many nodes to skip when the bot is already closer to a later landing. */
static inline int BotSFJ_TrackSkipAhead(const float *origin, const float *nodesXYZ, int current,
	int nodeCount, int maxSkip)
{
	int k, best = 0;
	float bestDist;

	if (current < 0 || current >= nodeCount)
		return 0;
	bestDist = BotSFJ_Dist2DSq(origin, &nodesXYZ[current * 3]);
	for (k = 1; k <= maxSkip && current + k < nodeCount; k++)
	{
		const float d = BotSFJ_Dist2DSq(origin, &nodesXYZ[(current + k) * 3]);

		if (d < bestDist)
		{
			bestDist = d;
			best = k;
		}
	}
	return best;
}

static inline int BotSFJ_ContinuitySlowedOut(float peakSpeed, float speed,
	float groundSpeed)
{
	return groundSpeed > 0.0f && peakSpeed > groundSpeed * 1.1f &&
		speed < groundSpeed * 0.8f;
}

static inline int BotSFJ_WaypointBudget(int budget)
{
	return budget < 1 ? 1 : (budget > 512 ? 512 : budget);
}

static inline int BotSFJ_WaypointVisible(int visibility)
{
	return visibility != 0 && visibility != 2;
}

static inline int BotSFJ_UseClippedForwardTarget(int passed, float endpointProgress,
	float firstTargetDistance)
{
	return passed && (endpointProgress < 0.0f || firstTargetDistance > 640.0f);
}

/* Horizontal progress only: a validated jump may be well above its trail. */
static inline int BotSFJ_WaypointPassed(const float *origin, const float *waypoint,
	const float *direction, float halfWidth)
{
	const float x = origin[0] - waypoint[0];
	const float y = origin[1] - waypoint[1];
	const float progress = x * direction[0] + y * direction[1];
	const float lateral = x * direction[1] - y * direction[0];

	return progress >= 0.0f && lateral * lateral <= halfWidth * halfWidth;
}

static inline int BotSFJ_AdvanceLinkAllows(int requiredFlags, int linked,
	int passable, float heightDelta, float baseHeightDelta, float alignment,
	float endpointProgress)
{
	return !requiredFlags && linked && passable &&
		fabsf(heightDelta) <= 32.0f && fabsf(baseHeightDelta) <= 64.0f &&
		alignment >= 0.9f && endpointProgress >= 0.0f;
}

static inline int BotSFJ_AdvanceDropLinkAllows(int requiredFlags, int linked,
	int passable, float heightDelta, float baseHeightDelta, float alignment,
	float endpointProgress, float maxDrop)
{
	return !requiredFlags && linked && passable &&
		BotSFJ_HeightAllows(heightDelta, 32.0f, maxDrop) &&
		BotSFJ_HeightAllows(baseHeightDelta, 64.0f, maxDrop) &&
		alignment >= 0.9f && endpointProgress >= 0.0f;
}

static inline int BotSFJ_StartIntervalMs(int frequency)
{
	if (frequency <= 0)
		return 0;
	if (frequency > 1000)
		frequency = 1000;
	return 100000 / frequency;
}

static inline int BotSFJ_WaypointSkipAllows(int requiredFlags, float heightDelta,
	float alignment, int clearPath)
{
	return !requiredFlags && fabsf(heightDelta) <= 32.0f &&
		alignment >= 0.9f && clearPath;
}

typedef enum
{
	BOT_SFJ_PHASE_OFF = 0,
	BOT_SFJ_PHASE_PREPARE,
	BOT_SFJ_PHASE_TAKEOFF,
	BOT_SFJ_PHASE_AIR,
	BOT_SFJ_PHASE_LANDING,
	BOT_SFJ_PHASE_REJUMP,
	BOT_SFJ_PHASE_ABORT
} bot_sfj_phase_t;

typedef enum
{
	BOT_SFJ_INTENT_NONE = 0,
	BOT_SFJ_INTENT_NAVIGATION,
	BOT_SFJ_INTENT_RETREAT,
	BOT_SFJ_INTENT_PURSUIT
} bot_sfj_intent_t;

/*
 * Hard conflicts always block and abort a strafe jump. Soft conflicts (queued
 * attack/jump/crouch/walk, weapon cooldown, saber specials, navigation jump
 * timers) only block initiation; once the controller is mid-jump they are
 * ignored and the navigation jump/crouch/walk inputs are masked.
 */
typedef enum
{
	BOT_SFJ_CONFLICT_NONE = 0,
	BOT_SFJ_CONFLICT_SOFT,
	BOT_SFJ_CONFLICT_HARD
} bot_sfj_conflict_t;

static inline int BotSFJ_PhaseInProgress(bot_sfj_phase_t phase)
{
	return phase == BOT_SFJ_PHASE_TAKEOFF || phase == BOT_SFJ_PHASE_AIR ||
		phase == BOT_SFJ_PHASE_LANDING || phase == BOT_SFJ_PHASE_REJUMP;
}

static inline int BotSFJ_ConflictBlocks(bot_sfj_conflict_t conflict, bot_sfj_phase_t phase)
{
	if (conflict == BOT_SFJ_CONFLICT_HARD)
		return 1;
	if (conflict == BOT_SFJ_CONFLICT_SOFT)
		return !BotSFJ_PhaseInProgress(phase);
	return 0;
}

static inline int BotSFJ_IntentIsFresh(int now, int intentTime)
{
	return intentTime > 0 && now >= intentTime &&
		now - intentTime <= BOT_SFJ_INTENT_MAX_AGE_MS;
}

static inline int BotSFJ_UpdatePursuitLatchEx(int latched, float distance,
	float previousDistance, int elapsedMs, float startDistance, float stopDistance)
{
	float separationRate;

	if (elapsedMs <= 0 || elapsedMs > 1000 || previousDistance <= 0.0f)
		return distance >= stopDistance ? latched : 0;
	separationRate = (distance - previousDistance) * 1000.0f / (float)elapsedMs;
	if (latched)
		return distance > stopDistance &&
			separationRate > BOT_SFJ_PURSUIT_STOP_RATE;
	return distance >= startDistance &&
		separationRate >= BOT_SFJ_PURSUIT_START_RATE;
}

static inline int BotSFJ_UpdatePursuitLatch(int latched, float distance, float previousDistance,
	int elapsedMs)
{
	return BotSFJ_UpdatePursuitLatchEx(latched, distance, previousDistance, elapsedMs,
		BOT_SFJ_PURSUIT_START_DISTANCE, BOT_SFJ_PURSUIT_STOP_DISTANCE);
}

/* Stop distance used with a configurable start distance (bot_minstrafe). */
static inline float BotSFJ_StopDistanceFor(float startDistance)
{
	if (startDistance <= 0.0f)
		return 0.0f;
	return startDistance * (BOT_SFJ_PURSUIT_STOP_DISTANCE / BOT_SFJ_PURSUIT_START_DISTANCE);
}

/*
 * Escape strafes may start once the bot is outside saber range instead of waiting for
 * the full bot_minstrafe gap: humans escaped at strafe speed almost as often as they
 * chased (396 vs 472 episodes), bots rarely (49 vs 122). bot_minstrafe below this
 * still wins, and 0 keeps "no minimum".
 */
#define BOT_SFJ_ESCAPE_MIN_DISTANCE 128.0f

static inline float BotSFJ_EscapeStartDistance(float minStrafe)
{
	if (minStrafe <= 0.0f)
		return 0.0f;
	return minStrafe < BOT_SFJ_ESCAPE_MIN_DISTANCE ? minStrafe : BOT_SFJ_ESCAPE_MIN_DISTANCE;
}

static inline int BotSFJ_CanOwnInput(int enabled, int freshIntent, int eligible,
	bot_sfj_phase_t phase)
{
	return enabled && freshIntent && eligible &&
		phase != BOT_SFJ_PHASE_OFF && phase != BOT_SFJ_PHASE_ABORT;
}

static inline bot_sfj_phase_t BotSFJ_NextPhase(bot_sfj_phase_t phase, int enabled,
	int freshIntent, int eligible, int grounded, int phaseAgeMs)
{
	if (!enabled)
		return BOT_SFJ_PHASE_OFF;
	if (!freshIntent || !eligible)
		return BOT_SFJ_PHASE_ABORT;

	switch (phase)
	{
	case BOT_SFJ_PHASE_PREPARE:
		return grounded && phaseAgeMs > 0 ? BOT_SFJ_PHASE_TAKEOFF : phase;
	case BOT_SFJ_PHASE_TAKEOFF:
	case BOT_SFJ_PHASE_REJUMP:
		return grounded ? phase : BOT_SFJ_PHASE_AIR;
	case BOT_SFJ_PHASE_AIR:
		return grounded ? BOT_SFJ_PHASE_LANDING : phase;
	case BOT_SFJ_PHASE_LANDING:
		return grounded && phaseAgeMs > 0 ? BOT_SFJ_PHASE_REJUMP : phase;
	default:
		return phase;
	}
}

static inline float BotSFJ_AngleDelta(float angle1, float angle2)
{
	float delta = fmodf(angle1 - angle2, 360.0f);
	if (delta > 180.0f)
		delta -= 360.0f;
	else if (delta < -180.0f)
		delta += 360.0f;
	return delta;
}

static inline float BotSFJ_SteerYaw(float previousYaw, float targetYaw, float maxDelta)
{
	float delta = BotSFJ_AngleDelta(targetYaw, previousYaw);

	if (delta > maxDelta)
		delta = maxDelta;
	else if (delta < -maxDelta)
		delta = -maxDelta;
	return previousYaw + delta;
}

/*
 * Human airborne wish-to-velocity angle by speed: median over WA/WD airborne
 * frames of strafetrack_inputs (2).csv (326 routes), where the wish yaw is
 * yaw + atan2(-rightmove, forwardmove) and the velocity heading comes from the
 * position change between frames.  See docs/bot-cvar-outline.md.
 */
static inline float BotSFJ_HumanWishOffset(float speed)
{
	static const float speeds[] = { 200.0f, 300.0f, 400.0f, 500.0f, 600.0f, 700.0f };
	static const float offsets[] = { 36.0f, 52.0f, 63.5f, 69.0f, 72.0f, 72.0f };
	int i;

	if (!(speed > speeds[0]))
		return offsets[0];
	for (i = 1; i < 6; i++)
	{
		if (speed <= speeds[i])
			return offsets[i - 1] + (offsets[i] - offsets[i - 1]) *
				(speed - speeds[i - 1]) / (speeds[i] - speeds[i - 1]);
	}
	return offsets[5];
}

/* PM_Accelerate's maximum-gain angle between velocity and wish direction. */
static inline float BotSFJ_OptimalWishOffset(float speed, float wishSpeed,
	float airAccelerate, float commandSeconds)
{
	float cosine;

	if (!(speed > 0.0f))
		return 0.0f;
	cosine = (wishSpeed - airAccelerate * commandSeconds * wishSpeed) / speed;
	if (cosine < -1.0f)
		cosine = -1.0f;
	else if (cosine > 1.0f)
		cosine = 1.0f;
	return acosf(cosine) * 57.29577951308232f;
}

#define BOT_SFJ_HUMAN_EXTRA_MAX 12.0f
#define BOT_SFJ_WISH_OFFSET_MAX 74.0f
#define BOT_SFJ_ROUTE_CLAMP 74.0f
/* Frame time of the strafetrack sample the human offsets were measured at. */
#define BOT_SFJ_HUMAN_SAMPLE_SECONDS 0.033f

/*
 * PM_AirMove uses PM_Accelerate(wishdir, ps->speed, pm_airaccelerate). Aim the
 * world wish direction at that acceleration boundary, widened by the margin
 * humans keep beyond it at this speed (they trade a little gain for turning),
 * and clamp toward the route. side > 0 turns left (wish left of velocity).
 * Command axes get their own view-yaw compensation below.
 */
static inline float BotSFJ_CommandYaw(float routeYaw, float velocityX, float velocityY,
	float wishSpeed, float airAccelerate, float commandSeconds, int side)
{
	const float radiansToDegrees = 57.29577951308232f;
	float speed = sqrtf(velocityX * velocityX + velocityY * velocityY);
	float velocityYaw;
	float offset;
	float extra;
	float yaw;

	if (speed < 32.0f || wishSpeed <= 0.0f)
		return routeYaw;
	if (commandSeconds < 0.001f)
		commandSeconds = 0.001f;
	else if (commandSeconds > 0.1f)
		commandSeconds = 0.1f;
	offset = BotSFJ_OptimalWishOffset(speed, wishSpeed, airAccelerate, commandSeconds);
	extra = BotSFJ_HumanWishOffset(speed) - BotSFJ_OptimalWishOffset(speed, wishSpeed,
		airAccelerate, BOT_SFJ_HUMAN_SAMPLE_SECONDS);
	if (extra > BOT_SFJ_HUMAN_EXTRA_MAX)
		extra = BOT_SFJ_HUMAN_EXTRA_MAX;
	if (extra > 0.0f)
		offset += extra;
	if (offset > BOT_SFJ_WISH_OFFSET_MAX)
		offset = BOT_SFJ_WISH_OFFSET_MAX;
	velocityYaw = atan2f(velocityY, velocityX) * radiansToDegrees;
	yaw = velocityYaw + (side < 0 ? -offset : offset);
	if (BotSFJ_AngleDelta(yaw, routeYaw) > BOT_SFJ_ROUTE_CLAMP)
		yaw = routeYaw + BOT_SFJ_ROUTE_CLAMP;
	else if (BotSFJ_AngleDelta(yaw, routeYaw) < -BOT_SFJ_ROUTE_CLAMP)
		yaw = routeYaw - BOT_SFJ_ROUTE_CLAMP;
	return yaw;
}

static inline int BotSFJ_SelectSide(float routeYaw, float velocityX, float velocityY,
	int fallbackSide)
{
	const float radiansToDegrees = 57.29577951308232f;
	float speed = sqrtf(velocityX * velocityX + velocityY * velocityY);
	float velocityYaw;
	float routeDelta;

	if (speed < 32.0f)
		return fallbackSide < 0 ? -1 : 1;
	velocityYaw = atan2f(velocityY, velocityX) * radiansToDegrees;
	routeDelta = BotSFJ_AngleDelta(routeYaw, velocityYaw);
	if (routeDelta > 5.0f)
		return 1;
	if (routeDelta < -5.0f)
		return -1;
	return fallbackSide < 0 ? -1 : 1;
}

static inline int BotSFJ_JumpPressed(bot_sfj_phase_t phase, int grounded)
{
	return grounded &&
		(phase == BOT_SFJ_PHASE_TAKEOFF || phase == BOT_SFJ_PHASE_REJUMP);
}

/* A queued release is not evidence until pmove has consumed its usercmd. */
static inline int BotSFJ_JumpReleaseConfirmed(int releaseTime, int commandTime,
	int jumpHeld)
{
	return releaseTime > 0 && commandTime >= releaseTime && !jumpHeld;
}

static inline bot_sfj_phase_t BotSFJ_ReleasedNextPhase(bot_sfj_phase_t phase,
	int grounded, int phaseAgeMs, int releaseConfirmed)
{
	if (grounded && (phase == BOT_SFJ_PHASE_AIR || phase == BOT_SFJ_PHASE_LANDING))
		return releaseConfirmed ? BOT_SFJ_PHASE_REJUMP : BOT_SFJ_PHASE_LANDING;
	return BotSFJ_NextPhase(phase, 1, 1, 1, grounded, phaseAgeMs);
}

/*
 * Strafe keys for a turn side (side > 0 turns left, as in BotSFJ_CommandYaw):
 * a left turn holds A (W+A full beat, A alone side-only), a right turn D, as
 * humans do. With the wish direction at velocity +/- offset this keeps the view
 * within about 25 degrees of the velocity. Positive rightmove points clockwise
 * in JKA; BotSFJ_ViewYawForAxes compensates view yaw so the resulting world wish
 * direction is identical to the predictor's.
 */
static inline void BotSFJ_HalfBeatAxes(int side, int strafeOnly,
	int *forward, int *right)
{
	*forward = strafeOnly ? 0 : 127;
	*right = side < 0 ? 127 : -127;
}

static inline float BotSFJ_AxesYaw(int forward, int right)
{
	if (!forward && !right)
		return 0.0f;
	return atan2f((float)right, (float)forward) * 57.29577951308232f;
}

static inline float BotSFJ_ViewYawForAxes(float wishYaw, int forward, int right)
{
	return wishYaw + BotSFJ_AxesYaw(forward, right);
}

/*
 * With a rate-limited view, pick the keys whose world wish direction is closest
 * to the wanted one: the hop's style, the other style on the same side, or the
 * forward key alone (the W-only frames humans pass through during a side
 * switch). Backward strafes mirror every candidate. Returns the remaining error.
 */
static inline float BotSFJ_PickAxesForView(float viewYaw, float wishYaw, int side,
	int strafeOnly, int backward, int *forward, int *right)
{
	int candidateForward[3], candidateRight[3];
	float best = 1000.0f;
	int i;

	BotSFJ_HalfBeatAxes(side, strafeOnly, &candidateForward[0], &candidateRight[0]);
	BotSFJ_HalfBeatAxes(side, !strafeOnly, &candidateForward[1], &candidateRight[1]);
	candidateForward[2] = 127;
	candidateRight[2] = 0;
	for (i = 0; i < 3; i++)
	{
		float error;

		if (backward)
		{
			candidateForward[i] = -candidateForward[i];
			candidateRight[i] = -candidateRight[i];
		}
		error = fabsf(BotSFJ_AngleDelta(viewYaw - BotSFJ_AxesYaw(candidateForward[i],
			candidateRight[i]), wishYaw));
		if (error < best - 0.01f)
		{
			best = error;
			*forward = candidateForward[i];
			*right = candidateRight[i];
		}
	}
	return best;
}

#define BOT_SFJ_ESCAPE_VIEW_CONE 60.0f

static inline float BotSFJ_CircleWishYaw(float routeYaw, int side, int ageMs, int backward)
{
	const float swing = (backward ? 15.0f : 30.0f) *
		(1.0f - BotSFJ_ClampSetting((float)ageMs, 0.0f, 120.0f) / 120.0f);

	return routeYaw - (side < 0 ? -swing : swing);
}

static inline int BotSFJ_EscapeTargetInView(float viewYaw, float targetYaw)
{
	return fabsf(BotSFJ_AngleDelta(viewYaw, targetYaw)) <= BOT_SFJ_ESCAPE_VIEW_CONE;
}

/* Change inverse key style, never the validated world-space wish direction. */
static inline int BotSFJ_BackwardTargetView(float wishYaw, float targetYaw, int side,
	int strafeOnly, int *forward, int *right, float *viewYaw)
{
	int candidateForward[3], candidateRight[3];
	int i, best = -1;
	float bestError = BOT_SFJ_ESCAPE_VIEW_CONE + 0.01f;

	BotSFJ_HalfBeatAxes(side, strafeOnly, &candidateForward[0], &candidateRight[0]);
	BotSFJ_HalfBeatAxes(side, !strafeOnly, &candidateForward[1], &candidateRight[1]);
	candidateForward[2] = 127;
	candidateRight[2] = 0;
	for (i = 0; i < 3; i++)
	{
		const float yaw = BotSFJ_ViewYawForAxes(wishYaw, -candidateForward[i], -candidateRight[i]);
		const float error = fabsf(BotSFJ_AngleDelta(yaw, targetYaw));

		if (BotSFJ_EscapeTargetInView(yaw, targetYaw) && error < bestError)
		{
			best = i;
			bestError = error;
			if (!i)
				break;
		}
	}
	if (best < 0)
		return 0;
	*forward = -candidateForward[best];
	*right = -candidateRight[best];
	*viewYaw = BotSFJ_ViewYawForAxes(wishYaw, *forward, *right);
	return 1;
}

/*
 * Human-shaped strafe aim. On the ground (circle-jump preparation and the
 * takeoff command) the view flicks to the target. Early in the air it settles
 * quickly, later it tracks slowly: the humans' takeoff yaw rate has a median of
 * 80 and a 90th percentile of 200 deg/s, steady flight 40-60 and 100-120 deg/s.
 * Recorded per-hop rates raise the bot's limit; lower skill turns slower.
 * Returns the maximum yaw rate in deg/s, 0 for no limit.
 */
#define BOT_SFJ_TAKEOFF_SETTLE_MS 150

static inline float BotSFJ_ViewYawRate(float skill, int grounded, int airMs,
	float recordedTakeoffRate, float recordedAirRate)
{
	float rate;

	if (grounded)
		return 0.0f;
	if (!(skill >= 1.0f))
		skill = 1.0f;
	else if (skill > 10.0f)
		skill = 10.0f;
	if (airMs < BOT_SFJ_TAKEOFF_SETTLE_MS)
	{
		if (skill >= 10.0f)
			return 0.0f;
		rate = 200.0f + 20.0f * skill;
		if (recordedTakeoffRate > 0.0f && isfinite(recordedTakeoffRate))
		{
			const float recorded = BotSFJ_ClampSetting(recordedTakeoffRate * 1.25f, 0.0f, 720.0f);

			if (recorded > rate)
				rate = recorded;
		}
		return rate;
	}
	rate = 60.0f + 6.0f * skill;
	if (recordedAirRate > 0.0f && isfinite(recordedAirRate))
	{
		const float recorded = BotSFJ_ClampSetting(recordedAirRate * 2.0f, 0.0f, 240.0f);

		if (recorded > rate)
			rate = recorded;
	}
	return rate;
}

/*
 * Per-hop human style copied from a recorded route node (node_style in the
 * .botroute): side-only share of the airborne frames (-1 unknown), when the
 * strafe side switched as a percentage of the air time (0 none), the first
 * strafe key side (+1 D, -1 A, 0 unknown), and the takeoff/flight yaw rates.
 */
typedef struct
{
	int sideOnlyPct;
	int switchPct;
	int firstSide;
	float takeoffRate;
	float airRate;
	int airMs;
	int forceJump;		/* the human force jumped on this hop */
	float forceHeight;	/* peak height above the takeoff point */
} bot_sfj_hop_style_t;

static inline void BotSFJ_HopStyleClear(bot_sfj_hop_style_t *style)
{
	style->sideOnlyPct = -1;
	style->switchPct = 0;
	style->firstSide = 0;
	style->takeoffRate = 0.0f;
	style->airRate = 0.0f;
	style->airMs = 0;
	style->forceJump = 0;
	style->forceHeight = 0.0f;
}

/* Full beat (W+side) below 400 u/s, where humans rarely let go of forward;
 * above it a half beat alternates with side-only hops on the same side. */
#define BOT_SFJ_SIDE_ONLY_SPEED 400.0f

static inline int BotSFJ_HopStrafeOnly(float speed, int previousStrafeOnly, int recordedSideOnlyPct)
{
	if (recordedSideOnlyPct >= 0)
		return recordedSideOnlyPct >= 50 && speed >= 300.0f;
	if (speed < BOT_SFJ_SIDE_ONLY_SPEED)
		return 0;
	return !previousStrafeOnly;
}

/*
 * Turn side for the next hop: keep turning toward the route, which keeps the
 * side across landings unless the route bends the other way (humans kept it on
 * 85% of hops). Within 20 degrees the recorded human key side decides.
 */
#define BOT_SFJ_HOP_SIDE_ROUTE_DEG 20.0f

static inline int BotSFJ_HopSide(float routeYaw, float velocityX, float velocityY,
	int previousSide, int recordedFirstKeySide)
{
	const float speed = sqrtf(velocityX * velocityX + velocityY * velocityY);

	if (recordedFirstKeySide && (speed < 32.0f ||
		fabsf(BotSFJ_AngleDelta(routeYaw, atan2f(velocityY, velocityX) * 57.29577951308232f)) <=
			BOT_SFJ_HOP_SIDE_ROUTE_DEG))
		return recordedFirstKeySide > 0 ? -1 : 1;
	return BotSFJ_SelectSide(routeYaw, velocityX, velocityY, previousSide);
}

/*
 * Mid-air side switch: at the recorded point of the hop (once), or when the
 * route has drifted more than 10 degrees to the other side of the velocity.
 */
#define BOT_SFJ_AIR_SWITCH_DEG 10.0f

static inline int BotSFJ_AirSideSwitch(int side, float routeYaw, float velocityX, float velocityY,
	int airMs, int recordedAirMs, int switchPct, int *switched)
{
	const float speed = sqrtf(velocityX * velocityX + velocityY * velocityY);
	float delta;

	if (speed < 32.0f)
		return side;
	if (!*switched && switchPct > 0 && switchPct < 100 && recordedAirMs > 0 &&
		airMs * 100 >= recordedAirMs * switchPct)
	{
		*switched = 1;
		side = -side;
	}
	delta = BotSFJ_AngleDelta(routeYaw, atan2f(velocityY, velocityX) * 57.29577951308232f);
	if (side > 0 && delta < -BOT_SFJ_AIR_SWITCH_DEG)
		return -1;
	if (side < 0 && delta > BOT_SFJ_AIR_SWITCH_DEG)
		return 1;
	return side;
}

/*
 * Escape strafes face along the escape route. Unless the bot strafes backwards
 * (view toward the enemy), it must not turn its back on a visible enemy inside
 * max(bot_minstrafe, 384) units.
 */
#define BOT_SFJ_ESCAPE_FACING_GUARD 384.0f

static inline int BotSFJ_EscapeFacingAllows(int backward, int enemyVisible,
	float enemyDistance, float minStrafe)
{
	const float guard = minStrafe > BOT_SFJ_ESCAPE_FACING_GUARD ? minStrafe : BOT_SFJ_ESCAPE_FACING_GUARD;

	return backward || !enemyVisible || enemyDistance >= guard;
}

/*
 * Incremental per-hop style measurement shared by the live recorder (one call
 * per usercmd) and the export backfill of older routes (one call per stored
 * input row). Feed airborne commands only; reset at liftoff.
 */
typedef struct
{
	int airMs;
	int keyed;
	int sideOnly;
	int firstSide;
	int lastSide;
	int switchMs;
	float takeoffRate;
	float airYawSum;
	int airYawMs;
	int forceJump;
	float riseMax;
} bot_sfj_style_accum_t;

static inline void BotSFJ_StyleAccumReset(bot_sfj_style_accum_t *acc)
{
	acc->airMs = 0;
	acc->keyed = 0;
	acc->sideOnly = 0;
	acc->firstSide = 0;
	acc->lastSide = 0;
	acc->switchMs = -1;
	acc->takeoffRate = 0.0f;
	acc->airYawSum = 0.0f;
	acc->airYawMs = 0;
	acc->forceJump = 0;
	acc->riseMax = 0.0f;
}

/* Force jump (levitation active) and the highest point above the takeoff, per airborne command. */
static inline void BotSFJ_StyleAccumForce(bot_sfj_style_accum_t *acc, int levitating, float riseHeight)
{
	if (levitating)
		acc->forceJump = 1;
	if (isfinite(riseHeight) && riseHeight > acc->riseMax)
		acc->riseMax = riseHeight;
}

static inline void BotSFJ_StyleAccumAdd(bot_sfj_style_accum_t *acc, int msec,
	int forwardmove, int rightmove, float yawDelta)
{
	const int side = rightmove > 0 ? 1 : (rightmove < 0 ? -1 : 0);

	if (msec <= 0 || msec > 200 || !isfinite(yawDelta))
		return;
	if (forwardmove || rightmove)
		acc->keyed++;
	if (!forwardmove && rightmove)
		acc->sideOnly++;
	if (side)
	{
		if (!acc->firstSide)
			acc->firstSide = side;
		else if (acc->switchMs < 0 && side != acc->firstSide)
			acc->switchMs = acc->airMs;
		acc->lastSide = side;
	}
	if (acc->airMs < BOT_SFJ_TAKEOFF_SETTLE_MS)
	{
		const float rate = fabsf(yawDelta) * 1000.0f / (float)msec;

		if (rate > acc->takeoffRate)
			acc->takeoffRate = rate;
	}
	else
	{
		acc->airYawSum += fabsf(yawDelta);
		acc->airYawMs += msec;
	}
	acc->airMs += msec;
}

static inline void BotSFJ_StyleAccumFinish(const bot_sfj_style_accum_t *acc, bot_sfj_hop_style_t *style)
{
	BotSFJ_HopStyleClear(style);
	if (acc->airMs <= 0)
		return;
	style->airMs = acc->airMs;
	style->sideOnlyPct = acc->keyed ? acc->sideOnly * 100 / acc->keyed : -1;
	style->switchPct = acc->switchMs > 0 ? acc->switchMs * 100 / acc->airMs : 0;
	style->firstSide = acc->firstSide;
	style->takeoffRate = acc->takeoffRate;
	style->airRate = acc->airYawMs > 0 ? acc->airYawSum * 1000.0f / (float)acc->airYawMs : 0.0f;
	style->forceJump = acc->forceJump;
	style->forceHeight = acc->forceJump ? acc->riseMax : 0.0f;
}

/*
 * Copy a recorded force jump: keep holding jump in the air while rising until
 * the bot is as high above its takeoff as the human got. Only with
 * levitation available; a hop the human did not force jump never holds.
 */
static inline int BotSFJ_ForceJumpHold(int recordedForce, float recordedHeight, int canLevitate,
	int grounded, float riseHeight, float velocityZ)
{
	if (!recordedForce || !canLevitate || grounded || !(velocityZ > 0.0f))
		return 0;
	if (recordedHeight > 0.0f && !(riseHeight < recordedHeight))
		return 0;
	return 1;
}

static inline float BotSFJ_ForceJumpRetryHeight(float recordedHeight, int failures,
	float maximumHeight)
{
	float height = recordedHeight + (float)(failures > 4 ? 4 : (failures < 0 ? 0 : failures)) * 32.0f;

	return BotSFJ_ClampSetting(height, 0.0f, maximumHeight);
}

static inline int BotSFJ_RecordedLandingAllows(float targetX, float targetY,
	float targetZ, float landingX, float landingY, float landingZ, float tolerance)
{
	const float x = landingX - targetX;
	const float y = landingY - targetY;

	return isfinite(x) && isfinite(y) && isfinite(landingZ) &&
		tolerance >= 0.0f && x * x + y * y <= tolerance * tolerance &&
		fabsf(landingZ - targetZ) <= 64.0f;
}

/* Reject hairpins, but allow a gently curving, linked trail to be considered. */
static inline int BotSFJ_CurveSegmentAllows(float localAlignment, float totalAlignment)
{
	return isfinite(localAlignment) && isfinite(totalAlignment) &&
		localAlignment >= 0.70710678f && totalAlignment >= 0.5f;
}

static inline int BotSFJ_AdvanceCurveLinkAllows(int requiredFlags, int linked,
	int passable, float heightDelta, float baseHeightDelta, float alignment,
	float endpointProgress, float maxDrop)
{
	return !requiredFlags && linked && passable &&
		BotSFJ_HeightAllows(heightDelta, 32.0f, maxDrop) &&
		BotSFJ_HeightAllows(baseHeightDelta, 64.0f, maxDrop) &&
		alignment >= 0.5f && endpointProgress >= 0.0f;
}

/*
 * bot_strafe_loosen relaxes obstruction checks that rejected most strafe jumps
 * on stairs, ramps, dense waypoint triangles and brush entities.  Hazard,
 * fall-damage and route-endpoint checks are never relaxed.
 */
static inline float BotSFJ_CorridorLinkRise(int loosen)
{
	return loosen ? 64.0f : 32.0f;
}

static inline float BotSFJ_CorridorBaseRise(int loosen)
{
	return loosen ? 96.0f : 64.0f;
}

static inline float BotSFJ_CorridorMinLength(int loosen, int grounded)
{
	if (!grounded)
		return 64.0f;
	return loosen ? 96.0f : 160.0f;
}

/* Whether an arc contact with an entity may count as a landing or a wall slide. */
static inline int BotSFJ_ArcEntityAllows(int isWorld, int isClient, int landing, int loosen)
{
	if (isWorld)
		return 1;
	if (!loosen)
		return 0;
	/* Brush entities can be landed on; players are only bumped into. */
	return landing ? !isClient : 1;
}

static inline int BotSFJ_EndpointAllows(float startX, float startY,
	float endX, float endY, float landingX, float landingY, float endTolerance)
{
	const float dx = endX - startX;
	const float dy = endY - startY;
	const float length = sqrtf(dx * dx + dy * dy);
	const float progress = (landingX - startX) * dx + (landingY - startY) * dy;

	return length > 1.0f && progress >= 0.0f &&
		progress <= length * (length + endTolerance);
}

static inline int BotSFJ_EndpointPlaneAllows(float fromX, float fromY,
	float endX, float endY, float landingX, float landingY, float endTolerance)
{
	const float dx = endX - fromX;
	const float dy = endY - fromY;
	const float length = sqrtf(dx * dx + dy * dy);
	const float beyond = (landingX - endX) * dx + (landingY - endY) * dy;

	return length > 1.0f && beyond <= endTolerance * length;
}

static inline int BotSFJ_PolylineEndpointAllows(const float points[][3],
	int pointCount, float landingX, float landingY, float endTolerance)
{
	if (pointCount < 2 || pointCount > BOT_SFJ_MAX_CORRIDOR_POINTS)
		return 0;
	return BotSFJ_EndpointPlaneAllows(points[pointCount - 2][0], points[pointCount - 2][1],
		points[pointCount - 1][0], points[pointCount - 1][1], landingX, landingY, endTolerance);
}

/*
 * A grounded JKA jump calls PM_CheckJump once in PM_WalkMove and again from
 * PM_AirMove in the same slice.  The second call applies this levitation
 * launch velocity even when the next user command releases jump.
 */
static inline float BotSFJ_JKALaunchVelocity(int forceLevel, float height,
	int forceEligible)
{
	static const float jumpHeight[] = {32.0f, 96.0f, 192.0f, 384.0f};
	static const float jumpStrength[] = {225.0f, 420.0f, 590.0f, 840.0f};

	if (!forceEligible || forceLevel <= 0 || forceLevel > 3)
		return BOT_SFJ_JUMP_VELOCITY;
	if (height < 0.0f)
		height = 0.0f;
	return ((jumpHeight[forceLevel] - height) / jumpHeight[forceLevel]) *
		jumpStrength[forceLevel] / 10.0f + BOT_SFJ_JUMP_VELOCITY;
}

static inline float BotSFJ_HeldJumpVelocity(int forceLevel, int canLevitate,
	int hasForce, int initialHold, int recordedHold, float targetHeight,
	float height, float velocity)
{
	const int holding = initialHold ||
		(recordedHold && velocity > 0.0f && height < targetHeight);

	if (holding && canLevitate && (height <= 32.0f || hasForce))
		return BotSFJ_JKALaunchVelocity(forceLevel, height, 1);
	if ((holding || recordedHold) && velocity > BOT_SFJ_JUMP_VELOCITY)
		return BOT_SFJ_JUMP_VELOCITY;
	return velocity;
}

static inline void BotSFJ_PredictReleasedVertical(int forceLevel, int forceEligible,
	int hasForcePower, float gravity, int commandMsec, int sliceMsec,
	float *height, float *velocity)
{
	int remaining = commandMsec;
	float z = 0.0f;
	float vz = BOT_SFJ_JUMP_VELOCITY;

	if (sliceMsec < 1)
		sliceMsec = 1;
	while (remaining > 0)
	{
		const int msec = remaining < sliceMsec ? remaining : sliceMsec;
		const float seconds = (float)msec / 1000.0f;
		float nextVz;

		if (forceEligible && forceLevel > 0 && (z <= 32.0f || hasForcePower))
			vz = BotSFJ_JKALaunchVelocity(forceLevel, z, 1);
		else if (vz > BOT_SFJ_JUMP_VELOCITY)
			vz = BOT_SFJ_JUMP_VELOCITY;
		nextVz = vz - gravity * seconds;
		z += (vz + nextVz) * 0.5f * seconds;
		vz = nextVz;
		remaining -= msec;
	}
	if (height)
		*height = z;
	if (velocity)
		*velocity = vz;
}

static inline int BotSFJ_PmoveSliceMsec(int commandMsec, int fixed,
	int fixedMsec, int raceChopped)
{
	if (commandMsec < 1)
		commandMsec = 1;
	if (raceChopped && commandMsec > 8)
		return 8;
	if (fixed)
	{
		if (fixedMsec < 1)
			fixedMsec = 1;
		else if (fixedMsec > 66)
			fixedMsec = 66;
		return commandMsec < fixedMsec ? commandMsec : fixedMsec;
	}
	return commandMsec < 66 ? commandMsec : 66;
}

/*
 * Mirrors BotInputToUserCommand's horizontal projection and directional-bit
 * override order.  Override values are the resulting usercmd axis values.
 */
static inline float BotSFJ_EffectiveMovement(float dirX, float dirY,
	float inputSpeed, float viewYaw, int hasForwardOverride, int forwardOverride,
	int hasRightOverride, int rightOverride, float *worldX, float *worldY)
{
	const float radians = viewYaw * 0.017453292519943295f;
	const float c = cosf(radians);
	const float s = sinf(radians);
	const float scaledSpeed = inputSpeed * 127.0f / 400.0f;
	float forward = c * dirX + s * dirY;
	float right = s * dirX - c * dirY;
	float maximum = fabsf(forward) > fabsf(right) ? fabsf(forward) : fabsf(right);
	float length;

	if (maximum > 0.0f)
	{
		forward *= scaledSpeed / maximum;
		right *= scaledSpeed / maximum;
	}
	if (hasForwardOverride)
		forward = (float)forwardOverride;
	if (hasRightOverride)
		right = (float)rightOverride;
	*worldX = c * forward + s * right;
	*worldY = s * forward - c * right;
	length = sqrtf(*worldX * *worldX + *worldY * *worldY);
	if (length > 0.0f)
	{
		*worldX /= length;
		*worldY /= length;
	}
	return length;
}

static inline int BotSFJ_LandingWithinCorridor(float startX, float startY,
	float endX, float endY, float landingX, float landingY,
	float halfWidth, float endTolerance)
{
	const float routeX = endX - startX;
	const float routeY = endY - startY;
	const float routeLengthSquared = routeX * routeX + routeY * routeY;
	float progress;
	float lateralX;
	float lateralY;

	if (routeLengthSquared <= 1.0f)
		return 0;
	progress = ((landingX - startX) * routeX +
		(landingY - startY) * routeY) / routeLengthSquared;
	if (progress < 0.0f)
		return 0;
	if (progress > 1.0f + endTolerance / sqrtf(routeLengthSquared))
		return 0;
	lateralX = landingX - (startX + progress * routeX);
	lateralY = landingY - (startY + progress * routeY);
	return lateralX * lateralX + lateralY * lateralY <= halfWidth * halfWidth;
}

static inline int BotSFJ_UseIsConflict(int deliberateActionUse,
	int commandButtonUse, int previousUseWasRandom)
{
	return deliberateActionUse ||
		(commandButtonUse && !previousUseWasRandom);
}

/*
 * A steep wall the predicted path only grazes (horizontal motion at most ~20 degrees
 * into it) just slides the player along it in pmove, so it should not veto or abort
 * a strafe jump. Head-on walls, ceilings and floors are not glancing.
 */
#define BOT_SFJ_GLANCING_WALL_DOT 0.35f
#define BOT_SFJ_MAX_WALL_SLIDES 3

static inline int BotSFJ_IsGlancingWall(float normalX, float normalY, float normalZ,
	float velocityX, float velocityY)
{
	const float normalLength = sqrtf(normalX * normalX + normalY * normalY);
	const float speed = sqrtf(velocityX * velocityX + velocityY * velocityY);
	float into;

	if (normalZ >= 0.7f || normalZ < -0.1f || normalLength <= 0.001f || speed <= 1.0f)
		return 0;
	into = -(normalX * velocityX + normalY * velocityY) / (normalLength * speed);
	return into <= BOT_SFJ_GLANCING_WALL_DOT;
}

static inline int BotSFJ_RouteSafetyAllows(int arcClear, int hazardFree,
	int staticLanding, float landingNormalZ, float routeContinuity)
{
	return arcClear && hazardFree && staticLanding &&
		landingNormalZ >= 0.7f && routeContinuity >= 0.8f;
}

/*
 * Re-jumps inside a running chain: a landing that is safe (hazard, trigger, floor
 * slope, drop and fall-speed checks all passed) is accepted outside the corridor
 * tolerance as long as it still heads roughly along the route. Bunny hopping past
 * a ledge or along a wall beats cancelling the next hop. First takeoffs keep the
 * strict corridor rule.
 */
#define BOT_SFJ_CHAIN_MIN_CONTINUITY 0.5f

static inline int BotSFJ_ChainLandingAllows(int chainRunning, int strictAllows,
	float landingNormalZ, float routeContinuity)
{
	if (strictAllows)
		return 1;
	return chainRunning && landingNormalZ >= 0.7f &&
		routeContinuity >= BOT_SFJ_CHAIN_MIN_CONTINUITY;
}

/*
 * Give recoverable speed losses a full second before releasing ownership.
 * Speed telemetry alone does not establish that the loss came from a wall.
 */
#define BOT_SFJ_WALL_SCRAPE_GRACE_MS 1000

static inline int BotSFJ_SlowedOutShouldRelease(int slowedOut, int slowedSince, int now)
{
	if (!slowedOut || slowedSince <= 0)
		return 0;
	return now - slowedSince >= BOT_SFJ_WALL_SCRAPE_GRACE_MS;
}

/*
 * Crouching on the ground at strafe speed starts a roll and ends the hop chain
 * (strafe-track humans crouched airborne 789 frames vs 8 grounded). Navigation and
 * combat-hop crouches are skipped while grounded above ground speed; deliberate
 * roll escapes pass allowRoll.
 */
/* Navigation crouch is held off this long after strafe input while still fast. */
#define BOT_SFJ_POST_LAND_NO_CROUCH_MS 300

static inline int BotSFJ_GroundCrouchAllowed(int grounded, float horizontalSpeed,
	float groundSpeed, int allowRoll)
{
	if (allowRoll || !grounded || groundSpeed <= 0.0f)
		return 1;
	return horizontalSpeed <= groundSpeed;
}

/*
 * Wall contact: bots may hit walls directly. The arc keeps going (velocity is
 * clipped like PM_ClipVelocity) while horizontal speed after the contact stays
 * above normal ground speed; at or below it the strafe is over and the bot
 * resumes normal navigation.
 */
/* Sustained sliding can touch the same wall on every 50 ms prediction slice. */
#define BOT_SFJ_MAX_WALL_CONTACTS 64
/* A jump a wall slows out sooner than this is not worth starting. */
#define BOT_SFJ_MIN_USEFUL_FLIGHT_S 0.3f

/* Called only after hazard, lethal-fall, walkable-landing and endpoint checks. */
static inline int BotSFJ_WallSlowdownLandingAllows(int loosen, float slowedAt)
{
	return loosen || slowedAt >= BOT_SFJ_MIN_USEFUL_FLIGHT_S;
}

static inline int BotSFJ_WallContactKeepsStrafe(float speedAfterContact, float groundSpeed)
{
	return groundSpeed > 0.0f && speedAfterContact > groundSpeed;
}

/* In flight: a strafe that has been faster than ground speed and has now
 * dropped to ground speed or below (usually a wall) hands control back. */
static inline int BotSFJ_SlowedOut(float peakSpeed, float horizontalSpeed, float groundSpeed)
{
	return groundSpeed > 0.0f && peakSpeed > groundSpeed && horizontalSpeed <= groundSpeed;
}

/*
 * Human strafe-route recording (bot_strafetrack).
 * A route starts on a circle jump: grounded, forward + strafe held, view
 * turning at least BOT_SFJ_TRACK_MIN_YAW_RATE deg/s, faster than ground speed,
 * jump pressed. Each landing is a node. The route ends once the player has
 * been on the ground at or below ground speed for BOT_SFJ_TRACK_END_MS.
 */
#define BOT_SFJ_TRACK_MIN_YAW_RATE 90.0f
#define BOT_SFJ_TRACK_END_MS 150
#define BOT_SFJ_TRACK_MIN_NODES 2
#define BOT_SFJ_TRACK_MIN_DISTANCE 256.0f
#define BOT_SFJ_TRACK_MAX_NODES 32

static inline int BotSFJ_TrackCircleJumpStart(int grounded, int forwardMove, int rightMove,
	float yawRate, float horizontalSpeed, float groundSpeed, int jumpPressed)
{
	return grounded && jumpPressed && forwardMove > 0 && rightMove != 0 &&
		fabsf(yawRate) >= BOT_SFJ_TRACK_MIN_YAW_RATE &&
		groundSpeed > 0.0f && horizontalSpeed > groundSpeed;
}

static inline int BotSFJ_TrackLanded(int wasGrounded, int grounded)
{
	return !wasGrounded && grounded;
}

static inline int BotSFJ_AvoidWallrunSide(int side, int selectedBlocked, int otherBlocked)
{
	return selectedBlocked && !otherBlocked ? -side : side;
}

/* Returns the updated "slow on ground since" time (0 = not slow). */
static inline int BotSFJ_TrackSlowSince(int slowSince, int now, int grounded,
	float horizontalSpeed, float groundSpeed)
{
	if (!grounded || horizontalSpeed > groundSpeed)
		return 0;
	return slowSince ? slowSince : (now ? now : 1);
}

static inline int BotSFJ_TrackShouldEnd(int slowSince, int now)
{
	return slowSince && now - slowSince >= BOT_SFJ_TRACK_END_MS;
}

/* Export selection: candidates arrive fastest first. Keep the fastest route near each
 * start area first (starts at least `spacing` apart), then fill the remaining slots in
 * speed order, so the per-map limit covers the whole map instead of one fast corridor. */
static inline int BotSFJ_TrackSelectCoverage(const float *startsXYZ, int count, int maxPick, float spacing,
	unsigned char *picked)
{
	int i, j, n = 0;
	const float spacingSq = spacing * spacing;

	for (i = 0; i < count; i++)
		picked[i] = 0;
	for (i = 0; i < count && n < maxPick; i++)
	{
		int clear = 1;

		for (j = 0; j < i && clear; j++)
		{
			float dx, dy, dz;

			if (!picked[j])
				continue;
			dx = startsXYZ[i * 3] - startsXYZ[j * 3];
			dy = startsXYZ[i * 3 + 1] - startsXYZ[j * 3 + 1];
			dz = startsXYZ[i * 3 + 2] - startsXYZ[j * 3 + 2];
			if (dx * dx + dy * dy + dz * dz < spacingSq)
				clear = 0;
		}
		if (clear)
		{
			picked[i] = 1;
			n++;
		}
	}
	for (i = 0; i < count && n < maxPick; i++)
	{
		if (!picked[i])
		{
			picked[i] = 1;
			n++;
		}
	}
	return n;
}

static inline int BotSFJ_TrackKeep(int nodes, float pathDistance, int touchedHazard)
{
	return !touchedHazard && nodes >= BOT_SFJ_TRACK_MIN_NODES &&
		pathDistance >= BOT_SFJ_TRACK_MIN_DISTANCE;
}

/*
 * Hand-authored .botroute strafe-jump hints (start -> end).  A bot is "on" a
 * hint when it is within startRadius of the start, or alongside the route
 * (within halfWidth laterally, heightTolerance of the interpolated route
 * height) before maxProgress of its length.  *progress receives the fraction
 * travelled (0 at the start, negative just behind it).
 */
#define BOT_SFJ_ROUTE_HINT_START_RADIUS 96.0f
#define BOT_SFJ_ROUTE_HINT_HALF_WIDTH 96.0f
#define BOT_SFJ_ROUTE_HINT_HEIGHT_TOLERANCE 64.0f
#define BOT_SFJ_ROUTE_HINT_MAX_PROGRESS 0.85f
#define BOT_SFJ_ROUTE_HINT_SPEED_GATE_PROGRESS 0.5f

static inline int BotSFJ_RouteHintProgress(const float *origin, const float *start,
	const float *end, float startRadius, float halfWidth, float heightTolerance,
	float maxProgress, float *progress)
{
	const float routeX = end[0] - start[0];
	const float routeY = end[1] - start[1];
	const float routeLengthSquared = routeX * routeX + routeY * routeY;
	float routeLength;
	float t;
	float lateralX;
	float lateralY;
	float routeZ;

	if (routeLengthSquared <= 1.0f)
		return 0;
	routeLength = sqrtf(routeLengthSquared);
	t = ((origin[0] - start[0]) * routeX + (origin[1] - start[1]) * routeY) /
		routeLengthSquared;
	if (t < -startRadius / routeLength || t > maxProgress)
		return 0;
	if (t < 0.0f)
	{
		const float dx = origin[0] - start[0];
		const float dy = origin[1] - start[1];

		if (dx * dx + dy * dy > startRadius * startRadius ||
			fabsf(origin[2] - start[2]) > heightTolerance)
			return 0;
	}
	else
	{
		lateralX = origin[0] - (start[0] + t * routeX);
		lateralY = origin[1] - (start[1] + t * routeY);
		routeZ = start[2] + t * (end[2] - start[2]);
		if (lateralX * lateralX + lateralY * lateralY > halfWidth * halfWidth ||
			fabsf(origin[2] - routeZ) > heightTolerance)
			return 0;
	}
	if (progress)
		*progress = t;
	return 1;
}

/*
 * min_speed from a route hint: the first part of the route is for building
 * speed, but past the gate a grounded bot slower than min_speed must not take
 * off on the hint (it would come up short of the gap).
 */
static inline int BotSFJ_RouteHintSpeedAllows(float progress, int grounded,
	float horizontalSpeed, float minSpeed)
{
	if (minSpeed <= 0.0f || !grounded ||
		progress < BOT_SFJ_ROUTE_HINT_SPEED_GATE_PROGRESS)
		return 1;
	return horizontalSpeed >= minSpeed;
}

/*
 * Waypoint navigation anti-backtracking.  Bots keep a small ring of recently
 * reached waypoints (stored as index + 1, 0 = empty) and avoid re-picking them
 * when they re-path.  A bot whose distance to its waypoint destination has not
 * improved by BOT_NAV_PROGRESS_MIN_GAIN for BOT_NAV_PROGRESS_STALL_MS is stuck.
 */
#define BOT_NAV_RECENT_WAYPOINTS 8
#define BOT_NAV_PROGRESS_STALL_MS 3000
#define BOT_NAV_PROGRESS_MIN_GAIN 32.0f

static inline int BotNav_RecentContains(const int *ring, int count, int index)
{
	int i;

	if (!ring || index < 0)
		return 0;
	for (i = 0; i < count; i++)
	{
		if (ring[i] == index + 1)
			return 1;
	}
	return 0;
}

/*
 * Waypoint skip goals.  bot_waypointskip picks a goal that many trail points
 * ahead; the bot follows the points in between and chains a new goal when it
 * arrives.  A single failed step must not reverse the bot along its trail.
 */
#define BOT_NAV_MAX_WAYPOINT_SKIP 256
#define BOT_NAV_SKIP_SHORTCUT_STEPS 8
#define BOT_NAV_SKIP_FLIP_DELAY_MS 750
#define BOT_NAV_SKIP_MIN_PROGRESS 16.0f

static inline int BotNav_WaypointSkipCap(int skip)
{
	return skip < 0 ? 0 : (skip > BOT_NAV_MAX_WAYPOINT_SKIP ? BOT_NAV_MAX_WAYPOINT_SKIP : skip);
}

/* Trail steps still separating index from goal when walking with step (+1/-1); -1 when goal is behind. */
static inline int BotNav_StepsToGoal(int index, int goal, int step)
{
	const int steps = (goal - index) * (step < 0 ? -1 : 1);

	return steps < 0 ? -1 : steps;
}

/*
 * Score for an off-trail linked neighbour.  It must not be a point the bot just
 * left, must head roughly toward the goal and must get meaningfully closer.
 * Returns a negative value when the neighbour is not acceptable.
 */
static inline float BotNav_GoalNeighborScore(float distanceBefore, float distanceAfter,
	float alignment, int recent)
{
	const float gain = distanceBefore - distanceAfter;

	if (recent || !isfinite(gain) || !isfinite(alignment) || alignment < 0.5f ||
		gain < BOT_NAV_SKIP_MIN_PROGRESS)
		return -1.0f;
	return gain + alignment * 32.0f;
}

/* Reverse the trail direction only after failing to advance for a while. */
static inline int BotNav_SkipShouldFlip(int failSince, int now)
{
	return failSince > 0 && now - failSince >= BOT_NAV_SKIP_FLIP_DELAY_MS;
}

/* Adds index to the ring (no duplicates); returns the new head. */
static inline int BotNav_RecentPush(int *ring, int count, int head, int index)
{
	if (!ring || count <= 0 || index < 0 || BotNav_RecentContains(ring, count, index))
		return head;
	if (head < 0 || head >= count)
		head = 0;
	ring[head] = index + 1;
	return (head + 1) % count;
}

/* True when progress has improved enough to reset the stall timer. */
static inline int BotNav_ProgressImproved(float best, float distance)
{
	return best <= 0.0f || distance < best - BOT_NAV_PROGRESS_MIN_GAIN;
}

static inline int BotNav_ProgressStalled(int lastImproveTime, int now)
{
	return lastImproveTime > 0 && now - lastImproveTime >= BOT_NAV_PROGRESS_STALL_MS;
}

/*
 * Recorded-route preference.  Long, fast routes the bots have finished before
 * win over short or failing ones; nearer entries win ties.  Untried routes
 * score as an even success rate, so new recordings still get attempts.
 */
static inline float BotSFJ_TrackRouteSuccess(int attempts, int completions)
{
	if (attempts < 0)
		attempts = 0;
	if (completions < 0)
		completions = 0;
	if (completions > attempts)
		completions = attempts;
	return ((float)completions + 1.0f) / ((float)attempts + 2.0f);
}

/* After a few attempts with no completion a route's score halves every two
 * further failures, so the bots fall back to routes they actually finish. */
#define BOT_SFJ_TRACK_FAIL_GRACE 4

static inline float BotSFJ_TrackFailureDecay(int attempts, int completions)
{
	float decay;

	if (completions > 0 || attempts < BOT_SFJ_TRACK_FAIL_GRACE)
		return 1.0f;
	decay = powf(0.5f, (float)(attempts - BOT_SFJ_TRACK_FAIL_GRACE + 1) * 0.5f);
	return decay < 0.05f ? 0.05f : decay;
}

static inline float BotSFJ_TrackRouteScore(float remainingLength, float averageSpeed,
	int attempts, int completions, float entryDistance, float pickupRadius)
{
	float speedFactor, nearFactor;

	if (!(remainingLength > 0.0f))
		return 0.0f;
	speedFactor = averageSpeed / 400.0f;
	if (!(speedFactor >= 0.5f))
		speedFactor = 0.5f;
	if (speedFactor > 2.5f)
		speedFactor = 2.5f;
	nearFactor = pickupRadius > 0.0f ? 1.0f - 0.5f * entryDistance / pickupRadius : 1.0f;
	if (nearFactor < 0.5f)
		nearFactor = 0.5f;
	if (nearFactor > 1.0f)
		nearFactor = 1.0f;
	return remainingLength * speedFactor *
		(0.25f + 1.5f * BotSFJ_TrackRouteSuccess(attempts, completions)) * nearFactor *
		BotSFJ_TrackFailureDecay(attempts, completions);
}

/*
 * Export ranking multiplier applied to the human value (length x speed) of a
 * recorded route: neutral until the bots have tried it a few times, then the
 * bots' success rate and measured speed retention, with routes that never get
 * completed dropping first.
 */
static inline float BotSFJ_TrackExportFactor(int attempts, int completions, float performance)
{
	float factor;

	if (attempts < BOT_SFJ_TRACK_FAIL_GRACE)
		return 1.0f;
	if (!(performance > 0.0f) || !isfinite(performance))
		performance = 1.0f;
	factor = (0.25f + 1.5f * BotSFJ_TrackRouteSuccess(attempts, completions)) * performance *
		BotSFJ_TrackFailureDecay(attempts, completions);
	return BotSFJ_ClampSetting(factor, 0.05f, 3.0f);
}

/*
 * Mirror strafe keys without changing the world-space wish direction.
 * BackwardTargetView additionally keeps the retreat target in the view cone.
 */
static inline void BotSFJ_BackwardAxes(int *forward, int *right)
{
	*forward = -*forward;
	*right = -*right;
}

#endif
