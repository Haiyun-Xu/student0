/**
 * This module translates a virtual address to a physical address, based on an
 * x86 architecture with Physical Address Extension (PAE).
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#ifndef MMU_H
#define MMU_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "page.h"
#include "ram.h"

/*
 * Bit operations defined for convenience.
 */
#define PFN_MASK (((uint64_t) 1 << 40) - 1) // 64-bits

#define PAGE_OFFSET_MASK ((1 << 12) - 1) // 32-bits
#define vaddr_off(vaddr) (vaddr & PAGE_OFFSET_MASK)

#define VADDR_PDPI_MASK (3 << 30)
#define VADDR_PDI_MASK (511 << 21)
#define VADDR_PTI_MASK (511 << 12)
#define vaddr_pdpi(vaddr) ((vaddr & VADDR_PDPI_MASK) >> 30)
#define vaddr_pdi(vaddr) ((vaddr & VADDR_PDI_MASK) >> 21)
#define vaddr_pti(vaddr) ((vaddr & VADDR_PTI_MASK) >> 12)

#define pfn_to_addr(pfn) (pfn << 12)

int main(int argc, char **argv);

#endif /* MMU_H */