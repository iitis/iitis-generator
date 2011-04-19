/*
 * Author: Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sync.h"
#include "generator.h"

#define PORT 31337

void mgc_sync(struct mg *mg)
{
	int i;

	for (i = 1; i < TRAFFIC_LINE_MAX; i++) {
		if (mg->lines[i])
			break;
	}

	if (i == TRAFFIC_LINE_MAX)
		return;

	if (mg->options.myid == mg->lines[i]->srcid)
		mgc_master(mg);
	else
		mgc_slave(mg);
}

void mgc_master(struct mg *mg)
{
	struct timeval tv;
	struct sockaddr_in dst;
	int s, i = 1;
	char buf[] = { 'S', 'T', 'A', 'R', 'T', '\0' };

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		die_errno("socket");

	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &i, sizeof i) < 0)
		die_errno("setsockopt");

	memset((char *) &dst, 0, sizeof dst);
	dst.sin_family = AF_INET;
	dst.sin_port = htons(PORT);
	dst.sin_addr.s_addr = INADDR_BROADCAST;

	/* wait 5 seconds */
	dbg(0, "Waiting for all nodes...\n");
	usleep(5000000);
	while (true) {
		gettimeofday(&tv, NULL);
		if (tv.tv_usec < 100)
			break;
	}

	dbg(0, "Sending START packet\n");
	if (sendto(s, buf, sizeof buf, 0, (struct sockaddr *) &dst, sizeof dst) < 0)
		die_errno("sendto");

	close(s);
}

void mgc_slave(struct mg *mg)
{
	struct sockaddr_in dst, src;
	int s;
	socklen_t slen = sizeof src;
	char buf[BUFSIZ];

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		die_errno("socket");

	memset((char *) &dst, 0, sizeof dst);
	dst.sin_family = AF_INET;
	dst.sin_port = htons(PORT);
	dst.sin_addr.s_addr = INADDR_ANY;

	if (bind(s, (struct sockaddr *) &dst, sizeof dst) < 0)
		die_errno("bind");

	dbg(0, "Waiting for START packet\n");
	while (true) {
		if (recvfrom(s, buf, BUFSIZ, 0, (struct sockaddr *) &src, &slen) < 0)
			die_errno("recvfrom");

		if (strncmp(buf, "START", 6) == 0) {
			break;
		} else {
			dbg(1, "received invalid START packet)\n");
		}
	}

	close(s);
}
