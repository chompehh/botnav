#ifndef AI_TRAVERSAL_H
#define AI_TRAVERSAL_H

#include <math.h>
#include <limits.h>

typedef struct
{
	int ducked;
	int waterLevel;
	int noDamageSurface;
	int noFallingDamage;
	int knockdown;
	int siege;
	int maxDamage;
	float forceJumpStart;
	float forceJumpHeight;
	float landingHeight;
} botnav_fall_context_t;

/* PM_CrashLand's event parameter, then ClientEvents' armor-bypassing damage.
 * Do not assume a successful roll or a temporary protection at the landing. */
static inline int BotNav_PredictFallDamage(float impactSpeed,
	const botnav_fall_context_t *context)
{
	float delta;
	int eventDelta, damage, heightDifference, reduction;

	if (!context || !isfinite(impactSpeed) || impactSpeed < 0.0f ||
		!isfinite(context->forceJumpStart) || !isfinite(context->landingHeight) ||
		!isfinite(context->forceJumpHeight))
		return INT_MAX;
	if (context->noFallingDamage || context->noDamageSurface || context->waterLevel >= 3)
		return 0;
	delta = impactSpeed * impactSpeed * 0.0001;
	if (context->ducked)
		delta *= 2.0f;
	if (context->waterLevel == 2)
		delta *= 0.25f;
	else if (context->waterLevel == 1)
		delta *= 0.5f;
	if (delta <= 7.0f)
		return 0;
	eventDelta = delta > 600.0f ? 600 : (int)delta;
	if (context->forceJumpStart)
	{
		if ((int)context->landingHeight >= (int)context->forceJumpStart)
		{
			if (eventDelta > 8)
				eventDelta = 8;
		}
		else if (eventDelta > 8)
		{
			heightDifference = (int)context->forceJumpStart - (int)context->landingHeight;
			reduction = (int)(context->forceJumpHeight - heightDifference);
			if (reduction < 0)
				reduction = 0;
			eventDelta = (int)(eventDelta - reduction * 0.3);
			if (eventDelta < 8)
				eventDelta = 8;
		}
	}
	if (eventDelta <= (context->knockdown ? 14 : 44))
		return 0;
	if (context->knockdown)
		damage = context->maxDamage && eventDelta >= context->maxDamage ?
			context->maxDamage : eventDelta;
	else if (context->siege && eventDelta > 60)
		damage = eventDelta;
	else
		damage = context->maxDamage && eventDelta * 0.16 >= context->maxDamage ?
			context->maxDamage : (int)(eventDelta * 0.16);
	/* G_Damage clamps a dispatched damage event to at least one point. */
	return damage < 1 ? 1 : damage;
}

static inline float BotNav_ImpactSpeed(float verticalSpeed, float gravity, float drop)
{
	float squared;

	if (!isfinite(verticalSpeed) || !isfinite(gravity) || !isfinite(drop) || gravity <= 0.0f)
		return -1.0f;
	squared = verticalSpeed * verticalSpeed + 2.0f * gravity * drop;
	return squared >= 0.0f ? sqrtf(squared) : -1.0f;
}

static inline int BotNav_LandingAllows(int found, int solid, float normalZ,
	int hazard, int damage, int health)
{
	return found && !solid && isfinite(normalZ) && normalZ >= 0.7f &&
		!hazard && damage >= 0 && damage < health;
}

static inline int BotNav_StepAllows(int grounded, int clearUp, int clearForward,
	int foundFloor, int solid, float normalZ, float rise, float stepSize, int hazard)
{
	return grounded && clearUp && clearForward && foundFloor && !solid && !hazard &&
		isfinite(normalZ) && normalZ >= 0.7f && isfinite(rise) &&
		rise >= -stepSize && rise <= stepSize;
}

static inline int BotNav_StepSegmentSamples(float distance, float maxDistance)
{
	if (!isfinite(distance) || !isfinite(maxDistance) || distance <= 0.0f ||
		distance > maxDistance || distance > 1024.0f)
		return 0;
	return (int)ceilf(distance / 8.0f);
}

static inline int BotNav_StepEndpointAllows(float actualHeight, float targetHeight,
	float stepSize)
{
	return isfinite(actualHeight) && isfinite(targetHeight) &&
		fabsf(actualHeight - targetHeight) <= stepSize;
}

#endif
