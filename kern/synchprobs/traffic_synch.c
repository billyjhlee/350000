#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
static struct lock *intersectionLock;
// static struct cv *cv_n;
// static struct cv *cv_s;
// static struct cv *cv_e;
// static struct cv *cv_w;

static volatile cv *cv_arr[4];

// static volatile int passed_cars = 0;
static volatile Direction direction_queue[4];
static volatile int arr_len = 0;
// static volatile int passed_cars = 0;
static volatile int exited_cars = 0;
static volatile int entered_cars = 0;
static volatile int waiting_cars = 0;
static volatile int leftover = 0;

void remove_element(int index);
void remove_element(int index)
{
   int i;
   for(i = index; i < arr_len - 1; i++) direction_queue[i] = direction_queue[i + 1];
}

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */

  // intersectionSem = sem_create("intersectionSem",1);
  // if (intersectionSem == NULL) {
  //   panic("could not create intersection semaphore");
  // }
  intersectionLock = lock_create("intersectionLock");
  // cv_n = cv_create("n");
  // cv_e = cv_create("e");
  // cv_s = cv_create("s");
  // cv_w = cv_create("w");

  for (int i = 0; i < 4; i++) {
    cv_arr[i] = cv_create(""+i);
    if (cv_arr[i] == NULL) {
      panic('failed to create a cv');
    }
  }

  if (intersectionLock == NULL) {
    panic("could not create intersection lock");
  }
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  for (int i = 0; i < 4; i++) {
    KASSERT(cv_arr[i] != NULL);
    cv_destroy(cv_arr[i]);
  }

  KASSERT(intersectionLock != NULL);
  lock_destroy(intersectionLock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  // (void)origin;  /* avoid compiler complaint about unused parameter */
  /* avoid compiler complaint about unused parameter */
  (void)destination; /* avoid compiler complaint about unused parameter */
  KASSERT(intersectionLock != NULL);
  lock_acquire(intersectionLock);
  // kprintf("B4ENTRY: %d, %d\n", origin, destination);

  int origin_in_queue = 0;
  for (int i = 0; i < arr_len; i++) {
    if (direction_queue[i] == origin) {
      origin_in_queue = 1;
      break;
    }
  }
  if (!origin_in_queue) {
    direction_queue[arr_len++] = origin;
  }

  if (direction_queue[0] != origin) {
    waiting_cars++;
  }

  while (arr_len > 0) {
    if (direction_queue[0] == origin) {
      if (entered_cars < 4 || waiting_cars < 4) {
        break; 
      } 
      leftover = 1;
    } 
    cv_wait(cv_arr[origin], intersectionLock);
  }

  entered_cars++;

  lock_release(intersectionLock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{  /* replace this default implementation with your own implementation */
  // (void)origin;  /* avoid compiler complaint about unused parameter */
  (void)destination; /* avoid compiler complaint about unused parameter */
  KASSERT(intersectionLock != NULL);
  lock_acquire(intersectionLock);

  exited_cars++;
  if ((exited_cars - entered_cars) == 0) {
    remove_element(0);
    if (leftover) {
      direction_queue[arr_len-1] = origin;
    } else arr_len -= 1;
    leftover = 0;
    exited_cars = 0;
    entered_cars = 0;
    waiting_cars = 0;
    if (arr_len > 0) {
      cv_broadcast(cv_arr[direction_queue[0]], intersectionLock);
    }
  } else {
    cv_broadcast(cv_arr[origin], intersectionLock);
  }

  lock_release(intersectionLock);
}
