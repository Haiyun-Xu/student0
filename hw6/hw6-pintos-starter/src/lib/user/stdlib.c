/**
 * This module contains the heap memory allocation functions.
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include <stdlib.h>

static struct list MM_LIST;
static uint32_t HEADER_SIZE = sizeof(struct mm_header);
static bool MM_LIST_INITIALIZED = false;

/**
 * Initialize configurations used by this file.
 */
static void init_configs(void) {
  if (!MM_LIST_INITIALIZED) {
    list_init(&MM_LIST);
    MM_LIST_INITIALIZED = true;
  }
}

/**
 * Find the first data block that is large enough for the request.
 * 
 * @param listElem The list element enclosed in the first data block
 * @param size The size of the requested data block
 * 
 * @return The list element enclosed in the first sufficiently large data
 *         block, or NULL if no existing data block is large enough
 */
static struct list_elem *find_first_fit(struct list_elem *listElem, size_t size) {
  // edge cases
  if (listElem == NULL) {
    return NULL;
  }

  size_t minimumSplitSize = size + HEADER_SIZE;
  struct list_elem *listHead = list_head(&MM_LIST), *listTail = list_tail(&MM_LIST);

  while (listElem != listHead && listElem != listTail) {
    struct mm_header *header = list_entry(listElem, struct mm_header, listElem);
    /*
     * if the current data block is free and is exactly the size requested, or
     * if it's large enough to contain both the requested data block plus a new
     * header, then it can be re-used
     */
    if (header->isFree && (header->size == size || header->size >= minimumSplitSize)) {
      return listElem;
    } else {
      listElem = list_next(listElem);
    }
  }

  // none of the existing data blocks is large enough
  return NULL;
}

/**
 * Allocates a data block of the requested size, and return the address of
 * the first byte. Returns NULL if the data block cannot be allocated.
 * 
 * @param size The size of the requested data block
 * 
 * @return The address of the first byte of the data block, or NULL if the
 *         data block cannot be allocated
 */
void* malloc(size_t size) {
  init_configs();
  // edge cases
  if (size == 0) {
    return NULL;
  }

  struct list_elem *listElem = list_begin(&MM_LIST);
  listElem = find_first_fit(listElem, size);
  void *dataBlock = NULL;

  if (listElem == NULL) {
    /*
     * if no data block is returned, then a new block must be allocated
     */
    struct mm_header *newHeader = (struct mm_header *) sbrk(HEADER_SIZE + size);
    // return NULL if unable to expand the heap segment
    if (newHeader == (struct mm_header *) -1) {
      return NULL;
    } else {
      newHeader->size = size;
      newHeader->isFree = false;
      // the heap is contiguous, so the new block must be appended to the list
      list_push_back(&MM_LIST, &(newHeader->listElem));
      
      dataBlock = (void *) ((uint8_t *)newHeader + HEADER_SIZE);
    }
    
  } else {
    struct mm_header *header = list_entry(listElem, struct mm_header, listElem);
    if (header->size == size) {
      /*
       * if the returned data block is exactly the right size, it can be
       * returned right away
       */
      dataBlock = (void *) ((uint8_t *)header + HEADER_SIZE);
      header->isFree = false;
    } else {
      dataBlock = (void *) ((uint8_t *)header + HEADER_SIZE);

      /*
       * otherwise, the returned data block is large enough for the request
       * plus a new header, so split the data block into two, and add the
       * new block into the list
       */
      struct mm_header *newHeader = (struct mm_header *) ((uint8_t *)dataBlock + size);
      newHeader->size = header->size - size - HEADER_SIZE;
      newHeader->isFree = true;
      list_insert(list_next(listElem), &(newHeader->listElem));

      header->size = size;
      header->isFree = false;
    }
  }
  
  memset(dataBlock, 0, size);
  return dataBlock;
}

/**
 * Allocates a data block that can contain a number of structures. Returns NULL
 * if the data block cannot be allocated.
 * 
 * @param number The number of structures within the data block
 * @param size   The size of each structure within the data block
 */
void* calloc (size_t number, size_t size) {
  return malloc(number * size);
}

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
void* realloc(void* ptr, size_t size) {
  init_configs();
  /*
   * edge cases: if ptr!=NULL and size=0, then it's equivalent to calling
   * free(ptr); else if ptr=NULL and size=0, then return NULL right away;
   * else if ptr=NULL and size!=0, then it's equivalent to calling
   * alloc(size)
   */
  if (size == 0) {
    free(ptr);
    return NULL;
  } else if (ptr == NULL) {
    return malloc(size);
  }

  void *newDataBlock = malloc(size);
  if (newDataBlock == NULL) {
    /*
     * if the new block cannot be allocated, return NULL without modifying
     * the original block
     */
    return NULL;
  }

  /*
   * otherwise, copy a number of bytes from the old block to the new block,
   * until either all of the old block is copied or all of the new block is
   * filled. Then free the old block, and return the new block.
   */
  struct mm_header *oldHeader = (struct mm_header *) ((uint8_t *)ptr - HEADER_SIZE);
  int bytesToCopy = oldHeader->size < size ? oldHeader->size : size;
  memcpy(newDataBlock, ptr, bytesToCopy);
  free(ptr);
  return newDataBlock;
}

/**
 * Free the given data block.
 * 
 * @param ptr The address of the first byte of the data block
 */
void free(void* ptr) {
  init_configs();
  // edge cases
  if (ptr == NULL) {
    return;
  }

  // retrieve the header of the freed block
  struct mm_header *currentHeader = (struct mm_header *) ((uint8_t *)ptr - HEADER_SIZE);
  currentHeader->isFree = true;
  struct list_elem *currentListElem = &(currentHeader->listElem);

  // if either of the two adjacent blocks are coalesceable, combine them
  struct list_elem *prevListElem = list_prev(currentListElem), *nextListElem = list_next(currentListElem);
  if (prevListElem != list_head(&MM_LIST)) {
    struct mm_header *prevHeader = list_entry(prevListElem, struct mm_header, listElem);
    if (prevHeader->isFree) {
      prevHeader->size += HEADER_SIZE + currentHeader->size;
      list_remove(currentListElem);
      currentListElem = prevListElem;
      currentHeader = prevHeader;
    }
  }
  if (nextListElem != list_tail(&MM_LIST)) {
    struct mm_header *nextHeader = list_entry(nextListElem, struct mm_header, listElem);
    if (nextHeader->isFree) {
      currentHeader->size += HEADER_SIZE + nextHeader->size;
      list_remove(nextListElem);
    }
  }

  return;
}
