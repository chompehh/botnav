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

#endif
