#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void)
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void)
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f)
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */

  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit ();

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel");

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/**
 * Resolve page faults caused by stack growth by mapping the faulting page to a
 * new physical page.
 * 
 * @param flags Page allocation configuration
 * @param virtual_address The address that cause the page fault
 * 
 * @return void
 */
static void
resolve_page_fault_from_stack_growth(enum palloc_flags flags, void *virtual_address) {
   // edge cases
   if (virtual_address == NULL) {
      printf("Cannot map page to the provided virtual address\n");
      return;
   }

   void *virtual_page_address = pg_round_down(virtual_address);
   struct thread *t = thread_current();

   // do not allocate a new page if the given virtual page already exist
   if (pagedir_get_page(t->pagedir, virtual_page_address) != NULL) {
      printf("The provided virtual address is already mapped\n");
   } else {
      void *kernel_virtual_page_address = palloc_get_page(flags);
      /*
       * if no free page is available in the selected pool, terminate the
       * thread with status -1
       */
      if (kernel_virtual_page_address == NULL) {
         syscall_exit(-1);
      }

      // free the page and terminate the thread if the mapping failed
      if (!pagedir_set_page(t->pagedir, virtual_page_address, kernel_virtual_page_address, true)) {
         palloc_free_page(kernel_virtual_page_address);
         thread_exit();
      }
   }

   return;
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) {
  // the number of bytes by which a push/pusha instruction decrements the esp
  int PUSH_DECREMENT = 4;
  int PUSHA_DECREMENT = 32;

  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  struct thread* t = thread_current ();
   
  /*
   * if the page was present and the page fault was due to a violation of
   * access rights (writing to code region, read/write violation user accessing
   * kernel space, etc), then the page fault couldn't have been caused by stack
   * growth; page faults caused by stack growth will always be triggered by
   * an address on an unmapped page.
   * 
   * two types of stack growth could cause a page fault:
   * 1) accessing an uninitialized local variable or address on the stack;
   * 2) initializing arguments and frame data on the stack;
   */
  if (!not_present) {
     syscall_exit(-1);
  } else {
     /*
      * if the address was accessed by user, the thread couldn't have been in
      * a syscall, and the address couldn't have belonged to the kernel space
      */
     if (user && !t->in_syscall && is_user_vaddr(fault_addr)) {
        /*
         * if the fauling user address is above the user stack pointer, then
         * this page fault was caused by type 1 stack growth (user thread
         * accessing uninitialized data on user stack), and could be either
         * a read or a write access
         */
        if (fault_addr >= f->esp) {
           resolve_page_fault_from_stack_growth(PAL_ZERO|PAL_USER, fault_addr);
           return;

           /*
            * if the faulting user address is below the user stack pointer, and
            * it was a write access, then this page fault was caused by type 2
            * stack growth (user thread initializing arguments on user stack)
            */
        } else if (
           fault_addr < f->esp
           && write
           && (
              (uint8_t *) fault_addr == (uint8_t *) f->esp - PUSH_DECREMENT
              || (uint8_t *) fault_addr == (uint8_t *) f->esp - PUSHA_DECREMENT
           )
        ) {
           resolve_page_fault_from_stack_growth(PAL_ZERO|PAL_USER, fault_addr);
           return;

           /*
            * otherwise, the page fault was not caused by stack growth, so
            * maintain the original behavior of exiting with -1 status
            */
        } else {
           syscall_exit(-1);
        }
     } else if (!user) {
        /*
         * if the address was accessed by kernel and the address belongs to user
         * space, then the kernel thread must have been in a syscall (i.e.
         * software interrupt), because hardware interrupt usually doesn't
         * involve the kernel accessing user space address (except in the case
         * of upcall), and PINTOS does not support upcall
         */
        if (is_user_vaddr(fault_addr) && t->in_syscall) {
           /*
            * since PINTOS does not support upcall and the kernel returns
            * syscall results via registers (so the kernel never pushes
            * arguments onto user stack), the only possible stack growth that
            * could cause a page fault at this time is a type 1 stack growth
            * (kernel thread accessing uninitialized data on user stack), so
            * the faulting user address must be above the user stack pointer.
            * 
            * Note that it was a kernel thread that triggered the page fault
            * and gets interrupted, so the interrupt states in the intr_frame
            * argument, including the ESP/stack pointer, belongs to the kernel
            * thread. However, since the faulting address belongs to the user
            * space, only the user stack pointer can tell us whether the access
            * was caused by a stack growth. Fortunately, the interrupted kernel
            * thread was in a syscall, and the syscall handler perserves and
            * exposes the user intr_frame pointer for this use case.
            */
           if (fault_addr >= ((struct intr_frame *) USER_INTR_FRAME_PTR)->esp) {
              resolve_page_fault_from_stack_growth(PAL_ZERO, fault_addr);
              return;

              /*
               * otherwise, the user thread passed an invalid adress as the
               * syscall argument, and it should be terminated with -1 status
               */
           } else {
              syscall_exit(-1);
           }

           /*
            * else if the address was accessed by kernel and the address belongs
            * to kernel space, then whether the kernel thread was in a syscall is
            * no longer important
            */
        } else if (!is_user_vaddr(fault_addr)) {
            /*
             * if the fauling kernel address is above the kernel stack pointer,
             * then this page fault was caused by type 1 stack growth (kernel
             * thread accessing uninitialized data on kernel stack), and could be
             * either a read or a write access
             */
            if (fault_addr >= f->esp) {
               resolve_page_fault_from_stack_growth(PAL_ZERO, fault_addr);
               return;

             /*
              * if the faulting kernel address is below the kernel stack pointer,
              * and it was a write access, then this page fault was caused by
              * type 2 stack growth (kernel thread initializing arguments on
              * kernel stack)
              */
            } else if (
               fault_addr < f->esp
               && write
               && (
                  (uint8_t *) fault_addr == (uint8_t *) f->esp - PUSH_DECREMENT
                  || (uint8_t *) fault_addr == (uint8_t *) f->esp - PUSHA_DECREMENT
               )
            ) {
               resolve_page_fault_from_stack_growth(PAL_ZERO, fault_addr);
               return;
            }
        }
     }
  }

  /*
   * printing page fault context to catch faults that were missed by the
   * conditions above.
   */
  printf ("Page fault at %p: %s error %s page in %s context.\n",
          fault_addr,
          not_present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");
  kill (f);
}
