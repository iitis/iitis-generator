/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#ifndef _GENERATOR_H_
#define _GENERATOR_H_

#include <libpjf/lib.h>
#include <event.h>

#define GENERATOR_VER "0.1"

#define IFNAME_FMT "mon%d"
#define IFINDEX_MAX 8

/** max number of arguments to command in a traffic file line */
#define LINE_ARGS_MAX 8

/** size of a buffer for frames */
#define PKT_BUFSIZE 1600

/** EtherType for generated packets */
#define PKT_ETHERTYPE 0x0111

/** radiotap header size */
#define PKT_RADIOTAP_HDRSIZE 10

/** ieee802.11 header size (without FCS) */
#define PKT_IEEE80211_HDRSIZE 24

/** ieee802.11 ACK size (with FCS) */
#define PKT_IEEE80211_ACKSIZE 14

/** ieee802.11 footer size (FCS field) */
#define PKT_IEEE80211_FCSSIZE 4

/** link layer control header size */
#define PKT_LLC_HDRSIZE 8

/** Length of all frame headers "in the air" */
#define PKT_HEADERS_SIZE (PKT_IEEE80211_HDRSIZE + PKT_LLC_HDRSIZE)

/** Total packet overhead */
#define PKT_TOTAL_OVERHEAD (PKT_HEADERS_SIZE + PKT_IEEE80211_FCSSIZE + sizeof(struct mg_hdr))

/** Maximum number of lines in traffic file */
#define TRAFFIC_LINE_MAX 10000

/** Default output root directory */
#define DEFAULT_STATS_ROOT "./out"

/** Write statistics each 1 second by default */
#define DEFAULT_STATS_PERIOD 1

/** Forced disk sync() each 10 seconds by default */
#define DEFAULT_SYNC_PERIOD 10

/** Heartbeat period */
#define HEARTBEAT_PERIOD 1000000

/** Number of samples in link stats averages */
#define LINK_EWMA_N 10

struct mg;
struct interface;
struct sniff_pkt;
struct line;
struct schedule;

/** Scheduler info */
struct schedule {
	struct event ev;                 /** libevent handle */
	struct timeval last;             /** absolute time of last schedule request */
	bool lagging;                    /** true if task is lagging */
};

/** Traffic file line */
struct line {
	struct mg *mg;                   /** root */
	struct schedule schedule;        /** scheduler info */

	uint32_t line_num;               /** line number in traffic file */
	uint32_t line_ctr;               /** line counter for sending */
	uint32_t line_ctr_rcv;           /** line counter for receiving */

	const char *contents;            /** line contents */
	struct timeval tv;               /** time of first packet (time anchor) */
	struct interface *interface;     /** interface number */
	uint8_t srcid;                   /** src id */
	uint8_t dstid;                   /** dst id */
	uint8_t rate;                    /** bitrate [in 0.5Mbps] or 0 for "auto" */
	bool    noack;                   /** no ACK? */

	int argc;                        /** length of argv */
	const char *argv[LINE_ARGS_MAX]; /** arguments, argv[0] is command */

	const char *cmd;                 /** command name */
	void *prv;                       /** command private data */

	ut *stats;                       /** statistics db */
	ut *linkstats;                   /** link statistics db */
};

/** Represents network interface */
struct interface {
	struct mg *mg;             /** root */
	int num;                   /** interface number */
	int fd;                    /** raw interface socket */
	struct event evread;       /** read event */

	ut *stats;                 /** statistics */
	thash *linkstats_root;     /** link statistics dbs: "srcid-dstid" -> ut *linkstats */
};

/** A function which does statistics aggregation
 * @param stats  stats db to export
 * @param arg    arg passed to mgstats_writer_add()
 * @retval true  proceed, write ut to file
 * @retval false dont write ut to file
 */
typedef bool (*stats_writer_handler_t)(struct mg *mg, ut *stats, void *arg);

/** Represents process of aggregation of statistics from many single sources and writing them to a
 * statistics file */
