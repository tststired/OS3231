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
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
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
	
	as->head = NULL;
	paddr_t **table = kmalloc(2048 * sizeof(paddr_t *));
	as->pagetable = table;
	
	for (int i = 0; i < 2048; i++) {
		as->pagetable[i] = NULL;
	}


	return as;
}


int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new_as;

	new_as = as_create();
	if (new_as == NULL) {
		return ENOMEM;
	}

	struct as_region *curr = NULL;

	//keep LL structure 
	for (struct as_region *old_r = old->head; old_r != NULL; old_r = old_r->next) {

		struct as_region *temp = kmalloc(sizeof(struct as_region));
		temp->next = NULL;
		temp->size = old_r->size;
		temp->readable = old_r->readable;
		temp->writeable = old_r->writeable;
		temp->executable = old_r->executable;
		temp->loading = old_r->loading;
		temp->vbase = old_r->vbase;
		
		if (new_as->head == NULL) {
			new_as->head = temp;
			curr = temp;
		} else {
			curr->next = temp;
			curr = temp;
		}
	}

	//pagetable copy
	for (int i = 0; i < 2048; i++) {
		if(old->pagetable[i] != NULL) {
			new_as->pagetable[i] = kmalloc(512 * sizeof(paddr_t));

			if (new_as->pagetable[i] == NULL) {
				return ENOMEM;
			}

			for (int j = 0; j < 512; j++) {
				if(old->pagetable[i][j]!=0) {
					vaddr_t newvad = alloc_kpages(1);
					bzero((void *) newvad, PAGE_SIZE);
					memmove((void *) newvad, (void *) PADDR_TO_KVADDR(old->pagetable[i][j] & PAGE_FRAME), PAGE_SIZE);
					paddr_t temp = KVADDR_TO_PADDR(newvad) & PAGE_FRAME;
					temp = temp|((TLBLO_DIRTY|TLBLO_VALID) & old->pagetable[i][j]); 
					new_as->pagetable[i][j] = temp;

				}
				else {
					new_as->pagetable[i][j] = 0;
				}
			}
		}
	} 
		
	*ret = new_as;
	return 0;
}


void
as_destroy(struct addrspace *as)
{	
	
	for (int i = 0; i < 2048; i++) {
		if (as->pagetable[i] != NULL) {
			for(int j = 0; j < 512; j++) {
				if(as->pagetable[i][j] != 0) {
					free_kpages(PADDR_TO_KVADDR(as->pagetable[i][j] & PAGE_FRAME));
				}
			}
			kfree(as->pagetable[i]);
		}
	}

	struct as_region* temp; 
	struct as_region* temp2 = as->head;

	while (temp2 != NULL) {
		temp = temp2; 
		temp2 = temp2->next;
		kfree(temp);
	}


	kfree(as->pagetable);
	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */

	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
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
	
	struct as_region *new_region = kmalloc(sizeof(struct as_region));


	new_region->next = NULL;
	new_region->size = memsize;
	new_region->readable = readable;
	new_region->writeable = writeable;
	new_region->executable = executable;
	new_region->loading = 0;
	new_region->vbase = vaddr;

	if (as->head == NULL) {
		as->head = new_region;
	} else {
		struct as_region *curr = as->head;
		while (curr->next != NULL) {
			curr = curr->next;
		}

		curr->next = new_region;
	}

	return 0; 
}

int
as_prepare_load(struct addrspace *as)
{

	for (struct as_region *curr = as->head; curr != NULL; curr = curr->next) {
		curr->loading = 1;
	}

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	
	for (struct as_region *curr = as->head; curr != NULL; curr = curr->next) {
		curr->loading = 0;
	}
	as_activate();

	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;
	

	return as_define_region(as, *stackptr - (16 * PAGE_SIZE), 16 * PAGE_SIZE, 1, 1, 0);
}

