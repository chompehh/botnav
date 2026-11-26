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
tracking if preservation fails (the console shows the SQLite cause). `resetdueltrack force` resets
anyway and gives up only the unextracted motor evidence; learned sequences, learning evidence,
preserved motor rows and strafe routes are always kept. Durable evidence survives raw tracking resets.
`resetlearningtrack` clears it and marks retained raw participants processed, so cleared learning
does not immediately reappear. `exportDuelTrack` includes `dueltrack_learned_motor.csv`.

Runtime profiles require at least five examples (or `bot_learningminsamples`, if higher),
three positive-net examples from three distinct humans, and at least 60% positive-net outcomes.
Exact/coarse context lookup retains force, mode, defense, footing, range and stance gates.
All database work happens during cache refresh, not during combat.

## Counter trials and throw continuation

`LocalBotCounterV1` aggregates bot counter trials separately from human motor
and coarse sequence evidence. Its version-3 keys separate start, active attack, return,
and transition phases and include the observed directional move, both stances, chain,
distance, closing-speed, and observed duration bands. Earlier keys remain stored but are
not consumed. Transition direction is the engine's declared chained attack; return direction
is the corresponding completed attack. The torso timer at first observation is only a
bounded remaining-time estimate, particularly if observation began late.

Grounded saber-only trials explore three entry delays per observed phase. Active-attack
delays preserve the old early prior; starts and transitions allow later entries, and returns
also try 35% and 70% of the observed remaining duration. Thirty percent of timing choices
explore, including at skill 10. Four samples are required for an option to compete on
observed net damage; among negative options the least-bad eligible one can win. Existing
blocking, recovery, engine acceptance, attack restrictions and footing safety remain
authoritative.

The aggregate separately records each timing/family/poke-wiggle variant and complete
engine-accepted three-swing recipes. Recipe outcomes bias existing legal recipe selection;
two bounded motor variants scale the existing poke and wiggle by 75% or 100%, with 20%
motor exploration. It does not replay inputs. Only requests surviving final input and footing
checks start trials. Unaccepted requests count as failures; accepted attacks collect
pairwise damage for two seconds or until the duel ends. Incomplete recipes are not scored;
third-party damage invalidates affected samples. Scores are observational trades, not
proof of interruption or causation.

The bounded cache is flushed at tracked duel completion and loaded outside the frame loop;
a failed database transaction retains pending samples for retry. A staged load preserves
pending samples on failure, and cache-load or full-cache record failures are logged.
The per-entry sample cap intentionally stops accepting further samples. `resetlearningtrack`
deletes these aggregates, while `resetdueltrack` preserves them. Counter rows are not in the
CSV export. `bot_learninghumansonly` disables bot trials. Raw commands, exact duration,
opponent identity, and human counter trials are not captured; pooled samples do not establish
independent-player confidence. Restart, in-game and replay validation remain necessary.

In flight, throw continuation reserves one further force tick rather than charging the
launch budget again. Active drain, escape reserve, lethal danger, invalid targets, return
state and traces remain authoritative. A recently advancing target-relative trajectory
or a newly completed bypass phase may extend a throw while its launch-relative health and
force lead has not worsened substantially. This is bounded below the engine timeout; a
geometric pass marker is not a confirmed hit. Trace and visibility failures still recall.

## Bounded live consumers

`bot_learning` and `bot_learningstrength` control these preferences. Skill/category bias scale
their influence; low-skill bots do not use them. With enough new human evidence:

- Throw completion can extend the built-in completion window by at most 250ms, scaled by
  preference strength. Hard threats and force safety still win; separately, observed safe
  progress can extend continuation beyond the older completion limits.
- Throw steering uses bounded learned yaw/pitch rate preferences. Every proposed aim is traced;
  an invalid preference falls back to the existing safe geometric route.
- Saber sweep rate changes by at most 20%, within the existing animation-phase controller.
  Attack acceptance, recovery, direction and movement-safety gates remain authoritative.

These are statistical timing/rate preferences, not raw key or trajectory playback. Primary
attack timing still follows engine acceptance and animation recovery. `bot_combatdebug` labels
pooled motor evidence `source=aggregate`, reports successful/total examples, and never names a
teacher for pooled data. Individual provenance remains auditable in the exported evidence.

## Learned-sequence outcome

The `net_damage` of a learned stimulus -> response -> follow-up sequence is no longer only
damage rows in the 2 s window. `BotLearn_ResourceOutcome` (`g_bot_learning.h`) adds a heal
correction (snapshot HP+AP edge minus the damage net, clamped to +/-40) and, in force duels,
0.11 x the FP edge. Snapshots come from the raw event stream before repeat collapsing. In the
latest duel tracks, damage events matched the HP+AP snapshots in saber-only duels (correlation
0.995) but missed drain heals in force duels (correlation 0.78). Each point of FP edge was
worth about 0.11 HP+AP over the next 4 s.
