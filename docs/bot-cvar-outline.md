# Bot AI Cvar Outline

This document describes all cvars added for the NewBotAI system and how they relate to one another.

## Core Enable

| Cvar | Default | Description |
|------|---------|-------------|
| `g_newBotAI` | `0` | Master switch. 0 = legacy bot AI, 1 = NewBotAI combat logic. |
| `g_flipKick` | `0` | Engine-level flipkick enable. 1 = JA+ style, 2 = flood-protected, 3 = JK2 style. Required for flipkicks and PTK combos. |
| `bot_navigation` | `1` | Enable waypoint-based navigation pathing while NewBotAI is active. |

## Targeting

| Cvar | Default | Description |
|------|---------|-------------|
| `g_newBotAITarget` | `-1` | Target selection mode. `-1` = default (closest), `-2` = humans only, `-3` = prefer humans then bots and issue/accept force duels while otherwise fighting normally, `-4` = prefer humans then bots but only offer force duels and retreat/heal instead of attacking, `>=0` = force specific client index. |
| `bot_targetdistance` | `4096` | Max distance at which bots will acquire, keep, or keep waypoint-pursuing a target through walls. If the target exceeds this distance, the lock is dropped unless a new in-range target is found. |
| `bot_target_timeout` | `3000` | How long (ms) a bot keeps its current lost-sight combat lock before fully falling back to normal navigation when it is not using waypoint pursuit retention. |
| `g_newBotAITargetDistance` | `4096` | Declared but currently unused (superseded by `bot_targetdistance`). |
| `bot_lowhangingfruitHP` | `40` | HP threshold below which a target is considered "low-hanging fruit" (easy kill). |
| `bot_lowhangingfruitdistance` | `1024` | Preferred cvar for the max distance at which bots will prioritize low-HP targets over a nearer normal target. |
| `bot_lowhanginfruitDistance` | `1024` | Legacy alias for `bot_lowhangingfruitdistance`; spelling intentionally omits the second `g`. |

## Aggression System

The aggression bias is the central personality axis. It flows through `BotGetAggressionBias()` which combines:

```
BotGetAggressionBias = clamp(bot_aggressionbias + healthComponent*bot_healthbias + forceComponent*bot_forcebias + hateLevelAggressionBias, -1, 1)
```

| Cvar | Default | Range | Description |
|------|---------|-------|-------------|
| `bot_aggressionbias` | `0` | `[-1, 1]` | Base aggression. Positive = aggressive, negative = defensive. Feeds into nearly every combat decision. |
| `bot_healthbias` | `0` | `[-1, 1]` | How much current health shifts aggression. At 1: full HP = +1 aggression, 0 HP = -1. |
| `bot_forcebias` | `0` | `[-1, 1]` | How much force-point advantage shifts aggression. At 1: full FP advantage = +1 aggression. |
| **Per-bot `.jkb` `hatelevel`** | `3` | `1-5` | Not a cvar, but feeds into aggression as `hateLevelAggressionBias`. Higher hatelevel = more aggressive personality. |

**How aggression is consumed:**
- `BotGetAggressionWeightedBonus(bs, biasPercent, maxBonus, aggressiveOnly)` scales a bonus by `aggressionBias * biasPercent/100 * maxBonus`. Only applies when aggression is positive (for `aggressiveOnly=true`).
- Retreat thresholds: `hardRetreatHealth = 30 - aggression*25`, `softRetreatHealth = 60 - aggression*35`.
- Saber throw defense break: `preferPull = (aggression > 0)` — pull when aggressive, push when defensive.
- Lightning: fires at/above `bot_lightningdistance` for bots that know lightning and have a usable lightning level.

## Combat Behavior Biases

Chance weights are clamped to 0-100 by `BotGetChanceBiasPercent()`. Legacy attack weights can also use `BotGetAggressionWeightedBonus()`; the new saber controller uses `bot_fanbias` directly to weight coordinated horizontal techniques without gating basic attacks.

| Cvar | Default | Description |
|------|---------|-------------|
| `bot_saberthrowbias` | `0` | Chance weight for saber throw decisions. Higher = more throws. Feeds into `NewBotAI_GetSaberthrow()`. |
| `bot_gripkickbias` | `0` | Chance weight for grip-kick combo initiation. Feeds into `NewBotAI_GetGrip()`. |
| `bot_fanbias` | `0` | Chance weight for coordinated human-derived horizontal saber techniques in NewBotAI saber-only duels and general saber combat. Higher values favor L2R/R2L pressure, counters, finishing and burst-exit sequences with bounded swing-phase yaw. Diagonal/vertical attacks remain intentional alternatives. It weights techniques, not permission to attack: `0` still allows primary attacks and useful chains. Legal force tactics remain separate. |
| `bot_fan_debug` | `0` | Print selected fan package, direction, range, HP, and FP when a bot commits to a fan-pressure entry. |
| `bot_fanhold` | `220` | How long (ms) each fan gate holds exclusive left/right strafe plus attack to start the current horizontal swing. |
| `bot_firstfandwell` | `250` | Special dwell (ms) used only between the first and second swings of a fan chain. |
| `bot_fandwell` | `250` | Free-movement dwell (ms) between all later fan swings in the chain. During this dwell the bot applies a triangular yaw offset: it drifts away from center during the first half of the dwell and returns by the end. |
| `bot_fanyawspeed` | `90` | Degrees per second used to build that triangular fan-dwell yaw offset, clamped to `[-360, 360]`. Negative values mirror the offset to the opposite side of the current swing direction. |
| `bot_wobbledelay` | `120` | Delay in ms after fan-chain attack hold begins before aim wobble starts. |
| `bot_wobbleyaw` | `6` | Fan-chain wobble horizontal amplitude in yaw degrees. |
| `bot_wobblepitch` | `2` | Fan-chain wobble vertical amplitude in pitch degrees. |
| `bot_wobblespeed` | `2` | Fan-chain wobble speed in cycles per second. |
| `bot_drainbias` | `0` | Scales ordinary non-drainlock drain holds. Higher = longer opportunistic drain taps. Feeds `BotGetDrainHoldBiasMs()`. |
| `bot_drainlockbias` | `0` | Chance weight for committing to long deep-drain taps when the bot has a big FP lead. Separate from that cvar-driven behavior, bots that are behind on HP will keep heal-driven deep drainlocks until topped off unless aggression becomes extremely reckless. Feeds `NewBotAI_ShouldDrainlockDeep()`. |
| `bot_antidrainbias` | `0` | Weight bonus for attacking drain-users. When enemy can drain and is low HP, bots prioritize killing them. Feeds `NewBotAI_GetAntiDrainWeight()`. |
| `bot_lightningbias` | `0` | Chance weight for using lightning. Applies to bots that know lightning and pass the normal range/visibility/resource checks. |
| `bot_lightningdistance` | `400` | Minimum range for lightning usage. Bot must be at least this far from the enemy. |
| `bot_mistakebias` | `0` | Chance weight (0-100) for imperfect combat decisions. Gripkick escapes use `bot_gkmistakebias` instead. Saber choices account for spacing, recovery, recent trades, and opponent pressure: countering or continuing after a hit is useful only when the opening supports it. Lower skill and higher bias favor weaker timing, positioning, and combinations rather than persistent inactivity. Level 10 has no configured mistake roll, but can still select another credible legal option; good-option variety is independent of mistakes. |
| `bot_gkmistakebias` | `0` | Chance weight (0-100) for gripkick escape mistakes only: delayed or never-attempted pull-outs, push instead of pull, mis-aimed escape pulls and being rattled by a fast gripkick (scaled by how fast the gripper moves the bot). Scales with skill the same way as `bot_mistakebias`; level 10 never makes them and `0` disables them at every level. Tune it separately to keep high-level bots clean while lower levels still fumble. |
| `bot_learning` | `1` | Enables learned sequence weights. Compatible `LocalBotLearnedSequence` rows enter an in-memory table on map load and between duels; force reactions, combo follow-ups, throw decisions and saber-controller attack preferences receive bounded bonuses. `0` keeps built-in choices only (recording still happens while duel tracking is on). |
| `bot_learningstrength` | `1` | Scales the learned bonus (`0` disables it). After scaling by strength and skill the bonus is capped at ±30 weight, so data can tip a close choice but cannot override hard safety gates. |
| `bot_learninghumansonly` | `0` | `0` learns from every tracked participant, but bot-performed rows count a quarter of a human row so poor bot execution does not teach that an action loses. `1` loads only human-performed sequences. |
| `bot_learningminsamples` | `4` | Minimum recorded samples before a context/response entry influences choices. Below this the bot pools similar contexts (neighbouring HP+armor/force buckets, see Learned Sequences), then falls back to the coarse (force/range only) context, then to the baked baseline (`bot_learningbaseline`), then to built-in weights. |
| `bot_learningbaseline` | `1` | Strength (`0` disables) of the built-in baseline baked from the last exported learning (`dueltrack_learned.csv`). It applies only where live learned data has fewer than `bot_learningminsamples` samples, so a `resetlearningtrack` no longer drops bots back to unlearned choices. Pools every context of a duel mode (saber-only vs full force) per stimulus/response/follow-up, and is withheld while the bot's own defense is broken/knocked down/lost, in return recovery or airborne. Works independently of `bot_learning`. |
| `bot_learning_debug` | `0` | `1` prints the top learned responses per context to the console after each map load (only at map load or `botlearn reload`). Use the `botlearn` server command to print the table at any time. While set, it also prints each retreat wall-escape choice and `-4` duel stalemate break-ups. |

Discretionary built-in and learned preferences share existing technique categories: saber techniques
use `bot_fanbias`, throws use `bot_saberthrowbias`, drain-denial pressure also uses
`bot_antidrainbias`, PTK uses its existing aggression/HP/FP controls, and grip/drain use their
respective biases. Category values affect preference, not action legality. Basic legal actions
remain available at zero category bias. Discretionary human/learned priors retain half strength at
neutral zero category values and reach full strength at 100 before aggression scaling; learned
contributions remain capped at ±30 afterward. `bot_fanbias 0` retains basic-technique selection.
Negative final aggression favors defensive force responses and shorter unproductive saber chains,
without withholding safe finishers or interrupting necessary recovery.

Force and saber-family selection samples credible alternatives rather than always taking the
largest score. Skill 7–10 increasingly favors the strongest score but retains good-option variety
independently of mistake rolls, including at level 10. Force choices persist through a short
per-bot decision window while the same enemy and a credible legal choice remain; saber choices
respect existing swing/transition commitment. Emergency defenses and committed tactical packages
remain higher priority than discretionary variety. No new bias cvars are introduced.

The human-technique controller owns saber movement, primary inputs and technique yaw while eligible. Horizontal selection holds lateral-only input through starts and transitions; forward pressure resumes during the accepted active swing. Next-swing direction follows the accepted engine move, including engine-imposed responses, rather than flipping on an attack-button request. Yaw preparation, active counter-yaw and recovery follow animation progress instead of legacy dwell timers. Legacy fan hold/dwell/yaw controls and `bot_fan_debug` still describe the legacy path; they do not schedule the new controller's attacks. The controller also uses the existing wobble amplitude, delay and speed controls for bounded active-swing yaw/pitch wiggle, separately from its counter-yaw. Random strafe overlays do not overwrite its selected footwork. Legal force actions, knockdown recovery, navigation, and saber retrieval can temporarily take priority.

Broken-parry recovery remains active through engine lockout and nearby enemy follow-up, rather than ending solely when the ordinary exit timer expires. Follow-up pressure uses current and short-horizon predicted range; distant swings no longer prolong a successful disengagement indefinitely. It suppresses offensive inputs and aim offsets, but permits a safety-checked escape jump once the engine is ready, even while waiting for nearby follow-up to clear. Safe retreat/lateral lanes and approach footing are held through meaningful phase boundaries; outside engine-sensitive swing selection, movement remains world-relative even while the view is offset. Bounded poke counter-yaw follows the accepted horizontal swing direction, rather than an arbitrary strafe direction on vertical attacks, and is composed separately from sweep smoothing and wiggle. These control curves are conservative approximations, not measured human motor templates.

Steerable throws use launch, bypass, rear placement, cut-through and recall phases. Ready saber defense selects a bypass immediately; broken defense, knockdown, an unavailable blade or a different weapon selects direct interception. Rear placement is relative to the opponent's facing. Both side lanes are tried before raised, narrower overhead alternatives. Candidate lanes must pass both the actual 4096u, level-dependent view trace and the saber-to-endpoint route check; unsafe or stale opportunities request recall. A deliberate bypass/rear detour can remain held when flight points away from the opponent, within bounded level-dependent budgets and safety gates. Cut-through redirects through the opponent, not farther behind them. Level 2 steering follows the engine's 400ms cadence and level 3 its 100ms cadence; level 1 is not steerable. Close committed interceptions and knockdown finishes receive a protected completion window independent of aggression soft recall. Optional longer commitments require contextual range, flight progress, force reserves and an opening; they are not an unconditional timeout increase. Force legality, unsafe routes, return state, drainlock exposure and lethal-danger recalls retain priority. Validate guarded targets near obstacles and live drain/flipkick interactions in game.

### Human throw evidence

Run `python3 scripts/dueltrack_report.py . --throw-evidence` from the repository root.
This standard-library report prints its exact SQL alongside every result, preserves source batches,
excludes conflicting event identities, and pairs only adjacent throw-start/end boundaries within
the same duel, participant and opponent. `--release-dir PATH` keeps a separately downloaded capture
in its own batch. The `flights` table is constructed by the report, not a persisted game table.

