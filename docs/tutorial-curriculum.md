# Duel tutorial curriculum

The catalog contains 150 beginner and 150 advanced original messages. The
durable coaching rotation introduces 12 beginner selections, then mixes both
tiers; Force use is not a skill rating. All entries use the same eligibility
rules for registered and guest players.

## Integration and safety

`G_DuelCurriculumAllowed` checks duel mode, every required power bit, and
minimum levels. Supply a mask reflecting the player's known powers *and* the
live duel, server and player restrictions; do not infer availability from
level alone. Bits correspond to `FP_*` in `codemp/qcommon/q_shared.h:369`.
The level-2 and level-3 masks add requirements to the level-1 mask. A missing
levels array rejects power-dependent lessons.

`G_DuelCurriculumRender` takes `duel_curriculum_inputs_t`. Set
`validMinimums` only for an activation threshold verified for the live
player, mode and settings. `validActionCosts` marks separately verified
initial expenditures, indexed by power; `validValues` marks the named values
below. A threshold is not necessarily the expenditure, and ongoing costs
are separate. The `%d` placeholders below are ordered as listed in the
format column. Omit any unknown value: rendering fails and selection must
choose another eligible entry. In particular,
Lightning and Drain both require **25 FP** to start, despite their different
table entries (`codemp/game/w_force.c:1922`, `codemp/game/w_force.c:2195`). A hold can cost more.
Jump and throw have their own checks. Special modes and active-power
deactivation can alter availability (`codemp/game/w_force.c:665`,
`codemp/game/bg_saber.c:2991`).
Do not pass a guessed or stale value. Rendering rejects truncation and any
message of 150 bytes or more. `G_DuelCurriculumSelect` is an optional,
zero-based pure selector: its first 12 eligible selections are beginner,
then it alternates eligible tiers. It skips unrenderable lessons.

### Input values for the current implementation

For ordinary power use, costs come from `forcePowerNeeded[level][power]`
(`codemp/game/bg_pmove.c:140`) and `BG_ForcePowerDrain`
(`codemp/game/bg_saber.c:55`), **not** from an assumed default level.
Applicable overrides are Pull 60 for `FT_WEAKPULL`, Push 95 for `WT_TRIBES`
(`codemp/game/w_force.c:3472`); Protect 40, Absorb 20 for `WT_TRIBES`
(`codemp/game/w_force.c:1795`, `codemp/game/w_force.c:1845`); Speed 75
in race (`codemp/game/w_force.c:1206`). The Push saber-lock branch uses
the normal cost; do not mark a single cost valid if the applicable branch
is ambiguous. `FT_WEAKPULL` first checks ordinary usability and then calls
the 60-FP start helper without checking whether that helper succeeded;
leave Pull's cost invalid under that tweak unless the insufficient-FP
branch is explicitly accounted for (`codemp/game/w_force.c:3459`).
Activation requirements can be different from costs:
Tribes Protect/Absorb require 50/30 FP but spend 40/20; level-3 Grip
requires 60 but acquisition spends 30 (`codemp/game/w_force.c:665`,
`codemp/game/w_force.c:5195`). Grip's additional hold and Protect's
additional upkeep/damage costs are **not** included in pair sums. Drain
activation costs zero; a processed shot costs 5. Lightning activation and
its ongoing ticks each use the level table cost
(`codemp/game/w_force.c:1133`, `codemp/game/w_force.c:1214`,
`codemp/game/w_force.c:5024`). Throw spends its table cost at launch
(`codemp/game/bg_saber.c:3302`).

Set the corresponding `DUEL_VALUE_*` validity bit only if the stated
conditions apply:

| Value | Meaning and source |
|---|---|
| `HEAL_GAIN` | Maximum uncapped Heal gain, 5/10/25 HP at levels 1/2/3 (`codemp/game/w_force.c:1237`). |
| `GRIP_START_COST`, `GRIP_HOLD_COST`, `GRIP_HOLD_MS` | 30 FP on successful acquisition, 1 FP per nominal 100 ms hold tick (`codemp/game/w_force.c:5195`, `codemp/game/w_force.c:4914`; `codemp/game/w_saber.h:55`). |
| `DRAIN_SHOT_COST`, `DRAIN_SHOT_MS` | 5 FP per processed shot at nominal 50 ms cadence (`codemp/game/w_force.c:2628`, `codemp/game/w_force.c:4842`, `codemp/game/w_force.c:5002`). A line-trace miss returns before the charge (`codemp/game/w_force.c:2528`); level-3 cone processing can charge without hitting. |
| `DRAIN_TARGET_LOSS` | Clean-hit maximum FP removed: 2/3/4 at levels 1/2/3, except level-3 `FT_DRAINDMGNERF` gives 3. Do not populate for race or other exceptional target rules (`codemp/game/w_force.c:2237`). Absorb and insufficient target FP alter the result. |
| `DRAIN_HEAL_GAIN` | Clean-hit maximum HP gained: 2/3/4 at levels 1/2/3, including 4 at level 3 with `FT_DRAINDMGNERF`. Leave invalid for race or JVM target modifiers; Absorb and missing HP affect the result (`codemp/game/w_force.c:2250`, `codemp/game/w_force.c:2307`). |
| `DRAIN_TARGET_REGEN_DELAY_MS`, `DRAIN_SELF_REGEN_DELAY_MS` | 800/500 ms with current `_draintest 0`; if that compile-time switch changes, use the actual cvars (`codemp/game/g_xcvar.h:621`, `codemp/game/w_force.c:2326`, `codemp/game/w_force.c:2630`). A missed line with `FT_FIXLINEDRAIN` has a different self delay (`codemp/game/w_force.c:2528`). |
| `THROW_BASE_DAMAGE` | 30 base contact damage before G_Damage modifiers and blocking (`codemp/game/w_saber.c:6120`, `codemp/game/w_saber.c:6231`, `codemp/game/w_saber.c:6953`). The defined 5 return constant is unused: do not infer a 5-damage return. Flight suppresses normal FP regeneration (`codemp/game/w_force.c:6305`). |
| `FLIPKICK_BASE_DAMAGE` | Set to 20 only for `g_flipKick >= 3` and nonzero damage enable; the gripkick bonus and other damage rules can change actual damage (`codemp/game/g_active.c:5724`). Other branches vary with velocity/scale. |
| `RAGE_HEALTH_DRAIN`, `RAGE_DRAIN_MS` | 2 HP at nominal 150/300/450 ms at levels 1/2/3. Rage needs 10 HP to activate and stops at 1 (`codemp/game/w_force.c:1885`, `codemp/game/w_force.c:4943`). |
| `LIGHTNING_SHOT_COST`, `LIGHTNING_SHOT_MS` | The current level's table cost and nominal 50 ms (`codemp/game/w_force.c:4842`, `codemp/game/w_force.c:5024`). |
| `LIGHTNING_BASE_MAX` | With a saber, the base hit roll is 1..1 if `g_fixLightning > 1`, otherwise 1..2, before Absorb and other damage rules. Leave invalid for non-saber cases (`codemp/game/w_force.c:1984`). |

