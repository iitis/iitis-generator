/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include <getopt.h>
#include <event.h>
#include <unistd.h>
#include <dlfcn.h>
#include <libpjf/main.h>

#include "generator.h"
#include "interface.h"
#include "cmd-packet.h"
#include "schedule.h"
#include "sync.h"
#include "stats.h"
#include "parser.h"

/** Reverse bits (http://graphics.stanford.edu/~seander/bithacks.html#BitReverseTable) */
const uint8_t REVERSE[256] =
{
#   define R2(n)    n,     n + 2*64,     n + 1*64,     n + 3*64
#   define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#   define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )
	R6(0), R6(2), R6(1), R6(3)
};

static void libevent_log(int severity, const char *msg)
{
	dbg(MAX(0, _EVENT_LOG_ERR - severity), "%s\n", msg);
}

/** Prints usage help screen */
static void help(void)
{
	printf("Usage: generator [OPTIONS] <TRAFFIC FILE> [<CONFIG FILE>]\n");
	printf("\n");
	printf("  A mac80211 packet generator.\n");
	printf("\n");
	printf("Options:\n");
	printf("  --id=<num>             my ID number [extract from hostname]\n");
	printf("  --root=<dir>           stats output dir root [%s]\n", DEFAULT_STATS_ROOT);
	printf("  --sess=<name>          stats session name - prefix of stats dir name\n");
	printf("  --world                make stats accessible and writable to world\n");
	printf("  --verbose,-V           be verbose (alias for --debug=5)\n");
	printf("  --debug=<num>          set debugging level\n");
	printf("  --help,-h              show this usage help screen\n");
	printf("  --version,-v           show version and copying information\n");
	printf("\n");
	printf("  Configuration file, if supplied, is applied after command line options.\n");
}

/** Prints version and copying information. */
static void version(void)
{
	printf("generator %s\n", GENERATOR_VER);
	printf("Copyright (C) 2011 IITiS PAN Gliwice www.iitis.pl\n");
	printf("Author: Paweł Foremski <pjf@iitis.pl>\n");
	printf("Licensed under GNU GPL v3.\n");
}

/** Apply default options */
static void apply_defaults(struct mg *mg)
{
	mg->options.stats      = DEFAULT_STATS_PERIOD;
	mg->options.stats_root = DEFAULT_STATS_ROOT;
	mg->options.sync       = DEFAULT_SYNC_PERIOD;
	mg->options.svc_ifname = DEFAULT_SVC_IFNAME;
}

/** Parses arguments and loads modules
 * @retval 0     ok
 * @retval 1     error, main() should exit (eg. wrong arg. given)
 * @retval 2     ok, but main() should exit (eg. on --version or --help) */
static int parse_argv(struct mg *mg, int argc, char *argv[])
{
	int i, c;

	static char *short_opts = "hvdV";
	static struct option long_opts[] = {
		/* name, has_arg, NULL, short_ch */
		{ "verbose",    0, NULL,  1  },
		{ "debug",      1, NULL,  2  },
		{ "help",       0, NULL,  3  },
		{ "version",    0, NULL,  4  },
		{ "id",         1, NULL,  5  },
		{ "root",       1, NULL,  6  },
		{ "sess",       1, NULL,  7  },
		{ "world",      0, NULL,  8  },
		{ 0, 0, 0, 0 }
	};

	for (;;) {
		c = getopt_long(argc, argv, short_opts, long_opts, &i);
		if (c == -1) break; /* end of options */

		switch (c) {
			case 'V':
			case  1 : debug = 5; break;
			case  2 : debug = atoi(optarg); break;
			case 'h':
			case  3 : help(); return 2;
			case 'v':
			case  4 : version(); return 2;
			case  5 : mg->options.myid = atoi(optarg); break;
			case  6 : mg->options.stats_root = mmatic_strdup(mg->mm, optarg); break;
			case  7 : mg->options.stats_sess = mmatic_strdup(mg->mm, optarg); break;
			case  8 : mg->options.world = true; break;
			default: help(); return 1;
		}
	}

	/* get traffic file path */
	if (argc - optind > 0) {
		mg->options.traf_file = argv[optind++];
	} else {
		help();
		return 1;
	}

	/* get configuration file path */
	if (argc - optind > 0)
		mg->options.conf_file = argv[optind++];

	return 0;
}

/** Setup options.myid basing on this host name */
static void fetch_myid(struct mg *mg)
{
	int i;
	char hostname[128];

	gethostname(hostname, sizeof hostname);
	for (i = 0; hostname[i]; i++) {
		if (isdigit(hostname[i]))
			break;
	}

	mg->options.myid = atoi(hostname + i);
	dbg(1, "my id: %d\n", mg->options.myid);
}

