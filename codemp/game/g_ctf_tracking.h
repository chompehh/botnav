#ifndef G_CTF_TRACKING_H
#define G_CTF_TRACKING_H

typedef enum {
	CTF_TRACK_PICKUP = 1,
	CTF_TRACK_DROP,
	CTF_TRACK_RETURN,
	CTF_TRACK_CAPTURE
} ctf_track_event_t;

static inline int G_CTFTrackClosesJourney(int event)
{
	return event == CTF_TRACK_DROP || event == CTF_TRACK_RETURN || event == CTF_TRACK_CAPTURE;
}

/* A reset/helper invocation is not evidence of a scored capture. */
static inline int G_CTFTrackSuccess(int event, int playerTeam)
{
	return (event == CTF_TRACK_CAPTURE || event == CTF_TRACK_RETURN) &&
		(playerTeam == 1 || playerTeam == 2);
}

#endif
