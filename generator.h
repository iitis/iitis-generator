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
#define PKTSIZE 1600

struct mg;

struct interface {
	struct mg *mg;          /** root */
	int fd;                /** raw interface socket */
	struct event *evread;  /** read event */
};

struct mg {
	mmatic *mm;                /** global memory manager */
	mmatic *mmtmp;             /** mm that can be freed anytime in main() */
	struct event_base *evb;    /** libevent base */

	struct {
		const char *traf_file; /** traffic file path */
	} options;

	struct interface interface[IFINDEX_MAX];
};

/** Reverse bits (http://graphics.stanford.edu/~seander/bithacks.html#BitReverseTable) */
extern const uint8_t REVERSE[256];
#define REV8(a)   REVERSE[(a) & 0xff]
#define REV16(a)  ((REV8((a) >> 8) << 8) | REV8(a))

#endif
