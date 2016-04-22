/*
 * cache.h
 *
 *  Created on: 15 Apr 2016
 *      Author: nick
 */

#ifndef CACHE_H_
#define CACHE_H_

#include <memkind.h>
#include "mpi.h"
#include "distributedheap.h"

extern memkind_t INTERNAL_LOCALCACHE_KIND;

static const unsigned long LOCAL_CACHE_SIZE = 16ul * 1024ul * 1024ul;

void gvi_cache_init();
void gvi_cache_registerLocalHeap(MPI_Win, void*, unsigned long*, int);
void gvi_cache_registerDistributedHeapMemory(void*, size_t);
void* gvi_cache_retrieveData(void*, size_t, int);
void gvi_cache_commitData(void*, int);

#endif /* CACHE_H_ */
