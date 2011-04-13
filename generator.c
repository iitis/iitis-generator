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

void packet(struct sniff_pkt *pkt)
{
	if (pkt->interface->num != 0)
		return;

	printf("%llu %d: line_num=%u line_ctr=%u\n",
		pkt->radio.tsft, pkt->radio.rssi,
		pkt->mg_hdr.line_num, pkt->mg_hdr.line_ctr);
}

/** Parse traffic file
 * @retval 0 success
 */
int parse_traffic(struct mg *mg)
{
	FILE *fp;
	char buf[BUFSIZ];
	uint32_t line_num = 0;
	int i, j, token;
	struct line *line;

	fp = fopen(mg->options.traf_file, "r");
	if (!fp)
		return 1;

	while (fgets(buf, sizeof buf, fp)) {
		line_num++;
		if (line_num >= N(mg->lines))
			die("Too many lines in the traffic file");

		if (buf[0] == '#' || buf[0] == '\r' || buf[0] == '\n')
			continue;

		line = mmatic_zalloc(sizeof *line, mg->mm);
		line->line_num = line_num;
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
						line->time_s = atoi(buf+i);

						for (; i < j; i++) {
							if (buf[i] == '.') {
								line->time_us = atoi(buf + i + 1);
								break;
							}
						}
						break;
					default:
						dbg(0, "token %d: %s\n", token, buf+i);
						break;
				}

				i = j;
			} else {
				j++;
			}
		} while(buf[j]);

		/* TODO: if line is OK, make and schedule event, passing *line to callback each time */
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
	if (mgi_init(mg) <= 0) {
		dbg(0, "no available interfaces found");
		return 2;
	}

	/* TODO: packet handler */
	mgi_set_callback(mg, packet);

	/* parse traffic file */
	if (parse_traffic(mg))
		return 3;

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
