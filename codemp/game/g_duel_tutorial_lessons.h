#ifndef G_DUEL_TUTORIAL_LESSONS_H
#define G_DUEL_TUTORIAL_LESSONS_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* These bit positions are the FP_* indices in q_shared.h. */
enum {
	DUEL_CURRICULUM_HEAL = 0, DUEL_CURRICULUM_JUMP = 1,
	DUEL_CURRICULUM_SPEED = 2, DUEL_CURRICULUM_PUSH = 3,
	DUEL_CURRICULUM_PULL = 4, DUEL_CURRICULUM_GRIP = 6,
	DUEL_CURRICULUM_LIGHTNING = 7, DUEL_CURRICULUM_RAGE = 8,
	DUEL_CURRICULUM_PROTECT = 9, DUEL_CURRICULUM_ABSORB = 10,
	DUEL_CURRICULUM_DRAIN = 13, DUEL_CURRICULUM_THROW = 17,
	DUEL_CURRICULUM_POWER_COUNT = 18
};

typedef enum {
	DUEL_CURRICULUM_LITERAL,
	DUEL_CURRICULUM_MINIMUM_FP,
	DUEL_CURRICULUM_THRESHOLD_AND_COST,
	DUEL_CURRICULUM_ACTION_COST,
	DUEL_CURRICULUM_TWO_COSTS_SUM,
	DUEL_CURRICULUM_VALUE,
	DUEL_CURRICULUM_TWO_VALUES,
	DUEL_CURRICULUM_COST_AND_VALUE,
	DUEL_CURRICULUM_PAIR_THROW_DAMAGE,
	DUEL_CURRICULUM_PTK_EXAMPLE,
	DUEL_CURRICULUM_DRAIN_EXCHANGE
} duel_curriculum_format_t;

typedef enum {
	DUEL_VALUE_HEAL_GAIN,
	DUEL_VALUE_GRIP_START_COST,
	DUEL_VALUE_GRIP_HOLD_COST,
	DUEL_VALUE_GRIP_HOLD_MS,
	DUEL_VALUE_DRAIN_SHOT_COST,
	DUEL_VALUE_DRAIN_SHOT_MS,
	DUEL_VALUE_DRAIN_TARGET_LOSS,
	DUEL_VALUE_DRAIN_HEAL_GAIN,
	DUEL_VALUE_DRAIN_TARGET_REGEN_DELAY_MS,
	DUEL_VALUE_DRAIN_SELF_REGEN_DELAY_MS,
	DUEL_VALUE_THROW_BASE_DAMAGE,
	DUEL_VALUE_FLIPKICK_BASE_DAMAGE,
	DUEL_VALUE_RAGE_HEALTH_DRAIN,
	DUEL_VALUE_RAGE_DRAIN_MS,
	DUEL_VALUE_LIGHTNING_SHOT_COST,
	DUEL_VALUE_LIGHTNING_SHOT_MS,
	DUEL_VALUE_LIGHTNING_BASE_MAX,
	DUEL_CURRICULUM_VALUE_COUNT
} duel_curriculum_value_t;

typedef struct {
	const char *text;
	int advanced;
	int fullForce;
	unsigned int requiredPowers;
	unsigned int level2Powers;
	unsigned int level3Powers;
	duel_curriculum_format_t format;
	int formatPower;
	int formatArg;
} duel_curriculum_lesson_t;

typedef struct {
	/* Only set a bit for an actual, current activation threshold. */
	unsigned int validMinimums;
	int minimumFP[DUEL_CURRICULUM_POWER_COUNT];
	/* Initial expenditure, distinct from activation threshold. */
	unsigned int validActionCosts;
	int actionCost[DUEL_CURRICULUM_POWER_COUNT];
	/* Values describe the current mechanics under the conditions in the text. */
	unsigned int validValues;
	int values[DUEL_CURRICULUM_VALUE_COUNT];
} duel_curriculum_inputs_t;

#define DC_POWER(p) (1u << DUEL_CURRICULUM_##p)
#define DC_LESSON(t,a,m,p,l2,l3) {t,a,m,p,l2,l3,DUEL_CURRICULUM_LITERAL,-1,-1}
#define DC_COST(t,p) {t,0,1,DC_POWER(p),0,0,DUEL_CURRICULUM_MINIMUM_FP,DUEL_CURRICULUM_##p,-1}
#define DC_START(t,p) {t,0,1,DC_POWER(p),0,0,DUEL_CURRICULUM_THRESHOLD_AND_COST,DUEL_CURRICULUM_##p,-1}
#define DC_ACT(t,a,p) {t,a,1,DC_POWER(p),0,0,DUEL_CURRICULUM_ACTION_COST,DUEL_CURRICULUM_##p,-1}
#define DC_ACT2(t,a,p,q) {t,a,1,DC_POWER(p)|DC_POWER(q),0,0,DUEL_CURRICULUM_ACTION_COST,DUEL_CURRICULUM_##p,-1}
#define DC_PAIR(t,a,p,q) {t,a,1,DC_POWER(p)|DC_POWER(q),0,0,DUEL_CURRICULUM_TWO_COSTS_SUM,DUEL_CURRICULUM_##p,DUEL_CURRICULUM_##q}
#define DC_PULL_THROW(t,a) {t,a,1,DC_POWER(PULL)|DC_POWER(THROW),0,0,DUEL_CURRICULUM_PAIR_THROW_DAMAGE,DUEL_CURRICULUM_PULL,DUEL_CURRICULUM_THROW}
#define DC_PTK_EXAMPLE(t,a) {t,a,1,DC_POWER(PULL)|DC_POWER(THROW),0,0,DUEL_CURRICULUM_PTK_EXAMPLE,-1,-1}
#define DC_DRAIN_EXCHANGE(t) {t,0,1,DC_POWER(DRAIN),DC_POWER(DRAIN),0,DUEL_CURRICULUM_DRAIN_EXCHANGE,-1,-1}
#define DC_VALUE(t,a,p,v) {t,a,1,p,0,0,DUEL_CURRICULUM_VALUE,DUEL_VALUE_##v,-1}
#define DC_ANYVALUE(t,a,v) {t,a,0,0,0,0,DUEL_CURRICULUM_VALUE,DUEL_VALUE_##v,-1}
#define DC_VALUES(t,a,p,v,w) {t,a,1,p,0,0,DUEL_CURRICULUM_TWO_VALUES,DUEL_VALUE_##v,DUEL_VALUE_##w}
#define DC_ACT_VALUE(t,a,p,v) {t,a,1,DC_POWER(p),0,0,DUEL_CURRICULUM_COST_AND_VALUE,DUEL_CURRICULUM_##p,DUEL_VALUE_##v}
#define DC_B(t,p) DC_LESSON(t,0,1,p,0,0)
#define DC_A(t,p) DC_LESSON(t,1,1,p,0,0)
#define DC_BT(t) DC_LESSON(t,0,1,DC_POWER(THROW),DC_POWER(THROW),DC_POWER(THROW))
#define DC_AT(t) DC_LESSON(t,1,1,DC_POWER(THROW),DC_POWER(THROW),DC_POWER(THROW))

