#ifndef G_DUEL_TUTORIAL_CHAIN_MESSAGES_H
#define G_DUEL_TUTORIAL_CHAIN_MESSAGES_H

enum
{
	DTC_POOR_FORCE,
	DTC_DRAIN_EXPOSURE,
	DTC_LOW_HEALTH,
	DTC_FORCE_DEFICIT,
	DTC_VS_PTK,
	DTC_VS_PK,
	DTC_VS_GK,
	DTC_VS_SABER,
	DTC_SABER_REPLY,
	DTC_BOT_SUCCESS,
	DTC_THEME_COUNT
};

typedef struct
{
	int theme;
	const char *text;
} duel_chain_message_t;

static const duel_chain_message_t g_duelChainMessages[] =
{
	{ DTC_POOR_FORCE, "%s: No damage credited, worse Force balance in %d/%d attempts; try a saber reply." },
	{ DTC_POOR_FORCE, "%s: No damage and net Force loss in %d/%d attempts; retreat briefly to rebuild." },
	{ DTC_POOR_FORCE, "%s: %d/%d attempts: zero credited damage, worse Force balance; save Force for defense." },
	{ DTC_POOR_FORCE, "%s: %d/%d no-damage FP losses; don't pay for a second Pull before the first throw arrives." },
	{ DTC_POOR_FORCE, "%s: %d/%d attempts had no credited damage and worse net Force; reset before another attack." },
	{ DTC_POOR_FORCE, "%s: %d/%d no-damage FP losses. Pull+Throw example: 40 FP for 30 damage, only if it lands." },
	{ DTC_POOR_FORCE, "%s: %d/%d attempts: no damage credited, net Force down; break off and recover." },
	{ DTC_POOR_FORCE, "%s: %d/%d no-damage FP losses; let my kick finish, then test a saber reply instead of casting." },
	{ DTC_POOR_FORCE, "%s: %d/%d attempts paired no damage credit with Force loss; let Force recover before returning." },
	{ DTC_POOR_FORCE, "%s: No damage credited, Force balance worsened in %d/%d attempts; test a low-cost saber option." },

	{ DTC_DRAIN_EXPOSURE, "%s: Drain active in %d/%d attempts; net Force fell, own Force ended under 20. Make space." },
	{ DTC_DRAIN_EXPOSURE, "%s: Drain active in %d/%d attempts; net Force fell, your Force ended under 20. Reset." },
	{ DTC_DRAIN_EXPOSURE, "%s: %d/%d ended below 20 FP with my Drain active earlier; another Pull delays recovery." },
	{ DTC_DRAIN_EXPOSURE, "%s: Drain active, net Force down, your Force under 20 at end: %d/%d attempts. Save reserve." },
	{ DTC_DRAIN_EXPOSURE, "%s: %d/%d attempts: Drain active, net Force fell, your Force ended below 20; break contact." },
	{ DTC_DRAIN_EXPOSURE, "%s: %d/%d fell below 20 FP with my Drain earlier; break range before spending on a reply." },
	{ DTC_DRAIN_EXPOSURE, "%s: %d/%d fell below 20 FP with my Drain earlier; a clean Drain hit removes up to 4 FP." },
	{ DTC_DRAIN_EXPOSURE, "%s: %d/%d attempts paired active Drain, falling net Force, and Force ending below 20; break contact." },
	{ DTC_DRAIN_EXPOSURE, "%s: %d/%d FP losses under 20 with my Drain earlier; rebuild first, not another costly PTK." },
	{ DTC_DRAIN_EXPOSURE, "%s: Net Force fell in %d/%d attempts with Drain active and your Force under 20; reset." },

	{ DTC_LOW_HEALTH, "%s: Attack at %d HP vs %d; no recovery followed. Use the available Heal or Drain." },
	{ DTC_LOW_HEALTH, "%s: Later attack: %d HP to their %d; Heal or Drain was available, but unused afterward." },
	{ DTC_LOW_HEALTH, "%s: You attacked at %d HP vs %d; no recovery followed. Use available Heal or Drain." },
	{ DTC_LOW_HEALTH, "%s: %d HP vs %d, still attacking; recovery was usable. Make room to Heal or reach with Drain." },
	{ DTC_LOW_HEALTH, "%s: %d HP to their %d at attack; no later recovery attempt. Heal or Drain was available." },
	{ DTC_LOW_HEALTH, "%s: Attack snapshot: %d HP vs %d; no recovery followed. Use the available Heal or Drain." },
	{ DTC_LOW_HEALTH, "%s: Later attack at %d HP, they had %d; no recovery followed though Heal or Drain was legal." },
	{ DTC_LOW_HEALTH, "%s: %d HP vs %d, no recovery; Heal/Drain was usable. Recover before trying the same entry." },
	{ DTC_LOW_HEALTH, "%s: At the later attack: %d HP vs %d; no recovery followed; Heal or Drain was available." },
	{ DTC_LOW_HEALTH, "%s: %d HP against their %d at attack; recovery wasn't tried despite legal Heal or Drain." },

	{ DTC_FORCE_DEFICIT, "%s: At attack: %d own Force to their %d; retreat briefly and rebuild your reserve." },
	{ DTC_FORCE_DEFICIT, "%s: You had %d Force, they had %d; avoid optional casts until you recover." },
	{ DTC_FORCE_DEFICIT, "%s: At attack: %d own Force, %d theirs; step back and regain Force." },
	{ DTC_FORCE_DEFICIT, "%s: %d FP vs my %d; a saber reply to PTK can return damage without paying for another Pull." },
	{ DTC_FORCE_DEFICIT, "%s: %d own Force to their %d; a saber reply avoids extra activation costs." },
	{ DTC_FORCE_DEFICIT, "%s: At attack: %d own Force vs %d theirs; save reserve for defense, not optional casts." },
	{ DTC_FORCE_DEFICIT, "%s: %d FP vs my %d; Pull+Throw costs 40 FP in the base example. Repeating it widens the gap." },
	{ DTC_FORCE_DEFICIT, "%s: Attack snapshot: %d Force for you, %d for them; wait for a useful opening." },
	{ DTC_FORCE_DEFICIT, "%s: You had %d Force to their %d; favor a low-cost reply while recovering." },
	{ DTC_FORCE_DEFICIT, "%s: %d own Force against their %d at attack; conserve it for a meaningful response." },

	{ DTC_VS_PTK, "%s: Observed PTK with bad HP/AP trades in %d/%d attempts; reset your defense." },
	{ DTC_VS_PTK, "%s: Observed PTK, poor trades: %d/%d attempts; saber reply during recovery can save Force." },
	{ DTC_VS_PTK, "%s: %d/%d bad trades with my PTK; test a saber swing as my kick ends, rather than a second Pull." },
	{ DTC_VS_PTK, "%s: %d/%d bad trades with my PTK; vary your saber entry between my throw and kick, not every cast." },
	{ DTC_VS_PTK, "%s: Observed PTK, %d/%d negative HP/AP trades; retreat to regain Force before re-entry." },
	{ DTC_VS_PTK, "%s: %d/%d poor trades alongside observed PTK; answer with saber to conserve Force." },
	{ DTC_VS_PTK, "%s: %d/%d bad PTK trades. Example: 30 saber + 30 throw + 20 kick = 80 damage if all land." },
	{ DTC_VS_PTK, "%s: %d/%d negative trades with PTK observed; avoid spending Force on a repeated plan." },
	{ DTC_VS_PTK, "%s: Observed PTK accompanied %d/%d poor trades; reset defense, then choose a new entry." },
	{ DTC_VS_PTK, "%s: %d/%d bad PTK trades; a fresh Pull+Throw example costs 40 FP for 30 damage if it lands." },

	{ DTC_VS_PK, "%s: Observed PK with bad HP/AP trades in %d/%d attempts; change your response." },
	{ DTC_VS_PK, "%s: PK pattern, %d/%d poor trades; a saber reply during recovery can conserve Force." },
	{ DTC_VS_PK, "%s: %d/%d bad PK trades; test a saber reply after my kick. Pull's base cost is 20 FP." },
	{ DTC_VS_PK, "%s: Bad trades in %d/%d attempts with PK observed; retreat and recover before re-engaging." },
	{ DTC_VS_PK, "%s: %d/%d bad PK trades; rebuild FP before re-entry. Under 20, another Pull can move you." },
	{ DTC_VS_PK, "%s: %d/%d poor trades alongside observed PK; consider a low-cost saber answer." },
	{ DTC_VS_PK, "%s: PK observed in %d/%d negative-trade attempts; vary your defense after the exchange." },
	{ DTC_VS_PK, "%s: %d/%d bad PK trades; let the kick pass, then test a saber hit instead of matching Pulls." },
	{ DTC_VS_PK, "%s: Observed PK and %d/%d bad trades; make space, then return with a different plan." },
	{ DTC_VS_PK, "%s: %d/%d negative trades alongside observed PK; don't spend Force just to repeat a plan." },

	{ DTC_VS_GK, "%s: Observed GK with bad HP/AP trades in %d/%d attempts; if grip starts, break hold before choke." },
	{ DTC_VS_GK, "%s: %d/%d bad GK trades; level-3 choke adds 40 base damage. Break the hold before that arrives." },
	{ DTC_VS_GK, "%s: %d/%d negative trades with observed GK; if gripped, break hold first, then make space." },
	{ DTC_VS_GK, "%s: Bad trades in %d/%d attempts with GK observed; if gripped, save Force for escape." },
	{ DTC_VS_GK, "%s: Observed GK, %d/%d poor trades; if held, break free before attacking." },
	{ DTC_VS_GK, "%s: %d/%d bad GK trades. Three 20-damage kicks + 40 choke = 100 HP; escape before the choke." },
	{ DTC_VS_GK, "%s: GK observed in %d/%d negative-trade attempts; reset after escaping, then recover." },
	{ DTC_VS_GK, "%s: %d/%d bad GK trades; if you have Rage and 50 FP, use it before choke, then escape." },
	{ DTC_VS_GK, "%s: Observed GK accompanied %d/%d bad trades; if held, prioritize escape over attack." },
	{ DTC_VS_GK, "%s: %d/%d negative trades alongside GK; if gripped, break hold, retreat, and rebuild Force." },

	{ DTC_VS_SABER, "%s: Saber swings with bad HP/AP trades in %d/%d attempts; defend, then reset." },
	{ DTC_VS_SABER, "%s: %d/%d poor trades with observed swings; retreat after the exchange to recover." },
	{ DTC_VS_SABER, "%s: Negative trades in %d/%d swing patterns; choose a defensive reply before re-entry." },
	{ DTC_VS_SABER, "%s: %d/%d bad saber trades; test a reply as my swing recovers, not another Pull into its arc." },
	{ DTC_VS_SABER, "%s: %d/%d poor trades during swing patterns; use a saber defense to spare Force." },
	{ DTC_VS_SABER, "%s: Saber pattern with %d/%d negative trades; reset defense before countering." },
	{ DTC_VS_SABER, "%s: %d/%d bad saber trades; vary when you enter between my swings instead of charging every time." },
	{ DTC_VS_SABER, "%s: Observed saber swings and %d/%d poor trades; save Force for defense." },
	{ DTC_VS_SABER, "%s: %d/%d negative trades vs swings; disengage briefly rather than answer each one." },
	{ DTC_VS_SABER, "%s: Poor exchanges in %d/%d saber attempts; keep a low-cost defense ready." },

	{ DTC_SABER_REPLY, "%s: Whole-window totals: %d dealt, %d taken; keep saber as a low-cost option." },
	{ DTC_SABER_REPLY, "%s: %d damage dealt vs %d taken, with saber replies and a good FP trade; keep that option." },
	{ DTC_SABER_REPLY, "%s: Window-wide damage was %d dealt, %d taken; consider a saber reply to save Force." },
	{ DTC_SABER_REPLY, "%s: %d dealt vs %d taken across this chain; you kept pace in FP with saber replies, too." },
	{ DTC_SABER_REPLY, "%s: %d dealt vs %d taken across all attacks; keep the saber reply before adding a costly Pull." },
	{ DTC_SABER_REPLY, "%s: Window totals were %d dealt, %d taken; preserve Force with a saber response when useful." },
	{ DTC_SABER_REPLY, "%s: %d dealt vs %d taken across this chain; your saber replies coincided with a better trade." },
	{ DTC_SABER_REPLY, "%s: You dealt %d and took %d overall; this positive exchange kept pace in net Force." },
	{ DTC_SABER_REPLY, "%s: Across all actions: %d dealt, %d taken; favor a saber answer over extra Force costs." },
	{ DTC_SABER_REPLY, "%s: %d dealt vs %d taken across this chain; fewer extra casts may preserve that FP advantage." },

	{ DTC_BOT_SUCCESS, "%s: Repeated habit: %d/%d positive trades vs bots in won duels; humans may read it differently." },
	{ DTC_BOT_SUCCESS, "%s: This pattern: %d/%d positive bot exchanges in won duels; people may answer another way." },
	{ DTC_BOT_SUCCESS, "%s: %d/%d positive trades vs bots in won duels; human opponents may read the repeat." },
	{ DTC_BOT_SUCCESS, "%s: Your repeated chain had %d/%d positive bot trades in won duels; human replies can differ." },
	{ DTC_BOT_SUCCESS, "%s: This habit: %d/%d positive bot trades in won duels; don't assume people will copy it." },
	{ DTC_BOT_SUCCESS, "%s: %d/%d positive exchanges with bots in won duels; a person may recognize the repeat." },
	{ DTC_BOT_SUCCESS, "%s: Your repeated sequence: %d/%d positive bot trades in won duels; human reads may differ." },
	{ DTC_BOT_SUCCESS, "%s: %d/%d bot exchanges were positive in won duels; vary the habit if a human adapts." },
	{ DTC_BOT_SUCCESS, "%s: This repeated approach had %d/%d positive bot trades in won duels; human responses vary." },
	{ DTC_BOT_SUCCESS, "%s: %d/%d positive bot trades in won duels; use the lesson cautiously against people." }
};

#define DUEL_CHAIN_MESSAGE_COUNT (sizeof(g_duelChainMessages) / sizeof(g_duelChainMessages[0]))

#endif
