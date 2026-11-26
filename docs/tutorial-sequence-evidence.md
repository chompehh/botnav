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
cooldown applies. This reminder follows the beginner curriculum rather than
interrupting the introduction.

## Group C: long, two-sided exchanges

`g_duel_tutorial_chains.h` and `g_duel_tutorial_chain_messages.h` add 100
personalized messages, ten per evidence theme. Their exact text is in the
Group C section of `tutorial-curriculum.md`.

Both participants must have complete paired full-force captures. Merge their
events chronologically, orient resources to the trainee, and count damage only
from the trainee's event stream to avoid counting the opponent's reciprocal
damage row again. Accepted swing starts, throw starts, power-state transitions,
confirmed flipkick targets and ordinary kick edges identify actions. Heal
uses a successful-use counter from `ForceHeal`, sampled in the existing event
context: all Heal levels are instant here, so neither active-power edges nor
selected-power guesses can prove that it was used. This counter does not add
duplicate FP-spend rows to the export. Selected
powers or a shared wall-flip animation alone do not. A simultaneous attack and
power transition retain both actions, but same-time ordering is not proof of a
reaction or an opponent's intent.

An exchange needs at least four actions, including two offensive actions by
each side. Drain and Heal do not satisfy the offensive-action count. An exact
signature retains the interleaved actor/action order; actions by the opponent
cannot be mistaken for the trainee's. Start resources come from a strictly
earlier snapshot within 250 ms. Observe through two seconds after the latest
action, capped at twelve seconds from the start. An endpoint must be within
250 ms of that deadline. Later damage is excluded; endpoint resource changes
can include that extra interval. A gap over 500 ms, a terminal/unknown snapshot,
a ninth action or an unfinished deadline discards the active exchange, not a
silently truncated chain. At most 32 finished windows are retained; overflow
or invalid ordering invalidates the observation and logs that no chain advice
was used.

`LocalBotTutorialChainV1` stores attempts and theme matches per identity,
opponent kind and exact signature. `LocalBotTutorialChainReceiptV1` makes
receipt and count updates transactional and idempotent. Human-opponent and
bot-opponent evidence are separate. Eligibility needs three observations of
that signature, two matching outcomes in two distinct duels, a matching latest
window, the normal three-informative-duel gate and four coaching rotations
since the same signature/theme was last taught. Low-health opportunities take
priority. Once beginner lessons are exhausted, or for players whose results and
signals do not favor beginner lessons, eligible long-chain evidence takes
priority over short-sequence or stock advice.

The chat label is a compact summary: trainee actions on the left, opponent
actions on the right. `You:PTK..Saber / Me:Pull..Throw` summarizes a longer chain;
`..` is not an exact listing. Exact matching uses the stored signature, not
this abbreviated label.

| Theme | Required latest evidence | Values in the message |
|---|---|---|
| Poor Force use | No attributed damage, no positive HP/AP gain or trade, net FP decline worse than the opponent | Matching windows / all observations of this chain |
| Drain exposure | At least two opponent-active-Drain samples 100 ms apart, trainee net FP decline, ends below 20 FP | Matches / observations; not proof that this chain caused Drainlock |
| Low-health aggression | Later attack at 60 HP or less, opponent at least 20 HP ahead, usable recovery option at that attack, no subsequent Heal/Drain action in the window | Latest attack HP / opponent HP |
| Force deficit | Later attack with opponent at least 20 FP ahead | Latest trainee FP / opponent FP |
| Against PTK, PK or GK | Recognized opponent combo with a negative HP/AP trade | Matches / observations; actions are not proof every hit landed |
| Against saber | At least two opponent swings and negative HP/AP trade | Matches / observations |
| Positive saber reply | At least two trainee swings, better HP change and HP/AP trade, more damage dealt than taken, net FP at least as good as opponent | Whole-window damage dealt / taken, not damage attributed solely to saber |
| Bot-specific success | Positive exchange in a human-won duel against a bot | Matches / all observations of this chain, including non-winning duels |

Recovery usability is captured with the engine's power-use checks and at
least 10 missing HP; Drain additionally needs 25 FP and a target with FP.
This describes an opportunity at the attack, not proof that later recovery
could reach the opponent or escape every follow-up. Net FP includes Drain,
regeneration and other actions; it is not gross combo cost. No message proves
an untried counter would have succeeded, or that repetition establishes
predictability. On human wins, only the bot-success theme may speak; repeated
success against bots is not promoted as a proven habit against humans.
Acknowledgments can precede completion of the introduction for players whose
results and signals do not favor beginner lessons.

