/*
 * Copyright (c) 2013
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

/*
 * Process support.
 *
 * There is (intentionally) not much here; you will need to add stuff
 * and maybe change around what's already present.
 *
 * p_lock is intended to be held when manipulating the pointers in the
 * proc structure, not while doing any significant work with the
 * things they point to. Rearrange this (and/or change it to be a
 * regular lock) as needed.
 *
 * Unless you're implementing multithreaded user processes, the only
 * process that will have more than one thread is the kernel process.
 */

#include <types.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <vfs.h>
#include <synch.h>
#include <kern/fcntl.h>  
#include <kern/limits.h>
#include <kern/errno.h>

/*
 * The process for the kernel; this holds all the kernel-only threads.
 */
struct proc *kproc;

/*
 * Mechanism for making the kernel menu thread sleep while processes are running
 */
#ifdef UW
/* count of the number of processes, excluding kproc */
static volatile unsigned int proc_count;
/* provides mutual exclusion for proc_count */
/* it would be better to use a lock here, but we use a semaphore because locks are not implemented in the base kernel */ 
static struct semaphore *proc_count_mutex;
/* used to signal the kernel menu thread when there are no processes */
struct semaphore *no_proc_sem;   
#endif  // UW

// GLOBAL PROCARRAY
// static proc *proccesses[__PID_MAX - __PID_MIN + 1];

/*
 * Create a proc structure.
 */
static
struct proc *
proc_create(const char *name)
{
	struct proc *proc;

	proc = kmalloc(sizeof(*proc));
	if (proc == NULL) {
		return NULL;
	}
	proc->p_name = kstrdup(name);
	if (proc->p_name == NULL) {
		kfree(proc);
		return NULL;
	}

	int err = proc_find_p_id(&proc->p_id);
  // kprintf("ALLOC %d", cp->p_id);
	if (err != 0) {
		kfree(proc->p_name);
    	kfree(proc);
    	return NULL;
  	}


	// * parent
	 // proc->parent_exited = false;
	 // proc->parent_exit_sem = NULL;

	threadarray_init(&proc->p_threads);
	spinlock_init(&proc->p_lock);

	/* VM fields */
	proc->p_addrspace = NULL;

	/* VFS fields */
	proc->p_cwd = NULL;

#ifdef UW 
	proc->console = NULL;
#endif // UW

	proc->children = array_create();

	//
	proc->parent = NULL;
	// proc->waiting_on = 0;
	// proc->w_sem = NULL;
	// proc->p_c_exit_code = 0;
	// proc->p_c_exited = false;

	proc->p_sem = sem_create("p_sem", 0);
	if (proc->p_sem == NULL) {
		kfree(proc);
		kfree(proc->p_name);
		array_destroy(proc->children);
		return NULL;
	}

	proc->p_exited = false;
	proc->p_exit_code = 0;

	return proc;
}

/*
 * Destroy a proc structure.
 */