/** Parse config file part passed as unitype object
 * @retval 0 success
 * @retval 1 error
 */
static int parse_config_ut(struct mg *mg, ut *cfg)
{
	thash *t;
	char *key, *key2;
	ut *subcfg;
	int num1, num2;

	t = ut_thash(cfg);

	/* parse global config */
	thash_iter_loop(t, key, subcfg) {
		if (isdigit(key[0]))
			continue;

		if (streq(key, "id")) {
			mg->options.myid = ut_int(subcfg);
		} else if (streq(key, "stats")) {
			mg->options.stats = ut_int(subcfg);
		} else if (streq(key, "sync")) {
			mg->options.sync = ut_int(subcfg);
		} else if (streq(key, "root")) {
			mg->options.stats_root = ut_char(subcfg);
		} else if (streq(key, "session")) {
			mg->options.stats_sess = ut_char(subcfg);
		} else if (streq(key, "dump")) {
			mg->options.dump = ut_bool(subcfg);
		} else if (streq(key, "dump-size")) {
			mg->options.dumpsize = ut_int(subcfg);
		} else if (streq(key, "dump-beacons")) {
			mg->options.dumpb = ut_bool(subcfg);
		} else if (streq(key, "svc-ifname")) {
			mg->options.svc_ifname = ut_char(subcfg);
		} else {
			dbg(0, "unrecognized configuration file option: %s\n", key);
			return 1;
		}
	}

	/* parse per-node config */
	thash_iter_loop(t, key, subcfg) {
		if (!(isdigit(key[0]) && ut_type(subcfg) == T_HASH))
			continue;

		/* parse list consisting of ranges and lists of ids */
		while (isdigit(key[0])) {
			num1 = strtol(key, &key2, 10);

			if (key2[0] == '-') {
				/* range */
				num2 = strtol(key2+1, &key, 10);
				key++;
			} else if (key2[0] == ',') {
				/* list */
				num2 = 0;
				key = key2 + 1;
			} else {
				/* single */
				num2 = 0;
				key = key2;
			}

			/* if its for me... */
			if (mg->options.myid == num1 ||
			    (num2 > 0 && (mg->options.myid >= num1 && mg->options.myid <= num2))) {
				parse_config_ut(mg, subcfg);
			}
		}
	}

	return 0;
}

/** Parse config file
 * @retval 0 success
 * @retval 1 syntax error
 * @retval 2 logic error
 * @retval 3 other error
 */
static int parse_config(struct mg *mg)
{
	FILE *fp;
	xstr *xs;
	char buf[4096], *str;
	json *js;
	ut *cfg;

	/* read file contents, ignoring empty lines and comments */
	fp = fopen(mg->options.conf_file, "r");
	if (!fp) {
		dbg(0, "%s: fopen() failed: %s\n", mg->options.conf_file, strerror(errno));
		return 3;
	}

	xs = xstr_create("{", mg->mmtmp);
	while (fgets(buf, sizeof buf, fp)) {
		str = pjf_trim(buf);
		if (!str || !str[0] || str[0] == '#')
			continue;

		xstr_append(xs, str);
	}
	xstr_append_char(xs, '}');
	fclose(fp);

	/* parse config file as loose JSON */
	js = json_create(mg->mmtmp);
	json_setopt(js, JSON_LOOSE, 1);

	cfg = json_parse(js, xstr_string(xs));
	if (!ut_ok(cfg)) {
		dbg(0, "parsing config file failed: %s\n", ut_err(cfg));
		return 1;
	}

	/* parse config */
	return (parse_config_ut(mg, cfg) ? 2 : 0);
}

/** Update given struct line with callback function addresses
 * @retval true success */
static bool find_line_cmd(struct line *line)
{
	static void *me = NULL;
	static char buf[128];

	/* a trick for conversion void* -> function address */
	static union {
		void *ptr;
		int (*fun_init)(struct line *, const char *);
		void (*fun_timeout)(int, short, void *);
		void (*fun_packet)(struct sniff_pkt *);
	} ptr2func;

	if (!me)
		me = dlopen(NULL, RTLD_NOW | RTLD_GLOBAL);

	/* initializer */
	snprintf(buf, sizeof buf, "cmd_%s_init", line->cmd);
	ptr2func.ptr = dlsym(me, buf);

	if (!ptr2func.ptr)
		return false;
	else
		line->cmd_init = ptr2func.fun_init;

	/* outgoing frames */
	snprintf(buf, sizeof buf, "cmd_%s_timeout", line->cmd);
	ptr2func.ptr = dlsym(me, buf);

	if (!ptr2func.ptr)
		return false;
	else
		line->cmd_timeout = ptr2func.fun_timeout;

	/* incoming frames */
	snprintf(buf, sizeof buf, "cmd_%s_packet", line->cmd);
	ptr2func.ptr = dlsym(me, buf);

	if (!ptr2func.ptr)
		return false;
	else
		line->cmd_packet = ptr2func.fun_packet;

	return true;
}

