/*
 * localheap.h
 *
 *  Created on: 29 Mar 2016
 *      Author: nick
 */

#ifndef SRC_LOCALHEAP_H_
#define SRC_LOCALHEAP_H_

#include "distmem.h"
#include <memkind.h>

extern memkind_t LOCALHEAP_KIND;
void* initialise_local_heap_space(int, int, void*);

#endif /* SRC_LOCALHEAP_H_ */
