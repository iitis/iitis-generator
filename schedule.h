/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include "generator.h"

/** Schedule all our lines (used once on startup) */
void mgs_all(struct mg *mg);

/** Accurately schedule event to run again
 * Uses absolute time to maintain accurate time pause
 * @param event      event
 * @param sch        scheduler info
 * @param time       time of pause */
void mgs_schedule(struct event *ev, struct schedule *sch, struct timeval *tv);

/** Version of mgs_reschedule_tv() accepting microseconds */
void mgs_uschedule(struct event *ev, struct schedule *sch, uint32_t time_us);

/** Schedule traffic line to run after given time
 * @param tv         time of pause, if 0 take line->tv
 * @note uses mgs_reschedule() */
void mgs_sleep(struct line *line, struct timeval *tv);

/** Version of mgs_sleep_tv() accepting microseconds */
void mgs_usleep(struct line *line, uint32_t time_us);


#endif
