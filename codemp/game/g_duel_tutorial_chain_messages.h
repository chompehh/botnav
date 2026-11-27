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
	{ DTC_POOR_FORCE, "%s: No ^1damage^7 credited, worse ^5Force^7 balance in %d/%d attempts; try a saber reply." },
	{ DTC_POOR_FORCE, "%s: No ^1damage^7 and net ^5Force^7 loss in %d/%d attempts; retreat briefly to rebuild." },
	{ DTC_POOR_FORCE, "%s: %d/%d attempts: zero credited ^1damage^7, worse ^5Force^7 balance; save ^5Force^7 for defense." },
	{ DTC_POOR_FORCE, "%s: %d/%d no-^1damage^7 ^5FP^7 losses; don't pay for a second Pull before the first throw arrives." },
	{ DTC_POOR_FORCE, "%s: %d/%d attempts had no credited ^1damage^7 and worse net ^5Force^7; reset before another attack." },
	{ DTC_POOR_FORCE, "%s: %d/%d no-^1damage^7 ^5FP^7 losses. Pull+Throw costs ^540 FP^7 for ^130 damage^7." },
	{ DTC_POOR_FORCE, "%s: %d/%d attempts: no ^1damage^7 credited, net ^5Force^7 down; break off and recover." },
	{ DTC_POOR_FORCE, "%s: %d/%d no-^1damage^7 ^5FP^7 losses; let my kick finish, then test a saber reply instead of casting." },
	{ DTC_POOR_FORCE, "%s: %d/%d attempts had no ^1damage^7 and lost ^5Force^7; recover before returning." },
	{ DTC_POOR_FORCE, "%s: No ^1damage^7 credited, ^5Force^7 balance worsened in %d/%d attempts; test a low-cost saber option." },

	{ DTC_DRAIN_EXPOSURE, "%s: Drain active in %d/%d attempts; net ^5Force^7 fell, own ^5Force^7 ended under 20. Make space." },
	{ DTC_DRAIN_EXPOSURE, "%s: Drain active in %d/%d attempts; net ^5Force^7 fell, your ^5Force^7 ended under 20. Reset." },
	{ DTC_DRAIN_EXPOSURE, "%s: %d/%d ended below ^520 FP^7 with my Drain active earlier; another Pull delays recovery." },
	{ DTC_DRAIN_EXPOSURE, "%s: Drain active, net ^5Force^7 down, your ^5Force^7 under 20 at end: %d/%d attempts. Save reserve." },
	{ DTC_DRAIN_EXPOSURE, "%s: %d/%d attempts: Drain active, net ^5Force^7 fell, your ^5Force^7 ended below 20; break contact." },
	{ DTC_DRAIN_EXPOSURE, "%s: %d/%d fell below ^520 FP^7 with my Drain earlier; break range before spending on a reply." },
	{ DTC_DRAIN_EXPOSURE, "%s: %d/%d fell below ^520 FP^7 with my Drain earlier; break line of sight to recover." },
	{ DTC_DRAIN_EXPOSURE, "%s: %d/%d attempts ended below ^520 Force^7 after Drain; break contact." },
	{ DTC_DRAIN_EXPOSURE, "%s: %d/%d ^5FP^7 losses under 20 with my Drain earlier; rebuild first, not another costly ^3PTK^7." },
	{ DTC_DRAIN_EXPOSURE, "%s: Net ^5Force^7 fell in %d/%d attempts with Drain active and your ^5Force^7 under 20; reset." },

	{ DTC_LOW_HEALTH, "%s: Attack at %d ^1HP^7 vs %d; no recovery followed. Use the available Heal or Drain." },
	{ DTC_LOW_HEALTH, "%s: Later attack: %d ^1HP^7 to their %d; Heal or Drain was available, but unused afterward." },
	{ DTC_LOW_HEALTH, "%s: You attacked at %d ^1HP^7 vs %d; no recovery followed. Use available Heal or Drain." },
	{ DTC_LOW_HEALTH, "%s: %d ^1HP^7 vs %d, still attacking; recovery was usable. Make room to Heal or reach with Drain." },
	{ DTC_LOW_HEALTH, "%s: %d ^1HP^7 to their %d at attack; no later recovery attempt. Heal or Drain was available." },
	{ DTC_LOW_HEALTH, "%s: Attack snapshot: %d ^1HP^7 vs %d; no recovery followed. Use the available Heal or Drain." },
	{ DTC_LOW_HEALTH, "%s: Later attack at %d ^1HP^7, they had %d; no recovery followed though Heal or Drain was legal." },
	{ DTC_LOW_HEALTH, "%s: %d ^1HP^7 vs %d, no recovery; Heal/Drain was usable. Recover before trying the same entry." },
	{ DTC_LOW_HEALTH, "%s: At the later attack: %d ^1HP^7 vs %d; no recovery followed; Heal or Drain was available." },
	{ DTC_LOW_HEALTH, "%s: %d ^1HP^7 against their %d at attack; recovery wasn't tried despite legal Heal or Drain." },

	{ DTC_FORCE_DEFICIT, "%s: At attack: %d own ^5Force^7 to their %d; retreat briefly and rebuild your reserve." },
	{ DTC_FORCE_DEFICIT, "%s: You had %d ^5Force^7, they had %d; avoid optional casts until you recover." },
	{ DTC_FORCE_DEFICIT, "%s: At attack: %d own ^5Force^7, %d theirs; step back and regain ^5Force^7." },
	{ DTC_FORCE_DEFICIT, "%s: %d ^5FP^7 vs my %d; a saber reply to ^3PTK^7 can return ^1damage^7 without paying for another Pull." },
	{ DTC_FORCE_DEFICIT, "%s: %d own ^5Force^7 to their %d; a saber reply avoids extra activation costs." },
	{ DTC_FORCE_DEFICIT, "%s: At attack: %d own ^5Force^7 vs %d theirs; save reserve for defense, not optional casts." },
	{ DTC_FORCE_DEFICIT, "%s: %d ^5FP^7 vs my %d; Pull+Throw costs ^540 FP^7 in the base example. Repeating it widens the gap." },
	{ DTC_FORCE_DEFICIT, "%s: Attack snapshot: %d ^5Force^7 for you, %d for them; wait for a useful opening." },
	{ DTC_FORCE_DEFICIT, "%s: You had %d ^5Force^7 to their %d; favor a low-cost reply while recovering." },
	{ DTC_FORCE_DEFICIT, "%s: %d own ^5Force^7 against their %d at attack; conserve it for a meaningful response." },

	{ DTC_VS_PTK, "%s: Observed ^3PTK^7 with bad ^1HP^7/^1AP^7 trades in %d/%d attempts; reset your defense." },
	{ DTC_VS_PTK, "%s: Observed ^3PTK^7, poor trades: %d/%d attempts; saber reply during recovery can save ^5Force^7." },
	{ DTC_VS_PTK, "%s: %d/%d bad trades with my ^3PTK^7; test a saber swing as my kick ends, rather than a second Pull." },
	{ DTC_VS_PTK, "%s: %d/%d bad trades with my ^3PTK^7; vary your saber entry between my throw and kick, not every cast." },
	{ DTC_VS_PTK, "%s: Observed ^3PTK^7, %d/%d negative ^1HP^7/^1AP^7 trades; retreat to regain ^5Force^7 before re-entry." },
	{ DTC_VS_PTK, "%s: %d/%d poor trades alongside observed ^3PTK^7; answer with saber to conserve ^5Force^7." },
	{ DTC_VS_PTK, "%s: %d/%d bad ^3PTK^7 trades. Example: 30 saber + 30 throw + 20 kick = ^180 damage^7 if all land." },
	{ DTC_VS_PTK, "%s: %d/%d negative trades with ^3PTK^7 observed; avoid spending ^5Force^7 on a repeated plan." },
	{ DTC_VS_PTK, "%s: Observed ^3PTK^7 accompanied %d/%d poor trades; reset defense, then choose a new entry." },
	{ DTC_VS_PTK, "%s: %d/%d bad ^3PTK^7 trades; a fresh Pull+Throw example costs ^540 FP^7 for ^130 damage^7 if it lands." },

	{ DTC_VS_PK, "%s: Observed ^3PK^7 with bad ^1HP^7/^1AP^7 trades in %d/%d attempts; change your response." },
	{ DTC_VS_PK, "%s: ^3PK^7 pattern, %d/%d poor trades; a saber reply during recovery can conserve ^5Force^7." },
	{ DTC_VS_PK, "%s: %d/%d bad ^3PK^7 trades; test a saber reply after my kick. Pull's base cost is ^520 FP^7." },
	{ DTC_VS_PK, "%s: Bad trades in %d/%d attempts with ^3PK^7 observed; retreat and recover before re-engaging." },
	{ DTC_VS_PK, "%s: %d/%d bad ^3PK^7 trades; rebuild ^5FP^7 before re-entry. Under 20, another Pull can move you." },
	{ DTC_VS_PK, "%s: %d/%d poor trades alongside observed ^3PK^7; consider a low-cost saber answer." },
	{ DTC_VS_PK, "%s: ^3PK^7 observed in %d/%d negative-trade attempts; vary your defense after the exchange." },
	{ DTC_VS_PK, "%s: %d/%d bad ^3PK^7 trades; let the kick pass, then test a saber hit instead of matching Pulls." },
	{ DTC_VS_PK, "%s: Observed ^3PK^7 and %d/%d bad trades; make space, then return with a different plan." },
	{ DTC_VS_PK, "%s: %d/%d negative trades alongside observed ^3PK^7; don't spend ^5Force^7 just to repeat a plan." },

	{ DTC_VS_GK, "%s: ^3GK^7 observed in %d/%d bad ^1HP^7/^1AP^7 trades; break the hold before choke." },
	{ DTC_VS_GK, "%s: %d/%d bad ^3GK^7 trades; level-3 choke adds 40 base ^1damage^7. Break the hold before that arrives." },
	{ DTC_VS_GK, "%s: %d/%d negative trades with observed ^3GK^7; if gripped, break hold first, then make space." },
	{ DTC_VS_GK, "%s: Bad trades in %d/%d attempts with ^3GK^7 observed; if gripped, save ^5Force^7 for escape." },
	{ DTC_VS_GK, "%s: Observed ^3GK^7, %d/%d poor trades; if held, break free before attacking." },
	{ DTC_VS_GK, "%s: %d/%d bad ^3GK^7 trades. Three ^120-damage^7 kicks + 40 choke = ^1100 HP^7; escape before the choke." },
	{ DTC_VS_GK, "%s: ^3GK^7 observed in %d/%d negative-trade attempts; reset after escaping, then recover." },
	{ DTC_VS_GK, "%s: %d/%d bad ^3GK^7 trades; if you have Rage and ^550 FP^7, use it before choke, then escape." },
	{ DTC_VS_GK, "%s: Observed ^3GK^7 accompanied %d/%d bad trades; if held, prioritize escape over attack." },
	{ DTC_VS_GK, "%s: %d/%d negative trades alongside ^3GK^7; if gripped, break hold, retreat, and rebuild ^5Force^7." },

	{ DTC_VS_SABER, "%s: Saber swings with bad ^1HP^7/^1AP^7 trades in %d/%d attempts; defend, then reset." },
	{ DTC_VS_SABER, "%s: %d/%d poor trades with observed swings; retreat after the exchange to recover." },
	{ DTC_VS_SABER, "%s: Negative trades in %d/%d swing patterns; choose a defensive reply before re-entry." },
	{ DTC_VS_SABER, "%s: %d/%d bad saber trades; test a reply as my swing recovers, not another Pull into its arc." },
	{ DTC_VS_SABER, "%s: %d/%d poor trades during swing patterns; use a saber defense to spare ^5Force^7." },
	{ DTC_VS_SABER, "%s: Saber pattern with %d/%d negative trades; reset defense before countering." },
	{ DTC_VS_SABER, "%s: %d/%d bad saber trades; vary when you enter between my swings instead of charging every time." },
	{ DTC_VS_SABER, "%s: Observed saber swings and %d/%d poor trades; save ^5Force^7 for defense." },
	{ DTC_VS_SABER, "%s: %d/%d negative trades vs swings; disengage briefly rather than answer each one." },
	{ DTC_VS_SABER, "%s: Poor exchanges in %d/%d saber attempts; keep a low-cost defense ready." },

	{ DTC_SABER_REPLY, "%s: Whole-window totals: %d dealt, %d taken; keep saber as a low-cost option." },
	{ DTC_SABER_REPLY, "%s: %d ^1damage^7 dealt vs %d taken, with saber replies and a good ^5FP^7 trade; keep that option." },
	{ DTC_SABER_REPLY, "%s: Window-wide ^1damage^7 was %d dealt, %d taken; consider a saber reply to save ^5Force^7." },
	{ DTC_SABER_REPLY, "%s: %d dealt vs %d taken across this chain; you kept pace in ^5FP^7 with saber replies, too." },
	{ DTC_SABER_REPLY, "%s: %d dealt vs %d taken across all attacks; keep the saber reply before adding a costly Pull." },
	{ DTC_SABER_REPLY, "%s: Window totals were %d dealt, %d taken; preserve ^5Force^7 with a saber response when useful." },
	{ DTC_SABER_REPLY, "%s: %d dealt vs %d taken across this chain; your saber replies coincided with a better trade." },
	{ DTC_SABER_REPLY, "%s: You dealt %d and took %d overall; this positive exchange kept pace in net ^5Force^7." },
	{ DTC_SABER_REPLY, "%s: Across all actions: %d dealt, %d taken; favor a saber answer over extra ^5Force^7 costs." },
	{ DTC_SABER_REPLY, "%s: %d dealt vs %d taken across this chain; fewer extra casts may preserve that ^5FP^7 advantage." },

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
