#ifndef __LIB_USER_STDLIB_H
#define __LIB_USER_STDLIB_H

#include <list.h>
#include <string.h>
#include <syscall.h>

struct mm_header {
  struct list_elem listElem;
  bool isFree;
  uint32_t size;
  /*
   * the zero-length array is used to pad the enclosing structure, such
   * that the data block that follows the structure is memory-aligned
   */
  char padding[0];
};

/**
 * Allocates a data block of the requested size, and return the address of
 * the first byte. Returns NULL if the data block cannot be allocated.
 * 
 * @param size The size of the requested data block
 * 
 * @return The address of the first byte of the data block, or NULL if the
 *         data block cannot be allocated
 */
void* malloc(size_t size);

/**
 * Allocates a data block that can contain a number of structures. Returns NULL
 * if the data block cannot be allocated.
 * 
 * @param number The number of structures within the data block
 * @param size   The size of each structure within the data block
 */
void* calloc (size_t number, size_t size);

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
void* realloc(void* ptr, size_t size);

/**
 * Free the given data block.
 * 
 * @param ptr The address of the first byte of the data block
 */
void free(void* ptr);

#endif /* lib/user/stdlib.h */
