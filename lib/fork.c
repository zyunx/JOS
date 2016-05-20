// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>
#include <inc/mmu.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
    pte_t *ptep = (pte_t *)uvpt + ((uintptr_t)addr >>  PGSHIFT);
	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
    if (!(err & FEC_WR)) {
        panic("pgfault: not a write access.\n");
    }
    if (!(*ptep & PTE_COW)) {
        cprintf("pgfault: addr: %x\n err %x eip %x\n", addr, err, utf->utf_eip);
        panic("pgfault: not a COW page\n");
    }

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
    if ((r = sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W))) {
        panic("pgfault: no free page %e\n", r);
    }
    
    memmove(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
    if ((r = sys_page_map(0, PFTEMP, 0, addr, PTE_P | PTE_U | PTE_W))) {
        panic("pgfault: map page %e\n", r);
    }
    if ((r = sys_page_unmap(0, PFTEMP))) {
        panic("pgfault: unmap PFTEMP %e\n", r);
    }
    
    //cprintf("pgfault: ok");
	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
    pte_t pte;

	// LAB 4: Your code here.
    pte = *((pte_t *)(uvpt) + pn);
    
    if (!(pte & PTE_P))
        panic("duppage: pn 0x%x frame not present.\n", pn);

    //cprintf("duppage: wdp %x [%x]\n", pn, pte);
    if ((pte & PTE_W) || (pte & PTE_COW)) {
        pte = PTE_U | PTE_COW | PTE_P; 

        //cprintf("duppage: map child page %x\n", pn);
        if ((r = sys_page_map(0, (void *)(pn*PGSIZE),
                    envid, (void *)(pn*PGSIZE), pte)))
        {
            panic("duppage: map child page %e\n", r);
        }
        //cprintf("duppage: child mapped %x\n", pn);

        //cprintf("duppage: map self %x\n", pn);
        if ((r = sys_page_map(0, (void *)(pn*PGSIZE),
                    0, (void *) (pn*PGSIZE), pte)))
        {

            panic("duppage: map my page %e\n", r);
        }
        //cprintf("duppage: self mapped %x\n", pn);
    } else {
        if ((r = sys_page_map(0, (void *)(pn*PGSIZE),
                    envid, (void *)(pn*PGSIZE), PTE_U)))
        {
            panic("duppage: map child page %e\n", r);
        }

    }         
/*
    if ((pte & (PTE_W | PTE_COW)))
    {
        pte = PTE_U | PTE_COW | PTE_P; 

        if ((r = sys_page_map(0, (void *)(pn*PGSIZE),
                    0, (void *) (pn*PGSIZE), pte)))
        {
            panic("duppage: map my page %e\n", r);
        }
        if ((r = sys_page_map(0, (void *)(pn*PGSIZE),
                    envid, (void *)(pn*PGSIZE), pte)))
        {
            panic("duppage: map child page %e\n", r);
        }
    } else {

        //cprintf("pn %x [%x]\n", pn, PTE_PERM(uvpt[pn]));

        if ((r = sys_page_map(0, (void *)(pn*PGSIZE), 
                    0, (void *)(pn*PGSIZE), PTE_U | PTE_W | PTE_P)))
        {
            panic("duppage: map my page %e\n", r);
        }
        if ((r = sys_page_map(0, (void *)(pn*PGSIZE), 
                    envid, (void *)(pn*PGSIZE), PTE_U | PTE_W | PTE_P)))
        {
            panic("duppage: map child page %e\n", r);
        }

    }*/
	//panic("duppage not implemented");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
    int r;
    int pn;
    envid_t envid;
    int i, j;

    // set pgfault handler for both self and child.
    set_pgfault_handler(pgfault);

    envid  = sys_exofork();
    if (envid < 0) {
        panic("fork: exofork %e\n", envid);
    } else if (envid == 0) {
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }
    
    // copy mappings
    pde_t *pdep = (pde_t*) uvpd;
    pte_t *ptep = (pte_t*) uvpt;
    for (i = 0; i < (UTOP >> PTSHIFT); i++) {
        if (pdep[i] & PTE_P) {
            for (j = 0; j < 1024; j++) {
                pn = i * 1024 + j;
                // except user exception handler
                if (pn == ((UXSTACKTOP >> PGSHIFT) - 1))
                    continue;
                //if (pn == ((USTACKTOP >> PGSHIFT) - 1))
                 //  continue;
                if (ptep[pn] & PTE_P) {
                    duppage(envid, pn);
                }
            }
        }
    }

    // map user stack
    // sys_page_alloc(envid, (void *) (USTACKTOP-PGSIZE), PTE_U | PTE_W);
    //duppage(envid, (USTACKTOP >> PGSHIFT)-1);
    // map user exception stack
    sys_page_alloc(envid, (void *) (UXSTACKTOP-PGSIZE) , PTE_U | PTE_W); 

    if ((r = sys_env_set_status(envid, ENV_RUNNABLE)))
        panic("fork: env set status %e\n", r);

    return envid; 
	//panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
