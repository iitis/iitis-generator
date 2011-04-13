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
#include "lib/radiotap.h"

#include "generator.h"

/* Code inspired by hostap.git/wlantest/inject.c */
int mgi_inject(struct interface *interface,
	struct ether_addr *bssid, struct ether_addr *dst, struct ether_addr *src,
	uint16_t ether_type, void *data, size_t len)
{
	int ret;

	dbg(15, "interface=%d len=%d\n", interface->num, len);

	/******************** static! **************************/
	/* radiotap header
	 * NOTE: this is always LSB!
	 * SEE: Documentation/networking/mac80211-injection.txt
	 * SEE: include/net/ieee80211_radiotap.h */
	static uint8_t rtap_hdr[PKT_RADIOTAP_HDRSIZE] = {
		0x00, 0x00,             /* radiotap version */
		0x08, 0x00,             /* radiotap length */
		0x00, 0x00, 0x00, 0x00
	};

	/* LLC Encapsulated Ethernet header */
	static uint8_t llc_hdr[PKT_LLC_HDRSIZE] = {
		0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, PKT_ETHERTYPE >> 8, PKT_ETHERTYPE & 0xff
	};
	/******************************************************/

	/* 802.11 header - IBSS data frame */
	uint8_t ieee80211_hdr[PKT_IEEE80211_HDRSIZE] = {
		0x08, 0x00,                         /* Frame Control: data */
		0x00, 0x00,                         /* Duration */
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* RA: dst */
		0x13, 0x22, 0x33, 0x44, 0x55, 0x66, /* SA: src */
		0x13, 0x22, 0x33, 0x44, 0x55, 0x66, /* BSSID */
		0x12, 0x34                          /* seq */
	};

	/* glue together in an IO vector */
	struct iovec iov[] = {
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
			.iov_base = data,
			.iov_len = len,
		}
	};

	/* message */
	struct msghdr msg = {
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

	ret = sendmsg(interface->fd, &msg, 0);
	if (ret < 0) {
		reterrno(-1, 1, "sendmsg");
	} else {
		return ret;
	}
}

void mgi_send(struct interface *interface,
	uint8_t dst, uint32_t line_num, int size)
{
	uint8_t pkt[PKT_BUFSIZE];
	struct mg_hdr *mg_hdr;
	struct timeval timestamp;
	int i;

	struct ether_addr bssid  = {{ 0x06, 0xFE, 0xEE, 0xED, 0xFF, interface->num }};
	struct ether_addr srcmac = {{ 0x06, 0xFE, 0xEE, 0xED, interface->num, interface->mg->myid }};
	struct ether_addr dstmac = {{ 0x06, 0xFE, 0xEE, 0xED, interface->num, dst }};

	size -= PKT_HEADERS_SIZE + PKT_IEEE80211_FCSSIZE;
	if (size < sizeof *mg_hdr) {
		dbg(1, "pkt too short\n");
		return;
	} else if (size > PKT_BUFSIZE) {
		dbg(1, "pkt too long\n");
		return;
	}

	/* fill the header */
	gettimeofday(&timestamp, NULL);

	mg_hdr = (void *) pkt;
	mg_hdr->mg_tag   = htonl(MG_TAG_V1);
	mg_hdr->time_s   = htonl(timestamp.tv_sec);
	mg_hdr->time_us  = htonl(timestamp.tv_usec);
	mg_hdr->line_num = htonl(line_num);

	static int ctr = 0; /* TODO */
	mg_hdr->line_ctr = htonl(ctr++);

	/* TODO: fill the rest */
	for (i = sizeof *mg_hdr; i < size; i++) {
		pkt[i] = 'A';
	}

	/* send */
	mgi_inject(interface, &bssid, &dstmac, &srcmac, PKT_ETHERTYPE, (void *) pkt, (size_t) size);
}

