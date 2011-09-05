/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include "cmd-packet.h"
#include "interface.h"
#include "generator.h"
#include "schedule.h"

int cmd_packet_init(struct line *line, const char *text)
{
	struct mg *mg = line->mg;
	struct cmd_packet *cp;
	struct mgp_line *pl;
	char *errmsg;

	pl = mgp_parse_line(mg->mm, text, 0, NULL, &errmsg,
		"size", "rep", "T", "burst", NULL);
	if (!pl) {
		dbg(0, "%s: line %d: packet: parse error: %s\n",
			mg->options.traf_file, line->line_num, errmsg);
		return 1;
	}

	/* rewrite into struct cmd_packet */
	cp = mmatic_zalloc(mg->mm, sizeof(struct cmd_packet));
	line->prv = cp;

	cp->len   = mgp_fetch_int(pl, "size", 100);
	cp->num   = mgp_int(mgp_fetch_int(pl, "rep", 1));
	cp->T     = mgp_fetch_int(pl, "T", 1000);
	cp->burst = mgp_fetch_int(pl, "burst", 1);

	return 0;
}

void cmd_packet_out(int fd, short evtype, void *arg)
{
	struct line *line = arg;
	struct cmd_packet *cp = line->prv;
	uint32_t i, len, burst;

	burst = mgp_int(cp->burst);
	len = mgp_int(cp->len);

	/* send ASAP */
	for (i = 0; i < burst; i++)
		mgi_send(line, NULL, 0, len);

	/* reschedule? */
	if (cp->num-- > 1)
		mgs_usleep(line, mgp_int(cp->T) * 1000);
	else
		line->mg->running--;
}

void cmd_packet_in(struct sniff_pkt *pkt)
{
	struct cmd_packet *cp = pkt->line->prv;

/*	if (cp->last_ctr) {
		if (cp->last_ctr + 1 != pkt->mg_hdr.line_ctr) {
			dbg(1, "line %u: lost: wanted=%u\n", pkt->line->line_num, cp->last_ctr + 1);
		}
	}*/

	cp->last_ctr = pkt->mg_hdr.line_ctr;
}
