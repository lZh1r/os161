#include <types.h>
#include <lib.h>
#include <vm.h>
#include <spinlock.h>
#include <addrspace.h>
#include <current.h>
#include <kern/errno.h>

int 
pt_upsert(struct addrspace *as, vaddr_t addr, pte_t **ret) {
    KASSERT(as != NULL);
    KASSERT(ret != NULL);
    struct page_directory *pdir = as->page_dir;
    int dir_index = PT_DIR(addr);
    int table_index = PT_TABLE(addr);

    struct page_table *pt = pdir->entries[dir_index];
    if (pt == NULL) {
        pt = kmalloc(sizeof(struct page_table));
        if (pt == NULL) {
            return ENOMEM;
        }
        pdir->entries[dir_index] = pt;
    }

    *ret = &pt->entries[table_index];
    
    return 0;
}

void
pt_map(pte_t *pte, paddr_t frame_addr) {
    KASSERT(pte != NULL);
    KASSERT(frame_addr % PAGE_SIZE == 0);
    *pte = frame_addr;
    *pte |= PTE_RESIDENT;
}

paddr_t
pt_unmap(pte_t *pte) {
    KASSERT(pte != NULL);
    if (!(*pte & PTE_RESIDENT)) {
        return 0;
    }
    *pte &= ~PTE_RESIDENT;
    return PTE_TO_PADDR(*pte);
}