/** Parse traffic file
 * @retval 0 success
 * @retval 1 syntax error
 * @retval 2 logic error
 */
static int parse_traffic(struct mg *mg)
{
	FILE *fp;
	const char *file;
	char buf[BUFSIZ];
	uint32_t line_num = 0;
	struct line *line;
	int i, rc;
	char *rest, *errmsg;
	struct mgp_line *pl;

	file = mg->options.traf_file;
	fp = fopen(file, "r");
	if (!fp)
		reterrno(1, 0, "could not open traffic file");

	while (fgets(buf, sizeof buf, fp)) {
		line_num++;
		if (line_num >= N(mg->lines))
			die("Too many lines in the traffic file");

		/* skip comments */
		if (buf[0] == '#' || buf[0] == '\r' || buf[0] == '\n')
			continue;

		/* parse line */
		pl = mgp_parse_line(mg->mm, buf, 8, &rest, &errmsg,
			"s", "ms", "iface", "src", "dst", "rate", "noack", "cmd", NULL);
		if (!pl) {
			dbg(0, "%s: line %d: parse error: %s\n", file, line_num, errmsg);
			return 1;
		}

		/* ...and rewrite into struct line */
		line = mmatic_zalloc(mg->mm, sizeof *line);
		mg->lines[line_num] = line;

		line->mg = mg;
		line->line_num = line_num;
		line->contents = mmatic_strdup(mg->mm, buf);
		line->stats = stats_create(mg->mm);

		/* time */
		line->tv.tv_sec = mgp_get_int(pl, "s", 0);
		line->tv.tv_usec = mgp_get_int(pl, "ms", 0) * 1000;

		/* interface */
		i = mgp_get_int(pl, "iface", 0);
		if (i >= IFINDEX_MAX) {
			dbg(0, "%s: line %d: too big interface number: %d\n", file, line_num, i);
			return 1;
		}
		line->interface = &mg->interface[i];
		if (line->interface->fd <= 0) {
			dbg(0, "%s: line %d: interface not opened: %d\n", file, line_num, i);
			return 2;
		}

		/* src/dst */
		line->srcid = mgp_get_int(pl, "src", 1);
		line->dstid = mgp_get_int(pl, "dst", 1);
		line->my    = (line->srcid == mg->options.myid);

		/* rate/noack */
		line->rate = mgp_get_float(pl, "rate", 0) * 2.0;  /* driver uses "half-rates"; NB: "auto" => 0 */
		line->noack = mgp_get_int(pl, "noack", 0);

		/*
		 * command
		 */
		line->cmd = mgp_get_string(pl, "cmd", "");
		if (!line->cmd) {
			dbg(0, "%s: line %d: no line command\n", file, line_num);
			return 2;
		}

		/* find command handlers */
		if (!find_line_cmd(line)) {
			dbg(0, "%s: line %d: invalid command: %s\n", file, line_num, line->cmd);
			return 2;
		}

		/* initialize scheduler of outgoing frames */
		mgs_setup(&line->schedule, mg, line->cmd_timeout, line);

		/* call command initializer */
		rc = line->cmd_init(line, rest);
		if (rc != 0)
			return rc;
	}

	fclose(fp);
	return 0;
}

static void heartbeat(int fd, short evtype, void *arg)
{
	static struct timeval now, diff, tv = {0, 0};
	struct mg *mg = arg;

	/* garbage collector - free whenever garbage > 128KB */
	if (mmatic_size(mg->mmtmp) > 1024 * 128) {
		mmatic_free(mg->mmtmp);
		mg->mmtmp = mmatic_create();
	}

	/* if no line generator is running and there was no packet to us in last 60 seconds - exit */
	if (mg->running == 0) {
		gettimeofday(&now, NULL);
		timersub(&now, &mg->last, &diff);

		if (diff.tv_sec >= 60) {
			dbg(0, "Finished, exiting...\n");
			mg->running--;
			event_base_loopexit(mg->evb, &tv);
			return;
		}
	}

	mgs_uschedule(&mg->hbs, HEARTBEAT_PERIOD);
}

