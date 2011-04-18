/*
 * Author: Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include "cmd-packet.h"
#include "generator.h"

int cmd_packet_init(struct line *line)
{
	int i;
	struct cmd_packet *cp;

	cp = mmatic_zalloc(sizeof(struct cmd_packet), line);

	/* defaults */
	cp->len = 100;
	cp->num = 1;
	cp->T = 1000;

	for (i = 1; i < line->argc; i++) {
		switch (i) {
			case 1:
				cp->len = atoi(line->argv[1]);

				if (cp->len < PKT_TOTAL_OVERHEAD) {
					dbg(0, "%s: line %d: frame too short (%d < %d)\n",
						line->mg->options.traf_file, line->line_num,
						cp->len, PKT_TOTAL_OVERHEAD);
					return 2;
				}
				break;
			case 2:
				cp->num = atoi(line->argv[2]);
				break;
			case 3:
				cp->T = atoi(line->argv[3]);
				break;
		}
	}

	dbg(11, "line %d: packet initialized with len=%d, num=%d, T=%d\n",
		line->line_num, cp->len, cp->num, cp->T);

	line->prv = cp;
	return 0;
}

void cmd_packet_out(int fd, short evtype, void *arg)
{
	struct line *line = arg;
	struct cmd_packet *cp = line->prv;
	struct timeval tv;

	/* send ASAP */
	mgi_send(line, NULL, cp->len);

	/* reschedule if requested */
	cp->num--;
	if (cp->num > 0) {
		tv.tv_sec = cp->T / 1000000;
		tv.tv_usec = cp->T % 1000000;
		evtimer_add(&line->ev, &tv);
	}
}

void cmd_packet_in(struct line *line, struct sniff_pkt *pkt)
{
	struct cmd_packet *cp = line->prv;

	if (cp->last_ctr && cp->last_ctr + 1 != pkt->mg_hdr.line_ctr) {
		dbg(1, "line %u: lost: wanted=%u\n", line->line_num, cp->last_ctr + 1);
	}

	cp->last_ctr = pkt->mg_hdr.line_ctr;
}
