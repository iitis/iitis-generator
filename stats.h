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

/** Initialize a stats database */
ut *mgstats_db_create(struct mg *mg);

/** Increase counter
 * @param name   stat name
 * @param num    increase amount
 */
void mgstats_db_count_num(ut *ut, const char *name, uint32_t num);

/** Increase counter by 1 */
#define mgstats_db_count(ut, name) mgstats_db_count_num(ut, name, 1)

/** Update EWMA level
 * @param name   stat name
 * @param n      number of samples (affects EWMA smoothing factor)
 * @param val    new value
 */
void mgstats_db_ewma(ut *ut, const char *name, uint32_t n, double val);

/** Aggregate statistics
 * @param src    source stats db, after call counters will be zeroed
 * @param dst    destination stats db
 */
void mgstats_db_aggregate(ut *dst, ut *src);

#endif