static const duel_curriculum_lesson_t g_duelCurriculumLessons[] = {
	/* Ordered introduction: teach individual actions before their combinations. */
	DC_LESSON("Bind your Force powers to keys you can reach, so you can react without cycling through the list.",0,1,0,0,0),
	DC_LESSON("Choose level 3 Push, Pull and Saber Throw; take level 3 Drain for dark side, or Heal and Absorb for light side.",0,1,0,0,0),
	DC_ACT("Push spends %d ^5FP^7. It can stun a ready defender during their counter, leaving an opening for saber throw.",0,PUSH),
	DC_ACT("Pull spends %d ^5FP^7. Its counter stun briefly exposes a ready defender to saber throw.",0,PULL),
	DC_B("Push and Pull can stun a grounded opponent. Push has a slightly longer stun than Pull.",DC_POWER(PUSH)|DC_POWER(PULL)),
	DC_B("Airborne or attacking opponents, or those below ^520 FP^7, can be moved by Push or Pull instead of stunned.",DC_POWER(PUSH)|DC_POWER(PULL)),
	DC_VALUE("A saber throw can deal %d base ^1damage^7 on contact.",0,DC_POWER(THROW),THROW_BASE_DAMAGE),
	DC_B("A throw can catch an opponent while they attack, recover or cannot block; watch for their defense.",DC_POWER(THROW)),
	DC_BT("Think of your crosshair as a laser pointer: the flying saber steers toward the traced point."),
	DC_B("Push can stun the opponent during their counter; throw into that opening for up to ^130 base damage^7.",DC_POWER(PUSH)|DC_POWER(THROW)),
	DC_PULL_THROW("Pull %d ^5FP^7 plus throw %d ^5FP^7 costs %d ^5FP^7; an unblocked throw deals %d base ^1damage^7.",0),
	DC_LESSON("A flipkick is a wall-flip off your opponent: get close and jump into them as if they were a wall.",0,0,0,0,0),
	DC_LESSON("For a front flipkick, hold forward and tap jump again while rising near your opponent. For a side flip, strafe and jump.",0,0,0,0,0),
	DC_ANYVALUE("A flipkick can deal %d base ^1damage^7; it bypasses armor and can knock the opponent down.",0,FLIPKICK_BASE_DAMAGE),
	DC_LESSON("If you use TaystJK, you can bind flipkick in the JAPro menu instead of timing the jumps manually.",0,0,0,0,0),
	DC_B("^3Pullkick^7 (^3PK^7): Pull, then flipkick as the opponent reaches you.",DC_POWER(PULL)),
	DC_B("^3PTK^7: throw, Pull into the blade, then flipkick. If the hits deal 30 and 20, that totals ^150 damage^7.",DC_POWER(PULL)|DC_POWER(THROW)),
	DC_START("Grip needs %d ^5FP^7 available; acquiring a target spends %d ^5FP^7, then holding spends more.",GRIP),
	DC_B("^3Gripkick^7 (^3GK^7): Grip the target, look down to bring them closer, and flipkick while holding them.",DC_POWER(GRIP)),
	DC_LESSON("Level 3 Grip deals 40 base choke ^1damage^7 after about 3 seconds; reductions or escape can change the outcome.",0,1,DC_POWER(GRIP),DC_POWER(GRIP),DC_POWER(GRIP)),
	DC_B("Example: three ^120-damage^7 kicks plus a level 3 Grip's 40 choke total ^1100 HP^7, enough to defeat a ^1100-HP^7 target.",DC_POWER(GRIP)),
	DC_START("Rage needs %d ^5FP^7 and spends %d ^5FP^7 to activate; use it before Grip's choke ^1damage^7, then escape.",RAGE),
	DC_DRAIN_EXCHANGE("Ten clean Drain ticks cost %d ^5FP^7, heal up to %d ^1HP^7 and remove up to %d enemy ^5FP^7."),
	DC_B("A held Drain has no fixed total: its effect depends on your ^5FP^7, missing ^1HP^7, target ^5FP^7 and time held.",DC_POWER(DRAIN)),
	DC_B("A ^3drainlock^7 delays ^5FP^7 recovery. Keeping an opponent below ^520 FP^7 can deny their Pull and expose them to your ^3pullkick^7.",DC_POWER(DRAIN)|DC_POWER(PULL)),
	DC_ACT_VALUE("Heal spends %d ^5FP^7 for up to %d ^1HP^7 at your level, capped by missing ^1HP^7.",0,HEAL,HEAL_GAIN),
	DC_B("If you have ^140 HP^7 and the opponent has 80, create space and Heal rather than charging into another exchange.",DC_POWER(HEAL)),
	DC_LESSON("If you have ^180 HP^7 and the opponent has 40, press the advantage, but leave enough ^5FP^7 to answer a counter.",0,1,0,0,0),
	DC_VALUES("Grip spends %d ^5FP^7 after acquiring a target, then %d ^5FP^7 per hold tick.",0,DC_POWER(GRIP),GRIP_START_COST,GRIP_HOLD_COST),
	DC_VALUES("Drain fires at a nominal %d ms cadence and costs %d ^5FP^7 per processed shot; line misses can skip the cost.",0,DC_POWER(DRAIN),DRAIN_SHOT_MS,DRAIN_SHOT_COST),
	DC_VALUES("Lightning's ongoing ticks cost %d ^5FP^7 each at a nominal %d ms cadence.",0,DC_POWER(LIGHTNING),LIGHTNING_SHOT_COST,LIGHTNING_SHOT_MS),
	/* Healing. */
	DC_B("Level 1 Heal restores up to ^15 HP^7 per activation, limited by your maximum ^1health^7.",DC_POWER(HEAL)),
	DC_LESSON("Level 2 Heal restores up to ^110 HP^7 per activation, limited by your maximum ^1health^7.",0,1,DC_POWER(HEAL),DC_POWER(HEAL),0),
	DC_LESSON("Level 3 Heal restores up to ^125 HP^7 per activation, limited by your maximum ^1health^7.",0,1,DC_POWER(HEAL),DC_POWER(HEAL),DC_POWER(HEAL)),
	DC_B("Heal cannot restore armor. Use it to repair ^1health^7 lost in the last exchange.",DC_POWER(HEAL)),
	DC_B("After Push creates separation, Heal can trade ^5FP^7 for ^1health^7 before the opponent closes again.",DC_POWER(PUSH)|DC_POWER(HEAL)),
	DC_B("A heal spends ^5FP^7 that might otherwise answer Push or Grip. Consider the next exchange first.",DC_POWER(HEAL)),
	DC_B("Example: at 90 of ^1100 HP^7, a ^125 HP^7 heal can restore only ^110 HP^7.",DC_POWER(HEAL)),
	DC_ACT("Heal costs %d ^5FP^7 even if the ^1health^7 cap limits its gain. Escape before paying that cost.",0,HEAL),
	DC_B("If the opponent is closing, healing in place may cost you the space needed to defend.",DC_POWER(HEAL)),
	/* Push. */
	DC_B("Push can interrupt an approaching opponent and create space for a Heal or a counterattack.",DC_POWER(PUSH)),
	DC_B("Push can create space after a close exchange. Reposition instead of immediately following them.",DC_POWER(PUSH)),
	DC_ACT("Push spends %d ^5FP^7 even if it misses; repeated attempts leave less ^5FP^7 for a counter.",0,PUSH),
	DC_B("A Push into open space gives you room to recover; a nearby wall limits the space gained.",DC_POWER(PUSH)),
	DC_PAIR("Push then throw costs %d and %d ^5FP^7: reserve %d ^5FP^7 before using both.",0,PUSH,THROW),
	DC_B("After pushing an opponent away, avoid running straight into the counterattack.",DC_POWER(PUSH)),
	DC_B("A failed Push still leaves you facing an opponent. Keep an exit direction in mind.",DC_POWER(PUSH)),
	DC_B("Watch for an opponent's active Absorb before depending on Push to create space.",DC_POWER(PUSH)),
	DC_B("A Push counter briefly occupies the defender; a ^130-base-damage^7 saber throw can punish that stun.",DC_POWER(PUSH)|DC_POWER(THROW)),
	DC_B("Pull-Drain can bring an opponent into Drain range; follow only if you have enough ^5FP^7 to use Drain.",DC_POWER(PULL)|DC_POWER(DRAIN)),
	/* Pull. */
	DC_B("Pull draws the fight closer. Prepare for a saber attack at the new distance.",DC_POWER(PULL)),
	DC_ACT("Pull spends %d ^5FP^7 on use; bringing the opponent closer also exposes your ^1HP^7.",0,PULL),
	DC_ACT("Pull costs %d ^5FP^7; delay your kick until the target is close enough to connect.",0,PULL),
	DC_B("Pull toward open ground rather than trapping yourself against a wall.",DC_POWER(PULL)),
	DC_B("After a missed Pull, move sideways instead of standing still in front of the opponent.",DC_POWER(PULL)),
	DC_B("A close Pull can change the spacing quickly. Leave room to react rather than holding forward.",DC_POWER(PULL)),
	DC_B("Against Absorb, Pull may be weakened; do not assume the opponent will move.",DC_POWER(PULL)),
	DC_B("Pull-throw can catch a target moving toward the blade; a clean throw has 30 base ^1damage^7 before reductions.",DC_POWER(PULL)|DC_POWER(THROW)),
	DC_B("After Pull, a close-range kick can knock the opponent down and open a saber follow-up.",DC_POWER(PULL)),
	DC_B("Vary a Pull approach with movement so the opponent cannot prepare for the same entry each time.",DC_POWER(PULL)),
	/* Grip. */
	DC_B("^3Gripkick^7: hold Grip and flipkick. A ^120-damage^7 kick plus level 3 Grip's 40 choke can total ^160 HP^7 if both land.",DC_POWER(GRIP)),
	DC_B("You can steer a gripped target with your view; abrupt turns can make their movement hard to predict.",DC_POWER(GRIP)),
	DC_B("A Grip attempt can fail against active Absorb. Keep a second plan for that response.",DC_POWER(GRIP)),
	DC_B("During Grip, watch the opponent's movement and your own distance; a kick can punish close range.",DC_POWER(GRIP)),
	DC_VALUES("Grip's hold ticks spend %d ^5FP^7 at a nominal %d ms interval after acquisition.",0,DC_POWER(GRIP),GRIP_HOLD_COST,GRIP_HOLD_MS),
	DC_B("A gripped opponent may have a counter. Leave room instead of assuming they are helpless.",DC_POWER(GRIP)),
	DC_B("Grip from farther away reduces your exposure to a close kick, but gives the target time to respond.",DC_POWER(GRIP)),
	DC_B("After Grip ends, check the opponent's position before choosing a saber follow-up.",DC_POWER(GRIP)),
	DC_B("A Grip into a wall gives the target less room to move, but also brings the wall into your escape path.",DC_POWER(GRIP)),
	DC_VALUE("Successful Grip spends %d ^5FP^7 immediately; reserve more for the hold and your escape.",0,DC_POWER(GRIP),GRIP_START_COST),
	/* Lightning and drain. */
	DC_VALUE("Lightning needs ^525 FP^7 to start and its ongoing ticks cost %d ^5FP^7 each.",0,DC_POWER(LIGHTNING),LIGHTNING_SHOT_COST),
	DC_VALUES("Lightning spends %d ^5FP^7 per tick at a nominal %d ms cadence; track the moving target.",0,DC_POWER(LIGHTNING),LIGHTNING_SHOT_COST,LIGHTNING_SHOT_MS),
	DC_VALUE("With a saber, Lightning rolls 1 to %d base ^1damage^7 per hit before Absorb or other reductions.",0,DC_POWER(LIGHTNING),LIGHTNING_BASE_MAX),
	DC_VALUE("Drain needs ^525 FP^7 to start and costs %d ^5FP^7 per processed shot; a line miss can skip that cost.",0,DC_POWER(DRAIN),DRAIN_SHOT_COST),
	DC_VALUE("A clean Drain hit can remove up to %d ^5FP^7 at your level; Absorb and target ^5FP^7 alter the result.",0,DC_POWER(DRAIN),DRAIN_TARGET_LOSS),
	DC_B("After Drain, watch whether the opponent retreats or closes before choosing a saber attack.",DC_POWER(DRAIN)),
	DC_VALUE("A clean Drain hit can restore up to %d ^1HP^7 at your level, capped by missing ^1HP^7.",0,DC_POWER(DRAIN),DRAIN_HEAL_GAIN),
	DC_B("Absorb changes the result of incoming Force attacks. Watch for it before holding Lightning.",DC_POWER(LIGHTNING)),
	DC_B("Switching from Lightning to a saber approach changes the distance you need to manage.",DC_POWER(LIGHTNING)),
	DC_B("Drain heals only while there is missing ^1HP^7; stop when you have recovered enough.",DC_POWER(DRAIN)),
	/* Defensive powers. */
	DC_ACT("Protect costs %d ^5FP^7 to activate and drains additional ^5FP^7 while active or absorbing ^1damage^7.",0,PROTECT),
	DC_B("Protect and Absorb answer different threats: ^1damage^7 protection is not the same as Force absorption.",DC_POWER(PROTECT)|DC_POWER(ABSORB)),
	DC_B("Absorb can lessen incoming Push, Pull or Grip; compare its effect with the attacker's power level.",DC_POWER(ABSORB)),
	DC_ACT("Absorb costs %d ^5FP^7 to activate and drains more while active; turn it on before a Force attack.",0,ABSORB),
	DC_PAIR("Protect then Push costs %d plus %d ^5FP^7, or %d ^5FP^7 before Protect's ongoing drain.",0,PROTECT,PUSH),
	DC_B("Neither Protect nor Absorb removes the need to track an opponent's saber.",DC_POWER(PROTECT)|DC_POWER(ABSORB)),
	DC_B("Absorb can change a Force exchange, but it does not make close-range positioning irrelevant.",DC_POWER(ABSORB)),
	DC_B("Watch your active powers after switching defenses; the earlier defense may have stopped.",DC_POWER(PROTECT)|DC_POWER(ABSORB)),
	DC_B("Choose a defensive power based on the threat you see, then keep an escape route open.",DC_POWER(PROTECT)|DC_POWER(ABSORB)),
	DC_B("A defensive power can end or be switched off. Recheck its active state before the next exchange.",DC_POWER(PROTECT)|DC_POWER(ABSORB)),
	/* Speed, Rage and jump. */
	DC_ACT("Speed spends %d ^5FP^7 to start. Stop at saber range instead of overshooting your opponent.",0,SPEED),
	DC_B("Speed can help you close on an opponent who is trying to regenerate ^5FP^7.",DC_POWER(SPEED)),
	DC_B("Speed can help you retreat; avoid running backward into walls or ledges.",DC_POWER(SPEED)),
	DC_B("A fast approach spends ^5FP^7 and can carry you past your target. Leave room to turn.",DC_POWER(SPEED)),
	DC_VALUES("Rage requires ^110 HP^7 to start and drains %d ^1HP^7 per tick at a nominal %d ms interval.",0,DC_POWER(RAGE),RAGE_HEALTH_DRAIN,RAGE_DRAIN_MS),
	DC_B("Rage has a recovery period after it ends. Plan your next exchange around that downtime.",DC_POWER(RAGE)),
	DC_B("Rage can help you survive a Grip; activate it before the choke ^1damage^7 if you can, then get away.",DC_POWER(RAGE)|DC_POWER(GRIP)),
	DC_B("Rage consumes ^1health^7 and eventually ends. Leave the attacker's Drain range before turning it off.",DC_POWER(RAGE)|DC_POWER(DRAIN)),
	DC_B("If you can afford it, keep enough ^5FP^7 after Rage to Drain once you have escaped.",DC_POWER(RAGE)|DC_POWER(DRAIN)),
	DC_B("Drain has a short tail after release. Let go early near full ^1HP^7 so extra shots do not waste ^5FP^7.",DC_POWER(DRAIN)),
	/* Throw and steering. */
	DC_BT("Use level 3 Saber Throw: the crosshair is a laser pointer, and the blade heads toward the point it hits."),
	DC_BT("The flying saber takes time to reach the pointer's destination; a stun can give it time to hit."),
	DC_BT("Pull's counter stun creates a short opening for a throw with 30 base ^1damage^7."),
	DC_BT("Push's longer counter stun gives a ^130-base-damage^7 throw more time to connect."),
	DC_BT("An opponent can block a throw after recovering from a stun; use the opening before their defense returns."),
	DC_BT("A wall can stop the blade before it reaches the target; the crosshair cannot make it pass through cover."),
	DC_BT("An attacking opponent cannot auto-counter Pull; pulling toward a flying blade can add 30 base ^1damage^7."),
	DC_BT("Below ^520 FP^7, an opponent loses their normal Push/Pull counter; expect movement rather than a counter stun."),
	DC_B("Release alternate attack to recall a thrown saber after its initial flight period.",DC_POWER(THROW)),
	DC_VALUE("A thrown saber's base contact ^1damage^7 is %d before reductions; blocks can prevent the hit.",0,DC_POWER(THROW),THROW_BASE_DAMAGE),
	/* Combined actions. */
	DC_PAIR("Push then saber throw costs %d plus %d ^5FP^7, or %d ^5FP^7; the throw can punish their retreat.",0,PUSH,THROW),
	DC_ACT("Pull costs %d ^5FP^7; confirm the target moved before following with a saber swing.",0,PULL),
	DC_B("A short Drain tap can heal after a hit; stop early so you retain ^5FP^7 for the next exchange.",DC_POWER(DRAIN)),
	DC_ACT("Speed costs %d ^5FP^7 to start; stop at saber range instead of running past the target.",0,SPEED),
	DC_B("A Grip attempt followed by a saber attack needs both ^5FP^7 and a clear route to the opponent.",DC_POWER(GRIP)),
	DC_B("Use Drain to pressure ^5FP^7, then judge whether the opponent has closed into saber range.",DC_POWER(DRAIN)),
	DC_PAIR("Push and Heal spend %d plus %d ^5FP^7, or %d ^5FP^7; use the separation to restore ^1HP^7.",0,PUSH,HEAL),
	DC_B("Pull and a kick require close spacing; time the flipkick as the opponent reaches you.",DC_POWER(PULL)),
	DC_B("A thrown saber may return while you reposition. Watch its path before starting another approach.",DC_POWER(THROW)),
	DC_ACT2("Push costs %d FP; reserve that much after using Lightning if you need Push to escape.",0,PUSH,LIGHTNING),
	/* Kick and recovery. */
	DC_LESSON("A flipkick can knock an opponent down, creating an opening for a follow-up.",0,0,0,0,0),
	DC_ANYVALUE("A flipkick can deal %d base ^1damage^7; the actual hit can differ.",0,FLIPKICK_BASE_DAMAGE),
	DC_LESSON("After a kick, watch the opponent's recovery before committing to a saber follow-up.",0,0,0,0,0),
	DC_LESSON("A kick that misses leaves you close to the opponent. Plan your landing direction.",0,0,0,0,0),
	DC_LESSON("After a knockdown, watch the opponent's recovery before committing to another close attack.",0,0,0,0,0),
	DC_LESSON("If you are knocked down, watch the attacker and choose a recovery direction away from hazards.",0,0,0,0,0),
	DC_LESSON("A wall behind you reduces retreat space after a kick. Move away from it before re-engaging.",0,0,0,0,0),
	DC_B("For a ^3pullkick^7, Pull first and kick as the target reaches you.",DC_POWER(PULL)),
	DC_PTK_EXAMPLE("Example: swing, throw and kick deal %d, %d and %d; swing + ^3PTK^7 totals %d ^1damage^7.",0),
	DC_LESSON("Approach at an angle to leave room to evade after a close-range kick.",0,0,0,0,0),
	/* Defense and reading. */
	DC_LESSON("Track the opponent's position while retreating; turning away can hide their approach.",0,0,0,0,0),
	DC_LESSON("Move sideways as well as backward to avoid being driven into a wall.",0,0,0,0,0),
	DC_LESSON("A missed attack is a cue to restore distance before starting another one.",0,0,0,0,0),
	DC_LESSON("Vary your approach after a knockdown; a repeated close entry is easier to counter.",0,0,0,0,0),
	DC_LESSON("Keep your footing in view near ledges; a retreat is only useful if it has room.",0,0,0,0,0),
	DC_LESSON("After the opponent spends ^5FP^7, watch for their next movement rather than assuming they are empty.",0,1,0,0,0),
	DC_LESSON("If both players are low on ^1HP^7, a reckless approach can end the duel before your attack lands.",0,0,0,0,0),
	DC_LESSON("An opponent backing away may be preparing a power. Do not follow in a straight line every time.",0,0,0,0,0),
	DC_LESSON("After your saber returns, check spacing before pressing the next attack.",0,0,0,0,0),
	DC_LESSON("Keep a clear route out of close range before starting an attack.",0,0,0,0,0),
	/* Beginner: defending and approaching thrown sabers. */
	DC_B("A thrown saber has travel time. Move out of its path rather than waiting for it to reach you.",DC_POWER(THROW)),
	DC_B("Cover can obstruct a thrown saber. Keep track of the opponent while moving behind it.",DC_POWER(THROW)),
	DC_B("The opponent can steer a level 3 throw. Watch the blade, not only the thrower.",DC_POWER(THROW)),
	DC_B("An opponent's returning saber is still a moving object. Watch its path as you change position.",DC_POWER(THROW)),
	DC_B("Closing on an opponent whose saber is away can pressure them, but watch for Force counters.",DC_POWER(THROW)),
	DC_B("If the opponent attacks while throwing, watch both the blade and their approach.",DC_POWER(THROW)),
	DC_B("If a throw passes you, track where the thrower is aiming before moving into its path again.",DC_POWER(THROW)),
	DC_B("Move sideways against a direct throw, then check whether the blade is turning toward you.",DC_POWER(THROW)),
	DC_B("At close range, a thrower can still reposition. Do not aim your approach at their old location.",DC_POWER(THROW)),
	DC_B("Avoid backing into a wall while tracking a throw; you need space to dodge its changing path.",DC_POWER(THROW)),
	/* Advanced: expenditure and timing. */
	DC_PAIR("Speed plus Heal costs %d and %d ^5FP^7, totaling %d ^5FP^7 before regeneration or any other drain.",1,SPEED,HEAL),
	DC_A("^3Anti-Drain^7 means denying healing: spend or preserve ^5FP^7 deliberately, and pressure the opponent's approach.",DC_POWER(DRAIN)),
	DC_PULL_THROW("Pull %d plus throw %d costs %d ^5FP^7; a clean throw has %d base ^1damage^7 before reductions.",1),
	DC_PAIR("Absorb and Speed cost %d and %d ^5FP^7, totaling %d ^5FP^7 before Absorb's ongoing drain.",1,ABSORB,SPEED),
	DC_PAIR("Protect and Pull cost %d and %d ^5FP^7, totaling %d ^5FP^7 before Protect's ongoing drain.",1,PROTECT,PULL),
	DC_VALUES("Grip spends %d ^5FP^7 on acquisition and %d per hold tick; its starting threshold is a separate check.",1,DC_POWER(GRIP),GRIP_START_COST,GRIP_HOLD_COST),
	DC_A("After Drain, both players have a recovery delay; repeated clean hits can keep the target's delay refreshed.",DC_POWER(DRAIN)),
	DC_A("Lightning can interrupt an opponent's approach; combine it with movement to control the distance.",DC_POWER(LIGHTNING)),
	DC_ACT("Throw spends %d ^5FP^7 on launch; normal ^5FP^7 regeneration is suppressed while the blade is in flight.",1,THROW),
	DC_ACT_VALUE("Heal spends %d ^5FP^7 for at most %d ^1HP^7 at your level. The ^1health^7 cap can reduce that return.",1,HEAL,HEAL_GAIN),
	/* Advanced: HP/AP accounting. */
	DC_A("A ^3drainlock^7 is repeated Drain pressure that delays an opponent's ^5FP^7 recovery and limits their options.",DC_POWER(DRAIN)),
	DC_A("^3Drainlock^7 is not permanent: breaking range or line of sight can let the target regenerate.",DC_POWER(DRAIN)),
	DC_A("A successful Drain delays the target's ^5FP^7 regeneration, helping you create a temporary Force advantage.",DC_POWER(DRAIN)),
	DC_VALUE("A clean Drain hit can remove up to %d ^5FP^7; Absorb and the target's remaining ^5FP^7 affect the result.",1,DC_POWER(DRAIN),DRAIN_TARGET_LOSS),
	DC_VALUE("A clean Drain hit can restore up to %d ^1HP^7; your missing ^1HP^7 and target conditions limit the gain.",1,DC_POWER(DRAIN),DRAIN_HEAL_GAIN),
	DC_LESSON("A flipkick can knock an opponent down; use the opening to control the next exchange.",1,0,0,0,0),
	DC_A("In a ^3drainlock^7, short taps can refresh the target's recovery delay without spending as much as a long hold.",DC_POWER(DRAIN)),
	DC_A("Drain costs ^5FP^7 too: holding it too long can leave you unable to capitalize on your opponent's low ^5FP^7.",DC_POWER(DRAIN)),
	DC_A("Escape Drain by breaking line of sight or moving out of range; avoid spending the ^5FP^7 you need for defense.",DC_POWER(DRAIN)),
	DC_LESSON("After an exchange, consider your remaining ^1health^7 and position before committing again.",1,0,0,0,0),
	/* Advanced: Heal and reserves. */
	DC_LESSON("Example: at 75 of ^1100 HP^7, level 3 Heal can restore ^125 HP^7; at 95 it restores only 5.",1,1,DC_POWER(HEAL),DC_POWER(HEAL),DC_POWER(HEAL)),
	DC_LESSON("Example: at 88 of ^1100 HP^7, a level 2 Heal restores ^110 HP^7, leaving ^198 HP^7.",1,1,DC_POWER(HEAL),DC_POWER(HEAL),0),
	DC_A("Example: at 97 of ^1100 HP^7, even a level 1 Heal is capped to ^13 HP^7 restored.",DC_POWER(HEAL)),
	DC_ACT_VALUE("Heal spends %d ^5FP^7 for at most %d ^1HP^7; near maximum ^1health^7 its ^5FP^7-to-^1HP^7 return is worse.",1,HEAL,HEAL_GAIN),
	DC_A("After Drain stops there is a restart delay, so time your next attempt instead of assuming it fires immediately.",DC_POWER(DRAIN)),
	DC_A("If the opponent is about to re-enter saber range, account for the time spent healing.",DC_POWER(HEAL)),
	DC_LESSON("Level 3 Heal trades ^5FP^7 for up to ^125 HP^7. Its actual gain is smaller near maximum ^1health^7.",1,1,DC_POWER(HEAL),DC_POWER(HEAL),DC_POWER(HEAL)),
	DC_A("After healing, compare your remaining ^5FP^7 with the cost of the defense you might need next.",DC_POWER(HEAL)),
	DC_A("Closing with Speed can surprise a player waiting for ^5FP^7 regeneration, but costs part of your own reserve.",DC_POWER(SPEED)),
	DC_A("Push can buy time to Heal, but an opponent with room to chase can close the gap again.",DC_POWER(PUSH)|DC_POWER(HEAL)),
	/* Advanced: Push and responses. */
	DC_A("Push creates separation in the direction of the exchange; position yourself before choosing the direction.",DC_POWER(PUSH)),
	DC_A("Push near a wall can leave the opponent nearby. Prepare for the rebound in spacing.",DC_POWER(PUSH)),
	DC_A("If the opponent activates Absorb, test their response with movement rather than repeatedly spending on Push.",DC_POWER(PUSH)),
	DC_A("Use Push to interrupt an entry, then move off the line the opponent used to approach.",DC_POWER(PUSH)),
	DC_A("Use the space after Push to decide between healing and preserving ^5FP^7 for a counter.",DC_POWER(PUSH)|DC_POWER(HEAL)),
	DC_A("A quick ^3pullkick^7 can interrupt an opponent trying to close for Drain.",DC_POWER(PULL)|DC_POWER(DRAIN)),
	DC_A("If Push fails, retreat around cover rather than continuing the same close-range approach.",DC_POWER(PUSH)),
	DC_A("Against a close gripper, Push can be a response; face the attacker and check available ^5FP^7.",DC_POWER(PUSH)),
	DC_A("Push toward open ground and keep a different path for your own retreat.",DC_POWER(PUSH)),
	DC_ACT("Push spends %d ^5FP^7 each use; two attempts cost twice that amount before regeneration.",1,PUSH),
	/* Advanced: Pull and close-range consequences. */
	DC_A("Pull changes both players' threat distance. Prepare a landing or escape route before drawing them in.",DC_POWER(PULL)),
	DC_A("Pull toward your saber range, not blindly into contact; the opponent can attack at close range too.",DC_POWER(PULL)),
	DC_A("A Pull and kick sequence needs aim, spacing and timing; the Pull alone does not guarantee a knockdown.",DC_POWER(PULL)),
	DC_A("After Pull moves a target, a saber reply avoids paying for another Force attack; its ^1damage^7 depends on the swing and hit.",DC_POWER(PULL)),
	DC_ACT("Pull spends %d ^5FP^7 even if the follow-up swing misses; reposition before the opponent counters.",1,PULL),
	DC_A("Pulling an opponent near a wall can shorten their travel; do not time the follow-up as if space were open.",DC_POWER(PULL)),
	DC_A("After an opponent resists Pull, change angle before spending on the same entry again.",DC_POWER(PULL)),
	DC_A("Watch whether Pull actually displaced the opponent before committing to a kick.",DC_POWER(PULL)),
	DC_A("Pull then retreat is a spacing mix-up: it creates a close threat without requiring an immediate swing.",DC_POWER(PULL)),
	DC_A("Pull-Drain can bring a target into range to recover ^1HP^7, but missing Pull wastes ^5FP^7 before the Drain.",DC_POWER(PULL)|DC_POWER(DRAIN)),
	/* Advanced: Grip. */
	DC_A("A wall can break Grip's connection; use the opening to change position or recover ^5FP^7.",DC_POWER(GRIP)),
	DC_A("A gripped opponent close enough to kick may punish the hold. Control separation during acquisition.",DC_POWER(GRIP)),
	DC_A("Grip followed by a saber approach requires releasing the hold and tracking the target's new location.",DC_POWER(GRIP)),
	DC_A("An opponent's Absorb can reduce or prevent Grip effects; watch the result before closing.",DC_POWER(GRIP)),
	DC_VALUES("Grip requires a separate threshold, then spends %d ^5FP^7 on acquisition and %d ^5FP^7 per hold tick.",1,DC_POWER(GRIP),GRIP_START_COST,GRIP_HOLD_COST),
	DC_A("Choose the Grip line before starting: terrain can obstruct the target or your own retreat.",DC_POWER(GRIP)),
	DC_A("A failed Grip leaves a committed attacker; strafe out of the opponent's counterattack line.",DC_POWER(GRIP)),
	DC_A("If a Grip ends without moving the opponent as expected, do not swing at their assumed landing spot.",DC_POWER(GRIP)),
	DC_ACT2("After Grip, Push spends %d FP; Grip's acquisition and hold are additional costs.",1,PUSH,GRIP),
	DC_A("An opponent attempting Grip needs a line to you. Break that line with terrain and change position.",DC_POWER(GRIP)),
	/* Advanced: Drain and Lightning. */
	DC_A("After hurting an opponent, watch for their attempt to Drain you back to recover ^1health^7.",DC_POWER(DRAIN)),
	DC_VALUES("Drain delays target ^5FP^7 recovery by %d ms after a valid hit and yours by %d ms per processed shot.",1,DC_POWER(DRAIN),DRAIN_TARGET_REGEN_DELAY_MS,DRAIN_SELF_REGEN_DELAY_MS),
	DC_A("Repeated Drain taps can deny regeneration while leaving more ^5FP^7 for a later pull-throw-kick.",DC_POWER(DRAIN)|DC_POWER(PULL)|DC_POWER(THROW)),
	DC_A("Against a throw used to deny Drain, evade the blade and look for the return before draining.",DC_POWER(DRAIN)|DC_POWER(THROW)),
	DC_VALUES("A clean Drain hit can remove up to %d enemy ^5FP^7 and restore up to %d ^1HP^7; both depend on conditions.",1,DC_POWER(DRAIN),DRAIN_TARGET_LOSS,DRAIN_HEAL_GAIN),
	DC_A("Lightning can pressure at a distance, but a close opponent may interrupt your sustained attack.",DC_POWER(LIGHTNING)),
	DC_ACT2("Push costs %d FP; reserve it after Lightning's starting and ongoing costs.",1,PUSH,LIGHTNING),
	DC_VALUE("With a saber, Lightning rolls 1 to %d base ^1damage^7 per hit; Absorb and armor change the result.",1,DC_POWER(LIGHTNING),LIGHTNING_BASE_MAX),
	DC_A("If Lightning is absorbed or the target reaches cover, stop spending and change the angle.",DC_POWER(LIGHTNING)),
	DC_VALUE("Drain spends %d ^5FP^7 per processed shot. After a hit, a saber approach pressures ^1HP^7 as well as ^5FP^7.",1,DC_POWER(DRAIN),DRAIN_SHOT_COST),
	/* Advanced: defense. */
	DC_A("Absorb can weaken incoming Push, Pull and Grip; stronger attacks may still affect you.",DC_POWER(ABSORB)),
	DC_A("Absorb can convert some incoming Force into ^5FP^7; check the exchange before relying on it.",DC_POWER(ABSORB)),
	DC_ACT("Protect costs %d ^5FP^7 to activate, then spends more over time and on qualifying ^1damage^7.",1,PROTECT),
	DC_A("Protect does not replace movement: each protected hit can still consume resources or deal ^1damage^7.",DC_POWER(PROTECT)),
	DC_A("Rage can buy time against Grip or a ^3drainlock^7, but needs enough ^1HP^7 and ^5FP^7 to activate.",DC_POWER(RAGE)),
	DC_A("Rage before choke ^1damage^7 can preserve more ^1health^7 than waiting until after it lands.",DC_POWER(RAGE)|DC_POWER(GRIP)),
	DC_A("After escaping with Rage, leave Drain range before ending it; then recover ^5FP^7 and look for a short Drain.",DC_POWER(RAGE)|DC_POWER(DRAIN)),
	DC_A("Pick Absorb for an incoming Force threat and Protect for qualifying ^1damage^7; they are not interchangeable.",DC_POWER(ABSORB)|DC_POWER(PROTECT)),
	DC_A("Rage drains ^1HP^7 over time. Do not stay in it longer than needed to escape.",DC_POWER(RAGE)),
	DC_A("An opponent's active Absorb changes the expected value of Grip, Push and Pull; change the approach.",DC_POWER(PUSH)|DC_POWER(PULL)|DC_POWER(GRIP)),
	/* Advanced: movement. */
	DC_A("Speed helps close a gap, but an overshoot gives the opponent your back. Brake before saber range.",DC_POWER(SPEED)),
	DC_A("Speed also helps disengage; steer around terrain rather than spending the full duration against a wall.",DC_POWER(SPEED)),
	DC_PAIR("Speed plus Pull costs %d plus %d ^5FP^7, or %d ^5FP^7 before regeneration; stop at saber range.",1,SPEED,PULL),
	DC_A("When the opponent tries to Drain, a ^3pullkick^7 can interrupt their healing attempt and threaten low ^1HP^7.",DC_POWER(PULL)|DC_POWER(DRAIN)),
	DC_A("A flying saber suppresses your normal ^5FP^7 regeneration; an opponent cannot Drain ^5FP^7 you do not have.",DC_POWER(THROW)|DC_POWER(DRAIN)),
	DC_A("Speed plus throw can deny Drain by spending ^5FP^7 and pressuring the opponent, but leaves little reserve.",DC_POWER(SPEED)|DC_POWER(THROW)|DC_POWER(DRAIN)),
	DC_VALUES("Rage needs ^110 HP^7 to start and drains %d ^1HP^7 every nominal %d ms; it stops at ^11 HP^7.",1,DC_POWER(RAGE),RAGE_HEALTH_DRAIN,RAGE_DRAIN_MS),
	DC_A("^3Anti-Drain^7 has a tradeoff: empty ^5FP^7 denies healing, but also prevents your own Force defense.",DC_POWER(DRAIN)),
	DC_A("Against ^3anti-Drain^7, wait for their saber to return and ^5FP^7 to recover, then use a brief Drain.",DC_POWER(DRAIN)|DC_POWER(THROW)),
	DC_A("A ^3drainlock^7 can be broken by cover, distance or a counterattack; none works in every position.",DC_POWER(DRAIN)),
	/* Advanced: throw mechanics. */
	DC_AT("Level 3 throw traces players and solid cover; the blade heads toward the crosshair's destination."),
	DC_LESSON("At throw level 3, the steering trace includes players, allowing direct aim at the target.",1,1,DC_POWER(THROW),DC_POWER(THROW),DC_POWER(THROW)),
	DC_AT("Treat the crosshair as a laser pointer: a nearby wall becomes the steering destination."),
	DC_AT("A counter-stunned opponent has fewer immediate saber options; a ^130-base-damage^7 throw can exploit that delay."),
	DC_AT("A moving or attacking opponent resists Pull less effectively; confirm whether they moved before following."),
	DC_AT("Below ^520 FP^7, an opponent is vulnerable to Pull movement and a ^120-damage^7 flipkick if it connects."),
	DC_AT("Push near an unguarded ledge threatens a low-^5FP^7 target through displacement, not guaranteed throw ^1damage^7."),
	DC_AT("A low-^5FP^7 defender may still evade or block after movement; low ^5FP^7 does not guarantee your next hit."),
	DC_AT("A distant pointer destination and a nearby destination produce different paths from the saber's position."),
	DC_AT("The saber's travel time can outlast Pull's stun; an opening is not proof the blade will arrive in time."),
	/* Advanced: throw control and counterplay. */
	DC_AT("Cover can end a throw's pressure; continuing to hold it also delays your own ^5FP^7 regeneration."),
	DC_AT("A defender may sidestep your throw after a stun; repeated failed throws spend ^5FP^7 without returning ^1damage^7."),
	DC_AT("A throw that never connects still costs ^5FP^7 and delays recovery; recall before committing to the same entry."),
	DC_AT("Watch the flying saber as well as the pointer: it approaches the traced point from its location."),
	DC_AT("Push and Pull stun through a defender's counter animation; the blade still needs an unblocked contact to deal 30 base ^1damage^7."),
	DC_AT("Use Push or Pull to stun opponents and open them up to incoming attacks."),
	DC_A("Release alternate attack after the initial flight period to call the saber back rather than hold it out.",DC_POWER(THROW)),
	DC_A("The saber also returns at its range limit or after its maximum flight time; do not plan an indefinite hold.",DC_POWER(THROW)),
	DC_A("During the return, move to avoid the opponent while tracking where your saber will reach you.",DC_POWER(THROW)),
	DC_A("A throw used to deny Drain also delays your own ^5FP^7 recovery; recall before you need a Force response.",DC_POWER(THROW)|DC_POWER(DRAIN)),
	/* Advanced: throw combinations. */
	DC_PAIR("Push then throw spends %d plus %d ^5FP^7, or %d ^5FP^7; Push's counter stun can expose the target to 30 base ^1damage^7.",1,PUSH,THROW),
	DC_AT("Pull's counter stun and a flying blade can combine for ^130 damage^7."),
	DC_AT("After a Grip release, a ^130-base-damage^7 throw is another threat, but spending its ^5FP^7 can leave no reserve for escape."),
	DC_AT("A throw can punish a Force counter's stun without committing you to a close kick; its clean base ^1damage^7 is 30."),
	DC_AT("After a blocked throw, repeated Pull attempts may spend ^5FP^7 without any ^1HP^7 return; change the next response."),
	DC_A("After an unsuccessful throw, check your ^5FP^7 and recall the saber before entering close range.",DC_POWER(THROW)),
	DC_A("Use a throw to pressure retreat, then keep enough distance to react if the opponent closes instead.",DC_POWER(THROW)),
	DC_A("A successful pull-throw can push the fight toward a close kick; judge the target's movement first.",DC_POWER(PULL)|DC_POWER(THROW)),
	DC_AT("A target behind cover can recover ^5FP^7 while your saber stays out; holding a blocked line may lose your resource advantage."),
	DC_A("A returning saber changes your attack options; create space until you can use it again.",DC_POWER(THROW)),
	/* Advanced: kick and survival. */
	DC_A("For ^3PTK^7, throw, Pull the target into the blade, then kick; hits dealing 30 and 20 total ^150 damage^7.",DC_POWER(PULL)|DC_POWER(THROW)),
	DC_ANYVALUE("A flipkick can deal %d base ^1damage^7; Grip and other factors can alter the hit.",1,FLIPKICK_BASE_DAMAGE),
	DC_A("If the throw in a ^3PTK^7 is blocked, the kick can still threaten; do not assume both hits land.",DC_POWER(PULL)|DC_POWER(THROW)),
	DC_LESSON("A kick's knockdown is not guaranteed; the target's state can affect the outcome.",1,0,0,0,0),
	DC_LESSON("Do not commit to a grounded follow-up until the kick actually knocks the opponent down.",1,0,0,0,0),
	DC_A("During Grip, a close opponent can kick. Distance reduces that counter's immediate reach.",DC_POWER(GRIP)),
	DC_A("Pull can set up a close kick, but the opponent's counterattack has the same short distance.",DC_POWER(PULL)),
	DC_LESSON("After a kick, use the resulting spacing to decide between pursuit and retreat, not an automatic chain.",1,0,0,0,0),
	DC_LESSON("After a flipkick lands, watch the knockdown before choosing whether to pursue or recover ^5FP^7.",1,0,0,0,0),
	DC_LESSON("At low ^1HP^7, treat a close kick as a direct ^1HP^7 threat even with substantial armor remaining.",1,0,0,0,0),
	/* Advanced: complete decision sequences. */
	DC_A("Mix ^3pullkick^7 and pull-throw-kick rather than committing to an identical entry every time.",DC_POWER(PULL)|DC_POWER(THROW)),
	DC_A("Short Drain taps after successful attacks can keep pressure on a low-^5FP^7 opponent while recovering ^1HP^7.",DC_POWER(DRAIN)),
	DC_ACT("Speed spends %d ^5FP^7; close and stop at saber range rather than running past the target.",1,SPEED),
	DC_ACT("Pull spends %d ^5FP^7; confirm displacement before committing to a close kick.",1,PULL),
	DC_VALUE("Grip spends %d ^5FP^7 on acquisition, plus hold ticks; release and track the target before swinging.",1,DC_POWER(GRIP),GRIP_START_COST),
	DC_ACT2("Push spends %d FP; keep that reserve after Lightning if you plan to Push away.",1,PUSH,LIGHTNING),
	DC_A("After a saber hit, ^3PTK^7 adds a throw and kick threat while the opponent recovers.",DC_POWER(PULL)|DC_POWER(THROW)),
	DC_A("Absorb, close distance, then use the saber: active defense does not guarantee the opponent cannot counter.",DC_POWER(ABSORB)),
	DC_A("Protect, move off the attack line, then reassess ^5FP^7: absorbing ^1damage^7 consumes resources.",DC_POWER(PROTECT)),
	DC_A("Example: a 30 swing, 20 kick, 50 ^3PTK^7 and 30 throw total ^1130 damage^7 if every hit deals those amounts.",DC_POWER(PULL)|DC_POWER(THROW)),
	/* Advanced: accounting for uncertain outcomes. */
	DC_PAIR("Heal then throw costs %d plus %d ^5FP^7, or %d ^5FP^7; a full-^1health^7 player cannot use Heal.",1,HEAL,THROW),
	DC_PAIR("Speed plus throw costs %d + %d = %d ^5FP^7; the low reserve can deny Drain healing but leaves you exposed.",1,SPEED,THROW),
	DC_PAIR("Absorb and Heal cost %d plus %d ^5FP^7, or %d ^5FP^7 before Absorb's ongoing drain.",1,ABSORB,HEAL),
	DC_A("An opponent can counter a long Drain hold: leave enough ^5FP^7 and space to react.",DC_POWER(DRAIN)),
	DC_PULL_THROW("Pull %d plus throw %d ^5FP^7 totals %d ^5FP^7; a clean throw has %d base ^1damage^7.",1),
	DC_A("Break an enemy Grip with Push or Pull if you can target the attacker and have enough ^5FP^7.",DC_POWER(GRIP)|DC_POWER(PUSH)|DC_POWER(PULL)),
	DC_A("After escaping a ^3drainlock^7, regenerate enough ^5FP^7 for a counter before entering the same Drain range.",DC_POWER(DRAIN)),
	DC_A("If the opponent moves behind cover, cancel the aimed follow-up and choose a new line.",0),
	DC_A("If a defensive power ends during a sequence, do not assume it remains active for the next hit.",0),
	DC_A("When a combination fails, identify whether aim, distance, ^5FP^7 or an enemy counter caused the failure.",0)
};

