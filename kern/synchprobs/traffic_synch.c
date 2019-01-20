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
static struct cv *cv_v;
static struct cv *cv_h;

// static volatile int passed_cars = 0;
static volatile int traffic_dir = -1;
static volatile int passed_cars = 0;
static volatile int exited_cars = 0;

void set_traffic_dir(Direction origin);
void set_traffic_dir(Direction origin) {
  if (origin == north || origin == south) traffic_dir = 0;
  else traffic_dir = 1;
}

int is_light_on(Direction origin);
int is_light_on(Direction origin) {
  if ((origin == north || origin == south) && traffic_dir == 0) return 1;
  else if ((origin == east || origin == west) && traffic_dir == 1) return 1;
  return 0;
}

int is_safe(Direction origin, Direction destination);
int is_safe(Direction origin, Direction destination) {
  if ((destination - origin == 1) ||
    (origin + destination == 2) ||
    (origin + destination == 4) ||
    (destination - origin == 3))
    return 1;
  return 0;
}

void red_light(Direction origin);
void red_light(Direction origin) {
  if (origin == north || origin == south) cv_wait(cv_v, intersectionLock);
  else cv_wait(cv_h, intersectionLock);
}

void green_light(Direction origin);
void green_light(Direction origin) {
  if (origin == north || origin == south) cv_broadcast(cv_v, intersectionLock);
  else cv_broadcast(cv_h, intersectionLock);
}

void switch_light(void);
void switch_light() {
  if (!traffic_dir) cv_broadcast(cv_h, intersectionLock);
  else cv_broadcast(cv_v, intersectionLock);
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
  cv_v = cv_create("vertical");
  cv_h = cv_create("horizontal");
  //

  if (intersectionLock == NULL || cv_v == NULL || cv_h == NULL) {
    panic("could not create intersection lock / cv");
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
  KASSERT(cv_v != NULL);
  KASSERT(cv_h != NULL);
  cv_destroy(cv_v);
  cv_destroy(cv_h);

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
  kprintf("B4ENTRY: %d, %d\n", origin, destination);
  /* replace this default implementation with your own implementation */
  // (void)origin;  /* avoid compiler complaint about unused parameter */
  /* avoid compiler complaint about unused parameter */
  KASSERT(intersectionLock != NULL);
  lock_acquire(intersectionLock);
  if (traffic_dir == -1) {
    set_traffic_dir(origin);
  }

  if (is_light_on(origin) && is_safe(origin, destination)) {
    passed_cars += 1;
  }

  while (passed_cars > 3) {
    red_light(origin);
  }

  while (!is_light_on(origin)) {
    red_light(origin);
  }

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
{
  kprintf("AFTEREXIT: %d, %d\n", origin, destination);
  /* replace this default implementation with your own implementation */
  // (void)origin;  /* avoid compiler complaint about unused parameter */
  (void)destination; /* avoid compiler complaint about unused parameter */
  KASSERT(intersectionLock != NULL);
  lock_acquire(intersectionLock);

  exited_cars += 1;
  if (exited_cars == 3 || exited_cars == passed_cars) {
    passed_cars = 0;
    exited_cars = 0;
    switch_light();
  } else {
    green_light(origin);
  }

  lock_release(intersectionLock);
}