struct stats_writer {
	stats_writer_handler_t handler;      /** callback handler */
	void *arg;                           /** argument to pass to handler */
	tlist *columns;                      /** column names to export */
	const char *dirname;                 /** optional directory under main stats dir */
	const char *filename;                /** stats file name */
	FILE *fh;                            /** open file handle */
};

/** Incoming frame callback type
 * @param pkt        captured frame */
typedef void (*mgi_packet_cb)(struct sniff_pkt *pkt);

/** Main data structure, root for all data */
struct mg {
	mmatic *mm;                /** global memory manager */
	mmatic *mmtmp;             /** mm that can be freed anytime in main() */

	struct event_base *evb;    /** libevent base */
	struct schedule hbs;       /** heartbeat schedule info */
	struct schedule syncs;     /** sync() schedule info */

	bool master;               /** true if this node is sync master */
	bool sender;               /** true if node has packets to send */
	bool receiver;             /** true if node should receive packets */

	bool synced;               /** true if origin is valid */
	struct timeval origin;     /** time origin (same on all nodes) */

	/** command line options */
	struct {
		uint8_t myid;           /** my id number */
		const char *traf_file;  /** traffic file path */
		uint32_t stats;         /** time between stats write [s] */
		const char *stats_root; /** stats root directory */
		const char *stats_name; /** stats directory */
		uint16_t sync;          /** sync time [s] */
	} options;

	/** interfaces - see interface.c */
	struct interface interface[IFINDEX_MAX];
	mgi_packet_cb packet_cb;   /** handler for incoming frames */

	/** traffic file lines (NB: sparse) */
	struct line *lines[TRAFFIC_LINE_MAX];

	/* hearbeat */
	int running;               /** number of still "running" lines */
	struct timeval last;       /** time of last frame destined to us */

	/* stats */
	struct event statsev;      /** stats write event */
	const char *stats_dir;     /** final stats dir path */
	tlist *stats_writers;      /** tlist of struct stats_writer */
};

/** mg frame format */
struct mg_hdr {
	uint32_t mg_tag;           /** mg protocol tag */
#define MG_TAG_V1 0xFEEEED01

	uint32_t time_s;           /** local time: seconds */
	uint32_t time_us;          /** local time: microseconds */
	uint32_t line_num;         /** traffic file line number */
	uint32_t line_ctr;         /** counter inside this single line */
};

/** Received packet info */
struct sniff_pkt {
	struct interface *interface;  /** interface packet arrived on */
	uint8_t pkt[PKT_BUFSIZE];     /** raw frame */
	bool dupe;                    /** if 1, its a duplicate */

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

	uint8_t *payload;             /** payload after mg header */
	uint32_t paylen;              /** payload length */

	struct line *line;            /** matching line */
};

/** message sent during time synchronization phase */
struct mg_sync_hdr {
	uint32_t code;                /** code */
#define MG_SYNC_OFFER 0x12340000
#define MG_SYNC_ACK   0xBAAABAAA

	uint8_t  node;                /** source node */
	uint32_t try;                 /** try number */
	uint32_t time_s;              /** start time: seconds */
	uint32_t time_us;             /** start time: microseconds */
};

/** structure used during synchronization phase */
struct mg_sync {
	struct mg *mg;                /** data root */
	struct mg_sync_hdr hdr;       /** last sync packet */
	int s;                        /** socket */

	struct event_base *evb;       /** event base */
	struct event evr;             /** read event */
	struct event evt;             /** timeout event */

	uint8_t node_min;             /** lowest node id */
	uint8_t node_max;             /** highest node id */
	uint32_t node_count;          /** number of nodes */
	uint8_t *exist;               /** 0 = nonexistent, 1 = exists */
	uint8_t *senders;             /** 1 = sends packets */
	uint8_t *receivers;           /** 1 = receives packets */
	uint8_t *acked;               /** 1 = node acked to time offer (used at master) */
};


/** Reverse bits (http://graphics.stanford.edu/~seander/bithacks.html#BitReverseTable) */
extern const uint8_t REVERSE[256];
#define REV8(a)   REVERSE[(a) & 0xff]
#define REV16(a)  ((REV8((a) >> 8) << 8) | REV8(a))

#endif
