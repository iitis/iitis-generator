#ifndef _INTERFACE_H_
#define _INTERFACE_H_

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
 * @param interface  interface to inject frame on
 * @param bssid      BSSID
 * @param dst        destination MAC
 * @param src        source MAC
 * @param ether_type ethernet type, set 0x0800 for IPv4
 * @param data       raw link-layer frame
 * @param len        frame length
 * @return           number of bytes sent to socket
 * @retval -1        sendmsg() failed
 */
int mgi_inject(struct interface *interface,
	struct ether_addr *bssid, struct ether_addr *dst, struct ether_addr *src,
	uint16_t ether_type, void *data, size_t len);

/** Send mg frame
 * A high-level interface to mgi_inject(). Payload is constructed from contents of configuration
 * file line.
 * @param interface  interface to send on
 * @param dst        destination node
 * @param line_num   configuration file line number
 * @param size       desired total frame length in air, incl. all headers
 */
void mgi_send(struct interface *interface,
	uint8_t dst, uint32_t line_num, int size);

/** Incoming frame callback type
 * @param pkt        captured frame
 */
typedef void (*mgi_packet_cb)(struct sniff_pkt *pkt);

/** Set incoming frame callback
 * @param cb         callback function
 */
void mgi_set_callback(struct mg *mg, mgi_packet_cb cb);

#endif
