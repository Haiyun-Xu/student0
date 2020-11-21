/**
 * This module contains memory management functions.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#pragma once

#ifndef MM_ALLOC_H
#define MM_ALLOC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "list.h"

struct mm_header {
  struct list_elem listElem;
  bool isFree;
  uint64_t size;
  /*
   * the zero-length array is used to pad the enclosing structure, such
   * that the data block that follows the structure is memory-aligned
   */
  char padding[0];
};
int HEADER_SIZE = sizeof (struct mm_header);

/**
 * Allocates a data block of the requested size, and return the address of
 * the first byte. Returns NULL if the data block cannot be allocated.
 * 
 * @param size The size of the requested data block
 * 
 * @return The address of the first byte of the data block, or NULL if the
 *         data block cannot be allocated
 */
void* mm_malloc(size_t size);

/**
 * Re-allocate a new data block of the requested size, containing the
 * given old data block at the front. If the new data block is smaller than
 * the old data block, the part of the old data block that exceeds the
 * capacity of the new data block will not be copied over.
 * 
 * @param ptr The address of the first byte of the old data block
 * @param size The size of the requested new data block
 * 
 * @return The address of the first byte of the data block, or NULL if the
 *         new data block cannot be allocated
 */
void* mm_realloc(void* ptr, size_t size);

/**
 * Free the given data block.
 * 
 * @param ptr The address of the first byte of the data block
 */
void mm_free(void* ptr);

#endif /* MM_ALLOC_H */
