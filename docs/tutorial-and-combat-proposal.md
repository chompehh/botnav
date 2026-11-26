# Combat and tutorial proposal for review

**Status: historical proposal.** The tutorial has since been revised; the
implemented catalog and current limitations are in `bot-improvements-review.md`.
The 98 items below remain proposals, not a list of enabled messages. The reported
screenshots showed jargon such as "force crashes" and "collapse frame" and
repeated generic advice immediately after an opening grip kick. The new tutorial
suppresses low-signal duels and uses plain-English lessons instead.

The original PDF is a historical reference; server mechanics and available
powers can differ. The counter and throw proposals should be checked against the
actual implementation and tests rather than assumed complete from this document.

## 1. Saber throws

### What the code currently does

- `NewBotAI_SaberThrowShouldHold` in `codemp/game/ai_main.c` treats both
  `NewBotAI_ShouldSuppressDrainlockSaberThrow` and
  `NewBotAI_WouldThrowInviteDrainlock` as immediate recall conditions. The
  former preserves the bot's own drain opportunity; the latter estimates
  whether *starting* a throw would leave too little force to escape an enemy
  drain. Its fixed 40-point throw budget is subtracted from the **current**
  force even after the throw has already spent some force. Thus a throw that
  was safe to start can become unsafe by this estimate during flight, without
  demonstrating that continuing it actually worsens the trade.
- The same function can also recall for lethal danger, invalid or lost target,
  forbidden force use, or a returning saber. Health disadvantage, nonpositive
  aggression and a large enemy force lead are softer reasons. They can still
  end a throw after the protected travel/completion windows expire.
- `NewBotAI_SaberThrowContextHoldCommitted` in `ai_combat_tuning.h` imposes
  duration and resource-dependent limits: normal level 2/3 hold is capped at
  2200 ms, with extensions capped at 2800/3200 ms; level 1 is capped at 750 ms.
  Depending on context, smaller travel and completion budgets can apply. The
  first-pass protection ends after 1500 ms or when the saber is not marked as
  heading toward the target. A sticky pass marker is based on horizontal
  flight direction, not confirmation of a hit.
- Steering and final input have separate clearance checks that can put the
  throw in recall, even if the hold decision wanted to continue. Visibility
  reacquisition is limited. The engine in `w_saber.c` independently returns
  the saber for released input after 500 ms, after six seconds, for range or
  other engine restrictions, and on relevant collisions. These cannot all be
  fixed by changing the bot's hold timer.
- These are credible code paths, not a diagnosis of the frequency of each
  cause in the reported duels. `bot_combatdebug` and captured `st:` decision
  notes already distinguish several hold/recall reasons; verify real instances
  before assigning a dominant cause.

### Proposed behavior

1. Separate **launch** budget checks from **in-flight** continuation. Snapshot
   the start situation and evaluate remaining cost and the current, observable
   enemy drain threat, rather than charging the full prospective cost twice.
   Preserving a possible drain opportunity alone should not automatically
   cancel a progressing throw that has not lost its advantage.
2. Keep holding while a legal, clear, viable interception is progressing and
   the bot has not actually lost enough health/force advantage or acquired a
   credible immediate drain or lethal threat to justify recall. An upcoming
   intercept must not be discarded merely because an arbitrary completion
   timer, aggression setting, or a brief sideways steering segment expired.
   Distinguish an actual hit, a geometric pass and an orbiting bypass.
3. Keep hard engine limits, valid target/entity checks, confirmed obstruction,
   resource exhaustion, lethal danger and meaningful drain-lock protection.
   Do not hold forever when interception has stalled, the target cannot be
   reacquired, or repeated safe paths fail. Assess the live cost/benefit rather
   than treating current force or time alone as proof of a lost advantage.
4. Record structured reason and resource snapshots at launch, continuation and
   recall, including target-relative flight, pass/hit and trace outcome. Keep
   debug readable and rate-limited.

**Acceptance:** exercise low/high force, active and potential enemy drain,
absorb, a held advantage, genuinely worsening advantage, missed/obstructed
flight, a curving approach, actual hit/return and engine timeout. Both the
mid-frame and final input checks must agree. In-game traces should show a
progressing safe throw surviving the old premature recall points.

