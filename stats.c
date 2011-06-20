/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include <time.h>
#include <stdarg.h>

#include "generator.h"
#include "stats.h"
#include "schedule.h"

static void _stats_files_free(void *arg)
{
	fclose((FILE *) arg);
}

static void _stats_writer_free(void *arg)
{
	struct stats_writer *sa = arg;

	tlist_free(sa->columns);
	mmatic_freeptr(sa->filename);
	mmatic_freeptr(sa->dirname);
	mmatic_freeptr(sa);
}

void _stats_write(struct mg *mg, struct stats_writer *sa, ut *stats)
{
	const char *key;
	FILE *fh;
	struct timeval now;
	char buf[256], fhkey[256];
	ut *val;

	/* fetch from cache */
	snprintf(fhkey, sizeof fhkey, "./%s/%s", sa->dirname, sa->filename);
	fh = thash_get(mg->stats_files, fhkey);

	/* if no entry, create */
	if (!fh) {
		/* create dir */
		snprintf(buf, sizeof buf, "%s/%s", mg->stats_dir, sa->dirname);
		if (pjf_mkdir(buf) != 0) die_errno(buf);

		/* create file */
		snprintf(buf, sizeof buf, "%s/%s/%s", mg->stats_dir, sa->dirname, sa->filename);
		fh = fopen(buf, "w");
		if (!fh) die_errno(buf);

		/* write column names in first line */
		fputs("#time", fh);
		tlist_iter_loop(sa->columns, key) {
			fputc(' ', fh);
			fputs(key, fh);
		}
		fputs("\n", fh);

		/* store file handle for further writes */
		thash_set(mg->stats_files, fhkey, fh);
	}

	/* 1. put time column */
	gettimeofday(&now, NULL);
	snprintf(buf, sizeof buf, "%u", (unsigned int) now.tv_sec);
	fputs(buf, fh);

	/* 2+ put requested columns */
	tlist_iter_loop(sa->columns, key) {
		val = uth_get(stats, key);
		if (!val) {
			dbg(10, "no such stats: %s\n", key);
			snprintf(buf, sizeof buf, " 0");
		} else {
			/* NB: dont use ut_char() as it allocates new memory */
			switch (ut_type(val)) {
				case T_UINT:
					snprintf(buf, sizeof buf, " %u", ut_uint(val));
					break;
				case T_DOUBLE:
					snprintf(buf, sizeof buf, " %g", ut_double(val));
					break;
				default:
					dbg(1, "unknown type of stat: %s\n", key);
					break;
			}
		}

		fputs(buf, fh);
	}

	fputs("\n", fh);
	fflush(fh);
}

/** Iterate through mg->stats_writers calling handlers and writing results to files */
static void _stats_handler(int fd, short evtype, void *mgarg)
{
	struct mg *mg = mgarg;
	mmatic *mmtmp;
	ut *ut;
	struct stats_writer *sa;
	struct timeval tv = {0, 0};

	/* reschedule us */
	tv.tv_sec = mg->options.stats;
	evtimer_add(&mg->statsev, &tv);

	/* aggregate and write */
	mmtmp = mmatic_create();
	tlist_iter_loop(mg->stats_writers, sa) {
		ut = ut_new_utthash(NULL, mmtmp);

		if (sa->handler(mg, ut, sa->arg))
			_stats_write(mg, sa, ut);
	}
	mmatic_free(mmtmp);
}

/*****/

void mgstats_init(struct mg *mg)
{
	/* writers */
	mg->stats_writers = tlist_create(_stats_writer_free, mg->mm);

	/* target files */
	mg->stats_files = thash_create_strkey(_stats_files_free, mg->mm);
}

void mgstats_start(struct mg *mg)
{
	const char *name;
	struct tm tm;
	char buf[128];
	struct timeval tv = {0, 0};

	/* schedule first stats write */
	evtimer_set(&mg->statsev, _stats_handler, mg);
	tv.tv_sec = mg->options.stats;
	evtimer_add(&mg->statsev, &tv);

	/* main stats dir */
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
}

void mgstats_writer_add(struct mg *mg,
	stats_writer_handler_t handler, void *arg,
	const char *dir, const char *file, ...)
{
	va_list va;
	const char *key;
	struct stats_writer *sa;

	sa = mmatic_zalloc(mg->mm, sizeof *sa);
	sa->handler = handler;
	sa->arg = arg;
	sa->dirname = mmatic_strdup(mg->mm, dir ? dir : "");
	sa->filename = mmatic_strdup(mg->mm, file);

	sa->columns = tlist_create(mmatic_freeptrs, mg->mm);
	va_start(va, file);
	while ((key = va_arg(va, const char *)))
		tlist_push(sa->columns, mmatic_strdup(mg->mm, key));
	va_end(va);

	tlist_push(mg->stats_writers, sa);
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

void mgstats_db_ewma(ut *ut, const char *name, uint32_t n, double val)
{
	double ewma;

	if (!ut)
		die("ut == NULL");

	ewma = uth_double(ut, name);
	ewma = EWMA(ewma, val, n);
	uth_set_double(ut, name, ewma);
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
				uth_set_uint(dst, key, ut_uint(val));
				uth_set_uint(src, key, 0);
				break;
			case T_DOUBLE:
				uth_set_double(dst, key, ut_double(val));
				break;
			default:
				dbg(1, "unknown type for stat '%s'\n", key);
				break;
		}
	}
}
