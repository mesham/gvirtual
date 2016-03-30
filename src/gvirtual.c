/*
 * gvirtual.c
 *
 *  Created on: 29 Mar 2016
 *      Author: nick
 */

#include "gvirtual.h"
#include "distmem_mpi.h"
#include <mpi.h>

static int myRank, totalRanks;
static void *globalDistributedMemoryHeapBottom;

void initialise_global_virtual_address_space() {
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
  MPI_Comm_size(MPI_COMM_WORLD, &totalRanks);
  distmem_mpi_init();
  initialise_local_heap_space(myRank, totalRanks);
  globalDistributedMemoryHeapBottom = (void *)GLOBAL_HEAP_BASE_ADDRESS + ((totalRanks + 1) * LOCAL_HEAP_SIZE);
}

int getHomeNode(void *globalAddress) {
  if (getLocalAddress(globalAddress) != NULL) {
    return myRank;
  } else {
    if (globalAddress < globalDistributedMemoryHeapBottom) {
      return (globalAddress - GLOBAL_HEAP_BASE_ADDRESS) / LOCAL_HEAP_SIZE;
    }
  }
  return -1;
}
