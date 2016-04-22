/*
 * rmaops_dist.c
 *
 *  Created on: 22 Apr 2016
 *      Author: nick
 */

#include "mpi.h"
#include "gvirtual.h"
#include "distmem_mpi.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
  int myrank, size;

  MPI_Init(&argc, &argv);
  gv_initialise();

  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  int* dist_data = (int*)distmem_mpi_malloc(DISTRIBUTEDHEAP_CONTIGUOUS_KIND, sizeof(int), 10 * size, MPI_COMM_WORLD);
  int i, j;
  for (i = 0; i < 10; i++) {
    dist_data[i] = myrank * i;
  }
  MPI_Barrier(MPI_COMM_WORLD);
  int sumTotal = 0;
  if (myrank == 0) {
    for (i = 0; i < size; i++) {
      int* cachedRemoteData;
      if (i == 0) {
        cachedRemoteData = dist_data;
      } else {
        cachedRemoteData = (int*)gv_acquireMutable(&dist_data[i * 10], sizeof(int) * 10);
      }
      for (j = 0; j < 10; j++) {
        sumTotal += cachedRemoteData[j];
        cachedRemoteData[j] = (100 * i) + j;
      }
      if (i != 0) gv_commitMakeConst(&dist_data[i * 10]);
    }
    printf("[%d] Total sum of all values is %d\n", myrank, sumTotal);
  }
  MPI_Barrier(MPI_COMM_WORLD);
  sumTotal = 0;
  for (i = 0; i < 10; i++) {
    sumTotal += dist_data[i];
  }
  printf("[%d] Modified total is %d\n", myrank, sumTotal);

  memkind_free(DISTRIBUTEDHEAP_CONTIGUOUS_KIND, dist_data);
  memkind_finalize();
  MPI_Finalize();
  return 0;
}