#define DUEL_CURRICULUM_COUNT (sizeof(g_duelCurriculumLessons) / sizeof(g_duelCurriculumLessons[0]))

static inline int G_DuelCurriculumAllowed(const duel_curriculum_lesson_t *lesson,
	int fullForce, unsigned int availablePowers, const int *powerLevels)
{
	unsigned int bit;
	int i;

	if (!lesson || (lesson->fullForce && !fullForce) ||
		(lesson->level2Powers & ~lesson->requiredPowers) ||
		(lesson->level3Powers & ~lesson->level2Powers) ||
		(lesson->requiredPowers & ~((1u << DUEL_CURRICULUM_POWER_COUNT) - 1u)) ||
		(lesson->requiredPowers & availablePowers) != lesson->requiredPowers)
		return 0;
	if (lesson->requiredPowers && !powerLevels)
		return 0;
	for (i = 0; i < DUEL_CURRICULUM_POWER_COUNT; ++i)
	{
		bit = 1u << i;
		if ((lesson->requiredPowers & bit) && powerLevels[i] < 1)
			return 0;
		if ((lesson->level2Powers & bit) && powerLevels[i] < 2)
			return 0;
		if ((lesson->level3Powers & bit) && powerLevels[i] < 3)
			return 0;
	}
	return 1;
}

