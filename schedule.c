/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include "schedule.h"
#include "generator.h"

void mgs_all(struct mg *mg)
{
	int i;

	for (i = 1; i < TRAFFIC_LINE_MAX; i++) {
		if (!mg->lines[i])
			continue;

		if (mg->lines[i]->srcid != mg->options.myid)
			continue;

		mg->lines[i]->schedule.last.tv_sec  = mg->origin.tv_sec;
		mg->lines[i]->schedule.last.tv_usec = mg->origin.tv_usec;
		mgs_sleep(mg->lines[i], NULL);

		mg->running++;
	}
}

void mgs_schedule(struct event *ev, struct schedule *sch, struct timeval *timeout)
{
	struct timeval now, wanted, tv;

	/* compute and update accurate absolute location of requested moment */
	timeradd(&sch->last, timeout, &wanted);

	/* store last schedule */
	sch->last.tv_sec  = wanted.tv_sec;
	sch->last.tv_usec = wanted.tv_usec;

	/* schedule */
	gettimeofday(&now, NULL);
	if (wanted.tv_sec < now.tv_sec) {
		dbg(1, "lagging over 1s\n");
		timerclear(&tv);
	} else {
		timersub(&wanted, &now, &tv);
	}
	evtimer_add(ev, &tv);
}

void mgs_uschedule(struct event *ev, struct schedule *sch, uint32_t time_us)
{
	struct timeval tv;

	tv.tv_sec  = time_us / 1000000;
	tv.tv_usec = time_us % 1000000;
	mgs_schedule(ev, sch, &tv);
}

void mgs_sleep(struct line *line, struct timeval *tv)
{
	mgs_schedule(&line->ev, &line->schedule, tv ? tv : &line->tv);
}

void mgs_usleep(struct line *line, uint32_t time_us)
{
	mgs_uschedule(&line->ev, &line->schedule, time_us);
}
