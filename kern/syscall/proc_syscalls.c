#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <synch.h>
#include <mips/trapframe.h>
#include <kern/limits.h>


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {
  // kprintf("exit1\n");
  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  curproc->p_exit_code = exitcode;
  curproc->p_exited = true;
  // GGG
  // kprintf("Exiting %d\n", curproc->p_id);

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  // kprintf("exit2 %d\n", p->p_id);

  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);
  // if (curproc->parent != NULL) {
  //   // GGG
  //   // kprintf("Exiting %d with parent %d\n", curproc->p_id, curproc->parent->p_id);
  //   if (curproc->parent->waiting_on == curproc->p_id) {
  //     // curproc->parent->w_sem = curproc->p_sem;
  //     // curproc->parent->p_c_exit_code = curproc->p_exit_code;
  //     // curproc->parent->p_c_exited = true;
  //     for (unsigned i = 0; i < array_num(curproc->parent->children); i++) {
  //       struct proc *child = ((struct proc *) array_get(curproc->parent-> children, i));
  //       if (child->p_id == curproc->p_id) {
  //         array_remove(curproc->parent->children, i);
  //         break;
  //       }
  //     }
  //   }
  // }
  V(curproc->p_sem);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);

  thread_exit();

  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = curproc->p_id;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
      userptr_t status,
      int options,
      pid_t *retval)
{
  int exitstatus;
  int result;

  // curproc->waiting_on = pid;

  // GGG
  // kprintf("X=Waiting on %d\n", pid);
  // kprintf("X=Wait parent %d\n", curproc->p_id);

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }

  result = proc_should_wait(pid, curproc);
  if (result == -1) {
    // GGG
    // kprintf("FAIL WAIT: %d =[p= %d\n", pid, curproc->p_id);
    // curproc->waiting_on = 0;
    return proc_echild_or_esrch(pid);
  }

  // if (!curproc->p_c_exited) {
  struct proc *child = (struct proc *) array_get(curproc->children, result);
    if (!child->p_exited) {
      P(child->p_sem);
    }
  // }

  /* for now, just pretend the exitstatus is 0 */
  exitstatus = _MKWAIT_EXIT(child->p_exit_code);
  // sem_destroy(child->p_sem);
  // proc_free_p_id(child->p_id);
  // kfree(child);
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }

  *retval = pid;
  return(0);
}

void fork_entrypoint(void *data1, unsigned long data2) {
  (void) data2;
  enter_forked_process((struct trapframe *) data1);
}

int sys_fork(struct trapframe *tf, pid_t *retval) {
  struct proc *cp = proc_create_runprogram(curproc->p_name);
  // this means that proc_create failed.
  // this fails when kmalloc or kstrdup fails
  // or when there are 2 many pids
  if (cp == NULL) {
    return ENOMEM;
  }

  int err = as_copy(curproc->p_addrspace, &cp->p_addrspace);
  // as_copy will return 0 if successful
  // so if err != 0 it was unsuccessful
  if (err) {
    proc_destroy(cp);
    return err;
  }

  // assign pid
  // err = proc_find_p_id(&cp->p_id);
  // // kprintf("ALLOC %d", cp->p_id);

  // if (err) {
  //   proc_destroy(cp);
  //   return err;
  // }

  // GGG
  // kprintf("Assigned %d\n", cp->p_id);
  // kprintf("Assign parent %d\n", curproc->p_id);

  // add child
  // struct proc *item = kmalloc(sizeof(struct proc));
  // item = cp;

  // cp->parent = kmalloc(sizeof(struct proc *));
  // if (cp->parent == NULL) {
  //   proc_destroy(cp);
  //   return ENOMEM;
  // }
  // cp->parent = curproc;
  cp->parent = curproc;
  array_add(curproc->children, (void *) cp, NULL);
  // cp->parent_exit_sem = curproc->p_sem;
  // cp->parent = curproc;

  // thread_fork
  struct trapframe *tf_copy = kmalloc(sizeof(struct trapframe));
  if (tf_copy == NULL) {
    proc_destroy(cp);
    return ENOMEM;
  }
  *tf_copy = *tf;

  err = thread_fork(curproc->p_name, cp, fork_entrypoint, tf_copy, 0);

  if (err) {
    kfree(tf_copy);
    as_destroy(cp->p_addrspace);
    proc_destroy(cp);
    return err;
  }

  *retval = cp->p_id;
  return 0;
}
