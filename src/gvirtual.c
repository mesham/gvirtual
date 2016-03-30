/*
 * gvirtual.c
 *
 *  Created on: 29 Mar 2016
 *      Author: nick
 */

#include "gvirtual.h"
#include "distmem_mpi.h"
#include "directory.h"
#include <mpi.h>
#include <stddef.h>

static int myRank, totalRanks;

void initialise_global_virtual_address_space() {
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
  MPI_Comm_size(MPI_COMM_WORLD, &totalRanks);
  distmem_mpi_init();
  initialise_local_heap_space(myRank, totalRanks);
  initialise_distributed_heap((void *)GLOBAL_HEAP_BASE_ADDRESS + ((totalRanks + 1) * LOCAL_HEAP_SIZE));
}