## 2. Learning counterattack timing

### What exists and what is missing

- The saber tactic already tracks enemy swing start, chain count, current and
  predicted range, closing speed, own recovery, and a narrow 0–150 ms counter
  for linked sweeps. It also has poke and animation wiggle. However,
  `NewBotAI_SaberNeedsSafetyExit` and later defense, escape and attack-legal
  gates can still choose or enforce a reset; the counter window is not a
  learned, per-direction timing policy.
- `LocalBotLearnedSequence` stores coarse action-token sequences and outcomes;
  it does not identify a particular opponent swing's directional phase or
  trial timing. Version-14 motor capture has input spans and outcomes, but its
  current learned profiles mainly adjust hold and aim rates, not counter-entry
  timing. A generic positive `swing` preference is not evidence that a
  specific entry moment or recipe succeeded.

### Proposed behavior

1. Trace the full path from sweep detection through dodge, tactic selection,
   escape phase, input and actual accepted swing. Identify which gate suppresses
   legal counters during sustained approach; distinguish a legitimate
   recovery/safety exit from a preventable retreat loop.
2. Capture complete, paired counter trials: opponent stance, actual directional
   move and chain position; observable animation start/progress and recovery;
   current and predicted distance, closing speed and footing; own legal
   recovery state; chosen entry delay, movement, recipe, poke/wiggle and actual
   accepted swing; subsequent dealt/received damage and interrupted attacks.
   Use engine animation/move data and observed timings rather than assuming
   every direction has the same wind-up, dangerous arc or recovery. A hit is
   not proof the attack caused an interruption.
3. Store versioned, bounded aggregate trial evidence across duels; distinguish
   humans and bot trials, successes and failures, missing data and incomplete
   captures. Never reinterpret old coarse tokens as phase-specific examples.
   Load/cache outside the per-frame decision path; maintain reset/export
   semantics and preserve existing learning data.
4. Select among a small set of legal entry timings, distances and existing
   swing combinations, including poke/wiggle when appropriate. Begin with the
   current evidence-based counter as a prior; vary slightly before and after
   comparable moments to gather evidence, without assuming the opponent's
   future inputs. Prefer proven positive trades after sufficient independent
   samples, but retain bounded exploration and avoid overfitting sparse data.
   Compare damage exchanged and defensive consequences, not win rate alone.
5. In saber-only duels, avoid an endless dodge/backpedal loop against sustained
   sweeps when a safe, legal counter entry is available. Keep actual engine
   acceptance, blocked/broken-parry recovery, movement/hazard safety and
   existing attack restrictions authoritative. A learned preference must not
   force an illegal swing or suppress a necessary defense.

**Acceptance:** deterministic cases for both sides/directions and differing
animation lengths, approach speeds, distance bands, multi-swing chains, failed
and successful attempts, missing samples, and recovery/obstacle gates. Verify
that trials explore multiple timings, outcomes persist across restart, and
reliable favorable evidence shifts later choices without overriding safety.
Use captured duel replays/in-game observation to check actual attacks rather
than only calculated scores.

## 3. Tutorial behavior and evidence

### Current limitations

`G_MaybeQueueBotTutorial` (`g_account.c`) runs after a bot beats a human.
Specific issues require three observations and two occurrences, plus a
minimum event count and duration, but the beginner/generic branches can still
send advice earlier. Recovery-specific advice is also queued before that
general gate. A short duel can trigger generic advice rather than silence. Only
logged-in identities load advice history; guests already have a normalized
`iphash:` tracking identity but it is not used to load tutorial history.
Guest beginners can remain in the basics branch indefinitely. Arcade has a
separate queue path, which also needs consistent gating.

### Proposed policy

1. For a new player, start unsolicited coaching only after **three completed,
   sufficiently informative duels**; do not treat a short opening grip-kick
   loss, dropped capture, disconnect or other low-signal duel as evidence of
   a strategic flaw. An opening grip-kick finish produces no strategic
   coaching for that duel, including for a player with history. Count wins and
   losses for observation; do not require players to lose to build history.
