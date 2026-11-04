# Combat sequence recording

Duel capture version 14 records every resolved Pmove command during an active tracked duel,
including idle commands between actions. `span_start` and `span_end` identify saber
action/recovery, saber-flight and force-active windows. Idle samples outside a window have
`span_id=0`. A span ID is local to that participant's duel; `sequence_id` remains the broader
action-sequence key. This supplements, rather than replaces, damage events and the older
50ms `attack_sample` snapshots.

`command_time` is the processed usercmd server time; `command_msec` is the player-state command
time actually advanced by Pmove. `command_yaw` and `command_pitch` are the actual post-Pmove
player-state view angles in degrees, including engine delta-angle effects.
`buttons`, `forwardmove`, `rightmove`, and `upmove` are the resolved Pmove command.
`command_source` is `1` for human and `2` for bot. Participant identity belongs to the event's
`participant_key`/`participant_kind`. On `command_sample` only, `amount` stores the saber-throw
force level; `learned_context` stores the live safety/resource context.

`span_kind` is `1` for saber action/recovery, `2` for saber flight, and `3` for force activity.
`span_outcome` is `0`
unknown, `1` observed damage, and `2` no damage observed during the span. The latter is not a
confirmed miss. Missing event indices or capture-drop diagnostics mean the command stream may be
incomplete; do not treat a partial span as complete evidence. Recording retains the existing
8192-event budget per participant; command samples are low-priority during compaction.
Motor extraction excludes any participant with dropped events or unknown historical coverage.
Per-command samples do not add geometry rows or perform the expensive steering trace.

The Shades FF manual describes steering a thrown saber by changing aim during flight. The new
samples preserve those changes and exact button transitions; they are evidence, not replay
instructions. Historical releases do not contain this complete stream and cannot establish
uninterrupted holds from throw airtime or occasional held-button observations.

## Durable evidence and confidence

`G_ExtractLearnedMotor` reconstructs observed press-to-release holds, allowing a throw's press
to precede its flight span. It retains the latest hold per button until that associated span
ends; it does not invent a missing edge or stitch different spans together. It rejects command
gaps, stance/throw-level changes, unobserved releases, fewer than three held commands, and
implausible/nonfinite angle steps. Intervals must be at most 100ms, with at most 45 degrees yaw
and 30 degrees pitch per step. Saber holds must be 1-3000ms; throw holds 1-6000ms.

Complete examples, including unsuccessful ones, are stored in `LocalBotLearnedMotor`.
They retain session, duel start, summary, participant, span, context, stance, throw level,
hold duration, mean absolute yaw/pitch rates and confirmed saber damage dealt minus damage
received from the actual opponent within the associated span. This is a local trade measure,
not proof that the input caused the outcome. Positive-net examples contribute preferred timing
and rates; all eligible examples contribute the success denominator.

Normal cache refresh extracts up to 64 previously unprocessed version-14 duels, transactionally.
`resetdueltrack` first preserves all outstanding eligible motor evidence and refuses to delete
tracking if preservation fails. Durable evidence survives raw tracking resets.
`resetlearningtrack` clears it and marks retained raw participants processed, so cleared learning
does not immediately reappear. `exportDuelTrack` includes `dueltrack_learned_motor.csv`.

Runtime profiles require at least five examples (or `bot_learningminsamples`, if higher),
three positive-net examples from three distinct humans, and at least 60% positive-net outcomes.
Exact/coarse context lookup retains force, mode, defense, footing, range and stance gates.
All database work happens during cache refresh, not during combat.

## Bounded live consumers

`bot_learning` and `bot_learningstrength` control these preferences. Skill/category bias scale
their influence; low-skill bots do not use them. With enough new human evidence:

- Throw completion can extend the built-in completion window by at most 250ms, scaled by
  preference strength. Existing recall limits, lethal threats, force budgets and drain-lock
  rules still win.
- Throw steering uses bounded learned yaw/pitch rate preferences. Every proposed aim is traced;
  an invalid preference falls back to the existing safe geometric route.
- Saber sweep rate changes by at most 20%, within the existing animation-phase controller.
  Attack acceptance, recovery, direction and movement-safety gates remain authoritative.

These are statistical timing/rate preferences, not raw key or trajectory playback. Primary
attack timing still follows engine acceptance and animation recovery. `bot_combatdebug` labels
pooled motor evidence `source=aggregate`, reports successful/total examples, and never names a
teacher for pooled data. Individual provenance remains auditable in the exported evidence.
