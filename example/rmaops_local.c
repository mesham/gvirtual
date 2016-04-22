/*
 * rmaops.c
 *
 *  Created on: 15 Apr 2016
 *      Author: nick
 */

#include "mpi.h"
#include "gvirtual.h"
#include "distmem_mpi.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
  MPI_Init(&argc, &argv);

  gv_initialise();

  int myrank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  int* data = (int*)memkind_malloc(LOCALHEAP_KIND, sizeof(int) * 10);
  int i;
  for (i = 0; i < 10; i++) {
    data[i] = myrank;
  }
  MPI_Barrier(MPI_COMM_WORLD);  // To ensure the values are set before we copy into the cache
  if (myrank == 0) {
    unsigned long intAddress;
    // Recv the global virtual address of rank 1's local heap allocated int array
    MPI_Recv(&intAddress, 1, MPI_UNSIGNED_LONG, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    // Grab the remote data based upon its global virtual address space and return the location in the cache (part of my local heap)
    int* cachedRemoteData = (int*)gv_acquireMutable((void*)intAddress, sizeof(int) * 10);
    for (i = 0; i < 10; i++) {
      cachedRemoteData[i] *= 10;  // Modify the cached value
    }
    gv_commitMakeConst((void*)intAddress);  // Commit the cached value back to original location on rank 1
    gv_release((void*)intAddress);
    MPI_Barrier(MPI_COMM_WORLD);  // Ensure the updated values are committed before rank 1 does the display
  } else if (myrank == 1) {
    unsigned long intAddress = (unsigned long)data;
    MPI_Send(&intAddress, 1, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD);  // Send the global virtual address to rank 0
    MPI_Barrier(MPI_COMM_WORLD);  // Ensure the updated values are committed before I display the value
    printf("%d\n", data[0]);      // This will display the value updated by rank 0
  } else {
    MPI_Barrier(MPI_COMM_WORLD);
  }

  memkind_free(LOCALHEAP_KIND, data);
  memkind_finalize();
  MPI_Finalize();
  return 0;
}
