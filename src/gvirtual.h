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

static const unsigned long GLOBAL_ADDRESS_SPACE_SIZE = 64ul * 1024ul * 1024ul * 1024ul;
static const unsigned long LOCAL_HEAP_SIZE = 64ul * 1024ul * 1024ul;

void initialise_global_virtual_address_space();

#endif /* SRC_GVIRTUAL_H_ */
