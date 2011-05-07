/*
 * Author: Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include "schedule.h"
#include "generator.h"

void mgs_all(struct mg *mg)
{
	struct timeval ref;
	int i;

	gettimeofday(&ref, NULL);

	for (i = 1; i < TRAFFIC_LINE_MAX; i++) {
		if (!mg->lines[i])
			continue;

		if (mg->lines[i]->srcid != mg->options.myid)
			continue;

		mg->lines[i]->schedule.last.tv_sec  = ref.tv_sec  + mg->lines[i]->tv.tv_sec;
		mg->lines[i]->schedule.last.tv_usec = ref.tv_usec + mg->lines[i]->tv.tv_usec;
		evtimer_add(&mg->lines[i]->ev, &mg->lines[i]->tv);

		mg->running++;
	}
}

void mgs_reschedule(struct event *ev, struct schedule *sch, uint32_t time_us)
{
	struct timeval now, wanted, tv;

	gettimeofday(&now, NULL);

	/* compute and update accurate absolute location of requested moment */
	wanted.tv_sec  = sch->last.tv_sec  + (time_us / 1000000);
	wanted.tv_usec = sch->last.tv_usec + (time_us % 1000000);

	if (wanted.tv_usec > 1000000) {
		wanted.tv_sec++;
		wanted.tv_usec -= 1000000;
	}

	sch->last.tv_sec  = wanted.tv_sec;
	sch->last.tv_usec = wanted.tv_usec;

	/* if it has already passed, schedule now */
	if (wanted.tv_sec < now.tv_sec) {
		dbg(1, "lagging over 1s\n");
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	} else if (wanted.tv_sec == now.tv_sec && wanted.tv_usec <= now.tv_usec) {
		dbg(3, "lagging %uus\n", now.tv_usec - wanted.tv_usec);
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	} else {
		if (wanted.tv_usec >= now.tv_usec) {
			tv.tv_sec  = wanted.tv_sec  - now.tv_sec;
			tv.tv_usec = wanted.tv_usec - now.tv_usec;
		} else { /* borrow 1 second */
			tv.tv_sec  = wanted.tv_sec  - now.tv_sec - 1;
			tv.tv_usec = 1000000 + wanted.tv_usec - now.tv_usec;
		}
	}

	evtimer_add(ev, &tv);
}

void mgs_usleep(struct line *line, uint32_t time_us)
{
	mgs_reschedule(&line->ev, &line->schedule, time_us);
}