The shipped repository exports produced the following airtime result:

```sql
SELECT batch, count(*) AS complete_flights, round(avg(flight_ms),1) AS mean_airtime_ms,
 min(flight_ms) AS min_ms, max(flight_ms) AS max_ms,
 sum(flight_ms >= 1500) AS flights_ge_1500ms,
 sum(flight_ms >= 3000) AS flights_ge_3000ms,
 sum(flight_ms >= 3000 AND damage > 0) AS damaging_flights_ge_3000ms
FROM flights GROUP BY 1;
```

| Batch | Complete flights | Mean ms | Min ms | Max ms | ≥1500ms | ≥3000ms | Damaging ≥3000ms |
|-------|------------------|---------|--------|--------|---------|---------|-----------------|
| repository | 900 | 836.8 | 225 | 6633 | 59 | 6 | 0 |

Airtime includes return travel and is **not** alt-input hold duration. There are no recorded
input-end events. The report separately measures sampled alt-button observations and forward input,
then joins flight windows to geometry and opponent drain spends. Held observations separated in time
do not prove an uninterrupted hold. Sampled cumulative yaw is not proof of a deliberate circle;
`MOD_MELEE` does not identify flipkicks, and simultaneous drain spending does not establish drain
denial. The observations support occasional longer commitments and forward pressure, not a universal
long-hold rule or a demonstrated spin/hold/flipkick motor recipe. The longest recorded airtime can
exceed the engine's six-second outbound limit because return travel is included.

