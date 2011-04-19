#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include "generator.h"

/** Schedule all our lines (used once on startup) */
void mgs_all(struct mg *mg);

/** Accurately schedule event to run again
 * Uses absolute time to maintain accurate time pause
 * @param event      event
 * @param sch        scheduler info
 * @param time_us    time of pause [us] */
void mgs_reschedule(struct event *ev, struct schedule *sch, uint32_t time_us);

/** Reschedule traffic line
 * @note uses mgs_reschedule() */
void mgs_usleep(struct line *line, uint32_t time_us);

#endif
