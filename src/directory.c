/*
 * directory.c
 *
 *  Created on: 30 Mar 2016
 *      Author: nick
 */

#include <stdlib.h>
#include <mpi.h>
#include <stdint.h>

struct memory_element_structure {
  uintptr_t globalAddress, localAddress;
  size_t numberElements;
  int homeNode;
  struct memory_element_structure* next, *prev;
};

struct memory_element_structure* root = NULL;

static struct memory_element_structure* getMemoryElementFromGlobalAddress(void*);
static struct memory_element_structure* getMemoryElementFromLocalAddress(void*);

void registerLocalMemory(void* globalAddress, void* localAddress, size_t numberElements, int homeNode) {
  struct memory_element_structure* newEntry = (struct memory_element_structure*)malloc(sizeof(struct memory_element_structure));
  newEntry->globalAddress = (uintptr_t)globalAddress;
  newEntry->localAddress = (uintptr_t)localAddress;
  newEntry->numberElements = numberElements;
  newEntry->homeNode = homeNode;
  newEntry->next = root;
  newEntry->prev = NULL;
  if (root != NULL) root->prev = newEntry;
  root = newEntry;
}

void registerRemoteMemory(void* globalAddress, size_t numberElements, int homeNode) {
  registerLocalMemory(globalAddress, NULL, numberElements, homeNode);
}

int getHomeNode(void* globalAddress) {
  struct memory_element_structure* specificMemoryItem = getMemoryElementFromGlobalAddress(globalAddress);
  if (specificMemoryItem == NULL) return -1;
  return specificMemoryItem->homeNode;
}

void removeMemory(void* localAddress) {
  struct memory_element_structure* specificMemoryItem = getMemoryElementFromLocalAddress(localAddress);
  if (specificMemoryItem != NULL) {
    if (specificMemoryItem->next != NULL) specificMemoryItem->next->prev = specificMemoryItem->prev;
    if (specificMemoryItem->prev != NULL) specificMemoryItem->prev->next = specificMemoryItem->next;
    if (specificMemoryItem == root) root = specificMemoryItem->next;
    free(specificMemoryItem);
  }
}

void* getLocalAddress(void* globalAddress) {
  struct memory_element_structure* specificMemoryItem = getMemoryElementFromGlobalAddress(globalAddress);
  if (specificMemoryItem == NULL) return NULL;
  return specificMemoryItem->localAddress;
}

void* getGlobalAddress(void* localAddress) {
  struct memory_element_structure* specificMemoryItem = getMemoryElementFromLocalAddress(localAddress);
  if (specificMemoryItem == NULL) return NULL;
  return specificMemoryItem->globalAddress;
}

static struct memory_element_structure* getMemoryElementFromGlobalAddress(void* globalAddress) {
  struct memory_element_structure* head = root;
  while (head != NULL) {
    if (head->globalAddress == globalAddress) return head;
    head = head->next;
  }
  return NULL;
}

static struct memory_element_structure* getMemoryElementFromLocalAddress(void* localAddress) {
  struct memory_element_structure* head = root;
  while (head != NULL) {
    if (head->localAddress == (uintptr_t)localAddress) return head;
    head = head->next;
  }
  return NULL;
}
