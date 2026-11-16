#ifndef G_HIT_DETECTION_H
#define G_HIT_DETECTION_H

#define G_HIT_DETECTION_MAX_SUBSTEPS 4

static inline int G_HitDetectionSubsteps(int configured)
{
	if (configured < 1)
		return 1;
	if (configured > G_HIT_DETECTION_MAX_SUBSTEPS)
		return G_HIT_DETECTION_MAX_SUBSTEPS;
	return configured;
}

static inline float G_HitDetectionAngleDelta(float from, float to)
{
	float delta = to - from;

	while (delta > 180.0f)
		delta -= 360.0f;
	while (delta < -180.0f)
		delta += 360.0f;
	return delta;
}

static inline float G_HitDetectionInterpolateAngle(float from, float to, float fraction)
{
	return from + G_HitDetectionAngleDelta(from, to) * fraction;
}

#define G_FORCE_GRIP_SWEEP_MAX_SAMPLES 64
#define G_FORCE_GRIP_SWEEP_MAX_MSEC 100
#define G_FORCE_GRIP_SWEEP_MAX_MOVE 64.0f

static inline int G_ForceGripSweepContinuous(int valid, int previousCommandTime,
	int commandTime, int previousServerTime, int serverTime,
	float distanceSquared, int sameViewFrame)
{
	return valid && sameViewFrame &&
		commandTime > previousCommandTime &&
		commandTime - previousCommandTime <= G_FORCE_GRIP_SWEEP_MAX_MSEC &&
		serverTime >= previousServerTime &&
		serverTime - previousServerTime <= G_FORCE_GRIP_SWEEP_MAX_MSEC &&
		distanceSquared >= 0.0f &&
		distanceSquared <= G_FORCE_GRIP_SWEEP_MAX_MOVE * G_FORCE_GRIP_SWEEP_MAX_MOVE;
}

static inline int G_ForceGripSweepSamples(float previousPitch, float previousYaw,
	float pitch, float yaw)
{
	float pitchDelta = G_HitDetectionAngleDelta(previousPitch, pitch);
	float yawDelta = G_HitDetectionAngleDelta(previousYaw, yaw);
	float steps;
	int samples;

	if (pitchDelta < 0.0f)
		pitchDelta = -pitchDelta;
	if (yawDelta < 0.0f)
		yawDelta = -yawDelta;
	/* At grip's 256-unit range, 3 degrees is smaller than a player's width. */
	steps = (pitchDelta + yawDelta) / 3.0f;
	samples = (int)steps;
	if ((float)samples < steps)
		samples++;
	if (samples < 1)
		samples = 1;
	if (samples > G_FORCE_GRIP_SWEEP_MAX_SAMPLES)
		samples = G_FORCE_GRIP_SWEEP_MAX_SAMPLES;
	return samples;
}

#endif
