/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "generator.h"
#include "stats.h"
#include "schedule.h"

static void _stats_writer_free(void *arg)
{
	struct stats_writer *sa = arg;

	tlist_free(sa->columns);
	mmatic_freeptr(sa->filename);
	mmatic_freeptr(sa->dirname);
	mmatic_freeptr(sa);
}

/** Append statistics line to file */
static void _stats_write(struct mg *mg, struct stats_writer *sa, stats *stats)
{
	const char *key;
	struct timeval now;
	char buf[256];
	struct stats_node *n;

	gettimeofday(&now, NULL);

	/* create file if needed */
	if (!sa->fh) {
		/* create dir */
		snprintf(buf, sizeof buf, "%s/%s", mg->stats_dir, sa->dirname);
		if (pjf_mkdir_mode(buf, mg->options.world ? 0777 : 0755) != 0)
			die("pjf_mkdir(%s) failed\n", buf);

		/* create file */
		snprintf(buf, sizeof buf, "%s/%s/%s", mg->stats_dir, sa->dirname, sa->filename);
		sa->fh = fopen(buf, "w");
		if (!sa->fh)
			die("fopen(%s) failed\n", buf);

		if (mg->options.world)
			chmod(buf, 0666);

		/* write column names in first line */
		fputs("#time", sa->fh);
		tlist_iter_loop(sa->columns, key) {
			fputc(' ', sa->fh);
			fputs(key, sa->fh);
		}
		fputs("\n", sa->fh);
	}

	/* 1. put time column */
	snprintf(buf, sizeof buf, "%lu", (unsigned int) now.tv_sec - mg->origin.tv_sec);
	fputs(buf, sa->fh);

	/* 2+ put requested columns */
	tlist_iter_loop(sa->columns, key) {
		n = thash_get(stats->db, key);
		if (!n) {
			dbg(10, "no such stats: %s\n", key);
			snprintf(buf, sizeof buf, " 0");
		} else {
			switch (n->type) {
				case STATS_COUNTER:
					snprintf(buf, sizeof buf, " %u", n->as.counter);
					break;
				case STATS_GAUGE:
					snprintf(buf, sizeof buf, " %d", n->as.gauge);
					break;
				default:
					dbg(1, "unknown type of stat: %s\n", key);
					break;
			}
		}

		fputs(buf, sa->fh);
	}

	fputs("\n", sa->fh);
	fflush(sa->fh);
}

/** Iterate through mg->stats_writers calling handlers and writing results to files */
static void _stats_handler(int fd, short evtype, void *mgarg)
{
	struct mg *mg = mgarg;
	mmatic *mmtmp;
	stats *stats;
	struct stats_writer *sa;
	struct timeval tv = {0, 0};

	/* reschedule us */
	tv.tv_sec = mg->options.stats;
	evtimer_add(&mg->statsev, &tv);

	/* aggregate and write */
	mmtmp = mmatic_create();
	tlist_iter_loop(mg->stats_writers, sa) {
		stats = stats_create(mmtmp);

		if (sa->handler(mg, stats, sa->arg))
			_stats_write(mg, sa, stats);
	}
	mmatic_free(mmtmp);
}

/*****/

void mgstats_init(struct mg *mg)
{
	/* writers */
	mg->stats_writers = tlist_create(_stats_writer_free, mg->mm);
}