2. A logged-in player or returning guest may use established tutorial
   evidence immediately, but login alone is not proof of a pattern. Require at
   least two comparable occurrences across informative duels for personalized
   recurring-pattern claims. Individual, explicitly described observations
   may be offered only when capture is complete; do not call them habits.
   Without enough evidence, remain silent rather than invent a diagnosis.
3. Load guest history using the existing IP-derived identity, without storing
   raw IPs in tutorial records. IPs can change, be shared or collide; never
   claim this identifies a particular person reliably, and do not automatically
   merge guest and account records. The existing 32-bit unsalted hash is
   pseudonymous, not anonymous or collision-resistant; evaluate a keyed,
   server-local identifier and migration/retention policy before extending
   durable use. Avoid exposing identifiers in chat or exports. Document reset
   behavior and make database failures visible without fabricating history.
4. Avoid double-counting the current duel between persisted and session
   history. Store actionable evidence, counts, relevant mode, and recent advice
   identifiers so rotation works across reconnects; retain recent history
   separately from lifetime aggregates. Define how `resetdueltrack` and
   learning resets affect tutorial history and test migration from old data.
5. Choose at most one coherent lesson per eligible duel, with short follow-up
   lines only when needed; respect existing chat mode, privacy, queue limits,
   cooldowns and `bot_nochat`. Suppress repeated lessons until useful new
   evidence or a reasonable interval. A saber-only match must not receive
   full-force instructions. Introductory lessons are teaching, not diagnoses.
6. Pattern evidence must include the actions and outcomes needed for the
   exact claim. Record counts and sequence examples, timing and force/range
   snapshots where supported. If an event stream is incomplete, a power is
   unavailable, or the causal relationship is unclear, omit the claim.
   Introduce additional evidence extraction for the patterns below; many are
   **not** presently supported by the existing seven primary-issue labels.
7. Keep advice adaptable to server rules and power availability. The Shades FF
   manual is a historical reference, not a guarantee of current mechanics:
   validate each detailed instruction against this mod before enabling it.
   Do not teach the manual's JA+-specific hiltthrow command, speculative
   saber-throw rejection or fixed costs/timings as universal features.

**Acceptance:** first three eligible duels, reconnect and restart, established
account and guest history, shared/changeable IP caveats, opening grip-kick,
incomplete capture, false repeated-pattern claims, normal and arcade paths,
saber-only/full-force filtering, opt-out and rotation. Include synthetic
sequences where only the correctly evidenced message is eligible.

## 4. Complete proposed tutorial chat catalog

Each quoted line is one proposed chat message, written in original wording.
Text in `{braces}` is a bounded, server-generated value, never arbitrary player
input. Paired lines within a numbered item are one lesson. No player receives
the whole catalog at once. Lessons marked **teaching** require sufficient
general history but make no claim about the player; **pattern** lessons
require the exact evidence stated. Counts refer to comparable, complete
observations, not every duel. `>=2` means at least two occurrences in at least
two informative duels unless the lesson explicitly says otherwise.

### Beginner: movement and saber — teaching

1. "Keep your opponent in view while you move. If you lose sight of them, finding your next defense gets harder."
2. "Try circling with left or right while turning to face your opponent. You can move without giving them your back."
3. "If you only run straight at someone, they can prepare for you. Try approaching from the side as well."
4. "After a swing misses, leave yourself room to recover before walking back into their saber."
5. "A fan is a series of side-to-side saber swings. Practice linking the swings while moving, rather than standing still."
6. "Try a short fan, then pause or change direction. An endless rhythm is easier to read."
7. "A poke is a short saber attack aimed into an opening. Start with one controlled swing before trying a longer combo."
8. "Small aim movements during a swing can change where your blade travels. Practice gently; wild turns can cost you sight of your opponent."
9. "When someone keeps swinging toward you, try entering with your own swing at different moments instead of only backing away."
10. "Not every swing opens at the same speed. Watch the animation and practice when your blade reaches the other player."
11. "If your saber is knocked away, focus on recovering it and making space before rushing back in."
12. "A strafe jump combines forward movement, a sideways key, jump, and a smooth turn toward that side. Practice somewhere safe first."
13. "To link strafe jumps, try jumping again as you land while keeping your turn smooth. Avoid practicing near a ledge."
14. "Walls can help you escape or change height, but check where you will land before committing."

