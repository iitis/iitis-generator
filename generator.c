/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include <getopt.h>
#include <event.h>
#include <unistd.h>
#include <libpjf/main.h>

#include "generator.h"
#include "interface.h"
#include "cmd-packet.h"
#include "schedule.h"
#include "sync.h"
#include "stats.h"

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
	printf("Usage: generator [OPTIONS] <TRAFFIC FILE>\n");
	printf("\n");
	printf("  A mac80211 packet generator.\n");
	printf("\n");
	printf("Options:\n");
	printf("  --id=<num>             my ID number [extract from hostname]\n");
	printf("  --stats=<num>          generate statistics each <num> seconds [%u]\n", DEFAULT_STATS_PERIOD);
	printf("  --sync=<num>           run disk sync() each <num> seconds (0=off) [%u]\n", DEFAULT_SYNC_PERIOD);
	printf("  --root=<dir>           statistics output dir root [%s]\n", DEFAULT_STATS_ROOT);
	printf("  --name=<name>          dir name under --root [date and time]\n");
	printf("  --verbose,-V           be verbose (alias for --debug=5)\n");
	printf("  --debug=<num>          set debugging level\n");
	printf("  --help,-h              show this usage help screen\n");
	printf("  --version,-v           show version and copying information\n");
}

/** Prints version and copying information. */
static void version(void)
{
	printf("generator %s\n", GENERATOR_VER);
	printf("Copyright (C) 2011 IITiS PAN Gliwice www.iitis.pl\n");
	printf("Author: Paweł Foremski <pjf@iitis.pl>\n");
	printf("Licensed under GNU GPL v3.\n");
}

/** Parses arguments and loads modules
 * @retval 0     ok
 * @retval 1     error, main() should exit (eg. wrong arg. given)
 * @retval 2     ok, but main() should exit (eg. on --version or --help) */
static int parse_argv(struct mg *mg, int argc, char *argv[])
{
	int i, c;
	char hostname[128];

	static char *short_opts = "hvdV";
	static struct option long_opts[] = {
		/* name, has_arg, NULL, short_ch */
		{ "verbose",    0, NULL,  1  },
		{ "debug",      1, NULL,  2  },
		{ "help",       0, NULL,  3  },
		{ "version",    0, NULL,  4  },
		{ "id",         1, NULL,  5  },
		{ "root",       1, NULL,  6  },
		{ "name",       1, NULL,  7  },
		{ "stats",      1, NULL,  8  },
		{ "sync",       1, NULL,  9  },
		{ 0, 0, 0, 0 }
	};

	mg->options.stats      = DEFAULT_STATS_PERIOD;
	mg->options.stats_root = DEFAULT_STATS_ROOT;
	mg->options.sync       = DEFAULT_SYNC_PERIOD;

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
			case  7 : mg->options.stats_name = mmatic_strdup(mg->mm, optarg); break;
			case  8 : mg->options.stats = atoi(optarg); break;
			case  9 : mg->options.sync = atoi(optarg); break;
			default: help(); return 1;
		}
	}

	if (argc - optind > 0) {
		mg->options.traf_file = argv[optind];
	} else {
		help();
		return 1;
	}

	if (mg->options.myid == 0) {
		gethostname(hostname, sizeof hostname);
		for (i = 0; hostname[i]; i++)
			if (isdigit(hostname[i]))
				break;

		mg->options.myid = atoi(hostname + i);
		dbg(1, "my id: %d\n", mg->options.myid);
	}

	return 0;
}

/** Pass incoming packet to proper handler */
static void handle_packet(struct sniff_pkt *pkt)
{
	/* TODO: stats (remember to drop duplicates) */

	if (streq(pkt->line->cmd, "packet")) {
		cmd_packet_in(pkt);
	}

	return;
}

/** Parse traffic file
 * @retval 0 success
 * @retval 1 syntax error
 * @retval 2 logic error
 */
