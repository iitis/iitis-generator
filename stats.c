/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include <time.h>
#include <stdarg.h>

#include "generator.h"
#include "stats.h"
#include "schedule.h"

static void _stats_files_close(void *arg)
{
	fclose((FILE *) arg);
}

#define _stats_write(mg, file, ut, ...) _stats_write_(mg, file, ut, __VA_ARGS__, NULL)
void _stats_write_(struct mg *mg, const char *file, ut *stats, ...)
{
	va_list va;
	const char *key;
	const char *path;
	FILE *fh;
	struct timeval now;
	char buf[128];
	ut *val;

	gettimeofday(&now, NULL);

	fh = thash_get(mg->stats_files, file);
	if (!fh) {
		path = mmatic_printf(mg->mm, "%s/%s", mg->stats_dir, file);

		fh = fopen(path, "w");
		if (!fh) die_errno(path);

		/* write first line */
		fputs("#time", fh);
		va_start(va, stats);
		while ((key = va_arg(va, const char *))) {
			fputc(' ', fh);
			fputs(key, fh);
		}
		va_end(va);
		fputs("\n", fh);

		/* store for further writes */
		thash_set(mg->stats_files, file, fh);
	}

	/* 1. time column */
	snprintf(buf, sizeof buf, "%u", (unsigned int) now.tv_sec);
	fputs(buf, fh);

	va_start(va, stats);
	while ((key = va_arg(va, const char *))) {
		val = uth_get(stats, key);
		if (!val) {
			dbg(10, "no such stats: %s\n", key);
			snprintf(buf, sizeof buf, " 0");
		} else {
			switch (ut_type(val)) {
				case T_UINT:
					snprintf(buf, sizeof buf, " %u", ut_uint(val));
					break;
				default:
					dbg(1, "unknown type of stat: %s\n", key);
					break;
			}
		}

		fputs(buf, fh);
	}
	va_end(va);

	fputs("\n", fh);
	fflush(fh);
}

static void _stats_handler(int fd, short evtype, void *mgarg)
{
	struct mg *mg = mgarg;
	int i;
	mmatic *mm;
	ut *ut;

	mgs_uschedule(&mg->statss, mg->options.stats * 1000000);

	/* aggregate all lines */
	mm = mmatic_create();
	ut = ut_new_utthash(NULL, mm);

	for (i = 1; i < TRAFFIC_LINE_MAX; i++) {
		if (!mg->lines[i])
			continue;

		mgstats_db_aggregate(ut, mg->lines[i]->stats);
	}

	_stats_write(mg, "stats.txt", ut,
		"received",
		"duplicates",
		"lost");

	mmatic_free(mm);
	/**********************/
}

/*****/

ut *mgstats_db_create(struct mg *mg)
{
	return ut_new_utthash(NULL, mg->mm);
}

void mgstats_db_count_num(ut *ut, const char *name, uint32_t num)
{
	uint32_t count;

	if (!ut)
		die("ut == NULL");

	count = uth_uint(ut, name);
	count += num;
	uth_set_uint(ut, name, count);
}

void mgstats_db_aggregate(ut *dst, ut *src)
{
	thash *srch;
	char *key;
	ut *val;

	srch = ut_thash(src);
	thash_iter_loop(srch, key, val) {
		switch (ut_type(val)) {
			case T_UINT:
				mgstats_db_count_num(dst, key, ut_uint(val));
				uth_set_uint(src, key, 0);
				break;
			default:
				dbg(1, "unknown type for stat '%s'\n", key);
				break;
		}
	}
}

void mgstats_init(struct mg *mg)
{
	const char *name;
	struct tm tm;
	char buf[128];

	/* schedule stats write */
	mgs_setup(&mg->statss, mg, _stats_handler, mg);
	mgs_uschedule(&mg->statss, mg->options.stats * 1000000);

	/* stats dir */
	if (mg->options.stats_name) {
		name = mg->options.stats_name;
	} else {
		localtime_r(&mg->origin.tv_sec, &tm);
		strftime(buf, sizeof buf, "%Y-%m-%d_%H:%M:%S", &tm);

		name = buf;
	}

	mg->stats_dir = mmatic_printf(mg->mm, "%s/%s/%u",
		mg->options.stats_root, name, mg->options.myid);

	if (pjf_mkdir(mg->stats_dir) == 0)
		dbg(1, "storing statistics in %s\n", mg->stats_dir);
	else
		die_errno("pjf_mkdir()");

	/* files */
	mg->stats_files = thash_create_strkey(_stats_files_close, mg->mm);
}
