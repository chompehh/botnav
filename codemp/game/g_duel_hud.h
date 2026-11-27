#ifndef G_DUEL_HUD_H
#define G_DUEL_HUD_H

/* Entity health fields have ten network bits. Preserve the fraction if needed. */
static inline void G_DuelHudNetworkValue(int value, int maximum, int *outValue, int *outMaximum)
{
	if (maximum < 1)
		maximum = 1;
	if (value < 0)
		value = 0;
	if (value > maximum)
		value = maximum;
	if (maximum > 1023)
	{
		value = (int)((double)value * 1023.0 / maximum);
		maximum = 1023;
	}
	*outValue = value;
	*outMaximum = maximum;
}

static inline int G_DuelHudOpponent(int self, int privateDuel, int privateOpponent,
	int regularDuel, int firstDuelist, int secondDuelist)
{
	if (privateDuel)
		return privateOpponent;
	if (regularDuel && self == firstDuelist)
		return secondDuelist;
	if (regularDuel && self == secondDuelist)
		return firstDuelist;
	return -1;
}

static inline int G_DuelHudDisplayValid(int mode, int receivedMode, int now, int receivedTime,
	int selfHealth, int opponent, int receivedOpponent, int maxClients)
{
	return (mode == 2 || mode == 3) && receivedMode == mode &&
		now >= receivedTime && now - receivedTime <= 1500 && selfHealth > 0 &&
		opponent >= 0 && opponent < maxClients && opponent == receivedOpponent;
}

#endif