static inline int G_DuelCurriculumRender(const duel_curriculum_lesson_t *lesson,
	const duel_curriculum_inputs_t *inputs, char *output, size_t outputSize)
{
	int written, first, second, i, j;
	unsigned int mask;

	if (!lesson || !lesson->text || !output || !outputSize)
		return 0;
	output[0] = '\0';
	if (lesson->format == DUEL_CURRICULUM_LITERAL)
	{
		size_t length = strlen(lesson->text);
		if (length >= outputSize || length >= 150)
			return 0;
		memcpy(output, lesson->text, length + 1);
		return 1;
	}
	if (!inputs)
		return 0;
	if (lesson->format == DUEL_CURRICULUM_DRAIN_EXCHANGE)
	{
		unsigned int required = (1u << DUEL_VALUE_DRAIN_SHOT_COST) |
			(1u << DUEL_VALUE_DRAIN_HEAL_GAIN) | (1u << DUEL_VALUE_DRAIN_TARGET_LOSS);
		int cost = inputs->values[DUEL_VALUE_DRAIN_SHOT_COST];
		int heal = inputs->values[DUEL_VALUE_DRAIN_HEAL_GAIN];
		int loss = inputs->values[DUEL_VALUE_DRAIN_TARGET_LOSS];
		if ((inputs->validValues & required) != required ||
			cost < 0 || cost > 999999 || heal < 0 || heal > 999999 || loss < 0 || loss > 999999)
			return 0;
		written = snprintf(output, outputSize, lesson->text, cost * 10, heal * 10, loss * 10);
		if (written < 0 || (size_t)written >= outputSize || written >= 150)
		{
			output[0] = '\0';
			return 0;
		}
		return 1;
	}
	if (lesson->format == DUEL_CURRICULUM_PTK_EXAMPLE)
	{
		unsigned int required = (1u << DUEL_VALUE_THROW_BASE_DAMAGE) |
			(1u << DUEL_VALUE_FLIPKICK_BASE_DAMAGE);
		int swing = 30;
		int thrown = inputs->values[DUEL_VALUE_THROW_BASE_DAMAGE];
		int kick = inputs->values[DUEL_VALUE_FLIPKICK_BASE_DAMAGE];
		if ((inputs->validValues & required) != required ||
			thrown < 0 || thrown > 999999 ||
			kick < 0 || kick > 999999)
			return 0;
		written = snprintf(output, outputSize, lesson->text, swing, thrown, kick, swing + thrown + kick);
		if (written < 0 || (size_t)written >= outputSize || written >= 150)
		{
			output[0] = '\0';
			return 0;
		}
		return 1;
	}
	i = lesson->formatPower;
	j = lesson->formatArg;
	first = second = 0;
	if (lesson->format == DUEL_CURRICULUM_MINIMUM_FP ||
		lesson->format == DUEL_CURRICULUM_THRESHOLD_AND_COST)
	{
		if (i < 0 || i >= DUEL_CURRICULUM_POWER_COUNT ||
			!(inputs->validMinimums & (1u << i)))
			return 0;
		first = inputs->minimumFP[i];
		if (lesson->format == DUEL_CURRICULUM_THRESHOLD_AND_COST)
		{
			if (!(inputs->validActionCosts & (1u << i)))
				return 0;
			second = inputs->actionCost[i];
		}
	}
	else if (lesson->format == DUEL_CURRICULUM_ACTION_COST ||
		lesson->format == DUEL_CURRICULUM_TWO_COSTS_SUM ||
		lesson->format == DUEL_CURRICULUM_PAIR_THROW_DAMAGE ||
		lesson->format == DUEL_CURRICULUM_COST_AND_VALUE)
	{
		if (i < 0 || i >= DUEL_CURRICULUM_POWER_COUNT ||
			!(inputs->validActionCosts & (1u << i)))
			return 0;
		first = inputs->actionCost[i];
		if (lesson->format == DUEL_CURRICULUM_TWO_COSTS_SUM ||
			lesson->format == DUEL_CURRICULUM_PAIR_THROW_DAMAGE)
		{
			if (j < 0 || j >= DUEL_CURRICULUM_POWER_COUNT ||
				!(inputs->validActionCosts & (1u << j)))
				return 0;
			if (lesson->format == DUEL_CURRICULUM_PAIR_THROW_DAMAGE &&
				(!(inputs->validValues & (1u << DUEL_VALUE_THROW_BASE_DAMAGE)) ||
				inputs->values[DUEL_VALUE_THROW_BASE_DAMAGE] < 0 ||
				inputs->values[DUEL_VALUE_THROW_BASE_DAMAGE] > 999999))
				return 0;
			second = inputs->actionCost[j];
		}
		else if (lesson->format == DUEL_CURRICULUM_COST_AND_VALUE)
		{
			if (j < 0 || j >= DUEL_CURRICULUM_VALUE_COUNT ||
				!(inputs->validValues & (1u << j)))
				return 0;
			second = inputs->values[j];
		}
	}
	else if (lesson->format == DUEL_CURRICULUM_VALUE ||
		lesson->format == DUEL_CURRICULUM_TWO_VALUES)
	{
		if (i < 0 || i >= DUEL_CURRICULUM_VALUE_COUNT)
			return 0;
		mask = 1u << i;
		if (!(inputs->validValues & mask))
			return 0;
		first = inputs->values[i];
		if (lesson->format == DUEL_CURRICULUM_TWO_VALUES)
		{
			if (j < 0 || j >= DUEL_CURRICULUM_VALUE_COUNT ||
				!(inputs->validValues & (1u << j)))
				return 0;
			second = inputs->values[j];
		}
	}
	else
		return 0;
	if (first < 0 || first > 999999 || second < 0 || second > 999999)
		return 0;
	if (lesson->format == DUEL_CURRICULUM_PAIR_THROW_DAMAGE)
		written = snprintf(output, outputSize, lesson->text, first, second, first + second,
			inputs->values[DUEL_VALUE_THROW_BASE_DAMAGE]);
	else if (lesson->format == DUEL_CURRICULUM_TWO_COSTS_SUM)
		written = snprintf(output, outputSize, lesson->text, first, second, first + second);
	else if (lesson->format == DUEL_CURRICULUM_THRESHOLD_AND_COST ||
		lesson->format == DUEL_CURRICULUM_TWO_VALUES ||
		lesson->format == DUEL_CURRICULUM_COST_AND_VALUE)
		written = snprintf(output, outputSize, lesson->text, first, second);
	else
		written = snprintf(output, outputSize, lesson->text, first);
	if (written < 0 || (size_t)written >= outputSize || written >= 150)
	{
		output[0] = '\0';
		return 0;
	}
	return 1;
}

