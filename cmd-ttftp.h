/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#ifndef _CMD_TTFTP_H_
#define _CMD_TTFTP_H_

#include "generator.h"
#include "parser.h"

#define TTFTP_DU_SIZE 1500
#define TTFTP_ACK_SIZE 68

struct cmd_ttftp {
	struct mgp_arg *size;        /**< frame size */
	int rep;                     /**< number of repetitions left */
	struct mgp_arg *T;           /**< time interval between repetitions [ms] */
	struct mgp_arg *MB;          /**< data size to send in single request [MB]; 0=inf. */
	struct mgp_arg *rate;        /**< data rate [Mbps] */

	bool req_handling;           /**< if true, request is currently handled */
	uint32_t req_size;           /**< request: frame size */
	uint32_t req_burst;          /**< request: number of frames in burst */
	uint32_t req_sleep;          /**< request: time between two bursts of data [us] */
	uint32_t req_left;           /**< request: number of frames to send */
};

/** Initialize the ttftp command */
int cmd_ttftp_init(struct line *line, const char *text);

/** Handle outgoing ttftp */
void cmd_ttftp_timeout(int fd, short evtype, void *arg);

/** Handle incoming ttftp */
void cmd_ttftp_packet(struct sniff_pkt *pkt);

#endif
