#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
syscall_exit (int status)
{
  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  thread_exit ();
}

/*
 * This does not check that the buffer consists of only mapped pages; it merely
 * checks the buffer exists entirely below PHYS_BASE.
 */
static void
validate_buffer_in_user_region (const void* buffer, size_t length)
{
  uintptr_t delta = PHYS_BASE - buffer;
  if (!is_user_vaddr (buffer) || length > delta)
    syscall_exit (-1);
}

/*
 * This does not check that the string consists of only mapped pages; it merely
 * checks the string exists entirely below PHYS_BASE.
 */
static void
validate_string_in_user_region (const char* string)
{
  uintptr_t delta = PHYS_BASE - (const void*) string;
  if (!is_user_vaddr (string) || strnlen (string, delta) == delta)
    syscall_exit (-1);
}

/**
 * Unmap the pages from start_page to end_page (inclusive). The pages will first
 * be cleared from the page table, then freed into the page pool.
 * 
 * Note: start_page could equal to end_page.
 * 
 * @param start_page The user virtual address of the first page to remove
 * @param end_page   The user virtual address of the last page to remove
 */
static void
unmap_user_pages(void *start_page, void *end_page) {
  ASSERT(((uint32_t)start_page & PGMASK) == 0);
  ASSERT(((uint32_t)end_page & PGMASK) == 0);
  ASSERT((uint8_t *)start_page <= (uint8_t *)end_page);

  struct thread *t = thread_current();
  // for each page: get the kernel virtual address, clear it from the page table, and free the page
  for (int index = 0; (uint8_t *)((uint32_t)start_page + PGSIZE * index) <= (uint8_t *)end_page; index++) {
    void *current_page = (void *)((uint32_t)start_page + PGSIZE * index);
    void *kernel_page = pagedir_get_page(t->pagedir, current_page);
    if (kernel_page != NULL) {
      pagedir_clear_page(t->pagedir, current_page);
      palloc_free_page(kernel_page);
    }
  }
}

/**
 * Map the pages from start_page to end_page (inclusive). The pages will first
 * be allocated from the page pool, then added into the page table.
 * 
 * Note: start_page could equal tp end_page.
 * 
 * @param start_page The user virtual address of the first page to remove
 * @param end_page   The user virtual address of the last page to remove
 * 
 * @return True if successful, false otherwise
 */
static bool
map_user_pages(void *start_page, void *end_page) {
  ASSERT(((uint32_t)start_page & PGMASK) == 0);
  ASSERT(((uint32_t)end_page & PGMASK) == 0);
  ASSERT((uint8_t *)start_page <= (uint8_t *)end_page);

  struct thread *t = thread_current();
  // add pages from start_page to end_page (inclusive)
  for (int index = 0; (uint8_t *)((uint32_t)start_page + PGSIZE * index) <= (uint8_t *)end_page; index++) {
    uint8_t *current_page = (uint8_t *)((uint32_t)start_page + PGSIZE * index);

    void *kernel_page = palloc_get_page(PAL_USER|PAL_ZERO);
    // if no more page is available, rollback the transaction
    if (kernel_page == NULL) {
      unmap_user_pages(start_page, end_page);
      return false;
    }
    // if the page couldn't be mapped, rollback the transaction
    if (!pagedir_set_page(t->pagedir, (void *)current_page, kernel_page, true)) {
      unmap_user_pages(start_page, end_page);
      palloc_free_page(kernel_page);
      return false;
    }
  }

  return true;
}

static int
syscall_open (const char* filename)
{
  struct thread* t = thread_current ();
  if (t->open_file != NULL)
    return -1;

  t->open_file = filesys_open (filename);
  if (t->open_file == NULL)
    return -1;

  return 2;
}

static int
syscall_write (int fd, void* buffer, unsigned size)
{
  struct thread* t = thread_current ();
  if (fd == STDOUT_FILENO)
    {
      putbuf (buffer, size);
      return size;
    }
  else if (fd != 2 || t->open_file == NULL)
    return -1;

  return (int) file_write (t->open_file, buffer, size);
}

static int
syscall_read (int fd, void* buffer, unsigned size)
{
  struct thread* t = thread_current ();
  if (fd != 2 || t->open_file == NULL)
    return -1;

  return (int) file_read (t->open_file, buffer, size);
}

static void
syscall_close (int fd)
{
  struct thread* t = thread_current ();
  if (fd == 2 && t->open_file != NULL)
    {
      file_close (t->open_file);
      t->open_file = NULL;
    }
}

