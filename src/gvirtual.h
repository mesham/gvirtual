/*
 * gvirtual.h
 *
 *  Created on: 29 Mar 2016
 *      Author: nick
 */

#ifndef SRC_GVIRTUAL_H_
#define SRC_GVIRTUAL_H_

#include "distmem.h"
#include <memkind.h>
#include "localheap.h"

struct global_address_space_descriptor {
  void *globalAddressSpaceStart, *globalAddressSpaceEnd, *localHeapGlobalAddressStart, *distributedMemoryHeapGlobalAddressStart;
};

static const unsigned long GLOBAL_ADDRESS_SPACE_SIZE = 64ul * 1024ul * 1024ul * 1024ul;
static const unsigned long LOCAL_HEAP_SIZE = 64ul * 1024ul * 1024ul;

extern memkind_t LOCALHEAP_KIND;
extern memkind_t DISTRIBUTEDHEAP_CONTIGUOUS_KIND;

void gv_initialise();
struct global_address_space_descriptor gv_getAddressSpaceDescription();
int gv_getHomeNode(void *);

#endif /* SRC_GVIRTUAL_H_ */
