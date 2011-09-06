/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include "cmd-ttftp.h"
#include "interface.h"
#include "generator.h"
#include "schedule.h"

int cmd_ttftp_init(struct line *line, const char *text)
{
	struct mg *mg = line->mg;
	struct cmd_ttftp *cp;
	struct mgp_line *pl;
	char *errmsg;

	pl = mgp_parse_line(mg->mm, text, 0, NULL, &errmsg,
		"size", "rep", "T", "rate", "MB", NULL);
	if (!pl) {
		dbg(0, "%s: line %d: ttftp: parse error: %s\n",
			mg->options.traf_file, line->line_num, errmsg);
		return 1;
	}

	/* rewrite into struct cmd_ttftp */
	cp = mmatic_zalloc(mg->mm, sizeof *cp);
	line->prv = cp;

	cp->size  = mgp_prepare_int(pl, "size", TTFTP_DU_SIZE);
	cp->rep   = mgp_get_int(pl, "rep", 1);
	cp->T     = mgp_prepare_int(pl, "T", 1000);
	cp->rate  = mgp_prepare_int(pl, "rate", 1);
	cp->MB    = mgp_prepare_int(pl, "MB", 1);

	return 0;
}

void cmd_ttftp_timeout(int fd, short evtype, void *arg)
{
	struct line *line = arg;
	struct cmd_ttftp *cp = line->prv;
	int todo, i;

	if (!cp->req_handling) {
		cp->req_handling = true;
		cp->req_size = mgp_int(cp->size);

		/* convert file size [MB] -> num. of frames */
		cp->req_left = 1024 * 1024 * mgp_int(cp->MB) / ((float) cp->req_size);

		/* rate of 1Mbps is realized by one 1500B frame each 12ms */
		cp->req_burst = mgp_int(cp->rate);
		cp->req_sleep = 12000.0 * ((float) cp->req_size) / 1500.0;
	}

	/* send */
	if (cp->req_left) {
		todo = MIN(cp->req_left, cp->req_burst);
	} else {
		todo = cp->req_burst;
	}

	for (i = 0; i < todo; i++)
		mgi_sendto(0, line, NULL, 0, cp->req_size);

	if (cp->req_left) {
		cp->req_left -= todo;

		if (cp->req_left == 0) {
			dbg(0, "ttftp: finished\n");
			cp->req_handling = false;

			/* request finished: reschedule? */
			if (cp->rep-- > 1)
				mgs_usleep(line, mgp_int(cp->T) * 1000);
			else
				line->mg->running--;

			return;
		}
	}

	mgs_usleep(line, cp->req_sleep);
}

void cmd_ttftp_packet(struct sniff_pkt *pkt)
{
	//struct cmd_ttftp *cp = pkt->line->prv;

	/* send ACK */
	if (!pkt->line->my) {
		mgi_sendto(pkt->line->srcid, pkt->line, NULL, 0, TTFTP_ACK_SIZE);
	}
}