#define DUEL_CURRICULUM_INTRO_COUNT 28

/* Walk the remaining beginner lessons without repeating them in this session. */
static inline int G_DuelCurriculumNextBasic(int start, unsigned int availablePowers,
	const int *powerLevels, const duel_curriculum_inputs_t *inputs,
	char *output, size_t outputSize)
{
	int i;
	char scratch[150];
	if (start < DUEL_CURRICULUM_INTRO_COUNT)
		start = DUEL_CURRICULUM_INTRO_COUNT;
	for (i = start; i < (int)DUEL_CURRICULUM_COUNT; ++i)
		if (!g_duelCurriculumLessons[i].advanced &&
			G_DuelCurriculumAllowed(&g_duelCurriculumLessons[i], 1, availablePowers, powerLevels) &&
			G_DuelCurriculumRender(&g_duelCurriculumLessons[i], inputs, scratch, sizeof(scratch)))
			return G_DuelCurriculumRender(&g_duelCurriculumLessons[i], inputs, output, outputSize) ? i : -1;
	return -1;
}

/* Rotation is zero-based; unavailable or unrenderable lessons are skipped. */
static inline int G_DuelCurriculumSelect(int rotation, int fullForce,
	unsigned int availablePowers, const int *powerLevels,
	const duel_curriculum_inputs_t *inputs, char *output, size_t outputSize)
{
	int i, count, tier, ordinal, pass;
	char scratch[150];

	if (rotation < 0 || !output || !outputSize)
		return -1;
	output[0] = '\0';
	if (fullForce && rotation < DUEL_CURRICULUM_INTRO_COUNT)
	{
		for (i = rotation; i < DUEL_CURRICULUM_INTRO_COUNT; ++i)
			if (G_DuelCurriculumAllowed(&g_duelCurriculumLessons[i], fullForce, availablePowers, powerLevels) &&
				G_DuelCurriculumRender(&g_duelCurriculumLessons[i], inputs, scratch, sizeof(scratch)))
				return G_DuelCurriculumRender(&g_duelCurriculumLessons[i], inputs, output, outputSize) ? i : -1;
		return -1;
	}
	tier = fullForce ? (rotation - DUEL_CURRICULUM_INTRO_COUNT) % 2 : rotation % 2;
	ordinal = fullForce ? (rotation - DUEL_CURRICULUM_INTRO_COUNT) / 2 : rotation / 2;
	for (pass = 0; pass < 2; ++pass)
	{
		count = 0;
		for (i = fullForce && tier == 0 ? DUEL_CURRICULUM_INTRO_COUNT : 0; i < (int)DUEL_CURRICULUM_COUNT; ++i)
			if (g_duelCurriculumLessons[i].advanced == tier &&
				G_DuelCurriculumAllowed(&g_duelCurriculumLessons[i], fullForce, availablePowers, powerLevels) &&
				G_DuelCurriculumRender(&g_duelCurriculumLessons[i], inputs, scratch, sizeof(scratch)))
				++count;
		if (count)
		{
			ordinal %= count;
			for (i = fullForce && tier == 0 ? DUEL_CURRICULUM_INTRO_COUNT : 0; i < (int)DUEL_CURRICULUM_COUNT; ++i)
				if (g_duelCurriculumLessons[i].advanced == tier &&
					G_DuelCurriculumAllowed(&g_duelCurriculumLessons[i], fullForce, availablePowers, powerLevels) &&
					G_DuelCurriculumRender(&g_duelCurriculumLessons[i], inputs, scratch, sizeof(scratch)) &&
					ordinal-- == 0)
					return G_DuelCurriculumRender(&g_duelCurriculumLessons[i], inputs, output, outputSize) ? i : -1;
		}
		tier = 1 - tier;
	}
	return -1;
}

