/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#ifndef _CMD_PACKET_H_
#define _CMD_PACKET_H_

#include "generator.h"

struct cmd_packet {
	uint16_t len;         /** frame length */
	uint32_t num;         /** number of repetitions left */
	uint32_t T;           /** time interval between repetitions */
	uint32_t each;        /** number of frames to send in one repetition */

	uint32_t last_ctr;    /** last ctr value */
};

/** Initialize the packet command */
int cmd_packet_init(struct line *line);

/** Handle outgoing packet */
void cmd_packet_out(int fd, short evtype, void *arg);

/** Handle incoming packet */
void cmd_packet_in(struct sniff_pkt *pkt);

#endif
