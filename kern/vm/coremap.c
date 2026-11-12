#include <types.h>
#include <lib.h>
#include <vm.h>
#include <spinlock.h>

static struct coremap_entry *coremap;
static unsigned int total_pages = 0;
static unsigned int used_pages;
static struct spinlock coremap_lock;
static unsigned int first_free_index;

static
unsigned
paddr_to_index(paddr_t paddr) {
    return (paddr & ~PAGE_SIZE) / PAGE_SIZE;
}

/* 
 * Convert a kernel virtual address to a physical one.
 * Only works with directly mapped addresses, i.e. from MIPS_KSEG0
 */
static
paddr_t
kvaddr_to_paddr(vaddr_t addr) {
    KASSERT(addr >= MIPS_KSEG0);
    KASSERT(addr < MIPS_KSEG1);

    return (paddr_t)(addr - MIPS_KSEG0);
}

void
coremap_init(void) {
    /* get the total amount of memory */
    paddr_t ramsize = ram_getsize();
    /* calculate how much space we need for the core map */
    total_pages = (size_t)(ramsize / PAGE_SIZE);
    size_t total_size = total_pages * sizeof(struct coremap_entry);
    size_t pages_needed = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

    /* now steal enough memory to allocate everything */
    paddr_t pcoremap = ram_stealmem(pages_needed);
    KASSERT(pcoremap != 0);
    coremap = (struct coremap_entry *)PADDR_TO_KVADDR(pcoremap);

    paddr_t firstfree = ram_getfirstfree();

    /* initialize the coremap entries */
    for (unsigned long i = 0; i < total_pages; i++) {
        struct coremap_entry *entry = &coremap[i];
        if (i * PAGE_SIZE < firstfree) {
            entry->length = 1;
            entry->status = CORE_RESERVED;
        } else {
            entry->length = 1;
            entry->status = CORE_FREE;
        }
    }

    /* initialize the rest of the stuff */
    used_pages = 0;
    first_free_index = paddr_to_index(firstfree);
    spinlock_init(&coremap_lock);
}

struct coremap_entry *
coremap_get(void) {
    return coremap;
}

unsigned int
coremap_get_total_pages(void) {
    return total_pages;
}

unsigned int
coremap_used_bytes(void) {
    return used_pages * PAGE_SIZE;
}

vaddr_t
alloc_kpages(unsigned npages) {
    if (!total_pages) {
        return PADDR_TO_KVADDR(ram_stealmem(npages));
    }

    if (npages > total_pages || (npages + used_pages) > total_pages) {
        return 0;
    }

    vaddr_t result = 0;

    spinlock_acquire(&coremap_lock);
    for (unsigned i = first_free_index; i < total_pages; i++) {
        struct coremap_entry *entry = &coremap[i];
        if (entry->status != CORE_FREE) {
            continue;
        }
        if (npages == 1) {
            entry->status = CORE_KERN;
            result = PADDR_TO_KVADDR(i * PAGE_SIZE);
            break;
        } else {
            int bad = 0;
            for (unsigned u = 1; u < npages; u++) {
                struct coremap_entry *subject = &coremap[i + u];
                if (subject->status != CORE_FREE) {
                    bad = 1;
                    break;
                }
            }

            if (bad) {
                continue;
            }

            result = PADDR_TO_KVADDR(i * PAGE_SIZE);
            entry->status = CORE_KERN;
            entry->length = npages;
            for (unsigned u = 1; u < npages; u++) {
                entry = &coremap[i + u];
                entry->status = CORE_KERN;
                entry->length = 0; // means that this is a tail
            }
            break;
        }
    }
    spinlock_release(&coremap_lock);

    return result;
}

void
free_kpages(vaddr_t addr) {
    /* get the physical address of the page in question */
    paddr_t paddr = kvaddr_to_paddr(addr);

    unsigned index = paddr_to_index(paddr);

    /* free the pages */
    spinlock_acquire(&coremap_lock);
    if (coremap[index].length > 1) {
        unsigned length = coremap[index].length;
        KASSERT(length != 0);
        for (unsigned i = 0; i < length; i++) {
            struct coremap_entry *entry = &coremap[index + i];
            if (i > 0) {
                // check that the invariant holds
                KASSERT(entry->length == 0);
            }
            entry->status = CORE_FREE;
            entry->length = 1;
        }
    } else {
        coremap[index].status = CORE_FREE;
    }
    spinlock_release(&coremap_lock);
}

int vm_fault(int faulttype, vaddr_t faultaddress) {
    (void)faulttype;
    (void)faultaddress;
    return 0;
}

void vm_tlbshootdown(const struct tlbshootdown *shoot) {
    (void)shoot;
}