static int parse_traffic(struct mg *mg)
{
	FILE *fp;
	char buf[BUFSIZ];
	uint32_t line_num = 0;
	int i, j, token, rc;
	struct line *line;
	double halfrate;

	void (*handle)(int, short, void *);
	int (*initialize)(struct line *line);

	fp = fopen(mg->options.traf_file, "r");
	if (!fp) {
		reterrno(1, 0, "could not open traffic file");
	}

	while (fgets(buf, sizeof buf, fp)) {
		line_num++;
		if (line_num >= N(mg->lines))
			die("Too many lines in the traffic file");

		if (buf[0] == '#' || buf[0] == '\r' || buf[0] == '\n')
			continue;

		line = mmatic_zalloc(mg->mm, sizeof *line);
		line->mg = mg;
		line->line_num = line_num;
		line->contents = mmatic_strdup(mg->mm, buf);
		mg->lines[line_num] = line;

		i = j = token = 0;
		do {
			if (buf[j] == '\0' || buf[j] == ' ' || buf[j] == '\r' || buf[j] == '\n') {
				token++;

				if (buf[j])
					buf[j++] = '\0';

				/* FORMAT: time interface_num src dst rate noack? command params... */
				switch (token) {
					case 1:
						line->tv.tv_sec = atoi(buf+i);

						for (; i < j; i++) {
							if (buf[i] == '.') {
								line->tv.tv_usec = atoi(buf + i + 1) * 1000;
								break;
							}
						}
						break;
					case 2:
						rc = atoi(buf+i);
						if (rc >= IFINDEX_MAX) {
							dbg(0, "%s: line %d: too big interface number: %d\n",
								mg->options.traf_file, line->line_num, rc);
							return 1;
						}

						line->interface = &mg->interface[rc];
						if (line->interface->fd <= 0) {
							dbg(0, "%s: line %d: interface not opened: %d\n",
								mg->options.traf_file, line->line_num, rc);
							return 2;
						}
						break;
					case 3:
						line->srcid = atoi(buf+i);
						break;
					case 4:
						line->dstid = atoi(buf+i);
						break;
					case 5:
						halfrate = strtod(buf+i, NULL);
						line->rate = halfrate * 2; /* NB: "auto" => 0 */
						break;
					case 6:
						line->noack = atoi(buf+i);
						break;
					case 7:
						line->cmd = mmatic_strdup(mg->mm, buf+i);
						/* NB: fall-through */
					default:
						if (line->argc == LINE_ARGS_MAX - 1) {
							dbg(0, "%s: line %d: too many line arguments: %d\n",
								mg->options.traf_file, line_num, line->argc);
							return 2;
						}

						line->argv[line->argc++] = mmatic_strdup(mg->mm, buf+i);
						break;
				}

				while (buf[j] == ' ')
					j++;

				i = j;
			} else {
				j++;
			}
		} while(buf[j]);

		line->argv[line->argc] = NULL;

		if (!line->cmd)
			continue;

		/* choose handler for command
		 * in future?: change to dynamic loader and dlsym() */
		if (streq(line->cmd, "packet")) {
			/* TODO assign incoming packet handler too */
			handle = cmd_packet_out;
			initialize = cmd_packet_init;
		} else {
			dbg(0, "%s: line %d: invalid command: %s\n",
				mg->options.traf_file, line->line_num, line->cmd);
			return 2;
		}

		/* stats */
		line->stats = mgstats_db_create(mg);
		line->linkstats = mgi_linkstats_get(line->interface, line->srcid, line->dstid);

		/* prepare (may contain further line parsing) */
		evtimer_set(&line->schedule.ev, handle, line);
		rc = initialize(line);
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

	/* TODO: garbage collector? */

	/* if no line generator is running and there was no packet to us in last 10 seconds - exit */
	if (mg->running == 0) {
		gettimeofday(&now, NULL);
		timersub(&now, &mg->last, &diff);

		if (diff.tv_sec >= 10) {
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

int main(int argc, char *argv[])
{
	mmatic *mm = mmatic_create();
	mmatic *mmtmp = mmatic_create();
	struct mg *mg;

	/*
	 * initialization
	 */
	mg = mmzalloc(sizeof(struct mg));
	mg->mm = mm;
	mg->mmtmp = mmtmp;

	if (parse_argv(mg, argc, argv))
		return 1;

	/* libevent */
	mg->evb = event_init();
	event_set_log_callback(libevent_log);

	/* init stats so mgstats_aggregator_add() works */
	mgstats_init(mg);

	/* raw interfaces */
	if (mgi_init(mg, handle_packet) <= 0) {
		dbg(0, "no available interfaces found");
		return 2;
	}

	/* parse traffic file */
	if (parse_traffic(mg))
		return 3;

	/* synchronize */
	mgc_sync(mg);

	/* heartbeat and disk sync signals */
	heartbeat_init(mg);
	sync_init(mg);

	/* schedule all line generators */
	mgs_schedule_all_lines(mg);

	/* start stats */
	mgstats_start(mg);

	/*
	 * main loop
	 */
	dbg(0, "Starting\n");
	event_base_dispatch(mg->evb);

	/* cleanup */
	event_base_free(mg->evb);
	mmatic_free(mm);

	return 0;
}