void
proc_destroy(struct proc *proc)
{
	/*
         * note: some parts of the process structure, such as the address space,
         *  are destroyed in sys_exit, before we get here
         *
         * note: depending on where this function is called from, curproc may not
         * be defined because the calling thread may have already detached itself
         * from the process.
	 */

	KASSERT(proc != NULL);
	KASSERT(proc != kproc);

	/*
	 * We don't take p_lock in here because we must have the only
	 * reference to this structure. (Otherwise it would be
	 * incorrect to destroy it.)
	 */

	/* VFS fields */
	if (proc->p_cwd) {
		VOP_DECREF(proc->p_cwd);
		proc->p_cwd = NULL;
	}


#ifndef UW  // in the UW version, space destruction occurs in sys_exit, not here
	if (proc->p_addrspace) {
		/*
		 * In case p is the currently running process (which
		 * it might be in some circumstances, or if this code
		 * gets moved into exit as suggested above), clear
		 * p_addrspace before calling as_destroy. Otherwise if
		 * as_destroy sleeps (which is quite possible) when we
		 * come back we'll be calling as_activate on a
		 * half-destroyed address space. This tends to be
		 * messily fatal.
		 */
		struct addrspace *as;

		as_deactivate();
		as = curproc_setas(NULL);
		as_destroy(as);
	}
#endif // UW

#ifdef UW
	if (proc->console) {
	  vfs_close(proc->console);
	}
#endif // UW
	threadarray_cleanup(&proc->p_threads);
	spinlock_cleanup(&proc->p_lock);

	kfree(proc->p_name);
	// kprintf("PROC_ID %d\n", proc->p_id);
	// kfree(proc);

	// if (proc->parent == NULL || (proc->parent != NULL && proc->parent->waiting_on != proc->p_id)) {
	// 	sem_destroy(proc->p_sem);
	// }

	// if (proc->w_sem != NULL) {
	// 	sem_destroy(proc->w_sem);
	// }

	// struct proc *tbd = NULL;
	unsigned array_len = array_num(proc->children);
	while (array_len!= 0) {
		// tbd = (struct proc *) array_get(proc->children, array_len - 1);
		struct proc *child = (struct proc *) array_get(proc->children, array_len - 1);
		if (child->p_exited) {
			sem_destroy(child->p_sem);
			// kprintf("PROC_ID %d\n", child->p_id);
			if (proc->p_id != 0) {
				proc_free_p_id(child->p_id);
			}
			kfree(child);
		}
		child->parent = NULL;
		array_remove(proc->children, array_len - 1);
		array_len = array_num(proc->children);
	}

	array_destroy(proc->children);

	if (proc->parent != NULL) {
	} else {
		sem_destroy(proc->p_sem);
		if (proc->p_id != 0) {
			proc_free_p_id(proc->p_id);
		}
		kfree(proc);
	}
	//parent
	// kfree(proc);


#ifdef UW
	/* decrement the process count */
        /* note: kproc is not included in the process count, but proc_destroy
	   is never called on kproc (see KASSERT above), so we're OK to decrement
	   the proc_count unconditionally here */
	P(proc_count_mutex); 
	KASSERT(proc_count > 0);
	proc_count--;
	/* signal the kernel menu thread if the process count has reached zero */
	if (proc_count == 0) {
	  V(no_proc_sem);
	}
	V(proc_count_mutex);
#endif // UW
	// if (proc->p_id >= __PID_MIN) {
	// 	kprintf("destroy index %d\n", proc->p_id);
	// 	proc_free_p_id(proc->p_id);
	// }
	// array_destroy(proc->children);

	// sem_destroy(proc->p_sem);
}

/*
 * Create the process structure for the kernel.
 */
void
proc_bootstrap(void)
{
	p_id_manager = bitmap_create(__PID_MAX - __PID_MIN + 1);
  	if (p_id_manager == NULL) {
  		panic("could not create p_id_manager\n");
  	}
  	p_id_manager_lock = lock_create("p_id_manager_lock");
  	if (p_id_manager_lock == NULL) {
  		panic("could not create p_id_manager_lock\n");
  	}
  kproc = proc_create("[kernel]");
  if (kproc == NULL) {
    panic("proc_create for kproc failed\n");
  }
#ifdef UW
  proc_count = 0;
  proc_count_mutex = sem_create("proc_count_mutex",1);
  if (proc_count_mutex == NULL) {
    panic("could not create proc_count_mutex semaphore\n");
  }
  no_proc_sem = sem_create("no_proc_sem",0);
  if (no_proc_sem == NULL) {
    panic("could not create no_proc_sem semaphore\n");
  }
#endif // UW 
}

/*
 * Create a fresh proc for use by runprogram.
 *
 * It will have no address space and will inherit the current
 * process's (that is, the kernel menu's) current directory.
 */
struct proc *
proc_create_runprogram(const char *name)
{
	struct proc *proc;
	char *console_path;

	proc = proc_create(name);
	if (proc == NULL) {
		return NULL;
	}

#ifdef UW
	/* open the console - this should always succeed */
	console_path = kstrdup("con:");
	if (console_path == NULL) {
	  panic("unable to copy console path name during process creation\n");
	}
	if (vfs_open(console_path,O_WRONLY,0,&(proc->console))) {
	  panic("unable to open the console during process creation\n");
	}
	kfree(console_path);
#endif // UW
	  
	/* VM fields */

	proc->p_addrspace = NULL;

	/* VFS fields */

#ifdef UW
	/* we do not need to acquire the p_lock here, the running thread should
           have the only reference to this process */
        /* also, acquiring the p_lock is problematic because VOP_INCREF may block */
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		proc->p_cwd = curproc->p_cwd;
	}
#else // UW
	spinlock_acquire(&curproc->p_lock);
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		proc->p_cwd = curproc->p_cwd;
	}
	spinlock_release(&curproc->p_lock);
#endif // UW

#ifdef UW
	/* increment the count of processes */
        /* we are assuming that all procs, including those created by fork(),
           are created using a call to proc_create_runprogram  */
	P(proc_count_mutex); 
	proc_count++;
	V(proc_count_mutex);