/**
 * Adjust the heap segment break by `increment` bytes, and return the updated
 * break address. If heap_start == heap_break, then the size of the heap segment
 * is 0 (at the beginning of a process, and if the process shrinks the heap to
 * a size of zero).
 * 
 * Note:
 * - "increment = 0" simply returns the current break addresss.
 * - if "increment" is negative, reduce the size of the heap segment. 
 * - if the user memory pool is exausted and palloc_get_page() fails, the segment
 *   break and page allocation state of the process remains unchanged, and -1 is
 *   returned;
 * - The heap needs to expand in multiple of pages, but sbrk() only grow by the
 *   increment specified. That leaves a gap between the increased break and the
 *   upper boundary of the newly allocated page. Accesses to this region should
 *   logically be invalid (because it belongs to the unmapped region), yet
 *   technically would not trigger any hardware error (because the page has been
 *   mapped). For this implementation, this scenario is acceptable;
 * 
 * @param increment The increment (in bytes) that should be applied
 *                  to the segment break
 * 
 * @return The new segment break address 
 */
static void *
syscall_sbrk(intptr_t increment) {
  struct thread *t = thread_current();
  
  // if there's no increment, return the current heap break
  if (increment == 0) {
    return t->heap_break;
  }
  // if the new_break falls below heap_start, return an error
  uint8_t *old_break = (uint8_t *) t->heap_break;
  uint8_t *new_break = (uint8_t *)((intptr_t)t->heap_break + increment);
  bool answer = (intptr_t)new_break < (intptr_t)t->heap_start;
  if (answer) {
    return (void *) -1;
  }

  /*
   * at this point, the increment is either negative or positive, and new_break
   * lies the region [heap_start, heap_break) U (heap_break, PHYS_BASE).
   * 
   * Note: either the old or new break could be sitting within a frame or on the
   * edge of a frame (i.e. the break is the page address). In that case, the
   * pageOf(break-1) should be allocated, but the pageOf(break) should not
   * (because the break address shouldn't be accessible).
   * 
   * Additionally, if old_break/new_break == heap_start, the page number
   * calculated here would belong to the data segment below
   */
  uint8_t *old_page = (uint8_t *)(((uint32_t)old_break - 1) & ~PGMASK);
  uint8_t *new_page = (uint8_t *)(((uint32_t)new_break - 1) & ~PGMASK);
  if (increment < 0) {
    /*
     * If the increment is negative, there are three cases:
     * 1) if new_break == heap_start, all pages in the range
     *    [pageOf(heap_start), pageOf(old_break-1)] should be deallocated;
     * 2) else if pageOf(new_break-1) == pageOf(old_break-1), no page needs to
     *    be deallocated;
     * 3) else if pageOf(new_break-1) != pageOf(old_break-1), all pages in the
     *    range [pageOf(new_break-1)+1, pageOf(old_break-1)] should be deallocated;
     * 
     * Note:
     * if new_break != heap_start and pageOf(new_break-1) != pageOf(old_break-1),
     * then there are four scenarios:
     * 1) both new_break and old_break are within the frame, in which case pages in
     *    [pageOf(new_break)+1/pageOf(new_break-1)+1, pageOf(old_break)/pageOf(old_break-1)]
     *    should be deallocated;
     * 2) only the new_break is on edge of a frame, in which case pages in
     *    [pageOf(new_break)/pageOf(new_break-1)+1, pageOf(old_break)/pageOf(old_break-1)]
     *    should be deallocated;
     * 3) only the old_break is on edge of a frame, in which case pages in
     *    [pageOf(new_break)+1/pageOf(new_break-1)+1, pageOf(old_break)-1/pageOf(old_break-1)] should
     *    be deallocated;
     * 4) both the old and new_breaks are on edge of frames, in which case pages in
     *    [pageOf(new_break)/pageOf(new_break-1)+1, pageOf(old_break)-1/pageOf(old_break-1)] should be
     *    deallocated;
     * The only range expression that takes care of all four scenarios is:
     * [pageOf(new_break-1)+1, pageOf(old_break-1)].
     */
    if (new_break == (uint8_t *)t->heap_start) {
      unmap_user_pages((void *)((uint32_t)t->heap_start & ~PGMASK), (void *)old_page);
    } else if (new_page != old_page) {
      unmap_user_pages((void *)((uint32_t)new_page + PGSIZE), (void *)old_page);
    }
  } else if (increment > 0) {
    /*
     * If the increment is positive, there are three cases as well:
     * 1) if heap_break == heap_start, all pages in the range
     *    [pageOf(heap_start), pageOf(new_break-1)] should be allocated;
     * 2) else if pageOf(old_break-1) == pageOf(new_break-1), no page needs to
     *    be allocated;
     * 3) else if pageOf(old-break-1) != pageOf(new_break-1), all pages in the
     *    range [pageOf(old_break-1)+1, pageOf(new_break-1)] should be allocated;
     * 
     * Note:
     * if heap_break != heap_start and pageOf(old_break-1) != pageOf(new_break-1),
     * then there are four scenarios:
     * 1) both old_break and new_break are within the frame, in which case pages in
     *    [pageOf(old_break)+1/pageOf(old_break-1)+1, pageOf(new_break)/pageOf(new_break-1)]
     *    should be allocated;
     * 2) only the old_break is on edge of a frame, in which case pages in
     *    [pageOf(old_break)/pageOf(old_break-1)+1, pageOf(new_break)/pageOf(new_break-1)]
     *    should be allocated;
     * 3) only the new_break is on edge of a frame, in which case pages in
     *    [pageOf(old_break)+1/pageOf(old_break-1)+1, pageOf(new_break)-1/pageOf(new_break-1)] should
     *    be deallocated;
     * 4) both the old and new_breaks are on edge of frames, in which case pages in
     *    [pageOf(old_break)/pageOf(old_break-1)+1, pageOf(new_break)-1/pageOf(new_break-1)] should be
     *    deallocated;
     * The only range expression that takes care of all four scenarios is:
     * [pageOf(old_break-1)+1, pageOf(new_break-1)].
     */
    if (old_break == (uint8_t *)t->heap_start) {
      if (!map_user_pages((void *)((uint32_t)t->heap_start & ~PGMASK), (void *)new_page)) {
        return (void *) -1;
      }
    } else if (old_page != new_page) {
      if (!map_user_pages((void *)((uint32_t)old_page + PGSIZE), (void *)new_page)) {
        return (void *) -1;
      }
    }
  }

  // update the heap break
  t->heap_break = (void *)new_break;
  return old_break;
}

