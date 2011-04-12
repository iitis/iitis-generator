/*
 * ath9k monitor mode packet generator
 * Paweł Foremski <pforemski@gmail.com> 2011
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
	printf("Author: Paweł Foremski <pforemski@gmail.com>\n");
	printf("Licensed under GNU GPL v3.\n");
}

/** Parses arguments and loads modules
 * @retval 0     ok
 * @retval 1     error, main() should exit (eg. wrong arg. given)
 * @retval 2     ok, but main() should exit (eg. on --version or --help) */
static int parse_argv(struct mg *mg, int argc, char *argv[])
{
	int i, c;

	static char *short_opts = "hvd";
	static struct option long_opts[] = {
		/* name, has_arg, NULL, short_ch */
		{ "verbose",    0, NULL,  1  },
		{ "debug",      1, NULL,  2  },
		{ "help",       0, NULL,  3  },
		{ "version",    0, NULL,  4  },
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
			default: help(); return 1;
		}
	}

	if (argc - optind > 0) {
		mg->options.traf_file = argv[optind];
	} else {
		help();
		return 1;
	}

	return 0;
}

void packet(struct sniff_pkt *pkt)
{
	if (pkt->interface->num != 0)
		return;

	printf("%llu %d: i=%d\n", pkt->radio.tsft, pkt->radio.rssi, pkt->mg_hdr.line_num);
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
	if (mgi_init(mg) <= 0)
		die("no available interfaces found");

	mgi_set_callback(mg, packet);

	/* TODO */
	mg->myid = 1;

	/*
	 * generate
	 */
	for (int i = 0; i < 100; i++) {
		mgi_send(&mg->interface[0], 2, i, 1000);
	}

	/*
	 * main loop
	 */
	event_base_dispatch(mg->evb);

	/* cleanup */
	event_base_free(mg->evb);
	mmatic_free(mm);

	return 0;
}
