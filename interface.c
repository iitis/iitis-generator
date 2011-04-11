/*
 * ath9k monitor mode packet generator
 * Paweł Foremski <pforemski@gmail.com> 2011
 * IITiS PAN Gliwice
 */

#include <endian.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>

#include <libpjf/lib.h>
#include <event.h>

#include "generator.h"

/* from hostap.git/wlantest/inject.c */
int mgi_inject(struct mg *mg, int ifidx,
	struct ether_addr *bssid, struct ether_addr *dst, struct ether_addr *src,
	uint16_t ether_type, void *data, size_t len)
{
	int ret;
	static uint16_t seq = 0;

	/* radiotap header
	 * NOTE: this is always LSB!
	 * SEE: Documentation/networking/mac80211-injection.txt
	 * SEE: include/net/ieee80211_radiotap.h */
	static uint8_t rtap_hdr[] = {
		0x00, 0x00,             /* radiotap version */
		0x08, 0x00,             /* radiotap length */
		0x00, 0x00, 0x00, 0x00
	};

	/* 802.11 header - IBSS data frame */
	static uint8_t ieee80211_hdr[] = {
		0x08, 0x00,                         /* Frame Control: data */
		0x00, 0x00,                         /* Duration */
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* RA: dst */
		0x13, 0x22, 0x33, 0x44, 0x55, 0x66, /* SA: src */
		0x13, 0x22, 0x33, 0x44, 0x55, 0x66, /* BSSID */
		0x12, 0x34                          /* seq */
	};

	/* LLC Encapsulated Ethernet header */
	static uint8_t llc_hdr[] = {
		0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00,
		0x08, 0x00
	};

	/* glue together in an IO vector */
	static struct iovec iov[] = {
		{
			.iov_base = &rtap_hdr,
			.iov_len = sizeof rtap_hdr,
		},
		{
			.iov_base = &ieee80211_hdr,
			.iov_len = sizeof ieee80211_hdr,
		},
		{
			.iov_base = &llc_hdr,
			.iov_len = sizeof llc_hdr,
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

	ieee80211_hdr[22] = (seq & 0x000f) << 4;
	ieee80211_hdr[23] = (seq & 0x0ff0) >> 4;
	seq = (seq + 1) % 4096;

	llc_hdr[6] = ether_type >> 8;
	llc_hdr[7] = ether_type & 0xff;

	iov[N(iov)-1].iov_base = data;
	iov[N(iov)-1].iov_len  = len;

	ret = sendmsg(mg->interface[ifidx].fd, &msg, 0);
	if (ret < 0) {
		reterrno(-1, 1, "sendmsg");
	} else {
		return ret;
	}
}

static void _mgi_sniff(int fd, short event, void *arg)
{
	//struct interface *interface = arg;
	uint8_t pkt[PKTSIZE];
	int ret;

	ret = recvfrom(fd, pkt, PKTSIZE, MSG_DONTWAIT, NULL, NULL);
	if (ret > 0) {
		dbg(7, "captured frame size %d bytes\n", ret);
		return;
	}

	//return (errno == EAGAIN ? 0 : -1);
}

int mgi_init(struct mg *mg)
{
	struct mmatic *mm = mg->mmtmp;
	struct sockaddr_ll ll;
	int count = 0;
	int fd;

	memset(&ll, 0, sizeof ll);
	ll.sll_family = AF_PACKET;

	/* open PF_PACKET raw sockets on interfaces */
	for (int i = 0; i < IFINDEX_MAX; i++) {
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

		dbg(1, "bound to " IFNAME_FMT "\n", i);
		count++;

		mg->interface[i].mg = mg;
		mg->interface[i].fd = fd;
		mg->interface[i].evread = mmatic_alloc(sizeof(struct event), mg->mm);

		/* monitor for incoming packets */
		event_set(mg->interface[i].evread,
			fd, EV_READ | EV_PERSIST, _mgi_sniff, &mg->interface[i]);

		event_add(mg->interface[i].evread, NULL);
	}

	return count;
}
