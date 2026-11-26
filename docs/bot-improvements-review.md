# Bot improvements: implementation review

## Tutorial curriculum and evidence

The full current message catalog, mechanic references and lesson conditions are
in `tutorial-curriculum.md`. It replaces the earlier 75-message catalog with
150 beginner and 150 advanced messages. The emphasis is force management,
health/armor exchanges and force-plus-saber combinations, not generic saber
movement tips. Numeric examples distinguish configured mechanics from illustrative
combo arithmetic. Throw steering is explained as a crosshair laser pointer:
the blade heads toward the point the crosshair trace hits.

Personalized sequence calculations, confidence requirements and all emitted
message templates are documented in `tutorial-sequence-evidence.md`.
Measured repeated exchanges take priority after the full-force introduction;
the low-health attack reminder may interrupt it without skipping lessons. Force decreases
are not automatically called power costs, and a repeated sequence does not
prove the opponent predicted it. Missing or incomplete evidence never becomes
a diagnosis.

Completed captures need at least 10 events and 12 seconds. New identities need
three informative duels before coaching. Short opening grip-kick finishes and
incomplete captures do not trigger advice, including for established players.
One lesson is queued after a qualifying loss to a bot; busy queues are left
alone. Beginner and advanced messages are teaching, not skill labels.

### Capture cvars

| Setting | Purpose |
| --- | --- |
| `bot_dueltracking 1` | Collect eligible duel events. |
| `bot_dueltracking_geometry 2` | Full geometry/resource context; `1` records selected combat geometry. |
| `bot_tutorial 1` | Private coaching; `2` enables public coaching. |
| `bot_nochat 0` | Permit delivery. |

Command capture is automatic during tracked duels. There is no separate
command-recording cvar. `bot_learninglog` records public chat, not personalized
evidence. `bot_learning`, `bot_learningstrength`, `bot_learninghumansonly` and
`bot_learningminsamples` control bot combat learning, not tutorial capture.
`bot_forcepowers` and `g_forcePowerDisable` govern legal actions; lessons must
respect the duel rules and available powers.

### Persistence and privacy

Accounts use their existing tracking identity. Guests use the existing
`iphash:` key, not a raw IP; account and guest histories are not merged.
The unsalted 32-bit hash is pseudonymous, not anonymous or collision-resistant:
shared addresses can share history, and changing an address can lose history.
No identity is shown in coaching chat.

Tutorial history is independent of asynchronous raw duel saving. Raw duel and
combat-learning resets preserve tutorial history. New identities can import
up to 100 recent version-14-or-newer captures only when explicit diagnostics
and retained events prove completeness. Existing histories cannot safely be
backfilled without a double-counting boundary.

Arcade retains basic non-diagnostic teaching after three complete informative
rounds; interrupted and short rounds do not count. It does not infer duel combo
efficiency from unrelated arcade fights.

## Routes and flags

Grounded non-carriers following routes yield to visible opponents in all modes
within `bot_targetdistance`, capped at 2048 units (512 when unlimited).
Airborne route movement is preserved. Deliberate chase/escape travel can
continue, except for nearby flag threats.

Flag threats take priority when the bot is within 512 units of its own flag
and the opponent within 768 units of that flag. Push is selected when the flag
lies between the players; pull when the enemy lies between the bot and flag.
Power legality, aim and emergency defense remain authoritative.

A carrier at home waits for its missing flag when a live connected non-carrier
teammate can retrieve it; otherwise it pursues the flag. Non-carriers prioritize
returning that missing flag rather than all escorting the waiting carrier.
Assigned defenders guard a safe home flag.

## Saber throws

Live continuation reserves the next force tick instead of charging the launch
budget twice. Levels 2 and 3 can continue below 5200 ms when visible progress
occurred within 900 ms and launch-relative health/armor and force leads worsened
by no more than 20 and 30 respectively. Lethal danger, drain exposure, obstruction,
illegality, engine return and a stalled flight override this protection.
Geometric progress is not proof of contact.

## Counterattack learning

Counter trials classify observed wind-up, active, return and transition phases
alongside direction, both stances, duration, distance and closing bands.
Three entry delays use 30% timing exploration; two poke/wiggle motor alternatives
use 20% exploration. Four samples are required before confidence-based use.
Timing, attack-family/motor trades and completed 12-recipe outcomes persist
in `LocalBotCounterV1`; self-trials require `bot_learninghumansonly 0`.

The observed animation duration is approximate. Trials do not establish
independent-opponent confidence, and the aggregate is not included in CSV export.
Live gameplay validation remains outstanding.
