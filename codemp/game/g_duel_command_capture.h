#ifndef G_DUEL_COMMAND_CAPTURE_H
#define G_DUEL_COMMAND_CAPTURE_H

#include <math.h>
#include <string.h>

#define BOTLEARN_SPAN_CLOSE 1
#define BOTLEARN_SPAN_OPEN 2
#define BOTLEARN_SPAN_KIND_SABER 1
#define BOTLEARN_SPAN_KIND_THROW 2
#define BOTLEARN_SPAN_KIND_FORCE 3
#define BOTLEARN_COMMAND_SOURCE_HUMAN 1
#define BOTLEARN_COMMAND_SOURCE_BOT 2
#define BOTLEARN_SPAN_OUTCOME_UNKNOWN 0
#define BOTLEARN_SPAN_OUTCOME_DAMAGE_OBSERVED 1
#define BOTLEARN_SPAN_OUTCOME_NO_DAMAGE_OBSERVED 2
#define BOTLEARN_MOTOR_ATTRIBUTION_UNKNOWN 0
#define BOTLEARN_MOTOR_ATTRIBUTION_PARTICIPANT 1
#define BOTLEARN_MOTOR_ATTRIBUTION_AGGREGATE 2

typedef struct
{
	int attribution;
	int commandSource;
	long long summaryId;
	const char *participantKey;
	int sampleCount;
	int distinctHumanParticipants;
} botlearn_motor_provenance_t;

static inline int BotLearn_MotorProvenanceValid(const botlearn_motor_provenance_t *source)
{
	if (!source || source->commandSource != BOTLEARN_COMMAND_SOURCE_HUMAN ||
		source->sampleCount <= 0)
		return 0;
	if (source->attribution == BOTLEARN_MOTOR_ATTRIBUTION_PARTICIPANT)
		return source->summaryId > 0 && source->participantKey && source->participantKey[0];
	if (source->attribution == BOTLEARN_MOTOR_ATTRIBUTION_AGGREGATE)
		return source->summaryId == 0 &&
			(!source->participantKey || !source->participantKey[0]) &&
			source->distinctHumanParticipants >= 3;
	return 0;
}

static inline const char *BotLearn_MotorAttributionName(int attribution)
{
	return attribution == BOTLEARN_MOTOR_ATTRIBUTION_PARTICIPANT ? "participant" :
		attribution == BOTLEARN_MOTOR_ATTRIBUTION_AGGREGATE ? "aggregate" : "unknown";
}

static inline int BotLearn_CommandSpanTransition(int activeKind, int observedKind)
{
	int transition = 0;
	if (activeKind > 0 && activeKind != observedKind)
		transition |= BOTLEARN_SPAN_CLOSE;
	if (observedKind > 0 && activeKind != observedKind)
		transition |= BOTLEARN_SPAN_OPEN;
	return transition;
}

/* A motor hold is evidence only when both transitions were observed in a
 * contiguous resolved-command stream. Bound measured durations and reject
 * malformed spans rather than turning an observed hold into raw input replay. */
static inline int BotLearn_MotorHoldEvidence(int startMs, int endMs,
	int commandCount, int contiguous, int releaseObserved, int commandSource)
{
	const int duration = endMs - startMs;
	if (!contiguous || !releaseObserved || commandCount < 3 ||
		commandSource != BOTLEARN_COMMAND_SOURCE_HUMAN || duration < 1 || duration > 3000)
		return 0;
	return duration;
}

static inline int BotLearn_ThrowHoldEvidence(int startMs, int endMs,
	int commandCount, int contiguous, int releaseObserved, int commandSource)
{
	const int duration = endMs - startMs;
	return contiguous && releaseObserved && commandCount >= 3 &&
		commandSource == BOTLEARN_COMMAND_SOURCE_HUMAN &&
		duration >= 1 && duration <= 6000 ? duration : 0;
}

/* Aggregate preferences require repeated complete spans and explicit aggregate
 * attribution across distinct humans. Do not invent identity for pooled data. */
static inline int BotLearn_MotorPreferenceSupported(const botlearn_motor_provenance_t *source,
	int completedHumanSpans, int totalHumanSpans)
{
	return BotLearn_MotorProvenanceValid(source) &&
		source->attribution == BOTLEARN_MOTOR_ATTRIBUTION_AGGREGATE &&
		completedHumanSpans >= 3 && totalHumanSpans >= 5 &&
		completedHumanSpans <= totalHumanSpans;
}

/* Aim evidence is eligibility-only: it authorizes statistical summaries, never
 * replay of the captured command angles. */
static inline int BotLearn_ThrowAimPreferenceSupported(const botlearn_motor_provenance_t *source,
	int completeHumanAimSpans, int totalHumanSpans)
{
	return BotLearn_MotorPreferenceSupported(source, completeHumanAimSpans, totalHumanSpans);
}

/* Reject sparse or implausibly large between-command angle jumps before
 * including them in throw-aim summaries. This never returns a replay angle. */