#endif // UW

	return proc;
}

/*
 * Add a thread to a process. Either the thread or the process might
 * or might not be current.
 */
int
proc_addthread(struct proc *proc, struct thread *t)
{
	int result;

	KASSERT(t->t_proc == NULL);

	spinlock_acquire(&proc->p_lock);
	result = threadarray_add(&proc->p_threads, t, NULL);
	spinlock_release(&proc->p_lock);
	if (result) {
		return result;
	}
	t->t_proc = proc;
	return 0;
}

/*
 * Remove a thread from its process. Either the thread or the process
 * might or might not be current.
 */
void
proc_remthread(struct thread *t)
{
	struct proc *proc;
	unsigned i, num;

	proc = t->t_proc;
	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	/* ugh: find the thread in the array */
	num = threadarray_num(&proc->p_threads);
	for (i=0; i<num; i++) {
		if (threadarray_get(&proc->p_threads, i) == t) {
			threadarray_remove(&proc->p_threads, i);
			spinlock_release(&proc->p_lock);
			t->t_proc = NULL;
			return;
		}
	}
	/* Did not find it. */
	spinlock_release(&proc->p_lock);
	panic("Thread (%p) has escaped from its process (%p)\n", t, proc);
}

/*
 * Fetch the address space of the current process. Caution: it isn't
 * refcounted. If you implement multithreaded processes, make sure to
 * set up a refcount scheme or some other method to make this safe.
 */
struct addrspace *
curproc_getas(void)
{
	struct addrspace *as;
#ifdef UW
        /* Until user processes are created, threads used in testing 
         * (i.e., kernel threads) have no process or address space.
         */
	if (curproc == NULL) {
		return NULL;
	}
#endif

	spinlock_acquire(&curproc->p_lock);
	as = curproc->p_addrspace;
	spinlock_release(&curproc->p_lock);
	return as;
}

/*
 * Change the address space of the current process, and return the old
 * one.
 */
struct addrspace *
curproc_setas(struct addrspace *newas)
{
	struct addrspace *oldas;
	struct proc *proc = curproc;

	spinlock_acquire(&proc->p_lock);
	oldas = proc->p_addrspace;
	proc->p_addrspace = newas;
	spinlock_release(&proc->p_lock);
	return oldas;
}

// tbf = to be found
int proc_find_p_id(pid_t *tbf) {
	// lock_acquire(p_id_manager_lock);
	// unsigned unused_p_id = 0;
	unsigned *unused_p_id = kmalloc(sizeof(unsigned));

	lock_acquire(p_id_manager_lock);
	int err = bitmap_alloc(p_id_manager, unused_p_id);
	lock_release(p_id_manager_lock);

	if (err != 0) {
		// lock_release(p_id_manager_lock);
		return err;
	}

	*tbf = *unused_p_id + __PID_MIN;
	kfree(unused_p_id);

	// lock_release(p_id_manager_lock);
	return 0;
}

// tbf = to be freed
void proc_free_p_id(pid_t tbf) {
	lock_acquire(p_id_manager_lock);
	bitmap_unmark(p_id_manager, tbf - __PID_MIN);
	lock_release(p_id_manager_lock);
}

// tbf = to be found
int proc_should_wait(pid_t tbf, struct proc *parent) {
	// kprintf("aloha");
	// for (unsigned i = 0; i < array_num(parent->children); i++) {
	// 	struct proc *child = ((struct proc *) array_get(parent->children, i));
	// 	kprintf("CHILD PID %d\n", child->p_id);
	// 	if (!(child->p_id >= __PID_MIN && child->p_id <= __PID_MAX)) {
	// 		kprintf("CHILD PID2 %d\n", child->p_id);
	// 		array_remove(parent->children, i);
	// 		i--;
	// 	}
	// }
	for (unsigned i = 0; i < array_num(parent->children); i++) {
		struct proc *child = ((struct proc *) array_get(parent->children, i));
		// if (!(child->p_id >= __PID_MIN && child->p_id <= __PID_MAX)) {
		// 	kprintf("CHILD PID2 %d\n", child->p_id);
		// 	array_remove(parent->children, i);
		// 	i--;
		// }
		if (child->p_id == tbf) return i;
	}
	return -1;
}

int proc_echild_or_esrch(pid_t tbf) {
	lock_acquire(p_id_manager_lock);
    int exists = bitmap_isset(p_id_manager, tbf - __PID_MIN);
    lock_release(p_id_manager_lock);
    if (exists) return ECHILD;
    return ESRCH;
}

