# Tutorial sequence evidence

Sequence evidence is separate from the existing pattern table and is collected
for full-force duels only. It uses only complete captures with at least 10
events and 12 seconds. Coaching requires
three informative duels overall, at least two distinct informative duels in
the same mode containing the same sequence, and an observation in the current
duel. Account identities and the existing hashed guest identities are used;
no raw address is stored. The sequence table and duel receipt table use a V3
namespace independently of older patterns and earlier sequence versions,
which are not imported.

The recognizer uses action transitions, not buttons or selected powers. It
recognizes throw-pull-kick, grip-kick, pull-kick, drain-kick, drain-pull,
drain-swing, drain alone, pull-swing, grip-swing and push-swing, plus pull or
push while the opponent's drain is active. The follow-up must start within one
second of the opener. Windows do not overlap. Their end is two seconds after
the last recognized action; the first snapshot at or after that point must be
within 250 ms, or the window is discarded. Damage events after the two-second
deadline are excluded, even when the endpoint snapshot is later. The snapshot
can include resource changes in that additional interval. The opener also
needs a preceding snapshot within 250 ms. Damage is counted only from the
opponent in the duel; damage amounts and health/armor snapshots are not
interchangeable. A duel ending before the window deadline leaves that outcome
unknown and it is not counted.

Flip kicks count only when the post-movement kick target is the duel opponent;
the shared wall-flip animation alone is not evidence of contact. A missed or
otherwise unconfirmed flip kick is not counted. Ordinary kick animation or
saber-move edges are accepted actions but do not imply contact. The live
action-state snapshots used here are not added to the raw duel-event export.
Consequently the persisted aggregates cannot be independently reconstructed
from that export alone.

For each recognized sequence the table accumulates attempts, distinct duels,
negative net health-and-armor trades, zero attributed damage with net force
decline, the subset with poor resource outcomes, both players' signed HP, AP
and FP changes, and attributed damage dealt and taken. It also retains the
latest sample. Healing, drain and regeneration
are included in the net changes. A negative trade means the player's HP+AP
change was lower than the opponent's; it is not proof that the sequence caused
the difference. An FP decline is a net change, **not** gross power cost: enemy
drain, regeneration and other actions may contribute. The poor-resource subset
requires zero attributed damage, no net HP+AP gain, no positive HP+AP trade, and
a larger net FP decline than the opponent's (or no opponent FP decline). A
successful drain can therefore remain a descriptive zero-damage/FP-decline
observation without being classified as a poor resource outcome. Selection
needs at least three attempts; a bad-trade or poor-resource
message needs two such observations including the latest. Repetition alone
requires four attempts. Neither repetition nor a loss proves the opponent
anticipated an action. Full-force coaching suppresses the older straight-entry
and saber-recovery patterns.

The rotating message forms are:

* `{sequence}: {losses}/{attempts} negative HP/AP trades in {duels} duels. Latest HP/AP you {hp}/{ap}, enemy {hp}/{ap}.`
* `{sequence}: {poor-force}/{attempts} no damage, worse FP trade in {duels} duels. Latest net FP you {fp}, enemy {fp}; HP you {hp}, enemy {hp}.`
* `{sequence}: {attempts} times in {duels} duels, {losses} negative trades. Latest damage dealt/taken {dealt}/{taken}; net FP you {fp}, enemy {fp}.`
* If a detailed message would exceed the chat limit, the corresponding shorter form is `{sequence}: {poor-force}/{attempts} no damage, worse FP trade in {duels} duels.`, `{sequence}: {losses}/{attempts} negative HP/AP trades in {duels} duels.`, or `{sequence}: {attempts} attempts in {duels} duels.`

Only formats whose evidence threshold is met are eligible for rotation. The
changes are signed. Counts and latest values refer to the bounded windows,
not the entire duel.

A receipt and all sequence updates are committed in one SQLite transaction.
Retrying the same identity, mode and duel key does not count it twice; an
error rolls back the receipt and all updates. SQLite errors suppress advice
instead of inventing evidence.

Separately, full-force coaching counts accepted swing and throw starts at
60 HP or less when the opponent has at least 20 more HP, Heal is known and
allowed, the player has its activation FP and at least 10 missing HP. Starts
less than one second apart count once. After observations in two duels, the
latest qualifying attack can produce: `{n} attacks in {duels} duels while low
on HP and behind. Latest: {self} HP vs {enemy}. Make space to Heal before
re-entering.` It is an opportunity to consider healing, not proof the attack
was a mistake or that a heal was possible after the snapshot. The usual pattern
cooldown applies. This reminder can interrupt the ordered introduction without
skipping a lesson.
