/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#ifndef _CMD_PACKET_H_
#define _CMD_PACKET_H_

#include "generator.h"
#include "parser.h"

struct cmd_packet {
	struct mgp_arg *len;         /**< frame length */
	int num;                     /**< number of repetitions left */
	struct mgp_arg *T;           /**< time interval between repetitions [ms] */
	struct mgp_arg *burst;       /**< number of frames to send in one repetition */

	uint32_t last_ctr;    /**< last ctr value */
};

/** Initialize the packet command */
int cmd_packet_init(struct line *line, const char *text);

/** Handle outgoing packet */
void cmd_packet_out(int fd, short evtype, void *arg);

/** Handle incoming packet */
void cmd_packet_in(struct sniff_pkt *pkt);

#endif