Historical saber tuning (dueltracks2, capture v8; newer recovery/aim/throw behavior is described above):
- Fan chain (`bot_fanbias`): the pause between L2R/R2L swings shrinks with skill and fan bias and reaches zero at skill 7+. The next swing is linked on the last frames of the current swing (strafe direction flips together with the swing direction), so the engine chains it immediately, as in the long human fan sessions. The chain runs past 3s (up to 6s) while swings keep landing within 700ms. While a horizontal swing is active the aim sweeps through the swing (up to ±80° half-arc at close range, scaled by skill/fan bias, faded out by 200u) instead of a slow fixed turn.
- Footing: swing starts use predicted 2D range (the range ~150ms ahead, at the swing's damage peak). Inside 60u (70u staff), or inside ~100u while not backing off, the bot steps in and swings together, holding forward until the peak would land at ~40–55u; above ~100u it steps in first. A 10u buffer keeps the in-reach decision from flickering. Swings are blocked while the bot is backing away from 100u or more. Chained swings keep forward pressure, and a fan chain that is still landing hits links straight into the next swing and keeps driving forward.
- Movement safety ignores the current opponent's body (walls, ledges and hazards are still checked), so the opponent standing in front no longer cancels the step or the attack.
- Decisiveness: the saber choice grade is held through the current swing (at least 500ms, at most 1s) instead of being re-rolled every 300ms. When control passes between the saber controller and the force/fan code, forward intent carries over for 200ms unless the new owner is deliberately escaping. Skill 7+ bots never start a swing while backpedalling outside a deliberate escape (humans: 0%); backpedal swings remain a low-skill mistake.
- Saber footing during swings: this also covers swings the saber controller starts itself. While a skill 7+ bot's swing is starting or chaining and no defense recovery is active, the backward part of its footing is dropped and only the sideways part is kept, if that step is safe. Before this, 29% of bot swing starts under 128 units had back input, against 0.4% for humans. When the bot is attacking, chaining or countering but its swing window is closed, it steps in (beyond 45 units) and circles instead of standing still. A GOOD-grade chain that has landed a hit in the last 700 ms keeps chaining instead of resetting.
- Aim: controller swings start ~20° off the target on the swing's starting side and sweep ~50–70° per swing at 200–450°/s (scaled by skill and `bot_fanbias`). Approaching without attacking, the aim is held 10–15° off centre like humans.
- Saber throw: bot levels above 2 use throw level 3 when bot force powers and game rules allow it; levels 1–2 retain their configured level. The early controller and final input share one hold policy: protect the initial 600ms and outbound first pass (up to 1.5s), and validated guarded-target routing, from soft health/aggression recalls. Disabled powers, unsafe routes, return state, drain-lock rules and lethal threats still recall immediately. Exposed or committed targets receive direct intercepts instead of unnecessary guarded-target routing. The former learned continuation ended at 900ms; contextual completion/extension budgets now also consider range, force and target commitment. A THROW outcome bonus is still not a learned duration. The aim trails sideways motion by 5–10°, then switches ~9° once the actual saber passes so the return cuts back through the target.
- Dodging: when an enemy swing starts within ~130u, skill 7+ bots strafe or jump sideways. Backpedalling is reserved for mistake rolls at lower skill. Jump attacks are discouraged unless the enemy is knocked down or getting up.
- Combos: at skill 7+ pull→kick has no gap, throw→pull ~66ms, grip→throw ~100ms, drain→pull ~66ms. Lower skills add 100–250ms plus a `bot_mistakebias` delay. Pull→swing is replaced by pull→kick or pull→throw unless the bot is already closing in on the target.
- Reactions: enemy drain → pull (or throw); enemy grip → throw, pull or jump rather than a late push; enemy throw → counter-throw; enemy pull → throw or pull back; enemy push → drain. Saber throws are favoured when the bot's own force is ~50 or less or as part of throw→pull. Mid-range throws at high force are discouraged. Pull is preferred while the enemy has force; drain is kept for when the enemy is low.
- Skill gradient: the historical controller took the best choice at skill 7+ except for a small mistake roll disappearing at skill 10. Discretionary force and saber-family choices now retain weighted variety among credible options at every level; higher levels favor the best-supported option more strongly. Skills 1–5 still draw from mistakes observed in the data: backpedal swings, long-range swings, late pushes and jump-dodging throws.

During the temporary duel no-strafe gate, free selection boundaries deliberately fall back to an ordinary vertical attack rather than withholding all attacks. A committed start/transition keeps its actual selection; unsafe or suppressed movement must not silently select a different swing. Movement safety is rechecked against the live position and final yaw before emitting technique inputs.

## PTK (Pull-Throw-Kick) System

| Cvar | Default | Description |
|------|---------|-------------|
| `bot_ptk_fpdifference` | `20` | Force-point advantage required before PTK weight is boosted. |
| `bot_ptk_hpdifference` | `15` | Health advantage required before PTK weight is boosted. |
| `bot_ptk_aggressionbias` | `0` | Aggression bias scaling for PTK weight. Higher = PTK only when very aggressive. |

**PTK chain flow:**
1. `NewBotAI_GetPTKWeight()` computes a weight based on FP/HP advantages + aggression.
2. This weight feeds into `NewBotAI_GetPull()` — if high enough, the bot pulls.
3. After a successful pull, `NewBotAI_Flipkick()` is called (the "kick" in PTK).
4. When saber is thrown, `NewBotAI_TrySaberThrowDefenseBreak()` selects pull (aggressive) or push (defensive).

## Aim & Response

| Cvar | Default | Description |
|------|---------|-------------|
| `bot_aimspeed` | `0` | 0 = legacy aim speed from per-bot `.jkb` turnspeed_combat. 1-10 = multiplier on legacy combat turning with strict monotonic progression (higher is always faster, 10 fastest). Affects aim turn speed only. |
| `bot_delay` | `0` | Preferred name for extra response delay (ms). The configured delay is still scaled by each bot’s `.jkb` reflex, then additionally scaled by bot level (1 gets full add-on, 10 gets none). |
| `bot_delayresponsetime` | `0` | Legacy alias for `bot_delay`. |
| `bot_responseTimeDelay` | `0` | Legacy alias for `bot_delay`. |

## Movement & Strafe

| Cvar | Default | Description |
|------|---------|-------------|
| `bot_strafefrequency` | `0` | Percentage chance (0-100) per think tick to enter a random strafe. 0 = disabled. |
| `bot_strafeduration` | `50` | Duration scale (0-100) for random strafes. 50 = 80-2500ms range. |
| `bot_strafeOffset` | `0` | Legacy strafe offset. |
| `bot_strafejumps` | `1` | Enables human-shaped strafe jumping with circle-jump starts and landing chains. The strafe key matches the turn side (left turn: W+A or A, right turn: W+D or D), so the view stays within about 25° of the velocity like a human's. Each hop keeps turning toward the route (the side survives landings unless the route bends the other way), uses full beat below 400 u/s and half beat / side-only above, and may switch side mid-air at the recorded point. Aim is rate-limited by skill: a flick on the ground, a fast settle for 150 ms after takeoff (200 + 20 x skill °/s, unlimited at skill 10), then a slow tracking turn (60 + 6 x skill °/s), raised to the recorded human rates. While the view catches up, the keys whose direction is closest to the wanted wish (including W only) are pressed. Validated waypoint lookahead allows gentle curves; direct enemy corridors remain straight. In CTF, carrying a flag counts as escape and chasing an enemy flag carrier counts as chase. |
| `bot_strafejumpfrequency` | `100` | Initiation frequency scale, clamped to 1–1000 when enabled: `100` waits 1000 ms before starting a new eligible sequence, `200` waits 500 ms, `50` waits 2000 ms. `0` or negative disables strafe jumping. No per-frame random rolls; established safe chains are not delayed. |
| `bot_strafejumpwaypoints` | `128` | Strafe corridor waypoint budget, clamped to 1–512, independent of ordinary `bot_waypointskip`. Dense trails need enough points to reach the minimum corridor length; look-ahead remains distance-bounded to about 1024 units and stops at required interactions, destinations, disconnected links, excessive bends or height changes. |
| `bot_strafe_minledge` | `200` | Downward route-search/continuity bound, units, clamped 0–200; not a landing-damage limit. Confirmed walkable landings use predicted engine fall damage versus current health. Lethal or unresolved falls and explicit hazards remain forbidden. A fresh validated jump may cross a gap. |
| `bot_strafe_wallmargin` | `32` | Endpoint setback from walls for direct enemy corridors, units, clamped 0–128. Lower values allow closer approaches without shrinking the collision hull or disabling wall-slide prediction. |
| `bot_strafe_tolerance` | `80` | Horizontal waypoint/recorded-node acceptance and landing-corridor tolerance, units, clamped 16–256. Nodes mark progress rather than requiring exact touchdown; grounded height and lateral checks prevent unrelated nodes being skipped. |
| `bot_strafe_loosen` | `1` | `1` relaxes non-hazard strafe-jump checks so bots commit to chains: waypoint lookahead allows 64-unit rises per link and 96 units overall, a 96-unit minimum grounded corridor, a temporarily hidden current waypoint, step-ups onto stairs/ramps in the arc simulation, takeoff from and landing on solid brush entities, bumping players, and pauses (instead of aborting) a running chain during a roll. Navigation crouch is held off while a chain owns input and for 300 ms after strafe input at speed, so it cannot turn into a roll. Hazard, `trigger_hurt`, lava, slime, no-drop, fall-damage and route-endpoint checks are unchanged. `0` restores the strict limits (32/64-unit rises, 160-unit corridor, world-only contacts, roll aborts). Compare with `strafeJumpStats`. |
| `bot_strafes_backwards` | `1` | Boolean master switch for backwards strafe jumps (a backward circle jump with S + strafe keys, view toward the enemy) during suitable combat retreats outside saber-only play. A retreat within max(`bot_minstrafe`, 384) units of a visible enemy is always done backwards when this is enabled, and is not started at all when it is disabled, so bots no longer turn their back to circle-jump away. `0` disables them; `1` enables the existing retreat selection. The legacy percentage below still controls how often an eligible retreat uses backwards input, preserving existing configurations. |
| `bot_strafe_backward` | `50` | Legacy percentage of eligible retreat strafe jumps done backwards when `bot_strafes_backwards` is enabled. The view faces back along the escape route and the keys are mirrored, preserving the wish direction; it does not independently lock aim onto the enemy. `0` disables backwards selection, `100` selects it for every eligible retreat. Retreating bots in full-force/FFA play may also flee along recorded routes that end at least 128 units farther from the enemy. |
| `bot_footing` | `1` | Final movement smoother. When separate subsystems queue opposite forward/back or left/right moves within 150–250 ms (shorter for higher skill), the reversal becomes a stop until the hold has passed, and crouch/walk cannot be re-pressed within the same hold after release. Stopping is always immediate. Strafe-jump chains, flipkicks, gripkick, pullkick jumps, rolls, knockdowns and swing dodges are exempt. The drain/retreat lateral step now holds one trace-checked side for 1.2–2.4 s instead of flipping every 1.5 s. `0` disables. |
| `bot_footing_debug` | `0` | `1` prints each bot's deferred direction reversals per second, to measure leg twitching. |
| `bot_strafe_nodetimeout` | `5000` | Recorded-route node progress timeout, milliseconds, clamped 1000–15000. Increase for longer or slower segments; stalled routes still fall back to normal navigation. |
| `bot_onlystrafes` | `0` | Test preference: starts eligible strafe navigation without the frequency wait and also considers safe direct enemy corridors without requiring increasing separation. Requires enabled strafe jumps and positive frequency. Does not force jumping: combat, required interactions, physics, proximity, collision and hazard checks still take priority, with ordinary navigation as fallback. |
| `bot_minstrafe` | `320` | Minimum opponent distance (units) before a bot starts a chase or escape strafe jump; the jump is cancelled once the gap shrinks below 75% of this. Lower it (even `0`) to let bots chase or retreat with a strafe as soon as they land after an engagement. Navigation strafes with a visible enemy are still held back inside close-combat range (~384 units) or when the enemy is closing fast. |
| `bot_strafetrack` | `1` | Records human strafe-jump routes. `0` off, `1` record (the console prints when a route begins on a circle jump and when it ends), `2` also prints each landing with its hop style. Spectators are never recorded; bots only with `bot_strafetrack_bots`. See "Recorded strafe routes" below. |
| `bot_strafetrack_bots` | `0` | Comparison mode: `1` also records bot strafe jumps (from the bot's own takeoff) into the same tables with `bot = 1` and the player name prefixed `[bot] `, so bot and human keys/yaw can be compared command by command after `exportStrafeTrack`. Bot recordings never go into `.botroute` files and are quiet unless `bot_strafetrack 2`. |
| `bot_strafejumps_debug` | `0` | `1` prints rejection reasons to the server console; `2` also reports starts; `3+` also broadcasts diagnostics to connected clients. All are rate-limited to once per second per bot. Includes initiation waits, final-input conflicts, and waypoint-skip rejection reasons. |
| `bot_hopfrequency` | `0` | Deprecated but retained for existing configurations. Controls ambient and discretionary combat hops only; leave `0` to disable them. Emergency jumps, kicks and required map traversal are separate. Use `bot_strafejumpfrequency` for navigation strafe initiation. |
| `bot_waypointskip` | `2` | Waypoint goal distance, in trail points, clamped 0–256 (`0` = legacy one-point-at-a-time). Out of visible combat the bot picks a goal this many points ahead along its trail direction, follows the points in between (taking the farthest directly reachable one, with a clear grounded hull path and safe floor, up to 8 points at a time), and chains a new goal in the same direction on arrival. The goal stops at flagged waypoints and the destination, which are still visited. When the next trail point fails, the bot looks for a visible unvisited point closer to the goal and only reverses direction after 750 ms without progress. Re-acquiring a lost waypoint also prefers points toward the goal instead of the nearest one, which is often behind. A visible enemy clears the goal. |
| `bot_redirectcooldown` | `800` | Cooldown (ms) after a wall-triggered redirect reaction. During this window the bot keeps the chosen redirect heading instead of immediately rerolling another wall reaction, reducing tight-space spin loops and repeated reaction hopping. |

`bot_strafejumps` supports only the effective JKA/co-op-JKA physics selected by `PM_GetMovePhysics`; special/ramp/super-jump styles and force-speed/rage modifiers fall back to normal AI movement. Its conservative predictor uses live speed/gravity, the command's actual pmove slicing, JKA air acceleration, and the level 0-3 levitation launch adjustment made by the second `PM_CheckJump` call in the takeoff slice. It approximates future released-command air movement; it does not run pmove, inject velocity, or change shared physics. Close combat, attacks, deliberate use, force use/jumps, saber techniques/defense, flipkicks, knockdowns, rolls, mounted/water/use states, forced movement, special jumps, steep or moving launch surfaces, and map-required interaction/jump/duck waypoint segments retain priority. Opportunistic random-use presses are suppressed only while this controller owns the command. Airborne jump is released; a landing may immediately re-jump only after pmove consumed a released command and cleared `PMF_JUMP_HELD`. Otherwise the landing release phase remains.

Recorded human routes are considered before improvised waypoint corridors. Circle starts can be approached from up to 3,072 units away along a swept, floor-checked path, even when a legacy waypoint is closer or points elsewhere; mid-route joins still require forward alignment. Routes can cross fatal drops: a validated jump is checked along its flight path and at its landing, not against the empty ground or hazards underneath the gap. Grounded approaches, circle preparation, and cancelled-jump walking never receive that exemption. Live flight hazards and landing checks remain active both in and out of combat. Required mover interactions remain barriers. Ordinary walking treats a wall or a small ledge riser ahead as an obstacle, not a pit: the floor is probed where the bot can actually reach, so it keeps walking (and jumping obstacles) instead of freezing. Grounded route connections may climb or drop single steps. Lava, slime, nodrop, trigger_hurt and lethal drops are still refused.

Arc validation performs bounded swept player-hull integration at the command's pmove slice size until a real static, walkable contact; unresolved falls are rejected rather than vertically probing for a floor. Ceilings, dynamic blockers, lava, slime, no-drop/void areas, and instant-kill `trigger_hurt` volumes along the full arc reject the intent. Landing-corridor slack uses `bot_strafe_tolerance` instead of requiring exact waypoint landings. Cached checks are reused only for matching route/input choices; every takeoff/rejump is refreshed, and meaningful airborne heading changes require revalidation. Instant-kill triggers are cached per map instead of being queried with an area search for every sample. Selection and final command ownership both require the AI's own queued movement to be aligned with the corridor.

The lookahead permits local turns up to 45 degrees and a total departure up to
60 degrees from the first segment, not arbitrary hairpins. Both starting sides
can be tested for clearance. Compensated view yaw makes diagonal and strafe-only
commands produce the world wish direction used by prediction. Required route
endpoints and lethal terrain remain barriers. Holding-position diagnostics
separate `beStill`, waiting, and camping; these are not blanket-disabled.

The latest uploaded human strafe export has 2,114 routes, 7,507 landing nodes,
and 461,386 input frames. A total of 57,846 input frames have `force_jump = 1`,
across 312 routes; on `mp/ctf4`, 38 of 82 routes include force-jump input. The
input rows include position, velocity, jump-held and force-jump state, but not
force-speed state, wallrun timing, or collision entities/outcomes. These exports
are snapshots split across `strafetrack_inputs (3).csv` and
`strafetrack_inputs_part2.csv` through `part4.csv`; do not add the older,
fully-contained snapshots to these counts.

Reproduce the route, frame, and force-jump counts using Python's CSV reader:

```python
import csv
from pathlib import Path
root = Path("/absolute/path/to/botnav")
def load(name):
    with (root / name).open(encoding="latin1", newline="") as source:
        return list(csv.DictReader(source))
routes = load("strafetrack_routes (3).csv")
inputs = sum((load(name) for name in [
    "strafetrack_inputs (3).csv", "strafetrack_inputs_part2.csv",
    "strafetrack_inputs_part3.csv", "strafetrack_inputs_part4.csv"
]), [])
force_routes = {r["route_id"] for r in inputs if r["force_jump"] == "1"}
ctf4 = {r["id"] for r in routes if r["mapname"].lower() == "mp/ctf4"}
print(len(routes), len(inputs), len(force_routes), len(ctf4), len(ctf4 & force_routes))
```

Recorded hop styles already carry a force-jump flag and estimated jump height.
Bots keep the jump held while rising toward that recorded height, when Levitation
is available and enabled. Confirm route starts, force-jump takeoffs, landing
completion, and retained speed with `strafeJumpStats reset` / `strafeJumpStats`.
The capture does not identify individual collision normals or entities, so bot
route stalls are the available evidence for collision-prone route selection.
Future recordings should include force-speed state, wallrun state/timing, and
bot collision outcomes before those behaviors can be replayed or learned safely.

Walls: the takeoff arc uses the real 15-unit player hull and removes speed into
walls while continuing bounded wall-slide simulation (up to 64 contact slices).
Brief speed losses near ground speed no longer immediately discard an established
chain: slowdown must persist for one second before recovery or release. Loosened
recorded routes may rebuild speed after a safe landing while node progress remains
within its timeout. Recoverable predicted contacts continue acceleration and
wall-slide simulation rather than requiring a collision-free hop. A safe landing
is still required; lava, slime, no-drop and `trigger_hurt`/kill checks are unchanged.

Initiation ignores queued navigation jump/delayed-jump/walk requests (the controller supplies its own jump timing once it owns input).

`strafeJumpStats [reset]` (server console) prints how often bots started, took off, followed recorded routes, aborted and were released by a wall/slow-down, plus the most frequent rejection reasons since the last reset. Rejection reasons are only tallied after the command has been run once (or while `bot_strafejumps_debug` is on), so run `strafeJumpStats reset`, play, then `strafeJumpStats` to compare wall rejections before and after tuning.

It also reports recorded completions, stalls, interruptions, blocked takeoffs,
and circle approaches started/reached. Compare these with takeoffs per route
start and retained speed, rather than treating every rejected frame as a failed
route. Live map acceptance still requires an in-game run: approach a human start
past a nearer jump/duck waypoint, complete and chain routes, bump an ordinary wall,
then repeat near lethal terrain and required doors/lifts. Confirm ordinary
navigation resumes when no reachable start or continuation exists, and compare
retreat view/input with `bot_strafes_backwards` set to `0` and `1`.

Movement references: [OpenJK](https://github.com/JACoders/OpenJK) maintains the
Jedi Academy engine, derived from the Quake III/id Tech 3 family.
[Injx's strafing theory](https://github.com/5xp/strafe-theory/blob/master/strafing-theory.md)
explains why half-beat's diagonal input followed by the opposite strafe-only
input can produce the same acceleration vectors as ordinary alternating strafes,
with different view angles. It is not a special acceleration boost or a rule to
hold one strafe key for two jumps. Use this repository's `PM_GetMovePhysics`,
`PM_AirMove`, `PM_CheckJump`, and command slicing as the authority for JKA
prediction; do not import CPMA/race constants into ordinary combat movement.

Dense waypoint trails use `bot_strafejumpwaypoints`, not a large ordinary skip setting. During a committed strafe sequence, already-passed, unflagged waypoints may advance within the validated linked corridor so navigation does not aim backward at them. Advancement remains bounded, stops at required interactions/destinations and visible-enemy combat locks, and rechecks collision/hazards at the bot's actual flight height. It does not demand a walking floor immediately under an airborne bot or blindly advance to the hundredth point. Budget exhaustion and rejected advances appear in strafe diagnostics.

Hand-authored strafe-jump routes: put a `botroutes/<map>.botroute` file next to the map's `.wnt` waypoint file (`routes/<map>.botroute` is also read), e.g. `botroutes/mp/ffa1.botroute`. Up to 256 single-jump routes and 128 recorded routes are read once per map (1 MB max):

```
// comments are allowed
strafejump
{
    start_pos   1200.5 -450.0 128.0
    end_pos     1850.0 -450.0 128.0
    min_speed   450
}
```

A bot uses a route when it is within 96 units of `start_pos`, or alongside the route (within 96 units sideways, 64 units of the route's height) before 85% of its length, and is moving toward `end_pos`. Routes take priority over the waypoint corridor and do not need straight waypoints, but every takeoff still passes the same arc/landing/hazard checks and enemy rules. `min_speed` (optional): the first half of the route is for building speed; past halfway, a grounded bot slower than `min_speed` will not take off on the route. `src_area`/`dest_area` from AAS-based route files are accepted and ignored (these bots use waypoints, not AAS). Use `/viewpos` in-game to read coordinates. With `bot_strafejumps_debug 1`, a speed-gated route prints `route hint: below min_speed past the speed gate`.

**Chaining and escape behaviour (no cvars):**
- **Re-jumps:** once a chain is running (air, landing or re-jump phase), a landing that fails the strict corridor or route-continuity check is still accepted if the floor is walkable and the landing heads at least half along the route (`BotSFJ_ChainLandingAllows`). The first takeoff keeps the strict rule. Hazard, trigger_hurt, lava, slime, nodrop, steep-floor and fall-speed checks are never relaxed.
- **Wall scrapes:** a chain is only released for being slowed below ground speed after it has stayed slowed for 200 ms (`BOT_SFJ_WALL_SCRAPE_GRACE_MS`), so a brief scrape no longer ends it.
- **No ground crouch at strafe speed:** the combat hop and the navigation duck do not crouch on the ground while the bot moves faster than ground speed, because in JKA that starts a roll and drops the chain. Deliberate dodge and wall-escape rolls are unchanged.
- **Escapes:** a retreating bot may start an escape strafe once the enemy is 128 units away (`BOT_SFJ_ESCAPE_MIN_DISTANCE`), or at `bot_minstrafe` if that is lower. The escape is cancelled below 75% of that distance. Chase strafes still use `bot_minstrafe`.

### Recorded strafe routes (`bot_strafetrack`, `exportStrafeTrack`)

With `bot_strafetrack 1` (default), the server records human strafe jumps on every map:
- **Start:** a circle jump. On the ground holding forward plus a strafe key, view turning at least 90°/s, faster than ground speed, then jumping. The console prints `Strafe route (session #N) began: <player> circle-jumped on <map> at (x y z), speed S` and the player sees `Recording strafe route (session #N ...)`. `N` is only a per-session counter that restarts whenever the game module loads; it is not the database id.
- **While recording:** every usercmd the server processes (about every 8 ms at 125 fps, not once per server frame) stores forward/strafe/jump input, yaw, pitch, yaw change, command time and msec, position, horizontal velocity, speed, grounded state, jump-held, force-jump state, and Force Speed active state and level. The last 150 ms before the start (the takeoff flick) are kept from a short pre-roll buffer and have negative `time_ms`. Ground-to-air and air-to-ground transitions are measured at command resolution, so a landing and its hop style cannot be lost when multiple hops occur between server frames. Each landing becomes a route node (position, speed, strafe key held on that hop, air time) plus the hop's style: side-only share of the air time, side-switch point (% of the air time), first key side, takeoff and in-air yaw speed, and whether the player force jumped and how high. Each landing is the bots' next goal.
- **No lag:** recording only copies numbers into a fixed per-player buffer. When a route ends the route and node rows are saved at once and the input rows are written 512 per server frame; a new route from the same player starts once they are in. Remaining rows are flushed on disconnect and shutdown.
- **End:** the player has been on the ground at or below normal ground speed for 150 ms. The console prints the result. Routes with fewer than 2 landings or under 256 units, or that touch lava/slime/no-drop, are discarded, as are routes cut short by death, teleporting, spectating or forced movement.

Kept routes go into the local tracking database (`LocalStrafeRoute`, `LocalStrafeRouteNode`, `LocalStrafeRouteInput`, the same file as duel tracking) and are never deleted by restarts, map changes or `resetdueltrack`. The player sees `Saved strafe route #<database id>`. Map names are stored in lower case, so `MP/FFA3` and `mp/ffa3` share routes; export also matches older mixed-case rows. A route saved with no input rows prints a warning, which usually means the server runs an older game module.

All three tables and their CSV exports include nullable `speed_active` and `speed_level` columns. `speed_active` is the actual `FP_SPEED` active bit (`0` or `1`); `speed_level` is the player's configured Force Speed level (`0`–`3`), retained even while inactive. Route values describe the first hop's takeoff; node values describe the **takeoff of the hop ending at that node**, never its landing. The processed takeoff command is preferred over the end-of-frame snapshot. Input values are sampled after every processed command, including pre-roll, so Speed activation, expiry and level changes during a hop remain visible. Additive migrations leave older rows `NULL` (unknown); export never infers Speed from velocity or backfills it from landing state.

With `bot_strafetrack_autoexport 1` (default), each saved route immediately rewrites the current map's `botroutes/<map>.botroute` and reloads it, and the file is rewritten again at map end/shutdown, so bots use new routes without a manual export. `0` restores the manual `exportStrafeTrack` workflow.

`exportStrafeTrack [prefix]` (server console):
- writes `strafetrack_routes.csv`, `strafetrack_nodes.csv` and `strafetrack_inputs.csv` next to the database (split into `_partN` files at 24 MB, like `exportDuelTrack`);
- regenerates `botroutes/<map>.botroute` (lower-case name) for every map with recorded human routes. Up to 256 routes per map are chosen, starts at least 192 units apart, in order of human value (length x average speed) times the bots' own result on the loaded map: neutral until the bots have tried a route 4 times, then (0.25 + 1.5 x success rate) x speed retention, halving every two further failures while never completed. Fast routes the bots finish stay; routes they keep failing drop out first. Routes saved by older builds get their hop style measured once from their stored inputs. Only the section between `// BEGIN recorded strafe routes (exportStrafeTrack)` and `// END recorded strafe routes` is replaced, so hand-written `strafejump` blocks are kept;
- reloads the routes, so bots start using them immediately.

Recorded route block format:

```
strafejump_route
{
    start_pos   1200.5 -450.0 128.0     // circle-jump position
    min_speed   320                     // speed at the circle jump
    speed_active 1                     // optional: Force Speed active at first takeoff
    speed_level  3                     // optional: configured Speed level at first takeoff (0..3)
    node        1500.0 -460.0 128.0 410 1 640   // landing x y z, speed, strafe side (-1 left, 1 right), air ms
    node_style  60 45 1 210 55          // hop to the node above: side-only %, side-switch % of air time, first key (1 D, -1 A), takeoff yaw deg/s, air yaw deg/s
    node_force  160                     // optional: the human force jumped on that hop, peak height above takeoff
    node_speed  1 3                     // optional: Speed active and level at TAKEOFF of the hop ending above
    node        1820.0 -470.0 132.0 470 -1 610
}
```

The route-level `speed_active` / `speed_level` keywords precede all nodes. `node_speed <active> <level>` follows its `node` and any `node_style` / `node_force` lines. Unknown route fields are omitted individually; `node_speed` is omitted if either value is unknown. Missing fields mean unknown (`-1`) to the loader, not inactive or level zero. `.botroute` stores takeoff snapshots only; command-by-command transitions are in `strafetrack_inputs.csv`.

Bots copy each hop: the first key side (when the route is within 20° of the velocity), side-only vs full beat, the mid-air switch point and the yaw speeds. On a `node_force` hop a bot with levitation (and `bot_forcepowers` on, levitation not in `g_forcePowerDisable`) keeps holding jump from takeoff until it is as high as the human was. Older game modules skip the `node_style` / `node_force` lines.

Recommended test cfg (fewest interferences, most route attempts): `bot_strafejumps 1`, `bot_onlystrafes 1`, `bot_strafejumpfrequency 100`, `bot_strafe_loosen 1`, `bot_strafe_tolerance 128`, `bot_strafe_wallmargin 24`, `bot_strafejumpwaypoints 192`, `bot_strafe_nodetimeout 6000`, `bot_minstrafe 0` (navigation only) or `448` (combat), `bot_strafes_backwards 0` or `1` (backward retreat), `bot_strafetrack 1`, `bot_strafetrack_autoexport 1`, `bot_strafejumps_debug 1`, bots at skill 10 first.

A grounded bot searches for human circle-jump starts within 3,072 horizontal units
and 64 vertical units, independently of its legacy waypoint heading or the
distance to the current waypoint. The approach must pass swept clearance,
footing and hazard checks; a recording is not permission to walk across a gap.
The bot walks at full speed to the recorded start and begins its jump only once it
is lined up there: grounded, within 24 units of the start height, within 16
units of the start-to-first-landing line and at most 16 units past the start,
and moving within 25 degrees of that heading (human takeoffs were 2-12 units
from the start, about 70% within 19 degrees). A running bot may begin up to
48 units early by the distance it covers before leaving the ground. When the
bot is not already behind the start, it first walks to a clear run-up point
96 (or 48) units behind it; without a clear run-up it walks straight to the
start and only position is checked. Being near the start is never enough on
its own. A blocked approach, two seconds without 16 units of progress, or an
approach over twenty seconds falls back to ordinary navigation and looks for
another start right away. A chain into another route at 250 u/s or more keeps
its momentum and uses the earlier 48-unit entry. Nearby mid-route pickups remain available as described below.

Recorded routes take precedence over single-jump hints and waypoint corridors.
During approach and chaining, incidental waypoint jump/duck flags and navigation
input cannot redirect the bot; nearby required mover interactions, deliberate
use, combat and hard engine-forced states still take priority. While on a route,
each landing node is the movement goal and the strafe frequency wait is skipped.
Every takeoff still passes landing and hazard checks. Finishing without a
continuation, a sustained stall, a node timeout, or an overriding interaction
returns control to ordinary navigation.

With `bot_strafe_loosen 1` (default) recorded routes allow for frame-rate differences between the human and the bot:
- a landing counts as reached within 30% of the hop length (at least 80 and at least `bot_strafe_tolerance`, at most 256 units), or once the bot passes the landing's perpendicular plane within that width;
- grounded height tolerance is 96 units instead of 64;
- a bot that lands short or wide but is already closer to one of the next two landings continues from there instead of dropping the route;
- pickup radius is 192 units instead of 96, and a bot may join a route at any landing (not only at the circle-jump start) if the next landing is ahead of it;
- each landing's timeout is at least twice the human's recorded air time plus 1.5 s (capped at 15 s), so long hops are not abandoned by `bot_strafe_nodetimeout`.

The terminal endpoint keeps its existing bounded tolerance; relaxing intermediate
node matching does not remove terminal checks. Nodes are confirmed on the ground,
not merely by flying past their horizontal coordinates.

Route choice and chaining:
- At takeoff, the controller probes the engine's lateral wallrun trace; if the selected diagonal side would trigger a sideways wallrun, it tries the opposite strafe with a freshly validated arc.
- A bounded, directed route network joins recorded endpoints to circle starts
  or intermediate landings
  through swept, floor-checked connectors up to 1,024 units long. Connectivity
  is built one route per frame and shared between bots. Planning considers safe
  acquisition cost, recorded speed, completion history, stalls, and individual
  hop reliability. A longer, faster or more reliable route may be chosen over
  a nearer one: its credit is up to 384 units of extra walking, scaled by its
  completion rate, hop reliability, length (full at 2,048 units) and average
  recorded speed (full at 600 u/s). Actual approaches are checked again
  against the live map.
- Strategic destinations are distinct from the next landing: CTF flag/base or
  carrier goals, visible targets or last-seen enemy positions, and other explicit
  objectives guide route-network selection. Last-seen enemies are pursued only
  within `bot_target_timeout` and `bot_targetdistance`; a flag carrier's return
  route cannot be displaced by an incidental pickup. When no such goal exists, the bot
  retains a distant reachable route endpoint broadly in its current direction.
  Connected routes may take a local detour to make overall progress. If no circle
  can be approached directly, idle bots can use a reachable waypoint trail to
  acquire a circle start; legacy navigation is a connector/fallback, not a nearer
  waypoint veto. Close combat, recovery, camping and required interactions retain
  their ownership gates.
- Nonnegative aggression favors route pursuit beyond the strafe start/stop
  distances. Defensive aggression and existing retreat decisions favor routes
  that gain enemy separation; carrying a flag keeps the objective destination.
  Backward-facing escape and saber-only restrictions still apply.
- Bot outcomes are kept in `botroutes/<map>.botroute_stats`, matched by start
  and last landing plus execution speed context so re-exports retain history.
  The versioned format begins with `botroute_stats 4`; each route stores start x/y/z, last landing x/y/z,
  attempts, completions, stalls, interruptions, measured distance, duration in
  milliseconds, speed integral and reference-speed integral, speed context,
  traversal deaths, unsafe-arc rejections, and 32 triples of hop failures, passes,
  and successful force-jump hold height.
  Context 0 is normal movement; 1-3 are active Force Speed levels; -1 preserves
  unknown legacy history for export ranking. Legacy eight-field and version 2
  rows remain readable. Version 3 files are migrated once: hop failures, stalls
  and unsafe-arc counts (inflated by the earlier arc-prediction rejections) are
  cleared, while deaths, passes and completions are kept. Joining mid-route or chaining in counts as an attempt.
  Interrupted attempts are excluded from the completion-rate denominator.
  Approach walking is excluded from the measured distance, duration and speed
  integrals; measurement begins at the circle start.
  Measured travel speed and retention relative to recorded speed influence
  selection; unmeasured history stays neutral. An unsuccessful route retains its
  five-second per-bot retry delay. Human route hops are trusted: when the bot's
  own arc prediction finds wall or ledge contact, an off-corridor landing or a
  slide, but no hazard (trigger_hurt, lava, slime, nodrop, open drop, damaging
  landing or mover), the bot still takes the hop the human took. Grounded
  frames on a trusted hop are released only for hazards, not for ledge lips.
  A hop loses that trust after a death on it, or after three or more real
  failures to carry speed through it that outnumber its passes. Only lethal
  failures or untrusted hops receive a shared cooldown: initially 15 seconds
  for stalls/unsafe arcs or 60 seconds for lethal traversal, doubling with
  failures up to five minutes. Reliability penalties
  persist across maps/restarts, while successful prefixes remain usable; bots
  stop before a quarantined hop rather than repeating it. Falling, lava, slime
  and kill-trigger deaths are attributed to the launched hop, including a
  three-second window after airborne input release. Unrelated combat deaths are
  interruptions, not terrain failures. Results are saved every 30 seconds when
  dirty, on reload, and at map end/shutdown.
- Force Speed for navigation comes only from human routes recorded with it.
  Plain waypoint travel no longer switches Speed on. A bot taking a hop
  recorded with active Speed (at a level no higher than its own) taps Force
  Speed itself once it is within 256 units of the circle start, when
  `bot_forcepowers` is on, Speed is not disabled, and it has the force power.
  Unknown (legacy) recordings never authorize Speed. On a route recorded
  without Speed, out of sight of enemies, a bot whose Speed did not come from a
  route taps it off once (after the engine's 1.5 s deactivation delay). Combat
  Speed use is unchanged. Normal or legacy-unknown routes may still be tried
  under Speed after validating their actual trajectory. Matching recordings
  receive preference. The 128-route export budget is shared between normal,
  active Speed levels, and unknown legacy recordings, with spatial coverage
  selected within each group so fast recordings cannot crowd out normal starts.
  Speed changes invalidate the arc cache and split measured
  execution history into the appropriate speed context.
- Recorded force-jump hops use a held-jump trajectory forecast, with up to four
  seconds of prediction and force-drain accounting. If the recorded hold predicts
  a short/unsafe landing, bounded longer holds are tested before launch and while
  still rising with jump held. Only a hold predicting a safe landing is emitted;
  it cannot restart levitation after jump release. Failed hops bias later probes
  toward up to 128 extra units of hold height, capped by the bot's levitation
  level; successful hold heights are remembered per hop and speed context.
  Additional height never bypasses ceiling, flight-hazard, terminal or
  landing checks.

### Waypoint navigation

- Bots remember their last 8 reached waypoints and avoid re-picking them when they re-path, unless no other waypoint is visible. This stops walking back to points already passed.
- When a drop or ledge is detected ahead, the bot switches to the next waypoint in its travel direction if that one is visible and safe, instead of dropping its waypoint and re-pathing to the nearest (often earlier) point.
- Walkable stairs and ramps use swept engine-step probes rather than unnecessary legacy jumps or repaths. Grounded strafe corridors may follow those validated steps when a straight hull sweep clips a riser; headroom, static walkable support, endpoint connection, hazards and the final jump arc are still checked.
- Confirmed navigation and strafe landings compare predicted engine fall damage against current health. Prediction includes momentum, gravity, crouch, landing water, Force-jump reduction and server damage rules, but assumes neither a successful roll nor temporary protection. Missing floors and lethal falls remain forbidden.
- A navigation jump over an obstacle clears any crouch, and no obstacle crouch starts while a navigation jump is active, so bots no longer crouch-jump at obstacles.
- Out of combat, a bot whose distance to its waypoint destination has not improved by 32 units for 3 s re-paths to an unvisited waypoint without reversing its path direction.

Live-map validation checklist:
- Test long, level waypoint corridors at low and high accumulated speed and with variable server frame times.
- Confirm jump is released during flight and for one command after landing; verify no force jump or flipkick occurs.
- Place doors, movers, players, low ceilings, walls, lethal ledges, lava/slime, death triggers, and voids along or below the predicted arc and confirm immediate abort. Confirmed nonlethal landings may continue; repeat at low health to check rejection.
- Exercise navigation, retreat, and increasing-separation pursuit; confirm direct close combat and an enemy off the routed corridor never start a jump.
- Toggle `bot_strafejumps` off during each phase and confirm ordinary movement/jumps resume without yaw or input ownership.
- Compare `bot_strafejumpfrequency` at 0, 50, 100 and 200, checking initiation waits and uninterrupted safe chains; repeat with `bot_onlystrafes 1`.
- Keep `bot_hopfrequency 0` and confirm ambient hops stay disabled. With `bot_strafejumps_debug 3`, inspect client-visible rejections while comparing `bot_waypointskip` at 0, 2 and 4 on linear and branched trails, including flagged waypoints, gaps and required destinations.
- On densely spaced trails, compare `bot_strafejumpwaypoints` at 16, 128 and 256. Verify corridor initiation and forward waypoint progression during flight without backtracking, and confirm flagged interactions, destinations and unsafe terrain remain barriers rather than being blindly skipped.

Human strafe-jump demonstrations are captured with `bot_strafetrack` and exported with `exportStrafeTrack` (see above); dueltrack CSVs do not include strafe routes.

### Saber technique adjustments from duel data

Figures come from the exported dueltrack and learned CSVs (DuckDB over `dueltrack_events*.csv`, partitioned by `parent_id`). Bots won 7% of saber-only duels against humans (11/156) and 30% of full-force duels (109/367).

- **Pursuit jump.** When the opponent moves away at more than 100 u/s at 96–260 units, the technique controller may jump forward once (20% per flight in saber-only, 8% in full force, adjusted by the learned `idle>jump>swing` bonus within ±`BOTLEARN_BONUS_CAP`/3, then a 2.2–4.2 s cooldown) and swing on landing within 0.9 s. Humans followed 21.4% of jumps with a swing within 0.9 s; bots 0.6%. The landing swing still needs a legal attack, and airborne swing starts stay gated.
- **Longer bursts.** A burst only ends early on a soft exposure (health behind, no landed hit) after 2 swings, plus one for skill 6+ and one when the learned `swing>swing` preference is positive (max 4). Hard exposure (enemy attacking, recently hurt, blocked) still ends it after 2.
- **Low-health escapes (full force only).** At 40 HP+armor or less, retreats choose (held 0.7–1.3 s) between a back-and-side diagonal (45), an eager escape jump (30, ± half the learned `swing>hop` bonus) and a plain back-pedal (25). Bots back-pedalled 81% of the time at low HP (humans 23%).
- **Fewer reflexive jumps in full force.** The escape jump is skipped while recently hurt in full-force play unless the jump escape was chosen. Bots jumped after 63% of hits (humans 29%).
- **Stall breaker.** Grounded, slower than 40 u/s within 150 units for 600 ms and not swinging or locked: the bot swings if in reach (half the time) or takes a 450 ms traced side step.
- **Hallways and corners.** Saber footwork traces no longer reject a direction because of a wall or prop in the way; only the floor up to the contact is checked (hazards, lava, slime, drops). Solid non-player brush entities count as floor. Bots took 17.4% of hits standing still against 8.1% for humans.

Saber-only adjustments from the newest sessions (`dueltrack_* (2).csv`: 32 saber-only duels, bots won 2; 18 full-force duels, bots won 9). Swings are the horizontal L2R/R2L moves; a chain link is an `attack_chain` row within 400 ms of the previous `swing_end`; range comes from the geometry row with the same `parent_id`/`event_index`:
- **Low health does not change saber-only play.** Saber-only bots cannot heal. Below 40 HP their damage per second fell from 3.79 to 0.13 and they won 0 of 20 such duels. In saber-only play the health-based retreats, the low-HP escape styles, the critical-health safety exit and the 70 HP fan-chain cutoff no longer apply. The bot only exits when a live enemy chain is running, and fan bias is scaled by the HP difference no lower than 0.75x (was 0.35x).
- **Responding to an incoming swing within 120 units (saber-only).** From 90–120 units the bot backs off aside and back (hit 2% when retreating vs 24% holding). From 60–90 units it steps in (60%) or holds and blocks (step-in hit 4%, retreat 24%). Under 60 units it steps in (70%) or holds (retreat hit 51%). Counter-swinging into a live swing was hit 38.5% of the time, so saber-only bots punish only within 300 ms after the enemy swing ends, within 110 units. While the enemy is linking swings (two or more within 400 ms) and the bot is 120–170 units away, it sidesteps instead of charging in.
- **Yellow and staff sweep chains (both modes).** Humans linked 70% of horizontal swings (bots 7.5%). Yellow chains ran 2–4 swings, staff chains 3–6, alternating L2R/R2L about 300 ms apart. A chained yellow sweep landed at least once in 46% of chains (isolated 25%); staff 37% (isolated 14%). Fan bias is now 1.4x in yellow (`SS_MEDIUM`) and staff, and a chain plans 2–4 (yellow) or 3–6 (staff) swings. It links without the random link roll until it reaches that length, unless the enemy is beyond 120 units.

## Retreat Wall Escapes

When a retreating bot (hard/soft retreat, or backing off from a fast incoming enemy) is about to back into a wall, a probe along its actual movement direction spots the wall shortly before contact. The bot then commits (~0.9 s) to one of: a vertical wallrun, flipkick then drain, drain then flipkick, a roll around the opponent, or a hop around/over the opponent. Each option is only offered when it is possible right now (flipkick enabled and in range, drain known and allowed, open space beside or above the enemy, levitation for the wallrun). The choice is uniformly random at first and then weighted by bot learning (`wallrun`, `roll`, `hop` learning tokens plus the existing kick/drain sequences); during tracked duels each choice is recorded as a decision so its outcome feeds the learned weights.

## Gripkick Tuning

| Cvar | Default | Description |
|------|---------|-------------|
| `bot_gripkickdwell` | `100` | Percent scaling of grip phase durations: the aim-down/hold phases (holding the target gripped with forward-only movement before the flipkick approach, and the dwell after a failed kick attempt) run at 1.5x this scaling, while the upward jerk phases run at 1x. 100 = default timings. Higher = longer dwell per phase, lower = faster cycling. Each upward jerk also rolls its own random pitch between 45 and 80 degrees so the swing height of the gripped target varies jerk to jerk. |
| `bot_bully` | `0` | 1 = bully mode: while gripping a knocked-down target that is falling fast enough to splat, release grip early to let them splat; and when the bot's back is to lava or a lethal fall, release grip after the first jerk phase begins so the jerk hurls the target off the edge. 0 (default) = keep holding the grip instead of releasing for the splat/hazard. |

## Duel & FFA Exploration (`g_newBotAITarget` -3 / -4)

| Cvar | Default | Description |
|------|---------|-------------|
| `bot_duelcountmax` | `3` | Completed duels before a -3/-4 bot returns to FFA to find a new opponent: duel issuing/acceptance is suppressed while the bot explores. |
| `bot_ffaexploretime` | `180000` | How long (ms) a -4 bot keeps exploring for a new non-dueling opponent once the post-duel FFA window begins (default 3 minutes), before it may duel again. |
| `bot_perfwarn` | `0` | Performance diagnostics: when `> 0`, logs any game frame, bot AI frame, or duel-save/learning-cache step that takes at least this many milliseconds. Use it to confirm that duel-end saves no longer cause lag spikes. |

Notes:
- `-4` bots fight normally once a duel actually starts (`duelInProgress`); the force-duel-only approach only applies while finding/challenging.
- `-3` bots target the true nearest enemy (no health weighting), like `-1`, while still issuing/accepting duels.
- Bots throttle self-initiated duel requests to one every 7 seconds by default. While humans are active, bot-vs-bot offers in `-3`/`-4` are throttled to once every 2 minutes so human duel opportunities are not crowded out; with no humans around the normal 7 second cooldown applies so bot force-duel Elo keeps building.
- After offering a duel, a bot stays passive for 3 seconds: it stands still, faces the target, and does not attack, so the offer can be accepted. It may drain if it was just hurt or is low on health; the hold ends early if the duel starts or the bot takes heavy damage.
- Human duel offers are accepted immediately (ahead of the reaction delay, request throttle, FFA explore window, and the bot's own pending offer), using the engine's full 4 second acceptance window; the bot walks into the 256-unit challenge range first if needed.
- Each bot remembers the last duel type (saber or force) a human offered it and uses that type for its own offers to humans until a human offers a different one. Bot-vs-bot offers and acceptances are always force duels.
- `-4` bots roam via waypoints during the explore window instead of walking up to the nearest bot and idling. If two bots stand near each other for ~4 seconds without a duel or an offer, they blacklist each other for 20 seconds and roam apart; bots that neither side can currently challenge are skipped when picking a target, so newcomers don't join idle groups.
- After a bot-vs-bot duel ends, both bots temporarily blacklist each other so they search for a new opponent instead of immediately rematching.
- Recorded duels are saved in small steps over several frames after the duel ends (summary, events in chunks, aggregates, learning), on one persistent database connection in WAL mode, so the end of a duel no longer causes a lag spike. Pending saves are flushed on map change or shutdown. The bot-learning cache reload is also deferred to quiet frames.
- Persistence acquires its write lock before each step; transient BUSY/LOCKED errors retain queued saves and retry later. Queue-overflow draining is bounded (128 steps / 10ms, apart from an individual database call), rather than spinning on a blocked writer. If all eight slots remain occupied, the new incoming duel is explicitly rejected and logged; existing queued saves are not overwritten. Shutdown flushing has a one-second budget; remaining unsaved recordings are reported as lost and their snapshots freed, while committed learning is unchanged. Avoid prolonged external writer locks and inspect these warnings before resetting or shutting down.
- Ranked bot-vs-bot ELO progression is limited to 5 ranked duels per bot level per UTC day/session bucket; extra bot-vs-bot duels still run but are logged unranked.

## Miscellaneous

| Cvar | Default | Description |
|------|---------|-------------|
| `bot_nochat` | `0` | Disable bot chat. |
| `bot_tutorial` | `0` | Enable concise trainer tells from bots after tracked duels; at `1` tells are private, at `2+` tells may broadcast publicly. Messages are capped to 3 per burst with a 7s cooldown between sends, and unregistered reminders are sent once per session. |
| `bot_learninglog` | `0` | Opt-in recording of accepted public chat in the existing events export, independently of tutorial chat and learned weight consumption. Private tells, team, clan and admin chat are excluded. Human explanations remain untrusted annotations, not automatic behavior rules. |
| `bot_dueltracking` | `0` | Record duel summaries, per-player force-spend stats, and sequential duel events for human-vs-human and human-vs-bot duels only (bot-vs-bot excluded). |
| `bot_dueltracking_geometry` | `0` | Optional geometry capture for tracked duel events. `1` logs only key spatial events (damage/range/air/knockdown), `2` logs all tracked events. Off by default to keep overhead low. |
| `bot_yawswitch` | `10` | Legacy/unused cvar; current wall-escape yaw behavior is hardcoded in `ai_main.c`. |
| `bot_forcepowers` | `1` | Enable bots using force powers. |
| `bot_forgimmick` | `0` | Force gimmick mode. |
| `bot_honorableduelacceptance` | `0` | Accept duel challenges honorably. |
| `bot_pvstype` | `1` | PVS check type for enemy scanning. |
| `bot_maxbots` | `0` | Max bots allowed (0 = unlimited). |
| `bot_team` | `0` | Force bot team. |
| `g_flipKickDamageScale` | `1` | Scale flipkick damage. |
| `g_movementStyle` | `1` | Movement physics style (affects bot movement code paths). |

Tracked duel data access:
- Stored in `fs_homepath/fs_game/dueltracks.db` (fallback: `fs_game/dueltracks.db`); Elo and account data remain in `data.db`.
- Database paths are printed to the server console on map load. `exportDuelTrack` reports exported CSV filenames and row counts without printing their filesystem paths. If a database cannot be opened or created, the failure is printed with the sqlite error instead of failing silently.
- Tracked rows from older builds (`dueltrack.db`, or `data.db` for the oldest builds, including the old `LocalArcadeTrack*` arcade tables) are not imported automatically. On map load the server prints how many legacy sessions it found. Run server console command `importDuelTrack` once to merge them into `dueltracks.db`: legacy files are only read, duels already present are skipped, aggregates are added only when every legacy duel was new, and each source is imported at most once (until `resetdueltrack` is run).
- Duel Elo for bots is one rating per bot level: `botlvl1` ... `botlvl10`, shared by every bot played at that level. Older per-bot keys (`<bot> [bot LN]`) in `data.db` are folded into `botlvlN` on map load.
- Use server console command `resetdueltrack` to clear duel tracking summaries, participants, events, geometry, and aggregates from `dueltracks.db`. It refuses while a duel is active. Pending saves are completed first; a busy/failed save prevents the reset. The command reports the path and number of rows cleared and clears stale runtime tracking. It also forgets which legacy sources were imported, so `importDuelTrack` can bring them back. Learned aggregates and individual learning evidence are retained; accounts, Elo duel history, and legacy databases are not reset.
- Use the separate server command `resetlearningtrack` to clear learned aggregates and evidence and refresh the runtime cache without deleting duel recordings, accounts or Elo. It refuses while a duel is active and flushes queued saves before resetting. These are commands, not persistent destructive cvars. Export/back up first.
- Use server console command `exportDuelTrack [prefix]` to export timestamped CSV files for:
  - `LocalDuelTrackSummary`
  - `LocalDuelTrackParticipant`
  - `LocalDuelTrackEvent` (includes sequence id/label, outcome-ranked quality, buttons, movement commands, saber stance and grounded state, saber moves, yaw delta, self/opponent HP/AP/FP snapshots, swing side, pre-swing strafe direction, radial movement intent/speed, measured yaw sweep, attack elapsed time, saber-throw yaw offset, and damage source/attacker identity)
  - `LocalDuelTrackGeometry` (when `bot_dueltracking_geometry` is enabled)
  - `LocalDuelTrackAggregate`
  - `LocalBotLearnedSequence` (`learned` CSV: source kind, skill band, context key, stimulus, response, follow-ups, samples, wins, net damage, average response time)
  - `LocalBotLearnedEvidence` (`learned_evidence` CSV: individual extracted examples with source duel, participant, action index, outcome window and recording provenance)
- Every exported CSV is capped below 25MB so it can be uploaded to GitHub. A larger export is split into `<name>.csv`, `<name>_part2.csv`, `<name>_part3.csv`, ...; each part repeats the header row and whole rows are never split across files. Leftover parts from an earlier, larger export are removed.
- `capture_version` and `capture_revision` identify the recording implementation and build revision for each session, also included in event and participant exports. Imported/historical sessions without provenance remain `0`/blank; an export schema version alone does not establish recording provenance.
- `input_start` records an attack-button request, not a successful swing; historical `attack_start` records retain their original meaning. `swing_start` records an accepted saber attack animation, and `attack_chain` marks accepted continuations through legal transitions, including same-direction links, already counted by `swing_start`. `swing_end` preserves measured signed yaw sweep, and `swing_damage` records confirmed in-hand saber damage. Count distinct swings containing hits, not raw hit events. `damage_dealt`/`damage` record actual attributed resource loss; `damage_source` is the engine means-of-death value (`MOD_*`), and `damage_attacker_key` identifies the actual attacker. Environmental damage must not be credited as opponent saber damage.
- New event diagnostics distinguish controller enablement from actual ownership: `controller_enabled` records `g_newBotAI`, `controller_candidate` identifies a bot routed toward the technique controller, and `controller_owns_inputs` records actual ownership. Candidate `1` with ownership `0` identifies refused ownership. `controller_family` identifies the selected family (`0` basic, `1` horizontal, `2` diagonal/vertical, `3` counter-entry, `4` finish, `5` burst). `controller_fanbias`, `controller_mistakebias` and `controller_skill` record configured biases and raw bot skill. Humans/no bot state have family/skill `-1` and candidate/ownership `0`; migrated old diagnostic fields are unknown (`-1`).

- `python3 scripts/dueltrack_report.py [csv_dir] [--revision REV]` (needs `pip install duckdb`) prints the tuning figures (swing-start footing, chains per swing, roll-stab use, learned sequences per mode) together with the SQL that produced each one. It handles the latin-1 bytes, the differing column sets of the parts, and duplicate rows. Use `--revision` to measure one build's capture.

### Comparing repository and release captures

Keep the [Dueltracks1 release attachments](https://github.com/chompehh/botnav/releases/tag/Dueltracks)
outside the checkout, in a separate batch directory. These attachments are not a ZIP
of the repository. Do not concatenate their aggregate or learned snapshots with
the checked-in snapshots and sum the results.

The initial metadata comparison found 647 repository session rows versus 212
release session rows, and 92 repository strafe routes versus 234 release routes.
All 92 repository route rows occur unchanged in the release. Session IDs are not
globally unique: 179 `(source_context, record_id)` keys overlap, but no
`(source_context, start_time, end_time, mapname)` keys overlap. Therefore joins on
`parent_id`/`record_id` must also include the source batch; matching an ID alone
does not establish that two exports describe the same duel.

Reproduce those metadata checks with Python's CSV parser (not physical line
counts, since CSV fields can contain newlines):

```python
import csv
import io
from pathlib import Path

def rows(path):
    data = Path(path).read_bytes()
    try:
        text = data.decode("utf-8-sig")
    except UnicodeDecodeError:
        text = data.decode("latin-1")
    return list(csv.DictReader(io.StringIO(text, newline="")))

# Set repository and release to their separate absolute directory paths.
a = rows(repository / "dueltrack_sessions.csv")
b = rows(release / "dueltrack_sessions.1.csv")
print("sessions:", len(a), len(b))
for fields in [("source_context", "record_id"),
               ("source_context", "start_time", "end_time", "mapname")]:
    keys = lambda records: {tuple(r[k] for k in fields) for r in records}
    print(fields, len(keys(a) & keys(b)))
a = rows(repository / "strafetrack_routes.csv")
b = rows(release / "strafetrack_routes.1.csv")
keys = lambda records: {tuple(sorted(r.items())) for r in records}
print("routes:", len(a), len(b), "exact overlap:", len(keys(a) & keys(b)))
```

These counts describe file scope, not proof of improved combat or bot chain
completion. Strafe routes record humans only. A human multi-hop route proves that
the route was demonstrated, not that the bot reproduced it.

The extended report uses only Python's standard library in evidence mode:

```sh
python3 /absolute/path/to/botnav/scripts/dueltrack_report.py \
  /absolute/path/to/botnav --saber-evidence --release-dir /absolute/path/to/release
```

It prints the source-file manifests and the exact SQL alongside each result.
For the repository and Dueltracks1 attachments examined here:

| Printed query | Repository | Dueltracks1 |
|---------------|------------|-------------|
| Batch duplicate identities and payload variants: raw event rows | 420,962 | 182,343 |
| Same query: event identities | 338,204 | 182,343 |
| Human adjacent accepted starts: explicit R2L → L2R links | 1,170 | 519 |
| Same query: explicit L2R → R2L links | 949 | 412 |

The repository repetitions reflect schema-upgrade exports; the report retains raw
variants and uses the newest export schema per identity. These are observed
accepted adjacent links, not independent duel outcomes or proof of a complete
three-attack technique. The learned-snapshot overlap query finds 13,250 shared
sequence keys: 13,147 unchanged aggregates and 103 changed aggregates. Summing
both snapshots would double-count shared learning evidence. Use the printed
provenance and animation-coverage queries before trying to reconstruct swing
phase or comparing capture revisions.

The accepted-triple query also finds staff R2L → L2R → R2L windows with both
continuations explicitly linked (446 repository / 247 release), and staff
L2R → R2L → L2R windows (398 / 213). These overlapping windows support the
alternating recipes' frequency priors; they do not establish player intention,
independent trials, causality, or superior damage per second. The inventory
checks internal part gaps, not trailing completeness: only an exporter manifest
could establish that no final part was omitted.

The same command now prints **Saber close-range input ownership and lateral bias**
and **Released human directional priors**. In the examined release, bot-owned
grounded close-range attack samples had no movement input in 263/2,087 samples,
versus 262/9,096 human samples. Bot right/left input counts were 902/888: this
capture does not demonstrate a global rightward bias. Samples are not independent
frames or duel outcomes. New chain entries discard the previous chain's strafe
side, while idle footing refreshes changed approach commands even inside 100 units.

Directional priors use confirmed `swing_damage` joined to an accepted start by
batch, duel, participant, opponent, actual move and captured start timestamp.
The printed SQL separates mode and both stances and requires grounded range under
128 units with no self defense/recovery. For example, released human full-force
staff-versus-yellow vertical starts had 6/16 confirmed hits, versus 3/16 for
TL2BR. These are limited observational samples, not causal comparisons. The
bounded single-direction priors only nudge compatible recipe weights; they do not
claim complete-recipe effectiveness or derive saber direction from generic
`swing` tokens. Missing hit records are not proof of misses, and repository and
release snapshots remain separate.

### Profile menu deployment

The Profile item in `ui/jamp/ingame.menu` is hidden only for Siege (`7`), where
Objectives occupies its slot. Arcade (`10`), Jedi-v-Merc (`11`), and JOM (`12`)
should retain Profile. The UI now refreshes `ui_about_gametype` directly from
`CS_SERVERINFO` whenever current server information is refreshed, including before
opening the in-game menu, rather than relying solely on a previous cgame update.
Unavailable server information leaves the existing value unchanged.

Deploy the matching client UI module and CMake's `japro-assets.pk3`, not only the
server game module. Clients that ship their own UI module and menus (for example
the base Android client) never load these menus, so the Profile button can be
missing there. As a fallback, `/ui_openprofile` (TaystJK UI) or
`/ui_openmenu ingame_player` opens the profile menu directly, and players in
Arcade, JvM and JoM are told about it when they join.
A server-only update cannot replace menus on an independently installed Android
client. The refresh guards against stale state; it does not prove that stale
state, rather than an overriding client asset, caused a particular screenshot.

The `ui_profile` unit tests check both Profile entry points, the in-game menu
manifest, Apply/Join actions, and the JvM red/blue versus Arcade/JoM free-join
visibility rules. Build `uix86_64` (or the platform's UI target) and `Assets`
together; the PK3 must contain the same `ui/jampingame.txt`,
`ui/jamp/ingame.menu`, `ui/jamp/ingame_player.menu`, and
`ui/jamp/ingame_playerforce.menu` as the source checkout.
These are static checks, not a reproduction of missing buttons on a live client.
If Profile is still missing with those artifacts installed, capture the client's
`path`, `fs_game`, and `ui_about_gametype` output with the menu open and check for
overriding menu files. In each mode, manually verify both Profile entry points,
changing a model/name, Apply, and joining from spectator. JvM is a team mode;
its red/blue choices must not be hidden merely because JoM is a class mode too.

Saber combat verification:
- Enable `g_newBotAI 1` to exercise the shared technique controller; the legacy StandardBotAI fallback is unchanged.
- Validate saber-only duels separately from full-force and arcade matches. Human techniques guide contextual attack and escape choices, not blind replay of recorded commands.
- With `bot_fanbias 0`, bots should still issue primary attacks, start real swings, and link legal transitions. Compare against `bot_fanbias 100` with the same stance and skill: horizontal L2R/R2L frequency and phase-coordinated yaw should increase, not merely the number of vertical attacks.
- Saber footwork (dueltracks3/jundon): fresh swings start when the predicted peak range is within 70u, or 70–100u while closing faster than ~150u/s; beyond ~130u bots keep stepping in. Yellow and staff alternate R2L/L2R (R2L opens ~54% of chains; an L2R→R2L link is taken about two-thirds as often as R2L→L2R unless the chain is landing hits) and use T2B only as a finisher. An unchained swing that did not land ends with a ~200ms sidestep-back. Against an enemy swing within 130u bots advance about 65% of the time and dodge back/aside otherwise, and may counter-swing 100–350ms into the enemy swing when inside 90u.
- Backward/lateral jumping exits should avoid unsafe terrain and return to engagement after landing; saber-only bots cannot heal by waiting.
- Compare primary input requests with accepted swing starts and damaging swings. Define chain frequency as accepted linked swings divided by all accepted swings (count each swing once), and compare damage per second within the same duel mode and stance.
- Capture version 8 grows event storage on demand, up to 8,192 records per participant, instead of stopping at 128. At the bound (or after a growth allocation failure), compaction prioritizes damage/knockdown events, preserves the opening record and recent tail, and samples older records. Capture continues through later exchanges; it is not unlimited or lossless.
- Participant exports expose `event_total`, `event_retained`, `event_dropped`, `event_critical_dropped`, `event_compactions` and `event_allocation_failures`. Event indices remain monotonic across compaction, so gaps identify missing records. Any capture loss disables outcome/sequence ranking for that participant: attack quality becomes `unknown` and good/bad sequence coaching is withheld. Do not compute complete-match event rates from compacted samples. Aggregate damage/force totals remain independent of retained events. Migrated old records have unknown coverage counters (`-1`), and old version-7 recordings remain truncated at their original limit.
- Confirm the deployed build with fresh `capture_revision` values before comparing results. Old rows can be re-exported by a newer build without acquiring movement measurements or proving that build was deployed.
- Compare defense resets separately from ordinary retreats: measure damage taken after a broken parry, whether the bot leaves immediate swing reach, and whether its next entry happens after recovery rather than while the opponent is still following up.
- Verify pursuit during offset aim with world-space displacement, not `forwardmove` alone. Local movement commands can change when the view turns even though the intended path stays the same. Starts and transitions must still preserve the movement inputs that select the accepted swing.
- Test poke and wiggle separately: counter-yaw should improve early contact; alternating yaw/pitch should be evaluated by repeated contact and net damage, not by visual oscillation or the largest recorded sweep.
- Test throw routing against guarded opponents near walls and under low ceilings, with both throw levels 2 and 3. Check the actual view-trace endpoint and saber route; a successful wide release alone does not prove that the saber passed behind the block.
- Use matched stance, skill, map, opponent and duel mode for baseline comparisons. Record first-hit timing, damage per accepted swing, recovery damage, safe re-entry and throw hit rate; do not combine duplicate damage summaries or treat aggregate sequence samples as independent duels.

Technique evidence and limitations:
- The technique catalog adds twelve three-attack recipes without changing persisted
  action-token IDs. Family diagnostics `6` through `17` identify recipe IDs `0`
  through `11`; original family IDs `0` through `5` keep their meaning. The first
  two recipes use the observed alternating-horizontal pattern, not a claim of
  demonstrated complete three-hit combos:

  | Recipe | Requested accepted-move sequence | Evidence status |
  |--------|----------------------------------|-----------------|
  | 0 | R2L → L2R → R2L | Alternating-horizontal prior |
  | 1 | L2R → R2L → L2R | Alternating-horizontal prior |
  | 2 | R2L → L2R → T2B | Experimental |
  | 3 | L2R → R2L → T2B | Experimental |
  | 4 | TL2BR → R2L → L2R | Experimental |
  | 5 | TR2BL → L2R → R2L | Experimental |
  | 6 | R2L → TL2BR → T2B | Experimental |
  | 7 | L2R → TR2BL → T2B | Experimental |
  | 8 | T2B → R2L → L2R | Experimental |
  | 9 | T2B → L2R → R2L | Experimental |
  | 10 | TL2BR → T2B → R2L | Experimental |
  | 11 | TR2BL → T2B → L2R | Experimental |

  These are ordinary attack selections, not forced engine transitions. Only
  accepted matching moves advance a recipe's execution evidence. Mixed recipes
  have low priors and require an exposed/recovering opponent without immediate
  attack/recent-hurt pressure. Catalog selection is limited to yellow/staff fan
  stances and preserves dedicated counter-entry, finishing, and burst families.
  Recipes cannot override defense recovery or movement
  safety. No telemetry result establishes that all ten experimental recipes
  improve DPS; use matched live comparisons before increasing their weights.
- Recipe execution evidence is a separate map-local cache keyed by exact recipe,
  full safety context, and both stances. Completed matching sequences receive a
  bounded observational outcome window; interrupted or engine-changed attempts
  are discarded rather than scored as successful executions. The existing
  persisted action-token aggregates remain unchanged. Recipe learning does not
  reconstruct fine-grained human motor templates from the coarse `swing` token,
  and its outcome association is not proof that the recipe caused the damage.
  Ordinary between-duel token-cache refreshes retain recipe evidence; map
  initialization and a successful `resetlearningtrack` clear it. Because this
  cache observes controlled bots, `bot_learninghumansonly 1` excludes its
  observations and bonuses. Outcomes use attributed damage within the observed
  pair; environmental, self, or third-party damage invalidates the sample,
  while healing supplies no reward.
- Active wiggle now fades at animation boundaries and its configured delay;
  amplitude remains bounded and depends on the accepted attack axis. Contact
  counter-yaw is range- and skill-scaled independently of wiggle. Neither net
  signed yaw sweep nor a large aim offset proves blade isolation or extra hit
  detection. These remain conservative control curves requiring live validation.
- Sugar Kane's saber-only duel 162 includes L2R at 25.377s, R2L at 25.707s, another L2R at 26.763s, and a recorded 40-damage finishing punish at 26.994s. This supports alternating horizontal pressure and finishing continuation, not a universal instruction to hold attack forever.
- BK's full-force duel 563 includes R2L/L2R/R2L at 10.300s/10.600s/10.900s, two recorded 40-damage successes, then 40 damage taken at 10.950s. Use this as a burst-and-counter-risk example, not a saber-only training outcome. His backward/lateral rising movement in duel 565 at 8.300s supports the escape shape, not unconditional jumping or a guaranteed successful exit.
- Duel 625 (capture version 7, revision `509427374`) confirms accepted bot attacks but records 10 vertical, 8 diagonal and only 1 horizontal swing in the retained prefix. The human used staff while the bot used red stance; their attack timing is not directly interchangeable.
- Historical geometry is event-sampled, and many old rows lack measured swing sweep. Direction/sequence choices are evidence-backed; bounded phase-aware yaw is a controller approximation, not an exact replay or a proven optimal early/extra-damage curve. Validate new yaw trajectories and damage trades in fresh, stance-matched recordings before tuning them.
- The same Sugar Kane yellow-stance sequence has only two geometry samples per interval: approximately -15.8 degrees over 25.377–25.707s and +31.5 degrees over 25.707–25.938s. BK's full-force yellow sequence has seven samples per 300ms interval and approximately -190.6/+238.8 degrees of accumulated yaw. These different shapes are not interchangeable; sparse endpoints cannot establish exact active-blade timing. The initial controller used a conservative 18-degree offset envelope and a 240-degrees/second turn limit. Neither that fallback nor the newer bounded counter-yaw is a replay of BK's large spins.

## Learned Sequences

At the end of every tracked duel, each participant's events are broken into short sequences of 2–4 tokens: the opponent's move (or `idle` when the participant acted on their own), the participant's response within 700ms, then up to two follow-ups within 1s. Tokens are `push`, `pull`, `grip`, `drain`, `throw`, `swing`, `kick`, `jump`, `knockdown`. Repeated holds of the same power within 400ms are merged; swings are not merged, so fan chains count as chains.

- Context key: HP+armor and force buckets (four each) for both sides, range bucket (<128u, <384u, beyond) and stance at the start of the sequence, so similar (not identical) HP/AP/FP/range snapshots already share a key. When the exact key has fewer than `bot_learningminsamples` samples, contexts up to two bucket steps away in HP+armor/force (same range, stances and safety dimensions) are pooled with weight 0.5 per step (1 step 0.5, 2 steps 0.25). A coarse key drops health buckets but retains both force buckets, range, stances and safety dimensions.
- Outcome: net damage (dealt − taken) over the 2s after the response, plus whether the participant won the duel.
- Storage: `LocalBotLearnedSequence` in `dueltracks.db`, keyed by source kind (human/bot), skill band (human, bot 1–3, 4–5, 6–7, 8–10), context and token sequence. Rows accumulate samples, wins, net damage and response time. `resetdueltrack` preserves both these aggregates and `LocalBotLearnedEvidence`; only `resetlearningtrack` clears learning.
- Map load: rows are loaded into memory, filtered by `bot_learninghumansonly` (bot rows are weighted ×0.25). Each context/response gets a confidence-weighted score based mainly on net damage; wins only count relative to the source's usual win rate (humans win most tracked duels, so a raw win rate would inflate every human row). With few samples the score is pulled towards zero, and the learned bonus only approaches its ±30 cap once a context has many samples (half strength at 20).
- Server command `botlearn` (or `botlearn print [lines]`) prints learned responses without a map restart. Finished duels mark the cache for automatic refresh between duels; `botlearn reload` also defers if a duel is active, so weights do not change halfway through an exchange.
- In game: the same lookup feeds force reactions, combo follow-ups (e.g. pull→kick), the saber-throw decision and fan entry. The technique controller consults capped initiative/counter/chain preferences at selection boundaries and records the bonus when its requested attack is accepted. Learned preferences never override legal-action, movement-safety or defense-recovery gates.
- New contexts separate saber-only from full-force duels and retain defensive, recovery, airborne and self-footing dimensions through exact, neighboring and coarse lookup. Footing distinguishes still, advance, retreat and lateral movement using the actor's own horizontal velocity toward the opponent, not relative closing speed. Legacy aggregates remain exportable but lack these dimensions and do not influence the new contexts. Fresh compatible recordings must reach the sample threshold; old CSV aggregates alone do not supply motor imitation.
- `LocalBotLearnedEvidence` preserves each extracted example's duel, participant, action index, outcome window, recording version/revision, extraction time and session ID. Match session ID plus summary ID when using archived recordings: summary IDs alone can be reused after a duel reset. Existing evidence receives its session from retained events, or a `legacy:<summary_id>` marker if unavailable. Deleted raw recordings cannot be reconstructed from evidence alone. Use examples behind aggregate scores rather than infer build-to-build improvement from an aggregate's latest `capture_version`.

### Baked baseline (`bot_learningbaseline`)

The last compatible export (`dueltrack_learned.csv`, 2,418 samples from 67 duels) is compiled into `BotLearn_FindBaseline` in `codemp/game/g_bot_learning.h`, using the same source weighting and baseline-relative win scoring as the live cache. Strongest kept entries:

| Mode | Sequence | Effect |
|------|----------|--------|
| Saber-only | idle → swing (→ swing) | Strong bonus: opening fan chains (+15 net damage/sample, 93% human wins) |
| Saber-only | swing → swing → swing | Bonus: continuing a fan chain after a swing exchange |
| Full force | idle → throw (→ pull), drain → throw → pull | Bonus: throw commitment with pull follow-up |
| Full force | drain → pull/jump, idle → pull → jump | Bonus |
| Full force | pull → drain, throw → drain, grip → drain | Penalty: draining after control/throw lost exchanges |
| Full force | throw → swing → swing, swing → swing, swing → jump | Penalty |

Through `G_BotLearnBonus` the baseline reaches fan entry (saber-only fan chains need `bot_fanbias` > 0), force reactions, combo follow-ups and throw decisions. New recordings override it once a context reaches `bot_learningminsamples`.

### Recent exported learning: interpretation

The checked-in `dueltrack_learned.csv` contains 13,250 aggregate rows, but only 2,311 rows (2,418 samples) satisfy the current safety/footing compatibility filter. Those samples comprise 1,336 human examples and 1,082 bot examples. Most exact sequences are sparse: the most sampled compatible row has only five samples. Neighbor/coarse pooling can supply confidence, but must retain safety dimensions.

| Response | Human samples / mean net damage | Bot samples / mean net damage |
|----------|---------------------------------|-------------------------------|
| Throw | 164 / +14.36 | 251 / +7.20 |
| Pull | 152 / +5.00 | 173 / +6.02 |
| Swing | 374 / +5.30 | 176 / +1.91 |
| Drain | 281 / -0.61 | 362 / -5.20 |
| Grip | 71 / -2.61 | 54 / +5.81 |

These descriptive totals pool unlike contexts and overlapping outcome windows; they are neither causal action scores nor isolated damage from the named move. They suggest investigating throw execution and whether bots overcommit to drain, not globally disabling drain or maximizing throws.

One compatible human throw→pull row against a knocked-down opponent has three samples and +63 total net damage; a bot row has four samples and +98 but no duel wins. Conversely, a human pull→grip row has four wins yet -71 net damage. The learner therefore uses local damage plus baseline-relative wins instead of treating victory as proof every move was good. This supports testing built-in throw commitment and timely pull follow-ups while preserving force budgets and drain-lock opportunities.

Current action learning can favor a throw or follow-up; it does not fit a continuous steering trajectory or derive an optimal alt-attack hold duration from those aggregate rows. Check current event/geometry samples and actual saber/view-trace positions for above/behind routes. A long `throw_start`→`throw_end` interval includes return flight and cannot alone demonstrate a long held throw or a successful block bypass.

New bot hold/recall decisions use existing duel decision events: `amount` is elapsed throw milliseconds and the note is `st:H|R:<reason>:p<phase>:x<passed>:h<heading>`. `H` means hold, `R` recall; phase, actual pass detection and target-heading status explain the decision. Transitions are recorded immediately and unchanged samples are throttled to 250ms. These are diagnostic annotations, not extra learning actions; use them alongside human input/geometry samples to investigate premature recalls without counting return flight as held time.

Duel capture version 9:
- `throw_start` is logged when a saber throw is released: amount = 2D distance to the opponent at release. In version 9, `throw_yaw_offset` uses saber flight direction when available and can be reused on subsequent rows; it is not a continuous camera-steering measurement. (Older builds required a saber state the engine never set at release, so no `throw_start` rows were exported.) `throw_end` is logged when the saber is back in hand: amount = damage the throw dealt in flight, note `hit` or `miss`. The interval includes return flight and is not necessarily the duration of held alt attack.
- `dodge` is logged for the defender when an enemy swing ends within close range without damaging them (note `air`, `blocked` or `evaded`).
- Knockdown notes record `knockdown_by_opponent` or `knockdown_self`, and the damage attacker key identifies who caused it.
- Duel winners no longer get `low_force` as their primary issue for deliberately spending force.
- `force` events whose power could only be guessed from the selected power (force lost to an enemy drain, or the cost of a saber throw) carry the note `selected_fallback`. Leaving the ground with jump held and upward velocity is noted `jump` instead of `airborne`. Learning ignores both guessed force spends and non-jump launches, so victims are not credited with actions they never took.
- `LocalDuelTrackAggregate` rows carry `capture_version` and `capture_revision` of the latest update (aggregate export format 3).

Current duel capture:
- Bounded attack/throw/recovery windows add `attack_sample` events at a minimum 50ms interval (limited by server frame cadence). Samples are low-priority during compaction; damage and defensive transitions retain higher priority. This is not unlimited, continuous motion capture.
- Event telemetry adds self/opponent pitch, target-relative yaw/pitch errors, saber position when valid and the live view-trace endpoint. Historical release-offset and cumulative-sweep fields retain their meanings.
- Saber defense is classified separately from absorb/protect buffs: none, ordinary parry, attack bounce, broken parry, knockdown or lost saber. Raw `saberBlocked` is also exported, and normal return-animation recovery is separate.
- Self/opponent footing identifies actual movement independently of the legacy relative-speed label. It is an execution context, not proof of deliberate movement intent.
- `torso_anim` and `torso_timer`, alongside accepted attack elapsed time, align samples with engine animation/recovery. Historical missing animation fields are `-1`; a full normalized animation curve cannot be reconstructed from these fields alone.
- `decision` records identify the accepted controller action and its learned context, stimulus, response, follow-up and applied bonus. Missing historical decisions remain unknown rather than appearing to be zero-bonus choices.

Interpreting historical recordings:
- `yaw_delta` compares the participants' facing directions, not aim against the bearing to the opponent.
- `yaw_sweep` is accumulated signed rotation since swing start. Opposing rotations can cancel; this field alone cannot establish wiggle frequency or sustained saber contact.
- `radial_speed` is relative closing speed, including both participants' velocities. It cannot by itself establish which participant deliberately advanced.
- Event-sampled geometry cannot establish continuous pitch motion, a throw's overhead/rear path, or the steering trace endpoint when those fields were not captured. Do not treat successful damage as proof of a particular bypass technique.
- Learned rows are cumulative sequence samples. `wins` describes the containing duel's outcome, not an independent action success; `avg_response_ms = 0` with stimulus `idle` means initiative, not zero reaction latency.

Public teaching annotations:
- Enable `bot_learninglog 1` before a demonstration session. `exportDuelTrack` includes `public_chat` records in its existing events CSV; no separate chat file is needed.
- Active participants' messages link to their tracked duel when it is persisted. Messages from the same participant within 15s of completion can be marked `recent_inferred`; spectators and unmatched messages remain session annotations instead of being assigned to another participant's duel.
- Chat rows include bounded full text, speaker type, map/session timing and association metadata. Public tutorial broadcasts are bot annotations; private tutorial tells are not recorded.
- Chat is excluded from combat sequence scoring. Review a human explanation against the associated combat window before treating it as a correction; the engine does not interpret arbitrary chat as commands or trusted training labels.

Recommended data: record duels against bots at skill 6+ so the skill gradient can be checked against data; the dueltracks2 set had no bots above skill 5.

Saber-only data: 392 of the 397 duels in the dueltracks3 upload were force-enabled, so saber-only footwork (swing start window, sidestep-back, advance/dodge split, counter-swing timing) is currently tuned from force-duel saber exchanges. To tune saber-only behaviour separately, record dedicated saber-only duels (force powers disabled for the duel) against humans and bots at skill 6+ in each stance, ideally a few dozen per stance, and export them with `exportDuelTrack` so the saber-only subset can be compared with the full-force set.

## Skill Tuning (Debug)

| Cvar | Default | Description |
|------|---------|-------------|
| `bot_s1` | `16` | Skill parameter 1. |
| `bot_s2` | `24` | Skill parameter 2. |
| `bot_s3` | `48` | Skill parameter 3. |
| `bot_s4` | `0.5` | Skill parameter 4. |
| `bot_s5` | `0` | Skill parameter 5. |
| `bot_s6` | `64` | Skill parameter 6. |

## Dependency Graph

```
g_newBotAI (master switch)
  |
  +-- g_flipKick (required for flipkick/PTK)
  |
  +-- Targeting
  |     +-- g_newBotAITarget
  |     +-- bot_targetdistance
  |     +-- bot_target_timeout
  |     +-- bot_lowhangingfruitHP / bot_lowhanginfruitDistance
  |
  +-- Aggression Core
  |     +-- bot_aggressionbias (base)
  |     +-- bot_healthbias (health modifier)
  |     +-- bot_forcebias (force modifier)
  |     +-- .jkb hatelevel (personality modifier)
  |     |
  |     +-- Consumed by:
  |           +-- bot_saberthrowbias --> saber throw weight
  |           +-- bot_gripkickbias --> grip initiation weight
  |           +-- Legacy fan hold/dwell/yaw controls --> legacy fan-chain patterns only
  |           +-- bot_drainbias --> drain hold duration
  |           +-- bot_antidrainbias --> anti-drain priority
  |           +-- bot_lightningbias + bot_lightningdistance --> lightning
  |           +-- bot_ptk_aggressionbias + bot_ptk_fpdifference + bot_ptk_hpdifference --> PTK
  |           +-- Retreat thresholds (health/distance)
  |           +-- Saber throw defense break (pull vs push)
  |
  +-- Shared Saber Techniques
  |     +-- bot_fanbias --> coordinated horizontal family weight (not attack permission)
  |     +-- bot_mistakebias + skill --> contextual timing/positioning choices
  |     +-- Accepted animation + final input ownership --> selection, pressure and bounded yaw
  |     +-- Live movement safety + engine/force/navigation priority --> legal inputs
  |
  +-- Aim & Response
  |     +-- bot_aimspeed
  |     +-- bot_delay / bot_delayresponsetime / bot_responseTimeDelay
  |
  +-- Movement
  |     +-- bot_strafefrequency / bot_strafeduration
  |     +-- bot_strafejumps
  |     +-- bot_hopfrequency
  |     +-- bot_navigation
  |     +-- bot_waypointskip
  |     +-- bot_redirectcooldown
  |
  +-- Gripkick
  |     +-- bot_gripkickdwell
  |     +-- bot_bully
  |
  +-- Misc
        +-- bot_nochat, bot_forcepowers, bot_maxbots, etc.
```

## Jedi vs Merc / Jedi or Merc

`g_gametype 11` (`jvm`) is team deathmatch: **Red Jedi/Tanks, Blue Mercs**.
`g_gametype 12` (`jom`) is free-for-all: Jedi, Merc and Tank are loadout
classes, not teams. Everyone else is an enemy, spawning and scores are individual.
CTF (8), CTY (9) and Arcade (10) retain their IDs and rules.

In JoM, use `/class jedi`, `/class merc`, or `/class tank` (also `/team jedi|merc|tank`,
or `/team 1`, `/team 2`, `/team 3` for Jedi, Merc, Tank; `/class 1|2|3` works too). `/team free`
and `/team spectator` behave as in FFA. JoM is not a team game: the server sends no team scores.
Clients running an older cgame that treats every gametype from 6 up as a team game will still
draw team scores; deploy the cgame from this repo.
Changing class kills and respawns an active player, has a five-second cooldown,
and is forbidden during private duels. Spectators select their next spawn's class.
Class persists across reconnects/map restarts and is replicated without changing
the player-state layout. Bots default to a mix of JoM classes; `class` userinfo
can select Merc or Tank. In JvM, Blue always uses Merc; Red can select Jedi or Tank.

| Cvar | Default | Meaning (only in JvM/JoM) |
|---|---:|---|
| `g_jediVmerc` | 0 | Legacy JA "Jedi vs Merc" modifiers for non-JvM gametypes (latched). In JvM/JoM the class rules below replace these bonuses instead of stacking. |
| `merc_startingweapons` | 102384 | Merc spawn weapons, same bit layout as `g_startingWeapons` (bit = 1 << weapon number). The default is bryar pistol, E-11 blaster, disruptor, bowcaster, repeater, DEMP2, flechette, rocket launcher, concussion rifle and old bryar pistol, i.e. every gun but no stun baton, thermal detonators, trip mines or det packs. The saber bit is always removed, melee is always added, `g_weaponDisable` still applies and ammo is filled for every weapon owned. |
| `merc_startingitems` | 0 | Merc spawn holdable items, same bit layout as `g_startingItems` (bit = 1 << holdable number). Medpacs are never given (Mercs heal only from pickups). |
| `g_mercforceregentime` | 200 | Merc Force-energy regeneration interval, milliseconds (minimum 1). |
| `merc_grapple` | 1 | Allow Merc and Tank grapple (`+grapple`), independent of legacy `g_allowGrapple`. |
| `merc_grappleFPscale` | 10 | Force points/second while the hook exists; fractional drain is accumulated across frames/releases. Clamped 0–1000. Depletion releases the real hook. |
| `merc_gripkickreduction` | 0 | Fraction of actual Force Grip and flipkick damage resisted by Mercs, clamped 0–1. Does not reduce lightning, punches or ordinary saber kicks. |
| `merc_forcejumplevel` | 1 | Force Jump rank for **both Mercs and Tanks**, clamped 0–3 and replicated for prediction. If Jump is globally disabled, positive ranks are capped at 1 (the normal basic-jump exception); rank 0 stays disabled. |
| `merc_botfloodprotect` | 100 | Minimum milliseconds between Merc bot primary/alternate gun attack commands, clamped 0–2000; all bot AI paths share the final command gate. Melee and saber attacks are not throttled. |
| `merc_flipkick` | 1 | Merc flipkick permission, replicated for shared movement prediction; rank-1 Mercs can front-flipkick. No duplicate `g_mercflipkick` cvar. |
| `merc_countergrip` | 1 | Allow Merc primary/alternate gun fire while gripped. Only grip-related attack suppression is bypassed; movement restrictions and other firing gates remain. Requires the updated cgame for matching prediction. |
| `merc_dropchance` | 50 | Merc weapon disarm percentage at 100 units, scaled inversely with pull distance and capped at 100%. Zero disables disarming Mercs; normal enemy, range, weapon and resistance gates still apply. |
| `jedi_tankscale` | 0.5 | Tank incoming combat-damage multiplier, clamped 0–10. Does not mitigate suicide, team changes, forced deaths or environmental hazards. |
| `jedi_saberdamagescale` | 2 | Tank outgoing saber-damage multiplier, clamped 0–10. |
| `jedi_healthregen` | 1 | Tank health restored per regeneration interval; fractional amounts accumulate. Zero disables regeneration. Never heals dead players or exceeds maximum health. |
| `jedi_healthregentime` | 1000 | Tank health regeneration interval in milliseconds, minimum 1. Default settings restore one HP per second. |
| `jedi_gripkickreduction` | 0 | Grip/flipkick damage reduction for Jedi-side classes, clamped 0–1, separate from Merc resistance. |
| `jedi_alwaysabsorb` | 1 | Tanks passively resist Push, Pull and Grip as with level-3 Absorb. No visible Absorb activation or Force-energy gain; Drain and Lightning are unaffected. |
| `jedi_pushpull` | 1 | Give Tanks rank-3 Push/Pull, subject to globally disabled powers. |
| `jom_cycleloadout` | 0 | In JoM, cycle the next spawn's class on death: Jedi → Merc → Tank → Jedi. Does not change other gametypes. |

Pickups by class (team objectives such as CTF flags are allowed for everyone):

| Class | Can pick up / use |
|---|---|
| Merc | Everything: weapons, ammo, health, armor, holdable items, powerups, ammo/health/shield dispensers and power converters. |
| Tank | Health and armor only (including health/shield power converters). |
| Jedi | Nothing. |

Jedi have their normally configured Force powers and only a saber. Pickups they
cannot take render at 50% opacity to them (including simple items, holograms and
respawn passes). Jedi can push/pull world items and disarm opponents using normal Force rules.
Jedi and Tanks deny non-saber weapons on contact without receiving a weapon or
ammunition, or producing a pickup sound, HUD notification or console message.
Map weapons use their ordinary respawn timers (including map `wait`/`random`
overrides); dropped weapons are removed. Ammo and other forbidden pickups remain
untouched and silent. Denial is disabled for spectators, race mode and private duels.
Deploy the updated cgame as well as the game module to suppress invalid local
pickup feedback.

Mercs spawn with melee plus `merc_startingweapons` and `merc_startingitems`
(`g_startingWeapons`/`g_startingItems` are not used for Mercs), but never a saber.
They have no Force powers except configured Jump, cannot push/pull/disarm, and
heal only through health pickups (no medpac use, starting medpacs or external Force healing).
In JoM full-force private duels, Mercs retain **only already owned** guns and ammo;
duels do not grant new guns or replace their inventory with a saber. Gun-duel
variants are disabled in JoM; saber-only duels require a saber on both sides.
JoM duels retain current health/armor at acceptance; Mercs and Tanks do not
receive the legacy automatic health/armor refill for winning a private duel.

Tanks have a saber, rank-3 saber offense/defense/throw, and the same configured
Jump as Mercs. Globally disabled saber throw is not granted; offense/defense keep
the normal rank-3 free-saber exception. Optional rank-3 Push/Pull is controlled by
`jedi_pushpull`. Tanks do not gain Heal or Drain, and cannot use medpacs or receive external
Force healing. They regenerate health using the settings above. Only health/armor pickups are
allowed, at ordinary opacity. Explicit class modifiers replace legacy
`g_jediVmerc` bonuses rather than stacking with them. Weapon loadouts are applied
at spawn, not granted every frame, so disarming a Merc remains effective.

FFA arena maps support JoM, and FFA/team arena maps support JvM. Both modes are
available through gametype votes and server-browser filters in the updated UI.
The in-game menu's Profile button is shown in Arcade, JvM and JoM as well.
Both Profile buttons open the loaded in-game customization menu; deploy the updated
UI module and menu assets together.

### Gun bot weapon and pickup priorities

Merc bots keep a usable ranged weapon instead of switching to melee just because
an enemy is close. Close-range choices prefer safe firing modes; unsafe splash
shots are suppressed and bots try a hazard-checked retreat. Melee remains the
fallback when no ranged ammunition is usable.

Skills 3–6 favor lower/medium-tier guns without banning stronger fallback choices.
Skills 1–2 retain legacy preferences, while higher skills score effective damage
output. Ammo availability and splash safety override these preferences.

Bots without usable ranged equipment prioritize actual available, legal weapon
and compatible-ammo pickups over random wandering. They route through reachable
waypoints and approach the item itself, retaining map and objective safety gates.
Saber approach movement refreshes stale footing directions and tries safe forward
lanes before retreating rather than injecting an unconditional attack.
Deploy matching game, cgame and UI modules for class display and prediction.
