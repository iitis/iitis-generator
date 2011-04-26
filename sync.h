#ifndef _SYNC_H_
#define _SYNC_H_

#include "generator.h"

/** Synchronize all nodes
 * Source of first traffic line sends broadcast UDP to 255.255.255.255 */
void mgc_sync(struct mg *mg);

/** Send sync packet */
void mgc_unlock(struct mg *mg, uint8_t key);

/** Wait for sync packet */
void mgc_lock(struct mg *mg, uint8_t key);

#endif
