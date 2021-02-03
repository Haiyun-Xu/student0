/*
This code is all loosely based off of the Linux Kernel. It is adapted to be slightly more readable for educational purposes.

In particular, to see a "real" version of an x86 PAE software paget able walk, see 
x86/include/asm/pgtable.h
arch/x86/include/asm/pgtable-3level_types.h
/arch/x86/include/asm/pgtable-3level.h

It may also help to read Chapter 3 of Mel Gorman's "Understanding the Linux Virtual Memory Manage" which can be found here: https://www.kernel.org/doc/gorman/html/understand/understand006.html

*/
#include <stdbool.h>
#include <stdint.h>

#define NO_ALIGN __attribute__((__packed__))

/**
 * Page table entry, 64 bits:
 *   1                  | 11       | 40                   | 3       | 1      | 1   | 1     | 1        | 1                         | 1                        | 1               | 1        | 1
 *   No Execute Enabled | Reserved | Physical Fram Number | Ignored | Global | PAT | Dirty | Accessed | Page-level Cache Disabled | Page-level Write Through | Supervisor-only | Writable | Present
 * 
 * See Table 4-11: "Format of a PAE Page-Table Entry that Maps a 4-KByte Page"
 * of the Intel IA32 manual Vol 3a.
 */
typedef struct NO_ALIGN page_table_entry {
  bool present : 1;
  bool writeable : 1;
  bool supervisor : 1;
  bool pwt : 1; // Page-level write-through
  bool pcd : 1; // Page-level cache disable
  bool accessed : 1;
  bool dirty : 1;
  bool pat : 1; // Page attribute table enable
  bool global : 1;
  int ignored : 3; // These bits are ignored by the processor
  uint64_t pfn : 40;
  int zero : 11; // These bits are reserved
  bool nxe : 1; // IA32 Extended Feature Enable Register No-Execute Enable (IA32_EFER.NXE)
} pte_t;

/**
 * Page directory entry, 64 bits:
 *   1                  | 11       | 40                    | 4       | 1          | 1       | 1        | 1                         | 1                        | 1               | 1        | 1
 *   No Execute Enabled | Reserved | Physical Frame Number | Ignored | Super-page | Ignored | Accesssd | Page-level Cache Disabled | Page-level Write Through | Supervisor-only | Writable | Present
 *
 * See Table 4-10: "Format of a PAE Page-Table Entry that Maps a 4-KByte Page"
 * of the Intel IA32 manual Vol 3a.
 */
typedef struct NO_ALIGN page_directory_entry {
  bool present : 1;
  bool writeable: 1;
  bool supervisor : 1;
  bool pwt : 1;
  bool pcd : 1;
  bool accessed: 1;
  bool ignored_first : 1;
  bool page_size : 1;
  int ignored_second : 4;
  uint64_t pfn : 40;
  int zero : 11;
  bool nxe : 1;
} pmd_t;

/**
 * Page directory pointer table entry, 64 bits:
 *   12       | 40                    | 3       | 4        | 1                         | 1                        | 2        | 1
 *   Reserved | Physical Frame Number | Ignored | Reserved | Page-level Cache Disabled | Page-level Write Through | Reserved | Present
 * 
 * See Table 4-8: "Format of a PAE Page-Directory-Pointer-Table Entry (PDPTE)"
 * of the Intel IA32 manual Vol 3a.
 */
typedef struct NO_ALIGN page_directory_pointer_table_entry {
  bool present : 1;
  int zero_first : 2;
  bool pwt : 1;
  bool pcd : 1;
  int zero_second : 4;
  bool ignored : 1;
  uint64_t pfn : 40;
  int zero_third: 12;
} pgd_t;

#ifndef PG_SIZE
#define PG_SIZE 4096
#endif

typedef uint8_t page_t[PG_SIZE];