static void _mgi_sniff(int fd, short event, void *arg)
{
	struct interface *interface = arg;
	struct sniff_pkt pkt;
	int ret, n;
	struct ieee80211_radiotap_iterator parser;
	uint8_t *ieee80211_hdr;
	struct mg_hdr *mg_hdr;

	gettimeofday(&pkt.timestamp, NULL);

	ret = recvfrom(fd, pkt.pkt, PKT_BUFSIZE, MSG_DONTWAIT, NULL, NULL);
	if (ret <= 0) {
		if (errno != EAGAIN)
			dbg(1, "recvfrom(): %s\n", strerror(errno));
		return;
	}

	/*
	 * init
	 */
	if (ieee80211_radiotap_iterator_init(&parser, (void *) pkt.pkt, ret) < 0) {
		dbg(1, "ieee80211_radiotap_iterator_init() failed\n");
		return;
	}

	dbg(15, "interface=%d len=%d max_length=%d\n", interface->num, ret, parser.max_length);

	pkt.interface = interface;
	pkt.size = ret - parser.max_length;

	if (pkt.size < PKT_HEADERS_SIZE + sizeof(struct mg_hdr)) {
		dbg(11, "skipping short frame\n");
		return;
	}

	/*
	 * parse IEEE 802.11 header
	 */
	ieee80211_hdr = (uint8_t *) pkt.pkt + parser.max_length;

	/* skip non-data frames */
	if (!(ieee80211_hdr[0] == 0x08 && ieee80211_hdr[1] == 0x00)) {
		dbg(14, "skipping non-data frame\n");
		return;
	}

	/* skip invalid BSSID */
	if (!(ieee80211_hdr[16] == 0x06 &&
	      ieee80211_hdr[17] == 0xFE &&
	      ieee80211_hdr[18] == 0xEE &&
	      ieee80211_hdr[19] == 0xED &&
	      ieee80211_hdr[20] == 0xFF)) {
		dbg(9, "skipping invalid bssid frame\n");
		return;
	}

	/* skip cross-channel transmissions */
	if (!(ieee80211_hdr[21] == interface->num)) {
		dbg(9, "skipping cross-channel frame\n");
		return;
	}

	pkt.dstid = ieee80211_hdr[9];
	pkt.srcid = ieee80211_hdr[15];

	if (pkt.srcid == interface->mg->myid) {
		dbg(13, "skipping local frame (%d)\n", pkt.srcid);
		return;
	}

	/* drop frames not destined to us
	 * NB: nice place for some inventions */
	if (pkt.dstid != interface->mg->myid) {
		dbg(9, "skipping not ours frame (%d)\n", pkt.dstid);
		return;
	}

	/*
	 * parse radiotap header
	 */
	memset(&(pkt.radio), 0, sizeof pkt.radio);
	while ((n = ieee80211_radiotap_iterator_next(&parser)) == 0) {
		switch (parser.this_arg_index) {
			case IEEE80211_RADIOTAP_TSFT:
				pkt.radio.tsft = le64toh(*((uint64_t *) parser.this_arg));
				break;
			case IEEE80211_RADIOTAP_FLAGS:
				pkt.radio.flags.val      = *parser.this_arg;
				pkt.radio.flags.cfp      = pkt.radio.flags.val & IEEE80211_RADIOTAP_F_CFP;
				pkt.radio.flags.shortpre = pkt.radio.flags.val & IEEE80211_RADIOTAP_F_SHORTPRE;
				pkt.radio.flags.frag     = pkt.radio.flags.val & IEEE80211_RADIOTAP_F_FRAG;
				pkt.radio.flags.badfcs   = pkt.radio.flags.val & IEEE80211_RADIOTAP_F_BADFCS;
				break;
			case IEEE80211_RADIOTAP_RATE:
				pkt.radio.rate = *(parser.this_arg);
				break;
			case IEEE80211_RADIOTAP_CHANNEL:
				pkt.radio.freq = le16toh(*((uint16_t *) parser.this_arg));
				/* NB: skip channel flags */
				break;
			case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
				pkt.radio.rssi = *((int8_t *) parser.this_arg);
				break;
			case IEEE80211_RADIOTAP_ANTENNA:
				pkt.radio.antnum = *(parser.this_arg);
				break;
			default:
				dbg(3, "unhandled arg %d value %u\n",
					parser.this_arg_index, *(parser.this_arg));
				break;
		}
	} if (n != -ENOENT) {
		dbg(1, "ieee80211_radiotap_iterator_next() failed\n");
		return;
	}

	/* skip locally generated frames */
	if (pkt.radio.tsft == 0)
		return;

	/* skip frames with bad FCS */
	if (pkt.radio.flags.badfcs) {
		dbg(9, "skipping bad FCS frame\n");
		return;
	}

	dbg(8, "frame: tsft=%llu rate=%u freq=%u rssi=%d size=%u\n",
		pkt.radio.tsft, pkt.radio.rate / 2, pkt.radio.freq, pkt.radio.rssi, pkt.size);

	/*
	 * parse mg header
	 */
	mg_hdr = (struct mg_hdr *) (pkt.pkt + parser.max_length + PKT_HEADERS_SIZE);
#define A(field) pkt.mg_hdr.field = ntohl(mg_hdr->field)
	A(mg_tag);
	A(time_s);
	A(time_us);
	A(line_num);
	A(line_ctr);
#undef A

	if (pkt.mg_hdr.mg_tag != MG_TAG_V1) {
		dbg(8, "skipping invalid mg tag frame (%x)\n", pkt.mg_hdr.mg_tag);
		return;
	}

	pkt.payload = (uint8_t *) mg_hdr + sizeof *mg_hdr;
	pkt.paylen  = pkt.size - PKT_HEADERS_SIZE - PKT_IEEE80211_FCSSIZE;

	/* pass to higher layers */
	interface->mg->packet_cb(&pkt);
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
		mg->interface[i].num = i;
		mg->interface[i].fd = fd;
		mg->interface[i].evread = mmatic_alloc(sizeof(struct event), mg->mm);

		/* monitor for incoming packets */
		event_set(mg->interface[i].evread,
			fd, EV_READ | EV_PERSIST, _mgi_sniff, &mg->interface[i]);

		event_add(mg->interface[i].evread, NULL);
	}

	return count;
}

void mgi_set_callback(struct mg *mg, mgi_packet_cb cb)
{
	mg->packet_cb = cb;
}
