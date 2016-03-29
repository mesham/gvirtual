/*
 * gvirtual.c
 *
 *  Created on: 29 Mar 2016
 *      Author: nick
 */

#include "gvirtual.h"
#include "distmem_mpi.h"

void initialise_global_virtual_address_space() {
  initialise_local_heap_space();
  distmem_mpi_init();
}
