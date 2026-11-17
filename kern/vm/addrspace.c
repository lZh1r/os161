/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */

    as->lock = lock_create("as lock");
    if (as->lock == NULL) {
        kfree(as);
        return NULL;
    }

    as->page_dir = kmalloc(sizeof(struct page_directory));
    if (as->page_dir == NULL) {
        lock_destroy(as->lock);
        kfree(as);
        return NULL;
    }

    for (int i = 0; i < PT_SIZE; i++) {
        as->page_dir->entries[i] = NULL;
    }

    as->regions = NULL;
    as->heap_start = 0;
    as->heap_end = 0;

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */

    lock_acquire(old->lock);
    // copy page tables over
    // TODO: copy over the underlying physical frames
    for (int i = 0; i < PT_SIZE; i++) {
        if (old->page_dir->entries[i] != NULL) {
            struct page_table *pt = kmalloc(sizeof(struct page_table));
            if (pt == NULL) {
                goto fail;
            }
            for (int j = 0; j < PT_SIZE; j++) {
                pte_t old_pte = old->page_dir->entries[i]->entries[j];
                if (old_pte & PTE_RESIDENT) {
                    paddr_t new_paddr = vm_copy(PTE_TO_PADDR(old_pte));
                    if (!new_paddr) {
                        goto fail;
                    }
                    pt->entries[j] = new_paddr | PTE_RESIDENT;
                } else {
                    pt->entries[j] = 0;
                }
            }
            newas->page_dir->entries[i] = pt;
        }
    }

    //copy memory regions
    struct vm_region *old_regions = old->regions;
    if (old_regions != NULL) {
        newas->regions = kmalloc(sizeof(struct vm_region));
        if (newas->regions == NULL) {
            goto fail;
        }
        newas->regions->start = old_regions->start;
        newas->regions->end = old_regions->end;
        newas->regions->flags = old_regions->flags;
        newas->regions->next = NULL;
        struct vm_region *current_new = newas->regions;
        struct vm_region *next = old_regions->next;
        while (next != NULL) {
            current_new->next = kmalloc(sizeof(struct vm_region));
            if (current_new->next == NULL) {
                goto fail;
            }
            current_new = current_new->next;

            current_new->start = next->start;
            current_new->end = next->end;
            current_new->flags = next->flags;
            current_new->next = NULL;

            next = next->next;
        }
    }

	newas->heap_start = old->heap_start;
	newas->heap_end = old->heap_end;

    lock_release(old->lock);

	*ret = newas;
	return 0;

fail:
    lock_release(old->lock);
    as_destroy(newas);
    return ENOMEM;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */

    for (int i = 0; i < PT_SIZE; i++) {
        if (as->page_dir->entries[i] != NULL) {
            struct page_table *pt = as->page_dir->entries[i];
            for (int j = 0; j < PT_SIZE; j++) {
                paddr_t frame = pt_unmap(&pt->entries[j]);
                if (frame) {
                    vm_free(frame);
                }
            }
            kfree(pt);
        }
    }

    struct vm_region *next = as->regions;
    while (next != NULL) {
        struct vm_region *temp = next->next;
        kfree(next);
        next = temp;
    }

    lock_destroy(as->lock);
    kfree(as->page_dir);
	kfree(as);
    itlb_flush();
}

