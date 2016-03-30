/*
 * directory.h
 *
 *  Created on: 30 Mar 2016
 *      Author: nick
 */

#ifndef DIRECTORY_H_
#define DIRECTORY_H_

void registerLocalMemory(void*, void*, size_t, int);
void registerRemoteMemory(void*, size_t, int);
void removeMemoryByLocalAddress(void*);
void removeMemoryByGlobalAddress(void*);
int getHomeNode(void*);
void* getGlobalAddress(void*);
void* getLocalAddress(void*);

#endif /* DIRECTORY_H_ */
