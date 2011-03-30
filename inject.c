/*
 * ath9k monitor mode packet generator
 * Paweł Foremski <pforemski@gmail.com> 2011
 * IITiS PAN Gliwice
 */

/* TODO: all needed? */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <endian.h>
#include <byteswap.h>
#include <linux/if_ether.h>

/* needed */
#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>

#include <libpjf/lib.h>

#include "generator.h"

/* from hostap.git/wlantest/inject.c */
int mgi_inject(struct mg *mg, int ifidx,
	struct ether_addr *bssid, struct ether_addr *dst, struct ether_addr *src,
	void *data, size_t len)
{
	int ret;
	uint16_t seqle;
	static uint16_t seq = 1;

	/* radiotap header
	 * NOTE: this is always LSB!
	 * SEE: Documentation/networking/mac80211-injection.txt
	 * SEE: include/net/ieee80211_radiotap.h
	 * TODO: setup radiotap headers for proper channel, antenna, rate, etc. - the packet injection
	 *       framework touches only 3 of RADIOTAP_FLAGS, but more might be needed by eg. ath9k
	 *       see Documentation/networking/mac80211-injection.txt */
	static uint8_t rtap_hdr[] = {
		0x00, 0x00,             /* radiotap version */
		0x0e, 0x00,             /* radiotap length */
		0x02, 0xc0, 0x00, 0x00, /* bitmap: flags, tx and rx flags */
		0x00,                   /* RADIOTAP_FLAGS */
		0x00,                   /* padding TODO: why? */
		0x00, 0x00,             /* RADIOTAP_RX_FLAGS */
		0x00, 0x00,             /* RADIOTAP_TX_FLAGS */
	};

	/* 802.11 header - IBSS data frame */
	static uint8_t ieee80211_hdr[] = {
		0x20, 0x00,                         /* Frame Control: data */
		0x00, 0x00,                         /* Duration */
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* RA: dst */
		0x13, 0x22, 0x33, 0x44, 0x55, 0x66, /* SA: src */
		0x13, 0x22, 0x33, 0x44, 0x55, 0x66, /* BSSID */
		0x12, 0x34                          /* seq */
	};

	/* glue together in an IO vector */
	static struct iovec iov[3] = {
		{
			.iov_base = &rtap_hdr,
			.iov_len = sizeof rtap_hdr,
		},
		{
			.iov_base = &ieee80211_hdr,
			.iov_len = sizeof rtap_hdr,
		},
		{
			.iov_base = NULL,
			.iov_len = 0,
		}
	};

	/* message */
	static struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = iov,
		.msg_iovlen = N(iov),
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = 0,
	};

	/******************/
	memcpy(ieee80211_hdr +  4,   dst, sizeof *dst);
	memcpy(ieee80211_hdr + 10,   src, sizeof *src);
	memcpy(ieee80211_hdr + 16, bssid, sizeof *bssid);

	seq = (seq + 1) % 4096;
	seqle = htole16(seq);
	memcpy(ieee80211_hdr + 22, &seqle, 2);

	iov[2].iov_base = data;
	iov[2].iov_len  = len;

	ret = sendmsg(mg->inject.fds[ifidx], &msg, 0);
	if (ret < 0) {
		reterrno(-1, 1, "sendmsg");
	} else {
		return ret;
	}
}

int mgi_init(struct mg *mg)
{
	struct mmatic *mm = mg->mmtmp;
	struct sockaddr_ll ll;
	int fd;
	int count = 0;

	for (int i = 0; i < IFINDEX_MAX; i++) {
		mg->inject.fds[i] = -1;

		memset(&ll, 0, sizeof ll);
		ll.sll_family = AF_PACKET;
		ll.sll_ifindex = if_nametoindex(mmprintf(IFNAME_FMT, i));

		if (ll.sll_ifindex == 0)
			continue;

		fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
		if (fd < 0)
			continue;

		if (bind(fd, (struct sockaddr *) &ll, sizeof ll) < 0) {
			close(fd);
			continue;
		}

		dbg(1, "bound to " IFNAME_FMT, i);
		mg->inject.fds[i] = fd;
		count++;
	}

	return count;
}