static void
syscall_handler (struct intr_frame *f)
{
  uint32_t* args = (uint32_t*) f->esp;
  struct thread* t = thread_current ();
  t->in_syscall = true;
  /*
   * in case the syscall triggers a page fault on a user address and needs the
   * user stack pointer to determine if the stack should be automatically
   * extended, the user intr_frame pointer is preserved and exposed for external
   * references
   */
  USER_INTR_FRAME_PTR = (void *) f;

  validate_buffer_in_user_region (args, sizeof(uint32_t));
  switch (args[0])
    {
    case SYS_EXIT:
      validate_buffer_in_user_region (&args[1], sizeof(uint32_t));
      syscall_exit ((int) args[1]);
      break;

    case SYS_OPEN:
      validate_buffer_in_user_region (&args[1], sizeof(uint32_t));
      validate_string_in_user_region ((char*) args[1]);
      f->eax = (uint32_t) syscall_open ((char*) args[1]);
      break;

    case SYS_WRITE:
      validate_buffer_in_user_region (&args[1], 3 * sizeof(uint32_t));
      validate_buffer_in_user_region ((void*) args[2], (unsigned) args[3]);
      f->eax = (uint32_t) syscall_write ((int) args[1], (void*) args[2], (unsigned) args[3]);
      break;

    case SYS_READ:
      validate_buffer_in_user_region (&args[1], 3 * sizeof(uint32_t));
      validate_buffer_in_user_region ((void*) args[2], (unsigned) args[3]);
      f->eax = (uint32_t) syscall_read ((int) args[1], (void*) args[2], (unsigned) args[3]);
      break;

    case SYS_CLOSE:
      validate_buffer_in_user_region (&args[1], sizeof(uint32_t));
      syscall_close ((int) args[1]);
      break;
    
    case SYS_SBRK:
      validate_buffer_in_user_region (&args[1], sizeof(intptr_t));
      f->eax = (uint32_t) syscall_sbrk((intptr_t) args[1]);
      break;

    default:
      printf ("Unimplemented system call: %d\n", (int) args[0]);
      break;
    }

  USER_INTR_FRAME_PTR = NULL;
  t->in_syscall = false;
}