### Beginner: full-force essentials — teaching

15. "Keep an eye on your force bar. Save enough for a defense or escape instead of spending everything on one attack."
16. "Bind your main powers where you can reach them without stopping your movement. Practice each one before combining them."
17. "A flip kick uses a quick double jump near an opponent. Practice the spacing before trying to chain it into another move."
18. "A side kick uses a sideways movement and jump near an opponent. Give yourself room to land afterward."
19. "A pull kick means pulling someone toward you and timing a kick as they arrive. Practice the pull and kick separately first."
20. "A grip kick means gripping an opponent and bringing them into range for a kick. Focus on control before trying several kicks."
21. "If you are gripped, try to find the attacker and pull them when your power and angle allow it."
22. "A pull throw combines a saber throw with a pull. Try it on an opponent who is already committed to another action."
23. "A pull-throw-kick adds a kick after the throw and pull. Start slowly and check that each part actually connects."
24. "A thrown saber can be steered with your view at higher throw levels. Watch the blade, not just your crosshair."
25. "A saber can block a throw, so try guiding the blade around the opponent instead of always throwing straight at their front."
26. "To defend a throw, keep track of the blade and move sideways. Also watch the thrower so they cannot walk up unnoticed."
27. "Drain can restore health while taking force from an opponent. A short tap may be enough; watch both bars."
28. "If your opponent is draining you, avoid spending your last force point without a plan. Make space or use an available counter."
29. "When knocked down, watch for the next attack before choosing how to get up. Standing immediately is not always safest."
30. "Practice push, pull and movement while under pressure. Knowing your escape keys matters more than a complicated combo."

### Developing play — teaching

31. "Try stepping sideways before you close distance. An approach angle is simply the direction you enter from."
32. "Mix a quick entry with a delayed one. If both start the same way, the other player has less time to guess which you chose."
33. "After they miss a swing, try moving in during the recovery instead of chasing the blade through its dangerous arc."
34. "If your attack is blocked, reset your spacing before starting again. Attacking during your own recovery can leave you exposed."
35. "Against a long fan, watch for the start and end of each swing. Try a short counter at different points and notice which trades work."
36. "Mix a poke into your longer saber chains. A brief attack can make someone react without committing you to the same full combo."
37. "Use small aim adjustments to bring the blade toward an opening, but keep your opponent visible enough to defend."
38. "A kick can leave you landing close to your opponent. Plan whether to continue pressure or move away before you jump."
39. "When your opponent is knocked down, don't rush every follow-up. Watch their get-up direction and be ready to adjust."
40. "If you are knocked down, try different safe get-ups rather than always rolling the same way."
41. "If they break your grip quickly, try a different start or direction next time instead of repeating the same motion."
42. "During a grip, move the opponent smoothly and vary where you bring them for kicks. Changing the kick alone may not change your angle."
43. "When a grip ends without a finish, check your remaining force before chasing. You may need to defend their next move."
44. "Try a short drain after taking damage rather than holding it automatically. Stop once you have enough health for your next exchange."
45. "After draining, let some force return when you can. Repeating an expensive attack immediately can undo the benefit."
46. "A drain lock is sustained pressure that leaves someone too low on force to respond comfortably. Keep an escape option before it starts."
47. "Absorb can help against force attacks when it is available and active. Practice activating it before the attack reaches you."
48. "Protect, heal, rage, speed and seeing each have different costs and restrictions. Learn what your current loadout actually allows."
49. "Use speed with a purpose: close distance, escape or change the timing of an entry. It still costs force you may need later."
50. "If a thrown saber misses, notice where it returns. Chasing the owner without watching the blade can get you hit."
51. "Use cover to make a throw harder to steer at you, but leave yourself an exit instead of getting trapped against the wall."
52. "Practice attacks at different distances. A combo that works up close may miss or leave you exposed farther away."

### Advanced force and matchup strategy — teaching

