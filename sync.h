#ifndef _SYNC_H_
#define _SYNC_H_

#include "generator.h"

/** Synchronize all nodes
 * Source of first traffic line sends broadcast UDP to 255.255.255.255 */
void mgc_sync(struct mg *mg);

#endif
