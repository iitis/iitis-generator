/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#ifndef _STAT_H_
#define _STAT_H_

#include "generator.h"

/** Inititalize mgstats data structures, etc. */
void mgstats_init(struct mg *mg);

/** Starts statistics system, periodic writes, etc.
 * @note requires mg->origin */
void mgstats_start(struct mg *mg);

/** Register statistics aggregation and writing process
 * @param handler   callback function which does aggregation
 * @param arg       handler argument
 * @param dir       optional directory under main stats dir
 * @param file      file name for statistics (eg. stats.txt)
 * @param ...       names and order of columns (ie. keys in stats database to export),
 *                  last name must be NULL
 */
void mgstats_writer_add(struct mg *mg,
	stats_writer_handler_t handler, void *arg,
	const char *dir, const char *file, ...);

/*****/

/** Initialize a stats database
 * @param mm     target memory
 */
stats *stats_create(mmatic *mm);

/** Increase counter
 * @param name   stat name
 * @param num    increase amount
 */
void stats_countN(stats *stats, const char *name, uint32_t num);

/** Increase counter by 1 */
#define stats_count(ut, name) stats_countN(ut, name, 1)

/** Set gauge level
 * @param name   stat name
 * @param val    new value
 */
void stats_set(stats *stats, const char *name, int val);

/** Aggregate statistics
 * @param src    source stats db, after call counters will be zeroed
 * @param dst    already existing, destination stats db
 */
void stats_aggregate(stats *dst, stats *src);

#endif