void
as_activate(void)
{
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

    itlb_flush();

	/*
	 * Write this.
	 */
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */

    KASSERT(as != NULL);

    if (memsize == 0 || vaddr >= MIPS_KSEG0 || vaddr + memsize > MIPS_KSEG0 || memsize > MIPS_KSEG0 - vaddr) {
        return EINVAL;
    }

    struct vm_region *reg = kmalloc(sizeof(struct vm_region));
    if (reg == NULL) {
        return ENOMEM;
    }

    reg->start = vaddr;
    reg->end = vaddr + memsize;
    reg->next = NULL;

    reg->flags = (readable ? VM_REG_READ : 0) | (writeable ? VM_REG_WRITE : 0) | (executable ? VM_REG_EXECUTE : 0);
    // everything is static except for the heap
    reg->flags |= VM_REG_STATIC;

    lock_acquire(as->lock);

    struct vm_region *cur = as->regions;
    struct vm_region *prev = NULL;
    while (cur != NULL) {
        // check for overlap
        if (reg->start < cur->end && cur->start < reg->end) {
            kfree(reg);
            lock_release(as->lock);
            return EINVAL;
        }

        if (cur->start >= reg->end) {
            reg->next = cur;
            if (prev == NULL) {
                as->regions = reg;
            } else {
                prev->next = reg;
            }
            lock_release(as->lock);
            return 0;
        }
        
        prev = cur;
        cur = cur->next;
    }

    if (prev == NULL) {
        as->regions = reg;
    } else {
        prev->next = reg;
    }

    lock_release(as->lock);
	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

    lock_acquire(as->lock);

    struct vm_region *cur = as->regions;
    while (cur != NULL) {
        pte_t *pte;
        int result;
        vaddr_t start = cur->start & ~(PAGE_SIZE - 1);
        vaddr_t end = (cur->end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        for (vaddr_t i = start; i < end; i += PAGE_SIZE) {
            if ((result = pt_upsert(as, start, &pte))) {
                lock_release(as->lock);
                return result;
            }
            if (*pte & PTE_RESIDENT) {
                continue;
            }
            paddr_t addr = vm_alloc(1, CORE_USER, false);
            if (addr == 0) {
                lock_release(as->lock);
                return ENOMEM;
            }
            pt_map(pte, addr);
        }
        cur = cur->next;
    }

    lock_release(as->lock);
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

    struct vm_region *heap = kmalloc(sizeof(struct vm_region));
    if (heap == NULL) {
        return ENOMEM;
    }
    heap->next = NULL;
    heap->flags = VM_REG_READ | VM_REG_WRITE;

    lock_acquire(as->lock);

    struct vm_region *prev = NULL;
    struct vm_region *cur = as->regions;
    while (cur != NULL) {
        prev = cur;
        cur = cur->next;
    }

    if (prev == NULL) {
        lock_release(as->lock);
        return EINVAL;
    }

    heap->start = (prev->end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    heap->end = heap->start;
    prev->next = heap;
    as->heap_start = heap->start;
    as->heap_end = heap->end;

    lock_release(as->lock);

    itlb_flush();

	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

    struct vm_region *stack = kmalloc(sizeof(struct vm_region));
    if (stack == NULL) {
        return ENOMEM;
    }
    stack->next = NULL;
    stack->start = USERSTACK - VM_STACK_PAGES * PAGE_SIZE;
    stack->end = USERSTACK;
    stack->flags = VM_REG_READ | VM_REG_WRITE | VM_REG_STATIC;

    lock_acquire(as->lock);

    struct vm_region *cur = as->regions;
    struct vm_region *prev = NULL;
    while (cur != NULL) {
        prev = cur;
        cur = cur->next;
    }

    if (prev == NULL) {
        as->regions = stack;
    } else {
        if (stack->start < prev->end) {
            kfree(stack);
            lock_release(as->lock);
            return EINVAL;
        }
        prev->next = stack;
    }

    lock_release(as->lock);
	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

struct vm_region *
as_probe(struct addrspace *as, vaddr_t addr) {
    lock_acquire(as->lock);
    struct vm_region *cur = as->regions;
    struct vm_region *ret = NULL;
    while (cur != NULL) {
        if (cur->start <= addr && addr < cur->end) {
            ret = cur;
            break;
        }
        cur = cur->next;
    }
    lock_release(as->lock);

    return ret;
}

int
vm_fault(int faulttype, vaddr_t faultaddress) {
    paddr_t paddr;
    pte_t *pte;
    (void)faulttype;
    struct addrspace *as = proc_getas();

    struct vm_region *reg = as_probe(as, faultaddress);
    if (reg == NULL) {
        return 1;
    }

    vaddr_t addr = faultaddress & ~(PAGE_SIZE - 1);
    paddr_t tlb_p = itlb_read(addr);
    // Already in tlb
    if (tlb_p) {
        panic("fdfd");
        return 0;
    }

    lock_acquire(as->lock);

    // Get the relevant pte
    int result = pt_upsert(as, addr, &pte);
    if (result) {
        lock_release(as->lock);
        return result;
    }

    // If not resident, allocate
    if (*pte & PTE_RESIDENT) {
        paddr = PTE_TO_PADDR(*pte);
    } else {
        paddr = vm_alloc(1, CORE_USER, false);
        if (paddr == 0) {
            lock_release(as->lock);
            return ENOMEM;
        }
        pt_map(pte, paddr);
    }

    // Write to the TLB
    itlb_write(addr, paddr);

    lock_release(as->lock);
    return 0;
}

void vm_tlbshootdown(const struct tlbshootdown *shoot) {
    (void)shoot;
}
