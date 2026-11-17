#include <types.h>
#include <lib.h>
#include <vm.h>
#include <machine/tlb.h>

void itlb_flush() {
    for (uint32_t i = 0; i < NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }
}

void itlb_write(vaddr_t vaddr, paddr_t paddr) {
    uint32_t hi, lo;
    KASSERT(vaddr % PAGE_SIZE == 0);
    KASSERT(paddr % PAGE_SIZE == 0);
    int index = tlb_probe(vaddr, 0);
    if (index >= 0) {
        tlb_read(&hi, &lo, index);
        if (lo & TLBLO_VALID) {
            panic("idk how to handle this tbh\n");
        }
        tlb_write(vaddr, paddr | TLBLO_VALID | TLBLO_DIRTY, index);
        return;
    }
    for (uint32_t i = 0; i < NUM_TLB; i++) {
        tlb_read(&hi, &lo, i);
        if (!(lo & TLBLO_VALID)) {
            tlb_write(vaddr, paddr | TLBLO_VALID | TLBLO_DIRTY, i);
            return;
        }
    }
    tlb_random(vaddr, paddr | TLBLO_VALID | TLBLO_DIRTY);
}

paddr_t itlb_read(vaddr_t vaddr) {
    uint32_t hi, lo;
    int index = tlb_probe(vaddr, 0);
    if (index < 0) {
        return 0;
    }
    tlb_read(&hi, &lo, index);
    if (!(lo & TLBLO_VALID)) {
        return 0;
    }
    return lo & ~(PAGE_SIZE - 1);
}