static inline int BotLearn_ThrowAimStepEvidence(float yawDelta, float pitchDelta,
	int intervalMs)
{
	if (!isfinite(yawDelta) || !isfinite(pitchDelta) || intervalMs < 1 || intervalMs > 100 ||
		yawDelta < -45.0f || yawDelta > 45.0f ||
		pitchDelta < -30.0f || pitchDelta > 30.0f)
		return 0;
	return 1;
}

typedef struct
{
	int time, msec, buttons, span, kind, context, stance, throwLevel;
	float yaw, pitch;
} botlearn_command_sample_t;

typedef struct
{
	int active, eligible, released, start, end, commands, span, context, stance, throwLevel;
	int netDamage, aimMsec, aimValid;
	float yawSum, pitchSum;
} botlearn_motor_hold_t;

typedef struct
{
	int holdMs, samples, successes, humans;
	float yawRate, pitchRate;
} botlearn_motor_profile_t;

static inline float BotLearn_CommandAngleDelta(float next, float previous)
{
	float delta = fmodf(next - previous + 180.0f, 360.0f);
	return (delta < 0.0f ? delta + 360.0f : delta) - 180.0f;
}

static inline void BotLearn_MotorCommand(botlearn_motor_hold_t *hold,
	const botlearn_command_sample_t *sample, const botlearn_command_sample_t *previous,
	int buttonMask, int desiredKind)
{
	const int pressed = (sample->buttons & buttonMask) != 0;
	const int contiguous = previous && sample->msec > 0 && sample->msec <= 100 &&
		sample->time > previous->time && sample->time - sample->msec == previous->time;
	float yawDelta, pitchDelta;

	if (pressed && (!previous || !(previous->buttons & buttonMask)))
	{
		memset(hold, 0, sizeof(*hold));
		hold->active = 1;
		hold->eligible = contiguous;
		hold->aimValid = 1;
		hold->start = sample->time - sample->msec;
		hold->context = sample->context;
		hold->stance = sample->stance;
		hold->throwLevel = sample->throwLevel;
	}
	if (!hold->active)
		return;
	if (!contiguous)
		hold->eligible = 0;
	if (hold->released)
		return;
	if (sample->stance != hold->stance ||
		(desiredKind == BOTLEARN_SPAN_KIND_THROW && sample->throwLevel != hold->throwLevel))
		hold->eligible = 0;
	if (sample->kind == desiredKind && sample->span > 0)
	{
		if (hold->span && hold->span != sample->span)
			hold->eligible = 0;
		hold->span = sample->span;
	}
	if (pressed)
	{
		hold->commands++;
		if (previous && contiguous)
		{
			yawDelta = BotLearn_CommandAngleDelta(sample->yaw, previous->yaw);
			pitchDelta = BotLearn_CommandAngleDelta(sample->pitch, previous->pitch);
			if (BotLearn_ThrowAimStepEvidence(yawDelta, pitchDelta, sample->msec))
			{
				hold->yawSum += fabsf(yawDelta);
				hold->pitchSum += fabsf(pitchDelta);
				hold->aimMsec += sample->msec;
			}
			else
				hold->aimValid = 0;
		}
	}
	else
	{
		hold->end = sample->time - sample->msec;
		hold->released = 1;
	}
}

static inline int BotLearn_MotorPreferenceReliable(int samples, int successes, int humans)
{
	botlearn_motor_provenance_t source;
	memset(&source, 0, sizeof(source));
	source.attribution = BOTLEARN_MOTOR_ATTRIBUTION_AGGREGATE;
	source.commandSource = BOTLEARN_COMMAND_SOURCE_HUMAN;
	source.sampleCount = samples;
	source.distinctHumanParticipants = humans;
	return BotLearn_MotorPreferenceSupported(&source, successes, samples) &&
		(float)successes / samples >= 0.6f;
}

static inline int BotLearn_MotorHoldExtension(int baseMs, int recordedMs, int permitted)
{
	if (!permitted || recordedMs <= baseMs)
		return baseMs;
	return recordedMs > baseMs + 250 ? baseMs + 250 : recordedMs;
}

static inline float BotLearn_MotorStrength(float strength, float bias, float skill)
{
	if (!isfinite(strength) || !isfinite(bias) || !isfinite(skill) ||
		strength <= 0.0f || skill <= 2.0f)
		return 0.0f;
	if (strength > 1.0f) strength = 1.0f;
	if (bias < 0.0f) bias = 0.0f;
	if (bias > 100.0f) bias = 100.0f;
	if (skill > 10.0f) skill = 10.0f;
	return strength * (0.5f + bias * 0.005f) * (skill - 2.0f) / 8.0f;
}

static inline float BotLearn_MotorRate(float base, float recorded, float strength)
{
	float change;
	if (!isfinite(base) || !isfinite(recorded) || !isfinite(strength) ||
		base <= 0.0f || recorded < 0.0f || strength <= 0.0f)
		return base;
	if (strength > 1.0f) strength = 1.0f;
	change = recorded - base;
	if (change > base * 0.2f) change = base * 0.2f;
	if (change < -base * 0.2f) change = -base * 0.2f;
	return base + change * strength;
}

#endif
