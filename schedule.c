/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include "schedule.h"
#include "generator.h"

void mgs_schedule_all_lines(struct mg *mg)
{
	int i;

	/* run all lines belonging to this node */
	for (i = 1; i < TRAFFIC_LINE_MAX; i++) {
		if (!mg->lines[i])
			continue;

		if (mg->lines[i]->srcid != mg->options.myid)
			continue;

		/* this will schedule first execution */
		mgs_sleep(mg->lines[i], NULL);
		mg->running++;
	}
}

void mgs_schedule(struct schedule *sch, struct timeval *timeout)
{
	struct timeval now, wanted, tv;

	/* first "last run" is at the origin */
	if (sch->last.tv_sec == 0) {
		sch->last.tv_sec = sch->mg->origin.tv_sec;
		sch->last.tv_usec = sch->mg->origin.tv_usec;
	}

	/* compute and update accurate absolute location of requested moment */
	timeradd(&sch->last, timeout, &wanted);

	/* store last schedule */
	sch->last.tv_sec  = wanted.tv_sec;
	sch->last.tv_usec = wanted.tv_usec;

	/* calculate time distance */
	gettimeofday(&now, NULL);
	if (now.tv_sec > wanted.tv_sec) {
		dbg(1, "lagging over 1s\n");
		sch->lagging = true;
		timerclear(&tv);
	} else if (now.tv_sec == wanted.tv_sec && now.tv_usec >= wanted.tv_usec) {
		sch->lagging = true;
		timerclear(&tv);
	} else {
		sch->lagging = false;
		timersub(&wanted, &now, &tv);
	}

	/* schedule */
	//if (sch->cb && sch->lagging) {
	//	sch->cb(-1, EV_TIMEOUT, sch->arg);
	//} else {
		evtimer_add(&sch->ev, &tv);
	//}
}

void mgs_uschedule(struct schedule *sch, uint32_t time_us)
{
	struct timeval tv;

	tv.tv_sec  = time_us / 1000000;
	tv.tv_usec = time_us % 1000000;
	mgs_schedule(sch, &tv);
}

void mgs_sleep(struct line *line, struct timeval *tv)
{
	mgs_schedule(&line->schedule, tv ? tv : &line->tv);
}

void mgs_usleep(struct line *line, uint32_t time_us)
{
	mgs_uschedule(&line->schedule, time_us);
}

void mgs_setup(struct schedule *sch, struct mg *mg, void (*cb)(int, short, void *), void *arg)
{
	sch->mg = mg;
	sch->cb = cb;
	sch->arg = arg;
	evtimer_set(&sch->ev, cb, arg);
}
