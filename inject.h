#ifndef _INJECT_H_
#define _INJECT_H_

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>

#include "generator.h"

/** Initialize generator data
 * @return number of successfully initialized interfaces */
int mgi_init(struct mg *mg);

/** Inject frame
 * @param ifdx     output interface index
 * @param bssid    BSSID
 * @param dst      destination MAC
 * @param src      source MAC
 * @param data     raw link-layer frame
 * @param len      frame length
 * @return         number of bytes sent to socket
 * @retval -1      sendmsg() failed
 * @note           not thread-safe
 */
int mgi_inject(struct mg *mg, int ifidx,
	struct ether_addr *bssid, struct ether_addr *dst, struct ether_addr *src,
	void *data, size_t len);

#endif
