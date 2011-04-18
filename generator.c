/*
 * ath9k monitor mode packet generator
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include <getopt.h>
#include <netinet/ether.h>
#include <libpjf/main.h>
#include <event.h>

#include "generator.h"
#include "interface.h"
#include "cmd-packet.h"

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
	printf("  --verbose              be verbose (alias for --debug=5)\n");
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

	static char *short_opts = "hvd";
	static struct option long_opts[] = {
		/* name, has_arg, NULL, short_ch */
		{ "verbose",    0, NULL,  1  },
		{ "debug",      1, NULL,  2  },
		{ "help",       0, NULL,  3  },
		{ "version",    0, NULL,  4  },
		{ "id",         1, NULL,  5  },
		{ 0, 0, 0, 0 }
	};

	for (;;) {
		c = getopt_long(argc, argv, short_opts, long_opts, &i);
		if (c == -1) break; /* end of options */

		switch (c) {
			case  1 : debug = 5; break;
			case  2 : debug = atoi(optarg); break;
			case 'h':
			case  3 : help(); return 2;
			case 'v':
			case  4 : version(); return 2;
			case  5 : mg->options.myid = atoi(optarg); break;
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
void handle_packet(struct sniff_pkt *pkt)
{
	struct line *line;

	if (pkt->mg_hdr.line_num >= TRAFFIC_LINE_MAX) {
		dbg(1, "received too big line number: %d\n", pkt->mg_hdr.line_num);
		return;
	}

	line = pkt->interface->mg->lines[pkt->mg_hdr.line_num];
	if (!line) {
		dbg(1, "received invalid line number: %d\n", pkt->mg_hdr.line_num);
		return;
	}

	if (streq(line->cmd, "packet")) {
		cmd_packet_in(line, pkt);
	}

	return;
}

/** Parse traffic file
 * @retval 0 success
 * @retval 1 syntax error
 * @retval 2 logic error
 */
int parse_traffic(struct mg *mg)
{
	FILE *fp;
	char buf[BUFSIZ];
	uint32_t line_num = 0;
	int i, j, token, rc;
	struct line *line;

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

		line = mmatic_zalloc(sizeof *line, mg->mm);
		line->mg = mg;
		line->line_num = line_num;
		line->contents = mmatic_strdup(buf, line);
		mg->lines[line_num] = line;

		i = j = token = 0;
		do {
			if (buf[j] == '\0' || buf[j] == ' ' || buf[j] == '\r' || buf[j] == '\n') {
				token++;

				if (buf[j]) {
					buf[j] = '\0';
					j++;
				}

				/* FORMAT: time interface_num src dst rate noack? command params... */
				switch (token) {
					case 1:
						line->tv.tv_sec = atoi(buf+i);

						for (; i < j; i++) {
							if (buf[i] == '.') {
								line->tv.tv_usec = atoi(buf + i + 1);
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
						line->rate = atoi(buf+i); /* NB: "auto" => 0 */
						break;
					case 6:
						line->noack = atoi(buf+i);
						break;
					case 7:
						line->cmd = mmatic_strdup(buf+i, mg->mm);
						/* NB: fall-through */
					default:
						if (line->argc == LINE_ARGS_MAX - 1) {
							dbg(0, "%s: line %d: too many line arguments: %d\n",
								mg->options.traf_file, line_num, line->argc);
							return 2;
						}

						line->argv[line->argc++] = mmatic_strdup(buf+i, mg->mm);
						break;
				}

				i = j;
			} else {
				j++;
			}
		} while(buf[j]);

		line->argv[line->argc] = NULL;

		/* choose handler for command
		 * in future?: change to dynamic loader and dlsym() */
		if (streq(line->cmd, "packet")) {
			handle = cmd_packet_out;
			initialize = cmd_packet_init;
		}

		/* prepare (may contain further line parsing) */
		evtimer_set(&line->ev, handle, line);
		rc = initialize(line);
		if (rc != 0)
			return rc;

		/* schedule */
		if (line->srcid == mg->options.myid)
			evtimer_add(&line->ev, &line->tv);
	}

	fclose(fp);
	return 0;
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

	/* raw interfaces */
	if (mgi_init(mg, handle_packet) <= 0) {
		dbg(0, "no available interfaces found");
		return 2;
	}

	/* parse traffic file */
	if (parse_traffic(mg))
		return 3;

	/* FIXME: naive synchronization of all nodes */
	struct timeval tv;
	while (true) {
		gettimeofday(&tv, NULL);
		if (tv.tv_sec % 10 == 0)
			break;
	}
	dbg(0, "Starting on %d.%d\n", tv.tv_sec, tv.tv_usec);

	/*
	 * main loop
	 */
	/* TODO: make garbage collector */
	event_base_dispatch(mg->evb);

	/* cleanup */
	event_base_free(mg->evb);
	mmatic_free(mm);

	return 0;
}
