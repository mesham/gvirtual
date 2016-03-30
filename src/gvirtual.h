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

#define LOCAL_HEAP_SIZE 4194304
#define GLOBAL_HEAP_BASE_ADDRESS 0x11111000
#define GLOBAL_MAX_ADDRESS 0xFF00000

void initialise_global_virtual_address_space();

#endif /* SRC_GVIRTUAL_H_ */