static void heartbeat_init(struct mg *mg)
{
	mgs_setup(&mg->hbs, mg, heartbeat, mg);
	mgs_uschedule(&mg->hbs, HEARTBEAT_PERIOD);
}

static void sync_run(int fd, short evtype, void *arg)
{
	struct mg *mg = arg;

	sync();
	mgs_uschedule(&mg->syncs, mg->options.sync * 1000000);
}

static void sync_init(struct mg *mg)
{
	if (mg->options.sync > 0) {
		mgs_setup(&mg->syncs, mg, sync_run, mg);
		mgs_uschedule(&mg->syncs, mg->options.sync * 1000000);
	}
}

/** Pass incoming packet to proper handler */
static void handle_packet(struct sniff_pkt *pkt)
{
	/* TODO: stats? (remember to drop duplicates) */

	pkt->line->cmd_packet(pkt);

	return;
}

/** Aggregate stats from all lines */
static bool _stats_aggregate_lines(struct mg *mg, stats *stats, void *arg)
{
	int i;

	for (i = 1; i < TRAFFIC_LINE_MAX; i++) {
		if (!mg->lines[i])
			continue;

		stats_aggregate(stats, mg->lines[i]->stats);
	}

	return true;
}

/** Global stats */
static bool _stats_global(struct mg *mg, stats *stats, void *arg)
{
	stats_aggregate(stats, mg->stats);
	return true;
}

/** Initialize global stats stuff */
static void _stats_init(struct mg *mg)
{
	mg->stats = stats_create(mg->mm);

	/* iitis-generator internal stats */
	mgstats_writer_add(mg,
		_stats_global, NULL,
		NULL, "internal-stats.txt",
		"scheduler_evt",
		"scheduler_lag",
		NULL);

	/* global stats of line generators */
	mgstats_writer_add(mg,
		_stats_aggregate_lines, NULL,
		NULL, "linestats.txt",
		"snt_ok",
		"snt_time",
		"snt_err",
		"rcv_ok",
		"rcv_ok_bytes",
		"rcv_dup",
		"rcv_dup_bytes",
		"rcv_lost",
		NULL);
}

int main(int argc, char *argv[])
{
	mmatic *mm = mmatic_create();
	mmatic *mmtmp = mmatic_create();
	struct mg *mg;
	int i;

	/*
	 * initialize and parse config
	 */

	mg = mmzalloc(sizeof(struct mg));
	mg->mm = mm;
	mg->mmtmp = mmtmp;
	apply_defaults(mg);

	/* get my id number from hostname */
	fetch_myid(mg);

	/* parse command line options */
	if (parse_argv(mg, argc, argv))
		return 1;

	/* parse configuration file options */
	if (mg->options.conf_file) {
		if (parse_config(mg))
			return 4;
	}

	/*
	 * config syntax looks OK, see if it is feasible
	 */

	/* init libevent */
	mg->evb = event_init();
	event_set_log_callback(libevent_log);

	/* init stats structures so mgstats_aggregator_add() used somewhere below works */
	mgstats_init(mg);

	/* attach to raw interfaces */
	if (mgi_init(mg, handle_packet) <= 0) {
		dbg(0, "no available interfaces found\n");
		return 2;
	}

	/* parse traffic file */
	if (parse_traffic(mg))
		return 3;

	/*
	 * all OK, prepare to start
	 */

	/* synchronize time reference point on all nodes */
	mgc_sync(mg);

	/* schedule stats writing */
	mgstats_start(mg);

	/* attach global stats */
	_stats_init(mg);

	/* schedule heartbeat and disk sync signals */
	heartbeat_init(mg);
	sync_init(mg);

	/* schedule the real work of this node: line generators */
	for (i = 1; i < TRAFFIC_LINE_MAX; i++) {
		if (!(mg->lines[i] && mg->lines[i]->my))
			continue;

		/* this will schedule first execution */
		mgs_sleep(mg->lines[i], NULL);
		mg->running++;
	}

	/* suppose last frame was received now */
	gettimeofday(&mg->last, NULL);

	/*
	 * start!
	 */

	dbg(0, "Starting\n");
	event_base_dispatch(mg->evb);

	/*******************************/

	/*
	 * cleanup after end of libevent loop
	 */

	event_base_free(mg->evb);
	mmatic_free(mg->mm);
	mmatic_free(mg->mmtmp);

	return 0;
}
