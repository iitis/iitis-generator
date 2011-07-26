/*
 * Paweł Foremski <pjf@iitis.pl> 2011
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
#include "stats.h"
#include "dump.h"

static bool _stats_write_interface(struct mg *mg, ut *dst, void *arg)
{
	struct interface *interface = arg;

	mgstats_db_aggregate(dst, interface->stats);
	return true;
}

static bool _stats_write_link(struct mg *mg, ut *dst, void *arg)
{
	mgstats_db_aggregate(dst, (ut *) arg);
	return true;
}

/*****/

/* Code inspired by hostap.git/wlantest/inject.c */
int mgi_inject(struct interface *interface,
	struct ether_addr *bssid, struct ether_addr *dst, struct ether_addr *src, uint8_t rate,
	uint16_t ether_type, void *data, size_t len)
{
	int ret;
	struct timeval t1, t2, diff;

	dbg(15, "interface=%d len=%d\n", interface->num, len);

	/* radiotap header
	 * NOTE: this is always LSB!
	 * SEE: Documentation/networking/mac80211-injection.txt
	 * SEE: include/net/ieee80211_radiotap.h */
	uint8_t rtap_hdr[PKT_RADIOTAP_HDRSIZE] = {
		0x00, 0x00,             /* radiotap version */
		0x0a, 0x00,             /* radiotap length */
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00
	};

	/* LLC Encapsulated Ethernet header */
	uint8_t llc_hdr[PKT_LLC_HDRSIZE] = {
		0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, PKT_ETHERTYPE >> 8, PKT_ETHERTYPE & 0xff
	};

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
	if (rate) {
		rtap_hdr[4] |= (1 << IEEE80211_RADIOTAP_RATE);
		rtap_hdr[8]  = rate;
	}

	memcpy(ieee80211_hdr +  4,   dst, sizeof *dst);
	memcpy(ieee80211_hdr + 10,   src, sizeof *src);
	memcpy(ieee80211_hdr + 16, bssid, sizeof *bssid);

	gettimeofday(&t1, NULL);
	ret = sendmsg(interface->fd, &msg, 0);
	gettimeofday(&t2, NULL);

	timersub(&t2, &t1, &diff);
	mgstats_db_count_num(interface->stats, "snt_time",
		diff.tv_sec * 1000000 + diff.tv_usec);

	if (ret < 0) {
		reterrno(-1, 0, "sendmsg");
	} else {
		mgstats_db_count(interface->stats, "snt_ok");
		mgstats_db_count_num(interface->stats, "snt_ok_bytes",
			iov[1].iov_len + iov[2].iov_len + iov[3].iov_len);
		return ret;
	}
}

void mgi_send(struct line *line, uint8_t *payload, int payload_size, int size)
{
	uint8_t pkt[PKT_BUFSIZE];
	struct mg_hdr *mg_hdr;
	struct timeval timestamp;
	struct interface *interface = line->interface;
	int i, j, k;
	struct timeval t1, t2, diff;

	struct ether_addr bssid  = {{ 0x06, 0xFE, 0xEE, 0xED, 0xFF, interface->num }};
	struct ether_addr srcmac = {{ 0x06, 0xFE, 0xEE, 0xED, interface->num, line->mg->options.myid }};
	struct ether_addr dstmac = {{ 0x06, 0xFE, 0xEE, 0xED, interface->num, line->dstid }};

	dbg(5, "sending line %d: %d -> %d size %d\n",
		line->line_num, line->mg->options.myid, line->dstid, size);

	gettimeofday(&t1, NULL);

	size -= PKT_HEADERS_SIZE + PKT_IEEE80211_FCSSIZE;
	if (size < sizeof *mg_hdr) {
		dbg(0, "pkt too short\n");
		return;
	} else if (size > PKT_BUFSIZE) {
		dbg(0, "pkt too long\n");
		return;
	}

	/* if NOACK, set broadcast bit */
	if (line->noack)
		dstmac.ether_addr_octet[0] |= 0x01;

	/* fill the header */
	gettimeofday(&timestamp, NULL);

	mg_hdr = (void *) pkt;
	mg_hdr->mg_tag   = htonl(MG_TAG_V1);
	mg_hdr->time_s   = htonl(timestamp.tv_sec);
	mg_hdr->time_us  = htonl(timestamp.tv_usec);
	mg_hdr->line_num = htonl(line->line_num);
	mg_hdr->line_ctr = htonl(++line->line_ctr);

	/* fill the rest */
	i = sizeof *mg_hdr;

	if (payload) {
		k = MIN(payload_size, size - i);
		memcpy(pkt+i, payload, k);
		i += k;
	}

	j = strlen(line->contents);
	while (i < size) {
		k = MIN(j, size - i);
		memcpy(pkt+i, line->contents, k);
		i += k;
	}

	/* send */
	mgi_inject(interface, &bssid, &dstmac, &srcmac, line->rate,
		PKT_ETHERTYPE, (void *) pkt, (size_t) size);

	gettimeofday(&t2, NULL);
	timersub(&t2, &t1, &diff);
	mgstats_db_count_num(line->stats, "snt_time",
		diff.tv_sec * 1000000 + diff.tv_usec);

	mgstats_db_count(line->stats, "snt_ok");
}