static inline int G_DuelCurriculumIntroGroup(int index)
{
	if (index < 0 || index >= DUEL_CURRICULUM_INTRO_COUNT) return -1;
	if (index < 2) return 0;
	if (index < 6) return 1;
	if (index < 11) return 2;
	if (index < 15) return 3;
	if (index < 17) return 4;
	if (index < 22) return 5;
	if (index < 25) return 6;
	if (index < 27) return 7;
	return 8;
}

static inline int G_DuelCurriculumIntroBatch(int start, unsigned int availablePowers,
	const int *powerLevels, const duel_curriculum_inputs_t *inputs,
	char messages[][150], int *indices, int limit)
{
	int count = 0, selected, group = -1;
	char message[150];
	if (start < 0 || start >= DUEL_CURRICULUM_INTRO_COUNT || limit <= 0)
		return 0;
	while (count < limit && start < DUEL_CURRICULUM_INTRO_COUNT)
	{
		selected = G_DuelCurriculumSelect(start, 1, availablePowers, powerLevels,
			inputs, message, sizeof(message));
		if (selected < 0 || (count && G_DuelCurriculumIntroGroup(selected) != group))
			break;
		group = G_DuelCurriculumIntroGroup(selected);
		memcpy(messages[count], message, strlen(message) + 1);
		indices[count] = selected;
		start = selected + 1;
		++count;
	}
	return count;
}

