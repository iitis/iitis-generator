/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>

#include "generator.h"

/** Initialize generator data
 * @param cb  handler for incoming frames destined to this node
 * @return number of successfully initialized interfaces */
int mgi_init(struct mg *mg, mgi_packet_cb cb);

/** Inject frame
 * @param interface  interface to inject frame on
 * @param bssid      BSSID
 * @param dst        destination MAC
 * @param src        source MAC
 * @param rate       bitrate [in 0.5Mbps]
 * @param ether_type ethernet type, set 0x0800 for IPv4
 * @param data       raw link-layer frame
 * @param len        frame length
 * @return           number of bytes sent to socket
 * @retval -1        sendmsg() failed
 */
int mgi_inject(struct interface *interface,
	struct ether_addr *bssid, struct ether_addr *dst, struct ether_addr *src, uint8_t rate,
	uint16_t ether_type, void *data, size_t len);

/** Send mg frame
 * A high-level interface to mgi_inject(). If payload is NULL or not long enough, its constructed
 * from contents of configuration file line.
 * @param line         traffic file line
 * @param payload      payload, may be NULL
 * @param payload_size bytes available under payload
 * @param size         desired total frame length in air, including all headers,
 *                     that is PKT_TOTAL_OVERHEAD */
void mgi_send(struct line *line, uint8_t *payload, int payload_size, int size);

#endif