static void _mgi_sniff(int fd, short event, void *arg)
{
	struct interface *interface = arg;
	struct sniff_pkt pkt;
	int n;
	struct ieee80211_radiotap_iterator parser;
	uint8_t *ieee80211_hdr;
	struct mg_hdr *mg_hdr;

	memset((void *) &pkt, 0, sizeof pkt);
	gettimeofday(&pkt.timestamp, NULL);
	pkt.interface = interface;

	pkt.len = recvfrom(fd, pkt.pkt, PKT_BUFSIZE, MSG_DONTWAIT, NULL, NULL);
	if (pkt.len <= 0) {
		if (errno != EAGAIN)
			dbg(1, "recvfrom(): %s\n", strerror(errno));
		return;
	}

	if (interface->mg->options.dump)
		mgd_dump(&pkt);

	/*
	 * parse radiotap header
	 */
	if (ieee80211_radiotap_iterator_init(&parser, (void *) pkt.pkt, pkt.len) < 0) {
		dbg(1, "ieee80211_radiotap_iterator_init() failed\n");
		return;
	}

	pkt.size = pkt.len - parser.max_length;

	while ((n = ieee80211_radiotap_iterator_next(&parser)) == 0) {
		switch (parser.this_arg_index) {
			case IEEE80211_RADIOTAP_TSFT:
				pkt.radio.tsft = le64toh(*((uint64_t *) parser.this_arg));
				break;

			case IEEE80211_RADIOTAP_FLAGS:
				pkt.radio.flags.val = *parser.this_arg;

				if (pkt.radio.flags.val & IEEE80211_RADIOTAP_F_CFP) {
					pkt.radio.flags.cfp = true;
					mgstats_db_count(interface->stats, "rcv_cfp");
				}
				if (pkt.radio.flags.val & IEEE80211_RADIOTAP_F_SHORTPRE) {
					pkt.radio.flags.shortpre = true;
					mgstats_db_count(interface->stats, "rcv_shortpre");
				}
				if (pkt.radio.flags.val & IEEE80211_RADIOTAP_F_FRAG) {
					pkt.radio.flags.frag = true;
					mgstats_db_count(interface->stats, "rcv_frag");
				}
				if (pkt.radio.flags.val & IEEE80211_RADIOTAP_F_BADFCS) {
					pkt.radio.flags.badfcs = true;
					mgstats_db_count(interface->stats, "rcv_badfcs");
				}
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

	/* loopback filter */
	if (pkt.radio.tsft == 0)
		return;

	mgstats_db_count(interface->stats, "rcv_all");
	mgstats_db_count_num(interface->stats, "rcv_all_bytes", pkt.size);

	if (pkt.radio.flags.badfcs) {
		dbg(9, "skipping bad FCS frame\n");
		return;
	}

	dbg(8, "frame: tsft=%llu rate=%u freq=%u rssi=%d size=%u\n",
		pkt.radio.tsft, pkt.radio.rate / 2, pkt.radio.freq, pkt.radio.rssi, pkt.size);

	/*
	 * parse IEEE 802.11 header
	 */
	if (pkt.size < PKT_IEEE80211_HDRSIZE) {
		if (pkt.size == PKT_IEEE80211_ACKSIZE) {
			mgstats_db_count(interface->stats, "rcv_ack");
		} else {
			dbg(1, "skipping invalid short frame (%d)\n", pkt.size);
			mgstats_db_count(interface->stats, "rcv_aliens");
		}

		return;
	}

	ieee80211_hdr = (uint8_t *) pkt.pkt + parser.max_length;
	pkt.dstid = ieee80211_hdr[9];
	pkt.srcid = ieee80211_hdr[15];

	/* skip non-data frames */
	if (ieee80211_hdr[0] != 0x08) {
		if (ieee80211_hdr[0] == 0x80) {
			mgstats_db_count(interface->stats, "rcv_beacons");
		} else {
			mgstats_db_count(interface->stats, "rcv_nondata");
		}

		return;
	}

	/* count ieee802.11 data retries */
	if (ieee80211_hdr[1] & 0x08)
		mgstats_db_count(interface->stats, "rcv_retry");

	/* skip invalid BSSID */
	if (!(ieee80211_hdr[16] == 0x06 &&
	      ieee80211_hdr[17] == 0xFE &&
	      ieee80211_hdr[18] == 0xEE &&
	      ieee80211_hdr[19] == 0xED &&
	      ieee80211_hdr[20] == 0xFF)) {
		dbg(9, "skipping invalid bssid frame\n");
		mgstats_db_count(interface->stats, "rcv_wrong_bssid");
		return;
	}

	/* skip cross-channel transmissions */
	if (!(ieee80211_hdr[21] == interface->num)) {
		dbg(9, "skipping cross-channel frame\n");
		mgstats_db_count(interface->stats, "rcv_wrong_channel");
		return;
	}

	/* drop frames not destined to us */
	if (pkt.dstid != interface->mg->options.myid) {
		dbg(9, "skipping not ours frame (%d)\n", pkt.dstid);
		mgstats_db_count(interface->stats, "rcv_wrong_dst");
		return;
	}

	/*
	 * parse mg header
	 */
	if (pkt.size < PKT_HEADERS_SIZE + PKT_IEEE80211_FCSSIZE + sizeof *mg_hdr) {
		dbg(11, "skipping short alien frame\n");
		mgstats_db_count(interface->stats, "rcv_aliens");
		return;
	}

	mg_hdr = (struct mg_hdr *) (pkt.pkt + parser.max_length + PKT_HEADERS_SIZE);
#define A(field) pkt.mg_hdr.field = ntohl(mg_hdr->field)
	A(mg_tag);
	A(time_s);
	A(time_us);
	A(line_num);
	A(line_ctr);
#undef A

	if (pkt.mg_hdr.mg_tag != MG_TAG_V1) {
		dbg(8, "skipping invalid mg tag alien frame (%x)\n", pkt.mg_hdr.mg_tag);
		mgstats_db_count(interface->stats, "rcv_aliens");
		return;
	}

	if (pkt.mg_hdr.line_num >= TRAFFIC_LINE_MAX) {
		dbg(1, "received too high line number (%d) - alien?\n", pkt.mg_hdr.line_num);
		mgstats_db_count(interface->stats, "rcv_aliens");
		return;
	}

	pkt.line = interface->mg->lines[pkt.mg_hdr.line_num];
	if (!pkt.line) {
		dbg(1, "received invalid line number (%d) - alien?\n", pkt.mg_hdr.line_num);
		mgstats_db_count(interface->stats, "rcv_aliens");
		return;
	}

	mgstats_db_count(interface->stats, "rcv_ok");
	mgstats_db_count_num(interface->stats, "rcv_ok_bytes", pkt.size);

	/* store time of last frame destined to us */
	gettimeofday(&interface->mg->last, NULL);

	/* handle duplicates - dont drop - may be needed for stats */
	n = pkt.mg_hdr.line_ctr - pkt.line->line_ctr_rcv;
	if (n == 1) {
		mgstats_db_count(pkt.line->linkstats, "rcv_ok");
		mgstats_db_count_num(pkt.line->linkstats, "rcv_ok_bytes", pkt.size);

		mgstats_db_count(pkt.line->stats, "rcv_ok");
		mgstats_db_count_num(pkt.line->stats, "rcv_ok_bytes", pkt.size);
	} else if (n < 1) {
		pkt.dupe = 1;

		mgstats_db_count(pkt.line->linkstats, "rcv_dup");
		mgstats_db_count_num(pkt.line->linkstats, "rcv_dup_bytes", pkt.size);

		mgstats_db_count(pkt.line->stats, "rcv_dup");
		mgstats_db_count_num(pkt.line->stats, "rcv_dup_bytes", pkt.size);
	} else { /* n > 1 */
		mgstats_db_count_num(pkt.line->linkstats, "rcv_lost", n - 1);
		mgstats_db_count_num(pkt.line->stats, "rcv_lost", n - 1);
	}

	mgstats_db_ewma(pkt.line->linkstats, "rssi", LINK_EWMA_N, pkt.radio.rssi);
	mgstats_db_ewma(pkt.line->linkstats, "rate", LINK_EWMA_N, pkt.radio.rate / 2.0);
	mgstats_db_ewma(pkt.line->linkstats, "antnum", LINK_EWMA_N, pkt.radio.antnum);

	pkt.payload = (uint8_t *) mg_hdr + sizeof *mg_hdr;
	pkt.paylen  = pkt.size - PKT_HEADERS_SIZE - PKT_IEEE80211_FCSSIZE;

	/* pass to higher layers */
	interface->mg->packet_cb(&pkt);

	pkt.line->line_ctr_rcv = pkt.mg_hdr.line_ctr;
}

int mgi_init(struct mg *mg, mgi_packet_cb cb)
{
	struct sockaddr_ll ll;
	int count = 0;
	int fd;
	char name[256];

	memset(&ll, 0, sizeof ll);
	ll.sll_family = AF_PACKET;

	mg->packet_cb = cb;

	/* open PF_PACKET raw sockets on interfaces */
	for (int i = 0; i < IFINDEX_MAX; i++) {
		snprintf(name, sizeof name, IFNAME_FMT, i);

		ll.sll_ifindex = if_nametoindex(name);
		if (ll.sll_ifindex == 0)
			continue;

		fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
		if (fd < 0)
			continue;

		if (bind(fd, (struct sockaddr *) &ll, sizeof ll) < 0) {
			close(fd);
			continue;
		}

		dbg(1, "bound to %s\n", name);
		count++;

		mg->interface[i].mg = mg;
		mg->interface[i].num = i;
		mg->interface[i].fd = fd;
		mg->interface[i].stats = mgstats_db_create(mg);
		mg->interface[i].linkstats_root = thash_create_strkey(NULL, mg->mm);

		/* monitor for incoming packets */
		event_set(&mg->interface[i].evread,
			fd, EV_READ | EV_PERSIST, _mgi_sniff, &mg->interface[i]);

		event_add(&mg->interface[i].evread, NULL);

		/* interface stats writer */
		mgstats_writer_add(mg, _stats_write_interface, &mg->interface[i],
			name, "interface.txt",
			"snt_ok",
			"snt_ok_bytes",
			"snt_time",

			"rcv_all",
			"rcv_all_bytes",
			"rcv_cfp",
			"rcv_shortpre",
			"rcv_frag",
			"rcv_badfcs",
			"rcv_beacons",
			"rcv_ack",
			"rcv_nondata",
			"rcv_retry",
			"rcv_wrong_bssid",
			"rcv_wrong_channel",
			"rcv_wrong_dst",
			"rcv_aliens",
			"rcv_ok",
			"rcv_ok_bytes",

			NULL);
	}

	return count;
}

ut *mgi_linkstats_get(struct interface *interface, uint8_t srcid, uint8_t dstid)
{
	ut *stats;
	char key[64], ifname[64], filename[64];

	if (dstid != interface->mg->options.myid)
		return NULL;

	snprintf(key, sizeof key, "%u-%u", srcid, dstid);
	stats = thash_get(interface->linkstats_root, key);

	if (!stats) {
		stats = ut_new_utthash(NULL, interface->mg->mm);
		thash_set(interface->linkstats_root, key, stats);

		snprintf(ifname, sizeof ifname, IFNAME_FMT, interface->num);
		snprintf(filename, sizeof filename, "link-%u-%u.txt", srcid, dstid);
		mgstats_writer_add(interface->mg, _stats_write_link, stats,
			ifname, filename,
			"rcv_ok",
			"rcv_ok_bytes",
			"rcv_dup",
			"rcv_dup_bytes",
			"rcv_lost",
			"rssi",
			"rate",
			"antnum",
			NULL);
	}

	return stats;
}