static inline int G_DuelCurriculumBasicBatch(int start, unsigned int availablePowers,
	const int *powerLevels, const duel_curriculum_inputs_t *inputs,
	char messages[][150], int *indices, int limit)
{
	int count = 0, selected, previous = -1;
	unsigned int topic = 0;
	char message[150];
	while (count < limit)
	{
		selected = G_DuelCurriculumNextBasic(start, availablePowers, powerLevels,
			inputs, message, sizeof(message));
		if (selected < 0 || (count && (selected != previous + 1 || !topic ||
			g_duelCurriculumLessons[selected].requiredPowers != topic)))
			break;
		topic = g_duelCurriculumLessons[selected].requiredPowers;
		memcpy(messages[count], message, strlen(message) + 1);
		indices[count] = previous = selected;
		start = selected + 1;
		++count;
	}
	return count;
}

#undef DC_ACT_VALUE
#undef DC_VALUES
#undef DC_ANYVALUE
#undef DC_VALUE
#undef DC_PAIR
#undef DC_PULL_THROW
#undef DC_PTK_EXAMPLE
#undef DC_DRAIN_EXCHANGE
#undef DC_ACT2
#undef DC_ACT
#undef DC_START
#undef DC_AT
#undef DC_BT
#undef DC_A
#undef DC_B
#undef DC_COST
#undef DC_LESSON
#undef DC_POWER

#endif