void mgstats_start(struct mg *mg)
{
	struct tm tm;
	char buf1[128], buf2[128];
	struct timeval now, tv;
	char *stats_session_root, *dest;

	if (mg->options.stats == 0) /* stats disabled */
		return;

	if (!mg->synced) {
		dbg(1, "not synced - disabling stats\n");
		return;
	}

	/* schedule first stats write on origin + stats */
	gettimeofday(&now, NULL);
	timersub(&mg->origin, &now, &tv);
	tv.tv_sec += mg->options.stats;

	evtimer_set(&mg->statsev, _stats_handler, mg);
	evtimer_add(&mg->statsev, &tv);

	/* make a tree-like directory structure */
	localtime_r(&mg->origin.tv_sec, &tm);
	strftime(buf1, sizeof buf1, "%Y.%m", &tm);
	strftime(buf2, sizeof buf2, "%Y.%m.%d-%H:%M:%S", &tm);

	if (mg->options.stats_sess)
		stats_session_root = mmatic_printf(mg->mmtmp, "%s/%s/%s/%s",
			mg->options.stats_root, buf1, mg->options.stats_sess, buf2);
	else
		stats_session_root = mmatic_printf(mg->mmtmp, "%s/%s/%s",
			mg->options.stats_root, buf1, buf2);

	/* this node's stats dir */
	mg->stats_dir = mmatic_printf(mg->mm, "%s/%u", stats_session_root, mg->options.myid);

	/* create it or die */
	if (pjf_mkdir_mode(mg->stats_dir, mg->options.world ? 0777 : 0755) == 0)
		dbg(1, "storing statistics in %s\n", mg->stats_dir);
	else
		die("pjf_mkdir(%s) failed\n", mg->stats_dir);

	/* master does special work */
	if (mg->master) {
		/* copy traffic file */
		dest = mmatic_printf(mg->mmtmp, "%s/traffic.txt", stats_session_root);
		if (pjf_copyfile(mg->options.traf_file, dest))
			die("pjf_copyfile() of traffic file failed\n");
		if (mg->options.world)
			chmod(dest, 0666);

		/* copy config file if exists */
		if (mg->options.conf_file) {
			dest = mmatic_printf(mg->mmtmp, "%s/generator.conf", stats_session_root);
			if (pjf_copyfile(mg->options.conf_file, dest))
				die("pjf_copyfile() of config file failed\n");
			if (mg->options.world)
				chmod(dest, 0666);
		}
	}
}

void mgstats_writer_add(struct mg *mg, stats_writer_handler_t handler, void *arg,
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

	sa->columns = tlist_create(mmatic_freeptr, mg->mm);
	va_start(va, file);
	while ((key = va_arg(va, const char *)))
		tlist_push(sa->columns, mmatic_strdup(mg->mm, key));
	va_end(va);

	tlist_push(mg->stats_writers, sa);
}

/*****/

stats *stats_create(mmatic *mm)
{
	stats *stats;

	stats = mmatic_zalloc(mm, sizeof *stats);
	stats->mm = mm;
	stats->db = thash_create_strkey(mmatic_freeptr, stats->mm);

	return stats;
}

void stats_countN(stats *stats, const char *name, uint32_t num)
{
	struct stats_node *n;

	pjf_assert(stats);

	n = thash_get(stats->db, name);
	if (n) {
		n->as.counter += num;
	} else {
		n = mmatic_zalloc(stats->mm, sizeof *n);
		n->type = STATS_COUNTER;
		thash_set(stats->db, name, n);

		n->as.counter = num;
	}
}

void stats_set(stats *stats, const char *name, int val)
{
	struct stats_node *n;

	pjf_assert(stats);

	n = thash_get(stats->db, name);
	if (!n) {
		n = mmatic_zalloc(stats->mm, sizeof *n);
		n->type = STATS_GAUGE;
		thash_set(stats->db, name, n);
	}

	n->as.gauge = val;
}

void stats_aggregate(stats *dst_stats, stats *src_stats)
{
	char *key;
	struct stats_node *src, *dst;

	thash_iter_loop(src_stats->db, key, src) {
		dst = thash_get(dst_stats->db, key);
		if (!dst) {
			dst = mmatic_zalloc(dst_stats->mm, sizeof *dst);
			thash_set(dst_stats->db, key, dst);
		}

		switch (src->type) {
			case STATS_COUNTER:
				/* sum */
				dst->as.counter += src->as.counter;
				src->as.counter = 0;
				break;
			case STATS_GAUGE:
				/* last */
				dst->as.gauge = src->as.gauge;
				break;
			default:
				die("unknown type for stat '%s'\n", key);
				break;
		}
	}
}
