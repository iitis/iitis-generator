/*
 * Author: Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <event.h>

#include "sync.h"
#include "generator.h"

#define PORT 31337
#define PASS "START"

#define SLAVE_TIMEOUT 3
#define MASTER_OFFER 5
#define MASTER_TIMEOUT 1

static void _make_offer(struct mg_sync *mgs)
{
	struct timeval now;
	gettimeofday(&now, NULL);

	mgs->hdr.code    = htonl(MG_SYNC_OFFER);
	mgs->hdr.node    = mgs->mg->options.myid;
	mgs->hdr.try     = htonl(ntohl(mgs->hdr.try) + 1);
	mgs->hdr.time_s  = htonl(now.tv_sec + MASTER_OFFER);
	mgs->hdr.time_us = htonl(0);

	mgs->mg->origin.tv_sec  = ntohl(mgs->hdr.time_s);
	mgs->mg->origin.tv_usec = ntohl(mgs->hdr.time_us);
}

void _master_read(int s, short evtype, void *arg)
{
	struct mg_sync *mgs = arg;
	struct sockaddr_in src;
	socklen_t slen = sizeof src;
	struct mg_sync_hdr hdr;

	if (recvfrom(s, &hdr, sizeof hdr, 0, (struct sockaddr *) &src, &slen) < 0)
		die_errno("recvfrom");

	if (ntohl(hdr.code) != MG_SYNC_ACK)
		return;

	/* ignore bogus ACK */
	if (ntohl(hdr.time_s)  != ntohl(mgs->hdr.time_s) ||
	    ntohl(hdr.time_us) != ntohl(mgs->hdr.time_us))
		return;

	if (hdr.node > mgs->node_max)
		return;

	dbg(1, "received time sync ACK from node %u: %lu\n", hdr.node, ntohl(hdr.time_s));
	mgs->acked[hdr.node] = 1;

	/* let it go if all replied */
	if (memcmp(mgs->exist, mgs->acked, mgs->node_max + 1) == 0) {
		dbg(1, "received all ACKs\n");
		event_base_loopbreak(mgs->evb);
	}
}

static void _master_tmout(int s, short evtype, void *arg)
{
	struct mg_sync *mgs = arg;
	struct sockaddr_in dst;
	struct timeval tv = { MASTER_TIMEOUT, 0 };
	int i;

	if (mgs->hdr.try > 0) {
		dbg(0, "Timeout waiting for all/some of %u slaves... (try %u)\n", mgs->node_count-1, mgs->hdr.try);

		for (i = 1; i < mgs->node_max + 1; i++) {
			if (mgs->exist[i] && !mgs->acked[i])
				dbg(1, "waiting for %u\n", i);
		}
	}

	memset((void *) mgs->acked, 0, mgs->node_max + 1);
	mgs->acked[mgs->mg->options.myid] = 1;

	memset((char *) &dst, 0, sizeof dst);
	dst.sin_family = AF_INET;
	dst.sin_port = htons(PORT);
	dst.sin_addr.s_addr = INADDR_BROADCAST;

	_make_offer(mgs);
	if (sendto(mgs->s, &mgs->hdr, sizeof mgs->hdr, 0, (struct sockaddr *) &dst, sizeof dst) < 0)
		die_errno("sendto");

	timeout_add(&mgs->evt, &tv);
}

static void _master(struct mg_sync *mgs)
{
	int s, i = 1;

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		die_errno("socket");

	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &i, sizeof i) < 0)
		die_errno("setsockopt");

	mgs->hdr.try = 0;
	mgs->s = s;

	event_set(&mgs->evr, s, EV_READ | EV_PERSIST, _master_read, mgs);
	event_base_set(mgs->evb, &mgs->evr);
	event_add(&mgs->evr, 0);

	timeout_set(&mgs->evt, _master_tmout, mgs);
	event_base_set(mgs->evb, &mgs->evt);
	_master_tmout(s, 0, mgs); /* send offer now */

	event_base_loop(mgs->evb, 0);

	close(s);
}

static void _slave_tmout(int fd, short evtype, void *arg)
{
	struct mg_sync *mgs = arg;
	event_base_loopbreak(mgs->evb);
}

