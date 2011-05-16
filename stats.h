/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#ifndef _STAT_H_
#define _STAT_H_

#include "generator.h"

/** Inititalize statistics system
 * Starts periodic aggregator
 */
void mgstats_init(struct mg *mg);

/** Initialize a stats database */
ut *mgstats_db_init(struct mg *mg);

/** Increase counter by 1 */
void mgstats_db_count_num(ut *ut, const char *name, uint32_t num);

/** Increase counter by 1 */
#define mgstats_db_count(ut, name) mgstats_db_count_num(ut, name, 1)

/** Aggregate statistics */
void mgstats_db_aggregate(ut *dst, ut *src);

#endif