53. "If your opponent always answers a saber attack with grip, show them a short swing and prepare for their grip instead of finishing the same chain."
54. "If they chase every retreat, stop short or change direction occasionally. The goal is to make their next attack miss, not simply run farther."
55. "Vary when you use grip. If every approach ends in grip, your opponent can prepare the same escape each time."
56. "A grip fake can draw an early turn or pull. Practice changing direction without losing control of the opponent."
57. "Try moving around a gripped opponent as well as turning them. Changing your own position can change where they need to aim."
58. "In a grip, vary both the timing and the direction of your kicks. Repeating either can make your next position predictable."
59. "When you escape a grip, note whether you still have enough force to counter. Sometimes making space is better than an immediate attack."
60. "If your opponent expects a drain after every hit, show a different response occasionally and watch what they do."
61. "When you have a force lead, short drain taps can maintain pressure while leaving room for another action. Watch whether the lead is actually growing."
62. "If your drain is healing more than you need, try shorter taps. Spending less can leave you with an escape or follow-up."
63. "If an opponent keeps their force low to limit your drain, look for a safe opening to attack instead of holding drain into nothing."
64. "Before a long throw, consider both the likely damage and the force needed to survive their reply. Cancel if the situation turns against you."
65. "Against repeated throws, mix movement and cover. If the owner approaches behind the blade, do not focus so hard on the saber that you lose them."
66. "A pull or push can create an opening, but a missed force action can leave you vulnerable. Confirm the target and your follow-up first."
67. "After a pull kick, check whether the opponent is still exposed before spending on another. The same follow-up will not always be safe."
68. "If both players are low on health, compare your available force and distance before choosing an all-in attack."
69. "Learn the map's safe routes and corners. A good escape leaves room to turn and defend, not just distance from the opponent."
70. "In team fights, keep track of allies and other threats before chasing a low-health opponent away from support."
71. "Practice one change at a time: entry distance, delay or swing direction. That makes it easier to tell which change improved the trade."
72. "A move that works once may just have caught someone by surprise. Look for repeated results before making it your default."

### Personalized sequence lessons — pattern evidence required

For these templates, `{n}` is the actual number of qualifying occurrences and
`{duels}` the actual number of distinct informative duels. Show counts only
when the underlying observations and time windows were recorded. These
phrases describe correlation, not certain intent or causation.

73. **Low-force punish:** `>=2` sequences where the player's force fell below
    the validated escape cost and an opponent's attack landed before recovery.
    "In {n} exchanges across {duels} duels, you ran low on force and were hit before it recovered. Save some for your escape."
74. **Long drains before counter:** `>=2` complete drain holds followed by
    damage while the player remained exposed.
    "I saw {n} long drains followed by a hit from me. Try a shorter tap, then move or defend before draining again."
75. **Drain into absorb:** `>=2` confirmed drain attempts against active absorb.
    "Your drain met my active absorb {n} times. Watch for it and switch attacks rather than holding drain."
76. **Late absorb:** `>=2` confirmed activations only after drain began and
    after measurable force loss; only when absorb was available.
    "In {n} exchanges, you turned absorb on after my drain had already started. Try preparing it sooner when I close in."
77. **Grip escape without aim:** `>=2` pull attempts while gripped, outside
    the valid aim region, with no break.
    "You tried to pull out of my grip {n} times while I was outside your aim. Find me first, then pull when you have the chance."
78. **Repeated grip start:** `>=2` comparable same-direction grip starts
    followed by confirmed enemy escapes.
    "I broke {n} grips that started in a similar direction. Try changing the first movement or waiting for a different opening."
79. **Repeated grip kick position:** `>=2` comparable repeated kick positions
    followed by a confirmed break.
    "You brought me back to a similar position before {n} grip escapes. Try a different direction for the next kick."
80. **Post-grip overcommit:** `>=2` grip endings followed by a force spend
    and confirmed counter before meaningful recovery.
    "After {n} of your grips ended, I caught your immediate follow-up. Check your force and my position before chasing."
