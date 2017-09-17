#ifndef WORK_WITH_SLABS_H
#define WORK_WITH_SLABS_H

#include <cstdio>
#include <mutex>
#include <atomic>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>
#include <cassert>

void init_slab_allocation();

void *alloc_block_in_slab(size_t size);

void free_block_in_slab(void *ptr);

void *realloc_block_in_slab(void *ptr, size_t new_size);

bool is_allocated_by_slab(void * ptr);

#endif
