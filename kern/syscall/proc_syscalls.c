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

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();

  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  V(curproc->p_sem);
  // if (curproc->parent != NULL && curproc->p_id == curproc->parent->in_wait_of) {
  //   P(curproc->parent->p_sem);
  // }
  // kprintf("exit4 %d\n", p->p_id);

  // if (curproc->parentP(curproc->parent->p_sem);

  // for (unsigned i = 0; i < array_num(curproc->parent->children); i++) {
  //   if (((struct proc *) array_get(curproc->parent->children, i))->p_id == curproc->p_id) {
  //     array_remove(curproc->parent->children, i);
  //     break;
  //   };
  // }

  // V(curproc->p_sem);

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
    return proc_echild_or_esrch(pid);
  }
  curproc->in_wait_of = pid;

  struct proc *child = (struct proc *) array_get(curproc->children, result);

  // ?
  if (!child->p_exited) {
    P(child->p_sem);
  }


  /* for now, just pretend the exitstatus is 0 */
  exitstatus = curproc->p_exit_code;
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
  err = proc_find_p_id(&cp->p_id);

  if (err) {
    proc_destroy(cp);
    return err;
  }
  // add child
  // struct proc *item = kmalloc(sizeof(struct proc));
  // item = cp;
  kprintf("fork_breakpoint: 1\n");
  array_add(curproc->children, (void *) cp, NULL);
  kprintf("fork_breakpoint: 2\n");
  cp->parent = kmalloc(sizeof(struct proc *));
  if (cp->parent == NULL) {
    proc_destroy(cp);
    return ENOMEM;
  }
  cp->parent = curproc;

  // thread_fork
  kprintf("fork_breakpoint: 3\n");
  struct trapframe *tf_copy = kmalloc(sizeof(struct trapframe));
  if (tf_copy == NULL) {
    proc_destroy(cp);
    return ENOMEM;
  }
  *tf_copy = *tf;
  kprintf("fork_breakpoint: 4\n");
  err = thread_fork(curproc->p_name, cp, fork_entrypoint, tf_copy, 0);
  kprintf("fork_breakpoint: 5\n");
  if (err) {
    kfree(tf_copy);
    as_destroy(cp->p_addrspace);
    proc_destroy(cp);
    return err;
  }
  kprintf("fork_breakpoint: 6\n");
  *retval = cp->p_id;
  return 0;
}