81. **Throw path punish:** `>=2` confirmed hits from an enemy thrown saber
    while the player approached its recorded path.
    "You moved into my thrown saber {n} times. Watch the blade's path while you approach, not just where I am standing."
82. **Throw cancellation:** `>=2` player throw releases before contact while
    the blade was still approaching; only with complete button and flight data.
    "You released {n} throws while the blade was still approaching me. Try holding a little longer when it is safe."
83. **Throw caught on return:** `>=2` confirmed hits from a returning saber.
    "My saber hit you on its way back {n} times. Keep watching it after the outward pass."
84. **Same get-up direction:** `>=2` recorded matching get-ups followed by
    confirmed opponent hits.
    "I hit you after the same get-up direction {n} times. Try waiting or choosing another safe direction."
85. **Recovery re-entry:** `>=2` confirmed hits while advancing during
    saber return or broken-parry recovery.
    "I hit you {n} times while you were moving back in during saber recovery. Let the recovery finish, then enter from the side."
86. **Repeated straight entry:** `>=2` straight approaches followed by
    confirmed enemy hits, with sufficient movement data.
    "I caught {n} straight approaches. Try a sideways step or a pause before coming into my range."
87. **Same entry timing:** `>=2` comparable entry delays after the same
    opponent action followed by confirmed unfavorable trades.
    "Your entries came at a similar moment {n} times, and I won those trades. Try entering a little earlier or later."
88. **Retreat against a fan:** `>=2` extended retreats during linked enemy
    swings followed by confirmed damage; no claim that countering was legal.
    "I kept landing swings as you backed away in {n} exchanges. When you have recovered, try testing a counter between my swings."
89. **Counter too early:** `>=2` accepted counters during the recorded
    opponent wind-up that led to unfavorable trades, with phase confidence.
    "Your early counters lost {n} trades against that swing. Try waiting a little longer or entering from another distance."
90. **Counter too late:** `>=2` accepted counters during the recorded
    dangerous arc that led to unfavorable trades, with phase confidence.
    "Your counters met the middle of my swing in {n} trades. Try starting earlier or waiting until it passes."
91. **Successful counter timing:** `>=2` comparable accepted counters and
    favorable damage trades; do not imply a proven interruption.
    "Your counters at that point in my swing won {n} trades. Keep that timing in mind, but change it if I adapt."
92. **Repeated attack direction:** `>=2` same-direction attacks followed by
    confirmed blocks and unfavorable trades.
    "I blocked the same attack direction in {n} exchanges and hit back. Mix in another swing or a short poke."
93. **Predictable response:** `>=2` same response to the same observed
    stimulus followed by confirmed opponent punish.
    "After my {action}, you answered with {response} in {n} punished exchanges. Try another response sometimes."
94. **Successful sequence:** `>=2` complete, comparable sequences with
    favorable damage trade; action names come from a fixed, human-readable
    vocabulary.
    "Your {action} into {response} gave you the better damage trade {n} times. That is worth practicing again."
95. **Risky sequence:** `>=2` complete, comparable sequences with
    unfavorable damage trade.
    "After your {action}, my {response} hit you in {n} exchanges. Try changing what you do immediately after {action}."
96. **Missed follow-up:** `>=2` confirmed whiffs followed by opponent damage
    during player recovery, with separate timestamps.
    "I hit you after {n} missed swings while your saber was recovering. Leave a little more space after a miss."

### Non-diagnostic system messages

97. **History unavailable, only if actually needed and explicitly surfaced:**
    "I couldn't load your coaching history, so I won't guess at your habits yet."
98. **Account reminder, rate-limited for guests after sufficient play:**
    "An account can keep your progress tied to your login. Guest coaching history may change if your connection changes."

## 5. Implementation order

After approval: first reproduce and test throw recall; then implement and
validate the continuation fix. Next trace the counterattack gates and add
versioned timing trials and bounded learning. Finally implement tutorial
history, evidence and gating before enabling each reviewed message. Update
`docs/bot-cvar-outline.md` and `docs/combat-sequence-recording.md` for actual
behavior and schema changes. The test plan includes the game module and
focused Boost tests, followed by recorded/in-game scenarios where engine
behavior cannot be represented in unit tests.