static void _slave_read(int s, short evtype, void *arg)
{
	struct sockaddr_in src;
	socklen_t slen = sizeof src;
	struct mg_sync *mgs = arg;
	struct timeval tv = { SLAVE_TIMEOUT, 0 };

	if (recvfrom(s, &mgs->hdr, sizeof mgs->hdr, 0, (struct sockaddr *) &src, &slen) < 0)
		die_errno("recvfrom");

	if (ntohl(mgs->hdr.code) != MG_SYNC_OFFER)
		return;

	dbg(1, "received time sync offer from node %u: %lu\n",
		mgs->hdr.node, ntohl(mgs->hdr.time_s));

	/* store origin offer */
	mgs->mg->origin.tv_sec  = ntohl(mgs->hdr.time_s);
	mgs->mg->origin.tv_usec = ntohl(mgs->hdr.time_us);

	/* setup timeout: 3 seconds from now to use origin */
	timeout_del(&mgs->evt);
	timeout_add(&mgs->evt, &tv);

	/* send reply */
	mgs->hdr.code = MG_SYNC_ACK;
	mgs->hdr.node = mgs->mg->options.myid;
	if (sendto(s, &mgs->hdr, sizeof mgs->hdr, 0, (struct sockaddr *) &src, sizeof src) < 0)
		die_errno("sendto");
}

static void _slave(struct mg_sync *mgs)
{
	struct sockaddr_in dst;
	int s;

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		die_errno("socket");

	memset((char *) &dst, 0, sizeof dst);
	dst.sin_family = AF_INET;
	dst.sin_port = htons(PORT);
	dst.sin_addr.s_addr = INADDR_ANY;

	if (bind(s, (struct sockaddr *) &dst, sizeof dst) < 0)
		die_errno("bind");

	mgs->s = s;

	event_set(&mgs->evr, s, EV_READ | EV_PERSIST, _slave_read, mgs);
	event_base_set(mgs->evb, &mgs->evr);
	event_add(&mgs->evr, 0);

	timeout_set(&mgs->evt, _slave_tmout, mgs);
	event_base_set(mgs->evb, &mgs->evt);

	event_base_loop(mgs->evb, 0);

	close(s);
}

/********************************/

void mgc_sync(struct mg *mg)
{
	int i;
	struct mg_sync mgs;

	mgs.mg = mg;
	mgs.node_min = UINT8_MAX;
	mgs.node_max = 0;
	mgs.node_count = 0;

	for (i = 1; i < TRAFFIC_LINE_MAX; i++) {
		if (!mg->lines[i])
			continue;

		if (mg->lines[i]->srcid < mgs.node_min)
			mgs.node_min = mg->lines[i]->srcid;

		if (mg->lines[i]->srcid > mgs.node_max)
			mgs.node_max = mg->lines[i]->srcid;

		if (mg->lines[i]->dstid < mgs.node_min)
			mgs.node_min = mg->lines[i]->dstid;

		if (mg->lines[i]->dstid > mgs.node_max)
			mgs.node_max = mg->lines[i]->dstid;
	}

	if (!mgs.node_max) {
		dbg(0, "Sync failed: node_max == 0\n");
		return;
	}

	mgs.evb = event_base_new();
	mgs.exist = mmatic_zalloc(mg->mmtmp, sizeof(uint8_t) * (mgs.node_max + 1));
	mgs.acked = mmatic_zalloc(mg->mmtmp, sizeof(uint8_t) * (mgs.node_max + 1));

	for (i = 1; i < TRAFFIC_LINE_MAX; i++) {
		if (!mg->lines[i])
			continue;

		if (!mgs.exist[mg->lines[i]->srcid]) {
			mgs.exist[mg->lines[i]->srcid] = 1;
			mgs.node_count++;
		}

		if (!mgs.exist[mg->lines[i]->dstid]) {
			mgs.exist[mg->lines[i]->dstid] = 1;
			mgs.node_count++;
		}
	}

	if (mg->options.myid == mgs.node_min) /* master */
		_master(&mgs);
	else if (mgs.exist[mg->options.myid]) /* slave */
		_slave(&mgs);
	else
		dbg(0, "Not in traffic file - not syncing\n");

	event_base_free(mgs.evb);
	mmatic_freeptr(mgs.exist);
	mmatic_freeptr(mgs.acked);
}
