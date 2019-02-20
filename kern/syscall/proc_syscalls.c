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

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {
  kprintf("exit1\n");
  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  curproc->p_exit_code = exitcode;
  curproc->p_exited = true;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  kprintf("exit2\n");

  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);
  kprintf("exit3\n");


  V(curproc->p_sem);
  kprintf("exit4\n");

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);
  kprintf("exit5\n");

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  kprintf("exit6\n");

  thread_exit();
  kprintf("exit7\n");

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

  kprintf("wait1\n");
  result = proc_should_wait(pid, curproc);
  if (!result) {
    return proc_echild_or_esrch(pid);
  }
  kprintf("wait2\n");

  struct proc *child = (struct proc *) array_get(curproc->children, result);

  // ?
  if (!child->p_exited) {
    P(child->p_sem);
  }
  kprintf("wait3\n");


  /* for now, just pretend the exitstatus is 0 */
  exitstatus = curproc->p_exit_code;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  kprintf("wait4\n");

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

  // kprintf("BP1\n");

  int err = as_copy(curproc->p_addrspace, &cp->p_addrspace);
  // as_copy will return 0 if successful
  // so if err != 0 it was unsuccessful
  if (err) {
    proc_destroy(cp);
    return err;
  }

  // kprintf("BP2\n");

  // assign pid
  err = proc_find_p_id(&cp->p_id);
  // kprintf("BP*2\n");

  if (err) {
    // kprintf("BP&23\n");

    proc_destroy(cp);
    return err;
  }

  // kprintf("BP3\n");

  // add child
  struct proc *item = kmalloc(sizeof(struct proc));
  *item = *cp;
  array_add(curproc->children, (void *) item, NULL);

  // kprintf("BP4\n");

  // thread_fork
  struct trapframe *tf_copy = kmalloc(sizeof(struct trapframe));
  if (tf_copy == NULL) {
    kfree(item);
    proc_destroy(cp);
    return ENOMEM;
  }
  *tf_copy = *tf;

  // kprintf("BP5\n");

  err = thread_fork(curproc->p_name, cp, fork_entrypoint, tf_copy, 0);

  if (err) {
    // kprintf("BP6\n");

    kfree(item);
    kfree(tf_copy);
    as_destroy(cp->p_addrspace);
    proc_destroy(cp);
    return err;
  }
  // kprintf("BP7\n");

  *retval = cp->p_id;
  return 0;
}
