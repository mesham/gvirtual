/*
 * directory.c
 *
 *  Created on: 30 Mar 2016
 *      Author: nick
 */

#include <stdlib.h>
#include <mpi.h>
#include <stdint.h>
#include <memkind.h>
#include "directory.h"

struct memory_element_structure {
  void* startAddress, *endAddress;
  int homeNode;
  unsigned long uuid;
  struct memory_element_structure* next, *prev;
};

struct memory_element_structure* root = NULL;

static struct memory_element_structure* getMemoryElementByAddress(void*);

void registerMemory(void* address, size_t numberElements, int homeNode) {
  registerMemoryStartEnd(address, (void*)address + numberElements, homeNode, 0);
}

void registerMemoryStartEnd(void* startAddress, void* endAddress, int homeNode, unsigned long uuid) {
  struct memory_element_structure* newEntry =
      (struct memory_element_structure*)memkind_malloc(MEMKIND_DEFAULT, sizeof(struct memory_element_structure));
  newEntry->startAddress = startAddress;
  newEntry->endAddress = endAddress;
  newEntry->homeNode = homeNode;
  newEntry->next = root;
  newEntry->prev = NULL;
  newEntry->uuid = uuid;
  if (root != NULL) root->prev = newEntry;
  root = newEntry;
}

int getHomeNode(void* address) {
  struct memory_element_structure* specificMemoryItem = getMemoryElementByAddress(address);
  if (specificMemoryItem == NULL) return -1;
  return specificMemoryItem->homeNode;
}

void removeMemoryByAddress(void* address) {
  struct memory_element_structure* specificMemoryItem = getMemoryElementByAddress(address);
  if (specificMemoryItem != NULL) {
    if (specificMemoryItem->next != NULL) specificMemoryItem->next->prev = specificMemoryItem->prev;
    if (specificMemoryItem->prev != NULL) specificMemoryItem->prev->next = specificMemoryItem->next;
    if (specificMemoryItem == root) root = specificMemoryItem->next;
    free(specificMemoryItem);
  }
}

void removeAllMemoriesByUUID(unsigned long uuid) {
  struct memory_element_structure* head = root, *prev_head = NULL;
  while (head != NULL) {
    if (head->uuid == uuid) {
      if (head->prev != NULL) head->prev->next = head->next;
      if (head->next != NULL) head->next->prev = head->prev;
      if (head == root) root = head->next;
      prev_head = head;
    }
    head = head->next;
    if (prev_head != NULL) {
      memkind_free(MEMKIND_DEFAULT, prev_head);
      prev_head = NULL;
    }
  }
}

static struct memory_element_structure* getMemoryElementByAddress(void* address) {
  struct memory_element_structure* head = root;
  while (head != NULL) {
    if (address >= head->startAddress && address <= head->endAddress) {
      return head;
    }
    head = head->next;
  }
  return NULL;
}
