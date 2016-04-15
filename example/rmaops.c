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

  if (myrank == 0) {
    unsigned long intAddress;
    MPI_Recv(&intAddress, 1, MPI_UNSIGNED_LONG, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    int* cachedRemoteData = (int*)gv_acquireMutable((void*)intAddress, sizeof(int) * 10);
    for (i = 0; i < 10; i++) {
      cachedRemoteData[i] *= 10;
    }
    gv_commitMakeConst((void*)intAddress);
    MPI_Barrier(MPI_COMM_WORLD);
  } else {
    unsigned long intAddress = (unsigned long)data;
    MPI_Send(&intAddress, 1, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);
    printf("%d\n", data[0]);
  }

  memkind_free(LOCALHEAP_KIND, data);
  memkind_finalize();
  return 0;
}