## Checked mechanics and caveats

- Force table values are level-dependent (`codemp/game/bg_pmove.c:140`).
  Charged jumping checks FP and has variable expenditure (`codemp/game/w_force.c:2653`,
  `codemp/game/w_force.c:2818`, `codemp/game/bg_saber.c:55`).
- Heal adds 5, 10 or 25 HP at levels 1, 2 or 3, capped at maximum health
  (`codemp/game/w_force.c:1220`). Protect spends FP to reduce qualifying
  damage (`codemp/game/g_combat.c:6041`). Absorb interactions depend on
  incoming power, levels and settings (`codemp/game/w_force.c:877`).
- Protect, Absorb and Rage normally switch each other off unless the
  Force-combo tweak permits coexistence (`codemp/game/w_force.c:1779`,
  `codemp/game/w_force.c:1830`, `codemp/game/w_force.c:1892`);
  `g_tweakForce` defaults to 0 (`codemp/game/g_xcvar.h:232`).
- Throw steering starts at level 2, traces world solids at level 2 and players
  as well at level 3, with different update intervals
  (`codemp/game/w_saber.c:7530`). The saber travels toward the traced point;
  it does not teleport. Release, maximum duration and distance can recall it
  (`codemp/game/w_saber.c:7495`).
- Flipkick uses `DAMAGE_NO_ARMOR`, but the damage branches differ. In the
  `g_flipKick == 2` branch, speed above the threshold becomes 20 plus the
  glitch bonus before scaling; other values use different calculations
  (`codemp/game/g_active.c:5724`). Defaults are `g_flipKick 0` and
  `g_flipKickDamageScale 1` (`codemp/game/g_xcvar.h:252`,
  `codemp/game/g_xcvar.h:294`). Thus 20 is not a universal kick damage.
  Knockdown also depends on target state and settings (`codemp/game/g_active.c:5750`).
  Armor bypass does not imply bypassing every other protection
  (`codemp/game/g_combat.c:3289`, `codemp/game/g_combat.c:6041`).
- Drain's regeneration delays currently use 800/500 ms because `_draintest`
  is zero. The alternative build branch uses the corresponding cvars
  (`codemp/game/w_force.c:2327`, `codemp/game/g_xcvar.h:621`).
  Arithmetic examples are labeled as examples; live placeholders report
  verified activation requirements, expenditures, damage and recovery timing.

The historical Shades manual was consulted for topics, particularly Force
management, pull/kick and laser-pointer steering; messages are newly written
and mechanics were checked against this mod rather than taken as current
rules.

## Catalog

The entries below reproduce the catalog text. `B`/`A` are the tiers; `any`
means either duel mode, while `force` requires full Force. Requirements list
all player powers, with `@2`/`@3` for minimum levels. `none` means no player
power is required. `%d` values are the checked inputs in the format column.

