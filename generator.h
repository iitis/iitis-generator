/*
 * ath9k monitor mode packet generator
 * Paweł Foremski <pforemski@gmail.com> 2011
 * IITiS PAN Gliwice
 */

#ifndef _GENERATOR_H_
#define _GENERATOR_H_

#include <libpjf/lib.h>

#define IFNAME_FMT "mon%d"
#define IFINDEX_MAX 8

struct mg {
	mmatic *mm;                /** global memory manager */
	mmatic *mmtmp;             /** mm that can be freed anytime in main() */

	struct {
		int fds[IFINDEX_MAX];  /** monitor sockets indexed by interface index */
	} inject;

};

#endif
