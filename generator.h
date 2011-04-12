/*
 * ath9k monitor mode packet generator
 * Paweł Foremski <pforemski@gmail.com> 2011
 * IITiS PAN Gliwice
 */

#ifndef _GENERATOR_H_
#define _GENERATOR_H_

#include <libpjf/lib.h>
#include <event.h>

#define GENERATOR_VER "0.1"

#define IFNAME_FMT "mon%d"
#define IFINDEX_MAX 8
#define PKT_BUFSIZE 2000

/** EtherType for generated packets */
#define PKT_ETHERTYPE 0x0111

struct mg;
struct interface;
struct sniff_pkt;

#include "interface.h"

struct interface {
	struct mg *mg;             /** root */
	int num;                   /** interface number */
	int fd;                    /** raw interface socket */
	struct event *evread;      /** read event */
};

struct mg {
	mmatic *mm;                /** global memory manager */
	mmatic *mmtmp;             /** mm that can be freed anytime in main() */
	struct event_base *evb;    /** libevent base */

	uint8_t myid;             /** my id number */

	struct {
		const char *traf_file; /** traffic file path */
	} options;

	/* interface.c */
	struct interface interface[IFINDEX_MAX];
	mgi_packet_cb packet_cb;
};

/* data format */
struct mg_hdr {
#define MG_TAG_V1 0xFEEEED01
	uint32_t mg_tag;           /** mg protocol tag */
	uint32_t time_s;           /** local time: seconds */
	uint32_t time_us;          /** local time: microseconds */
	uint32_t line_num;         /** configuration file line number */
	uint32_t line_ctr;         /** counter inside this single line */
};

struct sniff_pkt {
	struct interface *interface;  /** interface packet arrived on */
	uint8_t pkt[PKT_BUFSIZE];     /** raw frame */

	struct {
		uint64_t tsft;            /** time [us] */
		struct {
			uint8_t val;          /** radiotap flags value */
			bool    cfp;          /** frame received during Contention-Free Period */
			bool    shortpre;     /** short preamble */
			bool    frag;         /** fragmented */
			bool    badfcs;       /** bad FCS */
		} flags;
		uint8_t  rate;            /** bitrate in 0.5 Mbps */
		uint16_t freq;            /** frequency in MHz */
		int8_t   rssi;            /** signal strength in dBm */
		uint8_t  antnum;          /** antenna number */
	} radio;

	struct timeval timestamp;     /** local frame timestamp */
	uint8_t srcid;                /** source id */
	uint8_t dstid;                /** destination id */
	uint16_t size;                /** total packet size (without radiotap) */
	struct mg_hdr mg_hdr;         /** CPU-endian mg header */
};

/** Reverse bits (http://graphics.stanford.edu/~seander/bithacks.html#BitReverseTable) */
extern const uint8_t REVERSE[256];
#define REV8(a)   REVERSE[(a) & 0xff]
#define REV16(a)  ((REV8((a) >> 8) << 8) | REV8(a))

#endif