| # | Tier | Mode | Required | Format inputs | Message |
|---:|:---:|:---:|---|---|---|
| 1 | B | any | none | literal | HP is health: reaching zero ends the duel. Protect low HP before planning another attack. |
| 2 | B | any | none | literal | AP is armor. Some damage consumes armor, but attacks that bypass armor still threaten your HP. |
| 3 | B | force | none | literal | FP is Force power. Spending it on offense leaves less for your next defensive response. |
| 4 | B | any | none | literal | Watch HP and AP separately: armor is not a second health bar against every attack. |
| 5 | B | any | none | literal | At low HP, an armor-bypassing kick can finish you even if your armor is high. |
| 6 | B | any | none | literal | Example, not damage prediction: 40 HP minus 15 HP damage leaves 25 HP. |
| 7 | B | any | none | literal | Example: with 30 HP, two hits of 20 HP damage would be lethal; actual damage varies. |
| 8 | B | force | none | literal | Example: 60 FP minus a hypothetical 20 FP action leaves 40 FP for later actions. |
| 9 | B | any | none | literal | Example: 30 AP minus 10 armor damage leaves 20 AP; not every hit damages armor equally. |
| 10 | B | any | none | literal | After exchanging hits, check all three resources before choosing whether to close again. |
| 11 | B | force | Heal@1 | min HEAL; cost HEAL | Heal needs %d FP available and spends %d FP. It cannot heal past your maximum HP. |
| 12 | B | force | Speed@1 | min SPEED; cost SPEED | Speed needs %d FP available and spends %d FP to start; reserve FP for defense. |
| 13 | B | force | Push@1 | min PUSH; cost PUSH | Push needs %d FP available and spends %d FP on use. Face your opponent. |
| 14 | B | force | Pull@1 | min PULL; cost PULL | Pull needs %d FP available and spends %d FP on use. Prepare for close range. |
| 15 | B | force | Grip@1 | min GRIP; cost GRIP | Grip needs %d FP available; acquiring a target spends %d FP, then holding spends more. |
| 16 | B | force | Lightning@1 | min LIGHTNING; cost LIGHTNING | Lightning needs %d FP available and spends %d FP to start; ongoing ticks cost more. |
| 17 | B | force | Drain@1 | min DRAIN; cost DRAIN | Drain needs %d FP available and spends %d FP to start; processed shots cost more. |
| 18 | B | force | Protect@1 | min PROTECT; cost PROTECT | Protect needs %d FP available and spends %d FP to start; it also drains FP while active. |
| 19 | B | force | Absorb@1 | min ABSORB; cost ABSORB | Absorb needs %d FP available and spends %d FP to start; it also drains FP while active. |
| 20 | B | force | Rage@1 | min RAGE; cost RAGE | Rage needs %d FP available, spends %d FP, and requires at least 10 HP. |
| 21 | B | force | Jump@1 | literal | Force Jump drain starts from 4 to 20 FP by upward speed, then divides by jump level; race uses different scaling. |
| 22 | B | force | Throw@1 | min THROW; cost THROW | Saber Throw needs %d FP available and spends %d FP on launch; level 2 can steer. |
| 23 | B | force | Push@1, Pull@1 | cost PUSH; cost PULL; sum | Push costs %d FP and Pull %d FP: together they spend %d FP before any regeneration. |
| 24 | B | force | Push@1, Heal@1 | cost PUSH; cost HEAL; sum | Push costs %d FP and Heal %d FP: together they spend %d FP; Heal requires missing HP. |
| 25 | B | force | Pull@1, Throw@1 | cost PULL; cost THROW; sum | Pull costs %d FP and a saber throw %d FP: budget %d FP before the combination. |
| 26 | B | force | Speed@1, Push@1 | cost SPEED; cost PUSH; sum | Speed costs %d FP and Push %d FP: using both spends %d FP before any regeneration. |
| 27 | B | force | Heal@1 | cost HEAL; value HEAL_GAIN | Heal spends %d FP for up to %d HP at your level, capped by missing HP. |
| 28 | B | force | Grip@1 | value GRIP_START_COST; value GRIP_HOLD_COST | Grip spends %d FP after acquiring a target, then %d FP per hold tick. |
| 29 | B | force | Drain@1 | value DRAIN_SHOT_MS; value DRAIN_SHOT_COST | Drain fires at a nominal %d ms cadence and costs %d FP per processed shot; line misses can skip the cost. |
| 30 | B | force | Lightning@1 | value LIGHTNING_SHOT_COST; value LIGHTNING_SHOT_MS | Lightning's ongoing ticks cost %d FP each at a nominal %d ms cadence. |
| 31 | B | force | Heal@1 | literal | Level 1 Heal restores up to 5 HP per activation, limited by your maximum health. |
| 32 | B | force | Heal@2 | literal | Level 2 Heal restores up to 10 HP per activation, limited by your maximum health. |
| 33 | B | force | Heal@3 | literal | Level 3 Heal restores up to 25 HP per activation, limited by your maximum health. |
| 34 | B | force | Heal@1 | cost HEAL; value HEAL_GAIN | Your Heal spends %d FP to restore up to %d HP; it does not restore armor. |
| 35 | B | force | Heal@1 | cost HEAL; value HEAL_GAIN | At your level Heal costs %d FP for up to %d HP; missing fewer HP wastes the excess. |
| 36 | B | force | Push@1, Heal@1 | cost PUSH; cost HEAL; sum | Push and Heal cost %d and %d FP respectively: reserve %d FP before using both. |
| 37 | B | force | Heal@1 | literal | A heal spends FP that might otherwise answer Push or Grip. Consider the next exchange first. |
| 38 | B | force | Heal@1 | literal | Example: at 90 of 100 HP, a 25 HP heal can restore only 10 HP. |
| 39 | B | force | Heal@1 | cost HEAL | Heal costs %d FP even if the health cap limits its gain. Escape before paying that cost. |
| 40 | B | force | Heal@1, Push@1 | cost HEAL; cost PUSH; sum | Heal then Push costs %d plus %d FP, or %d FP total, before any regeneration. |
| 41 | B | force | Push@1 | literal | Push can disrupt an approaching opponent. Face them and account for the distance. |
| 42 | B | force | Push@1 | literal | Push can create space after a close exchange. Reposition instead of immediately following them. |
| 43 | B | force | Push@1 | cost PUSH | Push spends %d FP on use, including a missed attempt; aim before spending. |
| 44 | B | force | Push@1 | literal | A Push into open space gives you room to recover; a nearby wall limits the space gained. |
| 45 | B | force | Push@1, Throw@1 | cost PUSH; cost THROW; sum | Push then throw costs %d and %d FP: reserve %d FP before using both. |
| 46 | B | force | Push@1 | literal | After pushing an opponent away, avoid running straight into the counterattack. |
| 47 | B | force | Push@1 | literal | A failed Push still leaves you facing an opponent. Keep an exit direction in mind. |
| 48 | B | force | Push@1 | literal | Watch for an opponent's active Absorb before depending on Push to create space. |
| 49 | B | force | Push@1 | literal | Push can change an enemy's position; aim the follow-up at where they move, not where they stood. |
| 50 | B | force | Push@1, Pull@1 | cost PUSH; cost PULL; sum | Two different responses, Push then Pull, cost %d plus %d FP, or %d FP total. |
| 51 | B | force | Pull@1 | literal | Pull draws the fight closer. Prepare for a saber attack at the new distance. |
| 52 | B | force | Pull@1 | cost PULL | Pull spends %d FP on use; bringing the opponent closer also exposes your HP. |
| 53 | B | force | Pull@1 | cost PULL | A Pull costs %d FP; after it connects, aim your saber at the opponent's new position. |
| 54 | B | force | Pull@1 | literal | Pull toward open ground rather than trapping yourself against a wall. |
| 55 | B | force | Pull@1 | literal | After a missed Pull, move sideways instead of standing still in front of the opponent. |
| 56 | B | force | Pull@1 | literal | A close Pull can change the spacing quickly. Leave room to react rather than holding forward. |
| 57 | B | force | Pull@1 | literal | Against Absorb, Pull may be weakened; do not assume the opponent will move. |
| 58 | B | force | Pull@1, Throw@1 | cost PULL; cost THROW; sum | Pull plus a saber throw costs %d plus %d FP, or %d FP before regeneration. |
| 59 | B | force | Pull@1 | literal | After Pull, a kick can threaten the opponent at close range; a miss exposes you too. |
| 60 | B | force | Pull@1 | literal | Vary a Pull approach with movement so the opponent cannot prepare for the same entry each time. |
| 61 | B | force | Grip@1 | literal | Grip needs a target in front of you. Keep the opponent in view as you attempt it. |
| 62 | B | force | Grip@1 | value GRIP_START_COST; value GRIP_HOLD_COST | A successful Grip costs %d FP to start and another %d FP each hold tick. |
| 63 | B | force | Grip@1 | literal | A Grip attempt can fail against active Absorb. Keep a second plan for that response. |
| 64 | B | force | Grip@1 | literal | During Grip, watch the opponent's movement and your own distance; a kick can punish close range. |
| 65 | B | force | Grip@1 | value GRIP_HOLD_COST; value GRIP_HOLD_MS | Grip's hold ticks spend %d FP at a nominal %d ms interval after acquisition. |
| 66 | B | force | Grip@1 | literal | A gripped opponent may have a counter. Leave room instead of assuming they are helpless. |
| 67 | B | force | Grip@1 | literal | Grip from farther away reduces your exposure to a close kick, but acquisition still needs aim. |
| 68 | B | force | Grip@1 | literal | After Grip ends, check the opponent's position before choosing a saber follow-up. |
| 69 | B | force | Grip@1 | literal | A Grip into a wall gives the target less room to move, but also brings the wall into your escape path. |
| 70 | B | force | Grip@1 | value GRIP_START_COST | Successful Grip spends %d FP immediately; reserve more for the hold and your escape. |
| 71 | B | force | Lightning@1 | value LIGHTNING_SHOT_COST | Lightning needs 25 FP to start and its ongoing ticks cost %d FP each. |
| 72 | B | force | Lightning@1 | value LIGHTNING_SHOT_COST; value LIGHTNING_SHOT_MS | Lightning spends %d FP per tick at a nominal %d ms cadence; track the moving target. |
| 73 | B | force | Lightning@1 | value LIGHTNING_BASE_MAX | With a saber, Lightning rolls 1 to %d base damage per hit before Absorb and other damage rules. |
| 74 | B | force | Drain@1 | value DRAIN_SHOT_COST | Drain needs 25 FP to start and costs %d FP per processed shot; a line miss can skip that cost. |
| 75 | B | force | Drain@1 | value DRAIN_TARGET_LOSS | A clean Drain hit can remove up to %d FP at your level; Absorb and target FP alter the result. |
| 76 | B | force | Drain@1 | literal | After Drain, watch whether the opponent retreats or closes before choosing a saber attack. |
| 77 | B | force | Drain@1 | value DRAIN_HEAL_GAIN | A clean Drain hit can restore up to %d HP at your level, capped by missing HP. |
| 78 | B | force | Lightning@1 | literal | Absorb changes the result of incoming Force attacks. Watch for it before holding Lightning. |
| 79 | B | force | Lightning@1 | literal | Switching from Lightning to a saber approach changes the distance you need to manage. |
| 80 | B | force | Drain@1 | value DRAIN_SHOT_COST | Drain charges %d FP per processed shot; a missed line trace can skip that charge. |
| 81 | B | force | Protect@1 | cost PROTECT | Protect costs %d FP to activate and drains additional FP while active or absorbing damage. |
| 82 | B | force | Protect@1, Absorb@1 | literal | Protect and Absorb answer different threats: damage protection is not the same as Force absorption. |
| 83 | B | force | Absorb@1 | literal | Absorb can lessen incoming Push, Pull or Grip. Its result depends on power levels and rules. |
| 84 | B | force | Absorb@1 | cost ABSORB | Absorb costs %d FP to activate and drains more while active; turn it on before a Force attack. |
| 85 | B | force | Protect@1, Push@1 | cost PROTECT; cost PUSH; sum | Protect then Push costs %d plus %d FP, or %d FP before Protect's ongoing drain. |
| 86 | B | force | Protect@1, Absorb@1 | literal | Neither Protect nor Absorb removes the need to track an opponent's saber. |
| 87 | B | force | Absorb@1 | literal | Absorb can change a Force exchange, but it does not make close-range positioning irrelevant. |
| 88 | B | force | Protect@1, Absorb@1 | literal | Do not assume Protect and Absorb stay active together; server rules can make them exclusive. |
| 89 | B | force | Protect@1, Absorb@1 | literal | Choose a defensive power based on the threat you see, then keep an escape route open. |
| 90 | B | force | Protect@1, Absorb@1 | literal | A defensive power can end or be switched off. Recheck its active state before the next exchange. |
| 91 | B | force | Speed@1 | cost SPEED | Speed spends %d FP to start. Stop at saber range instead of overshooting your opponent. |
| 92 | B | force | Speed@1 | literal | Use Speed to change the distance, then face the opponent before committing to a saber attack. |
| 93 | B | force | Speed@1 | literal | Speed can help you retreat; avoid running backward into walls or ledges. |
| 94 | B | force | Speed@1 | literal | A fast approach spends FP and can carry you past your target. Leave room to turn. |
| 95 | B | force | Rage@1 | value RAGE_HEALTH_DRAIN; value RAGE_DRAIN_MS | Rage requires 10 HP to start and drains %d HP per tick at a nominal %d ms interval. |
| 96 | B | force | Rage@1 | literal | Rage has a recovery period after it ends. Plan your next exchange around that downtime. |
| 97 | B | force | Jump@1 | literal | Force Jump can change height, but the landing point matters as much as the takeoff. |
| 98 | B | force | Jump@1 | literal | Jumping uses FP under some conditions. Check your reserve after a high jump. |
| 99 | B | force | Jump@1 | literal | A jump can clear an attack line, but landing beside the opponent still leaves you in saber range. |
| 100 | B | force | Jump@1 | literal | Choose a landing area without a ledge or hazard before using a high Force Jump. |
| 101 | B | force | Throw@2 | literal | At throw level 2 or higher, aim the crosshair to steer the flying saber toward the traced point. |
| 102 | B | force | Throw@2 | literal | Think of the crosshair as a laser pointer: at throw level 2 or higher the saber steers toward its trace. |
| 103 | B | force | Throw@2 | literal | At throw level 2, the trace uses solid surfaces; level 3 also traces players. |
| 104 | B | force | Throw@3 | literal | At throw level 3, the steering trace can target players directly and updates faster than level 2. |
| 105 | B | force | Throw@2 | literal | A thrown saber turns toward your aim rather than teleporting to it. Lead a moving target. |
| 106 | B | force | Throw@2 | literal | A wall catches the steering trace. Move your aim around the wall to change the saber's path. |
| 107 | B | force | Throw@2 | literal | Aim past an opponent to guide the saber across their route, then watch where they move. |
| 108 | B | force | Throw@2 | literal | Aim above or below a target to change the saber's path; it still needs time to turn. |
| 109 | B | force | Throw@1 | literal | Release alternate attack to recall a thrown saber after its initial flight period. |
| 110 | B | force | Throw@1 | value THROW_BASE_DAMAGE | A thrown saber's base contact damage is %d before damage rules; blocks can prevent the hit. |
| 111 | B | force | Push@1, Throw@1 | cost PUSH; cost THROW; sum | Push then saber throw costs %d plus %d FP, or %d FP; aim at the displaced opponent. |
| 112 | B | force | Pull@1 | cost PULL | Pull costs %d FP; confirm the target moved before following with a saber swing. |
| 113 | B | force | Jump@1 | literal | After a Force Jump, turn toward the opponent before starting a saber swing on landing. |
| 114 | B | force | Speed@1 | cost SPEED | Speed costs %d FP to start; stop at saber range instead of running past the target. |
| 115 | B | force | Grip@1 | literal | A Grip attempt followed by a saber attack needs both FP and a clear route to the opponent. |
| 116 | B | force | Drain@1 | literal | Use Drain to pressure FP, then judge whether the opponent has closed into saber range. |
| 117 | B | force | Push@1, Heal@1 | cost PUSH; cost HEAL; sum | Push and Heal spend %d plus %d FP, or %d FP; use the separation to restore HP. |
| 118 | B | force | Pull@1 | literal | Pull and a kick require close spacing; do not assume a Pull guarantees the kick will connect. |
| 119 | B | force | Throw@1 | literal | A thrown saber may return while you reposition. Watch its path before starting another approach. |
| 120 | B | force | Push@1, Lightning@1 | cost PUSH | Push costs %d FP; reserve that much after using Lightning if you need Push to escape. |
| 121 | B | any | none | literal | Flipkick damage bypasses armor. Its damage depends on server settings and the kick. |
| 122 | B | any | none | value FLIPKICK_BASE_DAMAGE | This flipkick setting uses %d base damage, bypassing armor; other damage rules still apply. |
| 123 | B | any | none | literal | A flipkick can knock an opponent down, but knockdown rules vary by server. |
| 124 | B | any | none | literal | A kick that misses leaves you close to the opponent. Plan your landing direction. |
| 125 | B | any | none | literal | After a knockdown, watch the opponent's recovery before committing to another close attack. |
| 126 | B | any | none | literal | If you are knocked down, watch the attacker and choose a recovery direction away from hazards. |
| 127 | B | any | none | literal | A wall behind you reduces retreat space after a kick. Move away from it before re-engaging. |
| 128 | B | any | none | literal | A kick can deal HP damage through armor, so high AP does not make low HP safe. |
| 129 | B | any | none | literal | Example: 20 HP damage defeats a player at 20 HP even with armor, if the hit bypasses armor. |
| 130 | B | any | none | literal | Spacing matters for kicks: too far misses, while too close leaves little room to evade. |
| 131 | B | any | none | literal | Track the opponent's position while retreating; turning away can hide their approach. |
| 132 | B | any | none | literal | Move sideways as well as backward to avoid being driven into a wall. |
| 133 | B | any | none | literal | A missed attack is a cue to restore distance before starting another one. |
| 134 | B | any | none | literal | Look at the opponent's landing point after a jump, not only their position in the air. |
| 135 | B | any | none | literal | Keep your footing in view near ledges; a retreat is only useful if it has room. |
| 136 | B | force | none | literal | After the opponent spends FP, watch for their next movement rather than assuming they are empty. |
| 137 | B | any | none | literal | If both players are low on HP, a reckless approach can end the duel before your attack lands. |
| 138 | B | any | none | literal | An opponent backing away may be preparing a power. Do not follow in a straight line every time. |
| 139 | B | any | none | literal | After your saber returns, check spacing before pressing the next attack. |
| 140 | B | any | none | literal | Keep a clear route out of close range before starting an attack. |
| 141 | B | force | Throw@1 | literal | A thrown saber has travel time. Move out of its path rather than waiting for it to reach you. |
| 142 | B | force | Throw@1 | literal | Cover can obstruct a thrown saber. Keep track of the opponent while moving behind it. |
| 143 | B | force | Throw@1 | literal | The opponent can change the flight path of a level 2 or 3 throw. Watch the blade, not only the thrower. |
| 144 | B | force | Throw@1 | literal | An opponent's returning saber is still a moving object. Watch its path as you change position. |
| 145 | B | force | Throw@1 | literal | Closing on an opponent whose saber is away can pressure them, but watch for Force counters. |
| 146 | B | force | Throw@1 | value THROW_BASE_DAMAGE | The thrown blade's base contact damage is %d before armor and other damage rules; move off its path. |
| 147 | B | force | Throw@1 | literal | If a throw passes you, track where the thrower is aiming before moving into its path again. |
| 148 | B | force | Throw@1 | literal | Move sideways against a direct throw, then check whether the blade is turning toward you. |
| 149 | B | force | Throw@1 | literal | At close range, a thrower can still reposition. Do not aim your approach at their old location. |
| 150 | B | force | Throw@1 | literal | Avoid backing into a wall while tracking a throw; you need space to dodge its changing path. |
| 151 | A | force | Speed@1, Heal@1 | cost SPEED; cost HEAL; sum | Speed plus Heal costs %d and %d FP, totaling %d FP before regeneration or any other drain. |
| 152 | A | force | Push@1, Pull@1 | cost PUSH; cost PULL; sum | Push then Pull spends %d plus %d FP: reserve %d FP, even if the first action misses. |
| 153 | A | force | Pull@1, Throw@1 | cost PULL; cost THROW; sum | Pull then throw costs %d plus %d FP, totaling %d FP; these costs do not guarantee a hit. |
| 154 | A | force | Absorb@1, Speed@1 | cost ABSORB; cost SPEED; sum | Absorb and Speed cost %d and %d FP, totaling %d FP before Absorb's ongoing drain. |
| 155 | A | force | Protect@1, Pull@1 | cost PROTECT; cost PULL; sum | Protect and Pull cost %d and %d FP, totaling %d FP before Protect's ongoing drain. |
| 156 | A | force | Grip@1 | value GRIP_START_COST; value GRIP_HOLD_COST | Grip spends %d FP on acquisition and %d per hold tick; its starting threshold is a separate check. |
| 157 | A | force | Drain@1 | value DRAIN_SHOT_COST; value DRAIN_SHOT_MS | Drain spends %d FP per processed shot at a nominal %d ms cadence; a line miss can skip the cost. |
| 158 | A | force | Lightning@1 | value LIGHTNING_SHOT_COST; value LIGHTNING_SHOT_MS | Lightning spends %d FP per ongoing tick at a nominal %d ms cadence; it also has a start cost. |
| 159 | A | force | Throw@1 | cost THROW | Throw spends %d FP on launch; normal FP regeneration is suppressed while the blade is in flight. |
| 160 | A | force | Heal@1 | cost HEAL; value HEAL_GAIN | Heal spends %d FP for at most %d HP at your level. The health cap can reduce that return. |
| 161 | A | any | none | literal | Example: 35 HP after a hit followed by 20 armor-bypassing HP damage leaves 15 HP. |
| 162 | A | any | none | literal | Example: 18 HP and 100 AP still loses to 20 damage that explicitly bypasses armor. |
| 163 | A | force | Throw@1 | value THROW_BASE_DAMAGE | A thrown saber deals %d base contact damage before armor and other damage rules; a block can prevent it. |
| 164 | A | force | Drain@1 | value DRAIN_TARGET_LOSS | A clean Drain hit can remove up to %d FP; Absorb and the target's remaining FP affect the result. |
| 165 | A | force | Drain@1 | value DRAIN_HEAL_GAIN | A clean Drain hit can restore up to %d HP; your missing HP and target conditions limit the gain. |
| 166 | A | any | none | literal | Example: 30 AP minus 20 armor damage leaves 10 AP; the same hit may also damage HP. |
| 167 | A | any | none | literal | An armor-bypassing hit changes HP without spending AP. Reassess HP even if armor looks full. |
| 168 | A | any | none | literal | Damage and armor absorption vary with attack and rules. Do not treat HP plus AP as guaranteed survival. |
| 169 | A | any | none | literal | A small remaining HP pool changes the value of armor: a bypassing kick can finish through it. |
| 170 | A | any | none | literal | Measure a trade in HP lost, AP lost and position gained; one number does not describe the exchange. |
| 171 | A | force | Heal@3 | literal | Example: at 75 of 100 HP, level 3 Heal can restore 25 HP; at 95 it restores only 5. |
| 172 | A | force | Heal@2 | literal | Example: at 88 of 100 HP, a level 2 Heal restores 10 HP, leaving 98 HP. |
| 173 | A | force | Heal@1 | literal | Example: at 97 of 100 HP, even a level 1 Heal is capped to 3 HP restored. |
| 174 | A | force | Heal@1 | cost HEAL; value HEAL_GAIN | Heal spends %d FP for at most %d HP; near maximum health its FP-to-HP return is worse. |
| 175 | A | force | Pull@1, Heal@1 | cost PULL; cost HEAL; sum | Pull costs %d FP and Heal costs %d FP: %d FP in total before regeneration. |
| 176 | A | force | Heal@1, Push@1 | cost HEAL; cost PUSH; sum | Heal then Push spends %d plus %d FP, or %d FP, before later defense costs. |
| 177 | A | force | Heal@3 | literal | Level 3 Heal trades FP for up to 25 HP. Its actual gain is smaller near maximum health. |
| 178 | A | force | Heal@1 | cost HEAL; value HEAL_GAIN | Heal costs %d FP and restores up to %d HP, but zero AP. |
| 179 | A | force | Speed@1, Heal@1 | cost SPEED; cost HEAL; sum | Speed then Heal spends %d plus %d FP, or %d FP; check the cost before closing. |
| 180 | A | force | Push@1, Heal@1 | cost PUSH; cost HEAL; sum | Push then Heal costs %d plus %d FP, or %d FP. Push first to create space. |
| 181 | A | force | Push@1 | literal | Push creates separation in the direction of the exchange; position yourself before choosing the direction. |
| 182 | A | force | Push@1 | literal | Push near a wall can leave the opponent nearby. Prepare for the rebound in spacing. |
| 183 | A | force | Push@1 | literal | If the opponent activates Absorb, test their response with movement rather than repeatedly spending on Push. |
| 184 | A | force | Push@1 | literal | Use Push to interrupt an entry, then move off the line the opponent used to approach. |
| 185 | A | force | Push@1, Heal@1 | cost PUSH; cost HEAL; sum | Push and Heal cost %d and %d FP; keep %d FP for both before making space. |
| 186 | A | force | Push@1, Throw@1 | cost PUSH; cost THROW; sum | Push then throw costs %d and %d FP, or %d total; aim at the displaced opponent. |
| 187 | A | force | Push@1 | literal | If Push fails, retreat around cover rather than continuing the same close-range approach. |
| 188 | A | force | Push@1 | literal | Against a close gripper, Push can be a response; face the attacker and check available FP. |
| 189 | A | force | Push@1 | literal | Push toward open ground and keep a different path for your own retreat. |
| 190 | A | force | Push@1 | cost PUSH | Push spends %d FP each use; two attempts cost twice that amount before regeneration. |
| 191 | A | force | Pull@1 | literal | Pull changes both players' threat distance. Prepare a landing or escape route before drawing them in. |
| 192 | A | force | Pull@1 | literal | Pull toward your saber range, not blindly into contact; the opponent can attack at close range too. |
| 193 | A | force | Pull@1 | literal | A Pull and kick sequence needs aim, spacing and timing; the Pull alone does not guarantee a knockdown. |
| 194 | A | force | Pull@1 | literal | If Pull moves an opponent sideways, adjust your saber aim to their new position before swinging. |
| 195 | A | force | Pull@1 | cost PULL | Pull spends %d FP even if the follow-up swing misses; reposition before the opponent counters. |
| 196 | A | force | Pull@1 | literal | Pulling an opponent near a wall can shorten their travel; do not time the follow-up as if space were open. |
| 197 | A | force | Pull@1 | literal | After an opponent resists Pull, change angle before spending on the same entry again. |
| 198 | A | force | Pull@1 | literal | Watch whether Pull actually displaced the opponent before committing to a kick. |
| 199 | A | force | Pull@1 | literal | Pull then retreat is a spacing mix-up: it creates a close threat without requiring an immediate swing. |
| 200 | A | force | Pull@1, Push@1 | cost PULL; cost PUSH; sum | Pull then Push spends %d and %d FP, or %d FP before regeneration; keep an escape reserve. |
| 201 | A | force | Grip@1 | literal | Grip acquisition uses aim and target eligibility; a target behind obstruction is not a guaranteed catch. |
| 202 | A | force | Grip@1 | literal | A gripped opponent close enough to kick may punish the hold. Control separation during acquisition. |
| 203 | A | force | Grip@1 | literal | Grip followed by a saber approach requires releasing the hold and tracking the target's new location. |
| 204 | A | force | Grip@1 | literal | An opponent's Absorb can reduce or prevent Grip effects; watch the result before closing. |
| 205 | A | force | Grip@1 | value GRIP_START_COST; value GRIP_HOLD_COST | Grip requires a separate threshold, then spends %d FP on acquisition and %d FP per hold tick. |
| 206 | A | force | Grip@1 | literal | Choose the Grip line before starting: terrain can obstruct the target or your own retreat. |
| 207 | A | force | Grip@1 | literal | A failed Grip leaves a committed attacker; strafe out of the opponent's counterattack line. |
| 208 | A | force | Grip@1 | literal | If a Grip ends without moving the opponent as expected, do not swing at their assumed landing spot. |
| 209 | A | force | Push@1, Grip@1 | cost PUSH | After Grip, Push spends %d FP; Grip's acquisition and hold are additional costs. |
| 210 | A | force | Grip@1 | literal | An opponent attempting Grip needs a line to you. Break that line with terrain and change position. |
| 211 | A | force | Drain@1 | value DRAIN_TARGET_LOSS | A clean Drain hit removes up to %d FP at your level; Absorb and remaining target FP modify it. |
| 212 | A | force | Drain@1 | value DRAIN_TARGET_REGEN_DELAY_MS; value DRAIN_SELF_REGEN_DELAY_MS | Drain delays target FP recovery by %d ms after a valid hit and yours by %d ms per processed shot. |
| 213 | A | force | Drain@1 | value DRAIN_SHOT_COST | A processed Drain shot costs %d FP; a line-trace miss can skip the cost. |
| 214 | A | force | Drain@1 | value DRAIN_SHOT_MS | Drain has a nominal %d ms shot interval; track the opponent rather than firing into cover. |
| 215 | A | force | Drain@1 | value DRAIN_TARGET_LOSS; value DRAIN_HEAL_GAIN | A clean Drain hit can remove up to %d enemy FP and restore up to %d HP; both depend on conditions. |
| 216 | A | force | Lightning@1 | value LIGHTNING_SHOT_COST; value LIGHTNING_SHOT_MS | Lightning's ongoing ticks spend %d FP at a nominal %d ms interval; track the opponent. |
| 217 | A | force | Push@1, Lightning@1 | cost PUSH | Push costs %d FP; reserve it after Lightning's starting and ongoing costs. |
| 218 | A | force | Lightning@1 | value LIGHTNING_BASE_MAX | With a saber, Lightning rolls 1 to %d base damage per hit; Absorb and armor change the result. |
| 219 | A | force | Lightning@1 | value LIGHTNING_SHOT_COST | Lightning requires 25 FP to start and spends %d FP per ongoing tick; preserve a reserve for defense. |
| 220 | A | force | Drain@1 | value DRAIN_SHOT_COST | Drain spends %d FP per processed shot. After a hit, a saber approach pressures HP as well as FP. |
| 221 | A | force | Absorb@1 | literal | Absorb's effect on Push, Pull and Grip depends on the relative levels and server settings. |
| 222 | A | force | Absorb@1 | literal | Absorb can convert some incoming Force into FP; conversion depends on the attack and active rules. |
| 223 | A | force | Protect@1 | cost PROTECT | Protect costs %d FP to activate, then spends more over time and on qualifying damage. |
| 224 | A | force | Protect@1 | literal | Protect does not replace movement: each protected hit can still consume resources or deal damage. |
| 225 | A | force | Protect@1 | literal | Without the server's Force-combo option, activating Protect stops Absorb and Rage. |
| 226 | A | force | Absorb@1 | literal | Without the server's Force-combo option, activating Absorb stops Protect and Rage. |
| 227 | A | force | Rage@1 | literal | Without the server's Force-combo option, activating Rage stops Protect and Absorb. |
| 228 | A | force | Absorb@1, Protect@1 | literal | Pick Absorb for an incoming Force threat and Protect for qualifying damage; they are not interchangeable. |
| 229 | A | force | Protect@1, Absorb@1 | literal | Watch the active-power indicator after switching defenses; server rules may have stopped the previous one. |
| 230 | A | force | Push@1, Pull@1, Grip@1 | literal | An opponent's active Absorb changes the expected value of Grip, Push and Pull; change the approach. |
| 231 | A | force | Speed@1 | literal | Speed helps close a gap, but an overshoot gives the opponent your back. Brake before saber range. |
| 232 | A | force | Speed@1 | literal | Speed also helps disengage; steer around terrain rather than spending the full duration against a wall. |
| 233 | A | force | Speed@1, Pull@1 | cost SPEED; cost PULL; sum | Speed plus Pull costs %d plus %d FP, or %d FP before regeneration; stop at saber range. |
| 234 | A | force | Jump@1 | literal | A Jump entry changes height and FP. Choose the landing before planning the saber follow-up. |
| 235 | A | force | Jump@1 | literal | A high Jump can cross an attack line, but a predictable landing exposes you to a waiting opponent. |
| 236 | A | force | Jump@1 | literal | Jump away from the attacker's line, then turn toward them before choosing a landing attack. |
| 237 | A | force | Rage@1 | value RAGE_HEALTH_DRAIN; value RAGE_DRAIN_MS | Rage needs 10 HP to start and drains %d HP every nominal %d ms; it stops at 1 HP. |
| 238 | A | force | Speed@1, Push@1 | cost SPEED; cost PUSH; sum | Speed and Push cost %d and %d FP, totaling %d FP before regeneration; use Push to disengage. |
| 239 | A | force | Jump@1, Push@1 | literal | A low-FP jump can leave no reserve for Push after landing. Compare both costs before takeoff. |
| 240 | A | force | Jump@1 | literal | Before a high Jump near an edge, choose an exit that does not depend on a second jump. |
| 241 | A | force | Throw@2 | literal | At throw level 2, the blade steers toward the crosshair's solid-surface trace, not directly onto players. |
| 242 | A | force | Throw@3 | literal | At throw level 3, the steering trace includes players, allowing direct aim at the target. |
| 243 | A | force | Throw@2 | literal | Treat the crosshair as a laser pointer: a nearby wall becomes the steering destination. |
| 244 | A | force | Throw@2 | literal | Move the pointer beyond the opponent to send the flying saber across their route rather than at their feet. |
| 245 | A | force | Throw@2 | literal | Steer from one side of the opponent to the other; the blade turns over time, so lead their movement. |
| 246 | A | force | Throw@2 | literal | Aiming high changes the traced destination and the blade's path; it does not create an instant hit. |
| 247 | A | force | Throw@2 | literal | At level 2, placing the pointer on a player traces through them to solid geometry behind them. |
| 248 | A | force | Throw@3 | literal | Level 3 steering updates more frequently than level 2, but the flying blade still needs time to turn. |
| 249 | A | force | Throw@2 | literal | A distant pointer destination and a nearby destination produce different paths from the saber's position. |
| 250 | A | force | Throw@2 | literal | As you move, the steering trace starts at your view position. Re-aim after changing cover. |
| 251 | A | force | Throw@2 | literal | Steer around a wall by moving your view to an unobstructed trace; the blade cannot pass through the wall. |
| 252 | A | force | Throw@2 | literal | After a target sidesteps, move the pointer ahead of their path instead of tracing where they were. |
| 253 | A | force | Throw@2 | literal | Do not chase the target's every movement with the pointer; blade travel time can make the aim lag. |
| 254 | A | force | Throw@2 | literal | Watch the flying saber as well as the pointer: it approaches the traced point from its location. |
| 255 | A | force | Throw@2 | literal | A pointer aimed at the floor can pull the saber down; lift the aim to change its flight path. |
| 256 | A | force | Throw@2 | literal | Aiming at a wall behind the target gives level 2 steering a destination beyond the target. |
| 257 | A | force | Throw@1 | literal | Release alternate attack after the initial flight period to call the saber back rather than hold it out. |
| 258 | A | force | Throw@1 | literal | The saber also returns at its range limit or after its maximum flight time; do not plan an indefinite hold. |
| 259 | A | force | Throw@1 | literal | During the return, move to avoid the opponent while tracking where your saber will reach you. |
| 260 | A | force | Throw@1 | cost THROW | Throw spends %d FP on launch and suppresses normal FP regeneration until the saber returns. |
| 261 | A | force | Push@1, Throw@1 | cost PUSH; cost THROW; sum | Push then throw spends %d plus %d FP, or %d FP; adjust aim after Push moves the target. |
| 262 | A | force | Pull@1, Throw@2 | literal | Pull then steer the saber across the opponent's new position; check both the Pull and throw costs. |
| 263 | A | force | Grip@1, Throw@2 | literal | After a Grip release, steer a thrown saber toward the opponent's actual path, not their last held position. |
| 264 | A | force | Speed@1, Throw@2 | literal | Use Speed to change the angle of a steered throw; the aim trace follows your new view position. |
| 265 | A | force | Throw@2 | literal | Throw then sidestep: update the pointer after moving because cover may now block the trace. |
| 266 | A | force | Throw@1 | literal | After an unsuccessful throw, check your FP and recall the saber before entering close range. |
| 267 | A | force | Throw@1 | literal | Use a throw to pressure retreat, then keep enough distance to react if the opponent closes instead. |
| 268 | A | force | Push@1, Throw@1 | cost PUSH; cost THROW; sum | Push and throw spend %d and %d FP: %d FP total even if Push fails to displace the target. |
| 269 | A | force | Throw@2 | literal | Steer a throw past cover only after your view has a clear trace beyond the obstruction. |
| 270 | A | force | Throw@1 | literal | A returning saber changes your attack options; face the opponent while waiting for it to reach you. |
| 271 | A | any | none | literal | Flipkick uses armor-bypassing damage. AP cannot absorb it, although other damage rules may apply. |
| 272 | A | any | none | value FLIPKICK_BASE_DAMAGE | This flipkick setting uses %d base damage; gripkick bonuses and other damage rules can change the result. |
| 273 | A | any | none | literal | Example: two hypothetical 20 HP bypassing kicks remove 40 HP, regardless of AP. |
| 274 | A | any | none | literal | A kick's knockdown is not guaranteed: server rules and target state affect the outcome. |
| 275 | A | any | none | literal | Do not commit to a grounded follow-up until the kick actually knocks the opponent down. |
| 276 | A | force | Grip@1 | literal | During Grip, a close opponent can kick. Distance reduces that counter's immediate reach. |
| 277 | A | force | Pull@1 | literal | Pull can set up a close kick, but the opponent's counterattack has the same short distance. |
| 278 | A | any | none | literal | After a kick, use the resulting spacing to decide between pursuit and retreat, not an automatic chain. |
| 279 | A | any | none | literal | A kick near a ledge can alter both players' landing paths. Keep your own landing on solid ground. |
| 280 | A | any | none | literal | At low HP, treat a close kick as a direct HP threat even with substantial armor remaining. |
| 281 | A | force | Push@1, Heal@1 | cost PUSH; cost HEAL; sum | Push then Heal costs %d plus %d FP, totaling %d FP; use the separation to recover HP. |
| 282 | A | force | Drain@1 | value DRAIN_SHOT_COST | Drain spends %d FP per processed shot; after a hit, check spacing before closing with the saber. |
| 283 | A | force | Speed@1 | cost SPEED | Speed spends %d FP; close and stop at saber range rather than running past the target. |
| 284 | A | force | Pull@1 | cost PULL | Pull spends %d FP; confirm displacement before committing to a close kick. |
| 285 | A | force | Grip@1 | value GRIP_START_COST | Grip spends %d FP on acquisition, plus hold ticks; release and track the target before swinging. |
| 286 | A | force | Push@1, Lightning@1 | cost PUSH | Push spends %d FP; keep that reserve after Lightning if you plan to Push away. |
| 287 | A | force | Jump@1 | literal | Jump, land, then turn toward the target: an airborne approach does not guarantee a clean saber hit. |
| 288 | A | force | Absorb@1 | literal | Absorb, close distance, then use the saber: active defense does not guarantee the opponent cannot counter. |
| 289 | A | force | Protect@1 | literal | Protect, move off the attack line, then reassess FP: absorbing damage consumes resources. |
| 290 | A | force | none | literal | After any sequence, check HP, AP, FP and spacing before choosing the next one. |
| 291 | A | force | Heal@1, Throw@1 | cost HEAL; cost THROW; sum | Heal then throw costs %d plus %d FP, or %d FP; a full-health player cannot use Heal. |
| 292 | A | force | Speed@1, Throw@1 | cost SPEED; cost THROW; sum | Speed then throw costs %d and %d FP, or %d FP; normal FP regeneration stops during the throw. |
| 293 | A | force | Absorb@1, Heal@1 | cost ABSORB; cost HEAL; sum | Absorb and Heal cost %d plus %d FP, or %d FP before Absorb's ongoing drain. |
| 294 | A | force | Heal@1 | cost HEAL; value HEAL_GAIN | Heal costs %d FP for up to %d HP; any cap-limited gain lowers that efficiency. |
| 295 | A | force | Throw@1 | value THROW_BASE_DAMAGE | A throw's base contact damage is %d before armor and other damage rules; a block can negate it. |
| 296 | A | force | Grip@1 | value GRIP_START_COST; value GRIP_HOLD_COST | A successful Grip costs %d FP initially, then %d FP each hold tick; reserve more for escape. |
| 297 | A | force | Drain@1 | value DRAIN_SHOT_COST | Processed Drain shots cost %d FP each; end the hold before losing your escape reserve. |
| 298 | A | force | none | literal | If the opponent moves behind cover, cancel the aimed follow-up and choose a new line. |
| 299 | A | force | none | literal | If a defensive power ends during a sequence, do not assume it remains active for the next hit. |
| 300 | A | force | none | literal | When a combination fails, identify whether aim, distance, FP or an enemy counter caused the failure. |
