#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <spl.h>
#include <proc.h>

/* Place your page table functions here */

int pt_insert(vaddr_t vaddr, paddr_t paddr) 
{
    uint32_t upper = vaddr >> 21;
    uint32_t lower = vaddr << 11 >> 23;
    struct addrspace *as = proc_getas();

    if (as->pagetable[upper] == NULL) {
        as->pagetable[upper] = kmalloc(512 * sizeof(paddr_t));
        for (int i = 0; i < 512; i++) {
            as->pagetable[upper][i] = 0;
        }
    } 

    as->pagetable[upper][lower] = paddr;
    
    return 0;
}


paddr_t pt_lookup(vaddr_t vaddr) 
{
    uint32_t upper = vaddr >> 21;
    uint32_t lower = vaddr << 11 >> 23;
    struct addrspace *as = proc_getas();

    
    if (as->pagetable[upper] == NULL) {
        // kprintf("fail\n");
        return 0;
    }
    
    return as->pagetable[upper][lower];
}


int pt_update(vaddr_t vaddr, paddr_t paddr)
{
    uint32_t upper = vaddr >> 21;
    uint32_t lower = vaddr << 11 >> 23;
    struct addrspace *as = proc_getas();

    if (as->pagetable[upper] == 0) {
        as->pagetable = kmalloc(512 * sizeof(paddr_t));
        for (int i = 0; i < 512; i++) {
            as->pagetable[upper][i] = 0;
        }
    } 

    as->pagetable[upper][lower] = paddr; 

    return 0;
}


void vm_bootstrap(void)
{
    /* Initialise any global components of your VM sub-system here.  
     *  
     * You may or may not need to add anything here depending what's
     * provided or required by the assignment spec.
     */
}

int vm_fault(int faulttype, vaddr_t faultaddress)
{
    int spl;

    if (faulttype == VM_FAULT_READONLY) {
        return EFAULT;
    } else if (faultaddress == 0) {
        return EFAULT;
    }
    
    struct addrspace *as = proc_getas();
    if (as == NULL) {
        return EFAULT;
    }

    paddr_t pt_entry = pt_lookup(faultaddress);

    if (pt_entry != 0) {
        
        for (struct as_region *curr = as->head; curr != NULL; curr = curr->next) {
            if ((faultaddress >= curr->vbase) && (faultaddress < (curr->vbase + curr->size))) {
                if ((curr->loading == 1) || (curr->writeable != 0)) {
                    pt_entry |= TLBLO_DIRTY;
                }
                pt_entry |= TLBLO_VALID;
                
                spl = splhigh();
                tlb_random(faultaddress & PAGE_FRAME, pt_entry);
                splx(spl);
                
                return 0;
            }
        }
        return EFAULT;
    }
    
    for (struct as_region *curr = as->head; curr != NULL; curr = curr->next) {

        if ((faultaddress >= curr->vbase) && (faultaddress < (curr->vbase + curr->size))) {
            vaddr_t v = alloc_kpages(1);

            bzero((void *) v, PAGE_SIZE);
            paddr_t p = KVADDR_TO_PADDR(v) & PAGE_FRAME;
            if ((curr->loading == 1) || (curr->writeable != 0)) {
                p |= TLBLO_DIRTY;
            }
            p |= TLBLO_VALID;
            pt_insert(faultaddress, p);

            spl = splhigh();
            tlb_random(faultaddress & PAGE_FRAME, p);
            splx(spl);

            return 0;
        }
    }

    return EFAULT;
}

/*
 * SMP-specific functions.  Unused in our UNSW configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