## Historical Dueltracks release sample

The release's first three event attachments were analyzed programmatically,
not the whole release. The `prefix` filenames below contain complete
attachments: each is smaller than the requested 100 MB range. Parts 4 through
45 were not sampled. Metadata covers more captures than these event files.
No player keys, names, addresses or raw CSVs are committed.

To reproduce, download the following attachments into an input directory:

| Filename | Attachment |
|---|---|
| `sessions.csv` | `https://github.com/user-attachments/files/34650834/dueltrack_sessions.2.csv` |
| `participants.csv` | `https://github.com/user-attachments/files/34650833/dueltrack_participants.2.csv` |
| `events-prefix.csv` | `https://github.com/user-attachments/files/34650461/dueltrack_events.2.csv` |
| `events-part2-prefix.csv` | `https://github.com/user-attachments/files/34650504/dueltrack_events_part2.2.csv` |
| `events-part3-prefix.csv` | `https://github.com/user-attachments/files/34650638/dueltrack_events_part3.2.csv` |

```sh
python3 tools/analyze_tutorial_chains.py /path/to/input-directory
```

This is the executable query behind every figure below. It joins exact
reciprocal participant/opponent pairs, requires retained event counts to
match both participants and no dropped/critical-dropped events, and selects
full-force sessions. It excludes selected-power fallback events, groups
same-power observations separated by less than 800 ms into state-supported
bursts, and forms greedy nonoverlapping four-to-eight-action windows within
twelve seconds, with at least two observations per side. A valid resource
baseline must precede the first action by no more than one second; the
endpoint is the last action's snapshot. These **historical query windows
differ from the stricter live recognizer** and are not imported as tutorial
confidence. Families overlap; component medians are calculated independently.
Invalid/missing resource snapshots are excluded, not filled with zeros.

The query reads 95,269 event rows (export format 12, capture version 13).
Of 108 represented sessions, 107 have complete retained counts; 89 are
full-force, comprising 37 human/bot and 52 human/human duels. It produces
557 eligible windows in 85 duels: 460 human-first and 97 bot-first windows.
The following results include both actor kinds unless stated otherwise.
All tuple values are signed net `(HP, AP, FP)` changes, not cast costs.

| Window family | Windows / distinct duels | Actor median change | Opponent median change |
|---|---:|---|---|
| Pull then Throw | 134 / 63 | `(0, 0, -36)` | `(-9, -10, +1)` |
| Pull then Saber | 102 / 56 | `(0, 0, -11.5)` | `(0, -0.5, +3)` |
| Saber and Pull | 222 / 76 | `(0, 0, -24)` | `(-4, -1, -0.5)` |
| Starts behind in HP, at least two own offensive observations | 202 / 76 | `(0, 0, -16.5)` | `(-4, 0, -5)` |
| Both sides Drain-classified | 198 / 65 | `(0, 0, -6)` | `(0, 0, 0)` |
| Pull or Grip with opponent Drain-classified | 236 / 75 | `(0, 0, -26)` | `(-4, 0, 0)` |
| Second-half window, starts below 20 FP, opponent Drain-classified | 12 / 12 | `(-16, 0, +26)` | `(-11, 0, +28)` |
| Speed then Throw with opponent Drain-classified | 3 / 3 | `(0, 0, -32)` | `(-3, 0, +40)` |

An exact recurring eight-action example is
`A:Saber O:Saber O:Saber A:Throw A:Pull O:Throw O:Pull A:Throw`:
two windows in two duels, median net A `(-10, -20, -44.5)`,
O `(-10, -1, -34)`. This motivates recording multi-attack reply chains
and their resource exchanges, not prescribing this sequence as a counter.
The low-FP family usually regenerated FP within its window, so its figures
must not be reported as demonstrated causal Drainlocks.

The old export lacks confirmed kick targets, so PK/PTK/GK are **not
measurable from this sample**, rather than having zero occurrences. Force
bursts can include loss misattributed to an active power; starts do not prove
hits. Exact recurring templates have only two or three observations, and
Speed/Throw has only three windows. None proves causality, intent, guaranteed
damage, a successful counter or a player-level pattern across identities.
These observations inform the new themes; personalized kick-specific advice
requires the new live capture, and arithmetic lessons use independently
checked mechanics rather than treating net changes as costs.
