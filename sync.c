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
	struct timeval tv, rtt1, rtt2;
	uint32_t rtt;

	/* find first line */
	for (i = 1; i < TRAFFIC_LINE_MAX; i++) {
		if (mg->lines[i])
			break;
	}
	if (i == TRAFFIC_LINE_MAX)
		return;

	if (mg->options.myid == mg->lines[i]->srcid) { /* = master */
		/* wait 5 seconds */
		dbg(0, "Waiting ~5s...\n");
		usleep(5000000);

		mgc_unlock(mg, 1);
		gettimeofday(&rtt1, NULL);
		mgc_lock(mg, 2);
		gettimeofday(&rtt2, NULL);

		/* get RTT */
		if (rtt2.tv_sec == rtt1.tv_sec + 1)
			rtt = rtt2.tv_usec + (1000000 - rtt1.tv_usec);
		else if (rtt2.tv_sec == rtt1.tv_sec)
			rtt = rtt2.tv_usec - rtt1.tv_usec;
		else
			die("Too high sync RTT\n");

		dbg(0, "Sync RTT = %uus\n", rtt);

		mgc_unlock(mg, 3);
//		usleep(rtt / 2);
	} else { /* = slave */
		dbg(0, "Waiting for master...\n");

		mgc_lock(mg, 1);
		mgc_unlock(mg, 2);  /* sends N-1 times */
		mgc_lock(mg, 3);
	}

	/* XXX: all nodes should be on this line in the same time */
}

void mgc_unlock(struct mg *mg, uint8_t key)
{
	struct sockaddr_in dst;
	int s, i = 1;
	char buf[] = { 'S', 'T', 'A', 'R', 'T', key, '\0' };

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		die_errno("socket");

	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &i, sizeof i) < 0)
		die_errno("setsockopt");

	memset((char *) &dst, 0, sizeof dst);
	dst.sin_family = AF_INET;
	dst.sin_port = htons(PORT);
	dst.sin_addr.s_addr = INADDR_BROADCAST;

	dbg(1, "Sending START packet key=%u\n", key);
	if (sendto(s, buf, sizeof buf, 0, (struct sockaddr *) &dst, sizeof dst) < 0)
		die_errno("sendto");

	close(s);
}

void mgc_lock(struct mg *mg, uint8_t key)
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

	dbg(1, "Waiting for START packet key=%u\n", key);
	while (true) {
		if (recvfrom(s, buf, BUFSIZ, 0, (struct sockaddr *) &src, &slen) < 0)
			die_errno("recvfrom");

		if (strncmp(buf, "START", 5) == 0 && buf[5] == key) {
			break;
		} else {
			dbg(1, "received invalid START packet)\n");
		}
	}

	close(s);
}
