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

void initialiseGlobalVirtualAddressSpace();
struct global_address_space_descriptor getGlobalVirtualAddressSpaceDescription();

#endif /* SRC_GVIRTUAL_H_ */
