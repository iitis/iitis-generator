/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include "schedule.h"
#include "generator.h"
#include "stats.h"

void mgs_schedule(struct schedule *sch, struct timeval *timeout)
{
	struct timeval now, wanted, tv;

	stats_count(sch->mg->stats, "scheduler_evt");

	/* first "last run" is at the origin */
	if (sch->last.tv_sec == 0) {
		sch->last.tv_sec = sch->mg->origin.tv_sec;
		sch->last.tv_usec = sch->mg->origin.tv_usec;
	}

	/* compute and update accurate absolute location of requested moment */
	timeradd(&sch->last, timeout, &wanted);

	/* how it looks compared to time now? */
	gettimeofday(&now, NULL);

	if (timercmp(&now, &wanted, >)) {
		timerclear(&tv);
		stats_count(sch->mg->stats, "scheduler_lag");
	} else {
		timersub(&wanted, &now, &tv);
	}

	/* schedule */
	evtimer_add(&sch->ev, &tv);

	/* store last schedule */
	sch->last.tv_sec  = wanted.tv_sec;
	sch->last.tv_usec = wanted.tv_usec;
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
