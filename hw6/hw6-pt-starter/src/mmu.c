/**
 * This module translates a virtual address to a physical address, based on an
 * x86 architecture with Physical Address Extension (PAE).
 * 
 * @author Haiyun Xu <xuhaiyunhenry@gmail.com>
 * @copyright MIT
 */

#include "mmu.h"

/**
 * The format of the 32-bit virtual address is:
 *   2                            | 9                    | 9                | 12
 *   Page Directory Pointer Index | Page Directory Index | Page Table Index | Offset
 * 
 * The workflow of the translation is:
 * 1) CR3 register points to the Page Directory Pointer Table (PDP T), which
 *    gives the 64-bit Page Directory Pointer (PDP) when indexed by the 2-bit
 *    Page Directory Pointer Index (PDP I);
 * 2) PDP points to the Page Directory Table (PD T), which gives the 64-bit
 *    Page Directory (PD) when indexed by the 9-bit Page Directory Index (PD I);
 * 3) PD points to the Page Table (PT), which gives the 64-bit Page Table Entry
 *    (PTE) when indexed by the 9-bit Page Table Index (PT I);
 * 
 * Answers to practice questions:
 * 1) each page is 4KB;
 * 2) each PTE is 64b, or 8B;
 * 3) 512 PTEs fits in a single page;
 * 4) each PT is 4KB;
 * 5) each PDT is 4KB;
 * 6) each PDPT is 32B;
 */

/**
 * Get the physical frame number from the page directory pointer entry.
 * 
 * @param table_addr The PDP table's physical address
 * @param entry_index The PDP table entry's index
 * @param entry_pfn Pointer to a physical frame number, where the result will
 *                  be stored
 * 
 * @return Returns 0 if successful, or -1 if unsuccessful
 */
int get_page_directory_pointer(paddr_ptr table_addr, uint64_t entry_index, paddr_ptr *entry_pfn) {
  int pfn_offset = 12;
  uint64_t bytes_per_entry = sizeof(uint64_t);
  uint64_t num_entries_per_table = 1 << 2;
  uint64_t pdpe_pfn_mask = PFN_MASK << pfn_offset;
  uint64_t present_bit_mask = 1;
  
  // edge cases
  if (entry_index >= num_entries_per_table) {
    return -1;
  }

  paddr_ptr entry_addr = table_addr + entry_index * bytes_per_entry;
  paddr_ptr entry = 0;
  ram_fetch(entry_addr, (void *) &entry, bytes_per_entry);

  // check if the entry is present
  if (!(entry & present_bit_mask)) {
    return -1;
  }

  *entry_pfn = (entry & pdpe_pfn_mask) >> pfn_offset;
  return 0;
}

/**
 * Get the physical frame number from the page directory entry.
 * 
 * @param table_pfn The PD table's physical frame number
 * @param entry_index The PD table entry's index
 * @param entry_pfn Pointer to a physical frame number, where the result will
 *                  be stored
 * 
 * @return Returns 0 if successful, or -1 if unsuccessful
 */
int get_page_directory(paddr_ptr table_pfn, uint64_t entry_index, paddr_ptr *entry_pfn) {
  int pfn_offset = 12;
  uint64_t bytes_per_entry = sizeof(uint64_t);
  uint64_t num_entries_per_table = 1 << 9;
  uint64_t pde_pfn_mask = PFN_MASK << pfn_offset;
  uint64_t present_bit_mask = 1;
  
  // edge cases
  if (entry_index >= num_entries_per_table) {
    return -1;
  }

  paddr_ptr entry_addr = pfn_to_addr(table_pfn) + entry_index * bytes_per_entry;
  paddr_ptr entry = 0;
  ram_fetch(entry_addr, (void *) &entry, bytes_per_entry);

  // check if the entry is present
  if (!(entry & present_bit_mask)) {
    return -1;
  }

  *entry_pfn = (entry & pde_pfn_mask) >> pfn_offset;
  return 0;
}

