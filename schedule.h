/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include "generator.h"

/** Schedule all our lines (used once on startup) */
void mgs_schedule_all_lines(struct mg *mg);

/** Accurately schedule event to run again
 * Uses absolute time to maintain accurate time pause
 * @param event      event
 * @param sch        scheduler info
 * @param time       time of pause */
void mgs_schedule(struct schedule *sch, struct timeval *tv);

/** Version of mgs_schedule() accepting microseconds */
void mgs_uschedule(struct schedule *sch, uint32_t time_us);

/** Schedule traffic line to run after given time
 * @param tv         time of pause, if 0 take line->tv
 * @note uses mgs_schedule() */
void mgs_sleep(struct line *line, struct timeval *tv);

/** Version of mgs_sleep() accepting microseconds */
void mgs_usleep(struct line *line, uint32_t time_us);

/** Setup a struct schedule
 * @param cb         libevent handler
 * @param arg        argument to callback (passed as 3rd arg) */
void mgs_setup(struct schedule *sch, struct mg *mg, void (*cb)(int, short, void *arg), void *arg);

#endif