/**
 * Get the physical frame number from the page table entry.
 * 
 * @param table_pfn The page table's physical frame number
 * @param entry_index The page table entry's index
 * @param entry_pfn Pointer to a physical frame number, where the result will
 *                  be stored
 * 
 * @return Returns 0 if successful, or -1 if unsuccessful
 */
int get_page_table_entry(paddr_ptr table_pfn, uint64_t entry_index, paddr_ptr *entry_pfn) {
  int pfn_offset = 12;
  uint64_t bytes_per_entry = sizeof(uint64_t);
  uint64_t num_entries_per_table = 1 << 9;
  uint64_t pte_pfn_mask = PFN_MASK << pfn_offset;
  uint64_t present_bit_mask = 1;
  
  // edge cases
  if (entry_index >= num_entries_per_table) {
    return -1;
  }

  paddr_ptr entry_addr = pfn_to_addr(table_pfn) + entry_index * bytes_per_entry;
  paddr_ptr entry = 0;
  ram_fetch(entry_addr, (void *) &entry, bytes_per_entry);

  // check if the entry is present
  if (!(entry & present_bit_mask)) {
    return -1;
  }

  *entry_pfn = (entry & pte_pfn_mask) >> pfn_offset;
  return 0;
}

/**
 * Translates the virtual address, and stores the physical address in the given
 * pointer.
 * 
 * @param vaddr The virtual address
 * @param cr3   The address of the page directory pointer table
 * @param paddr Pointer to the result physical address
 * 
 * @return Returns 0 if successful, or -1 if failed or encountered a page fault.
 * In the case of failure, the value in paddr will not be modified
 */
int virtual_to_physical_address(vaddr_ptr vaddr, paddr_ptr cr3, paddr_ptr *paddr) {
  // edge cases
  if (paddr == NULL) {
    return -1;
  }

  // extract the table indices from the virtual address
  uint64_t pdp_index = (uint64_t) vaddr_pdpi(vaddr);
  uint64_t pd_index = (uint64_t) vaddr_pdi(vaddr);
  uint64_t pt_index = (uint64_t) vaddr_pti(vaddr);

  paddr_ptr pfn = 0;
  if (
    get_page_directory_pointer(cr3, pdp_index, &pfn)
    || get_page_directory(pfn, pd_index, &pfn)
    || get_page_table_entry(pfn, pt_index, &pfn)
  ) {
    return -1;
  }

  *paddr = pfn_to_addr(pfn) + (uint64_t) vaddr_off(vaddr);
  return 0;
}

char *str_from_virt(vaddr_ptr vaddr, paddr_ptr cr3) {
  size_t buf_len = 1;
  char *buf = malloc(buf_len);
  char c = ' ';
  paddr_ptr paddr;

  for (int i=0; c; i++) {
    if(virtual_to_physical_address(vaddr + i, cr3, &paddr)){
      printf("Page fault occured at address %p\n", (void *) (uint64_t) (vaddr + i));
      return NULL;
    }

    ram_fetch(paddr, &c, 1);
    buf[i] = c;
    if (i + 1 >= buf_len) {
      buf_len <<= 1;
      buf = realloc(buf, buf_len);
    }
    buf[i + 1] = '\0';
  }
  return buf;
}

int main(int argc, char **argv) {
  // edge cases
  if (argc != 4) {
    printf("Usage: ./mmu <mem_file> <cr3> <vaddr>\n");
    return 1;
  }

  ram_init();
  ram_load(argv[1]);

  paddr_ptr cr3 = strtol(argv[2], NULL, 0);
  vaddr_ptr vaddr = (uint32_t) strtol(argv[3], NULL, 0);
  paddr_ptr paddr;

  if(virtual_to_physical_address(vaddr, cr3, &paddr)){
    printf("Page fault occured at address %p\n", (void *) (uint64_t) vaddr);
    exit(1);
  }

  char *str = str_from_virt(vaddr, cr3);
  printf("Virtual address %p translated to physical address %p\n", (void *) (uint64_t) vaddr, (void *) paddr);
  printf("String representation of data at virtual address %p: %s\n", (void *) (uint64_t) vaddr, str);

  return 0;
}
