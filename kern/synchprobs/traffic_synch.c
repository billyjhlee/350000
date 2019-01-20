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
static struct cv *cv_n;
static struct cv *cv_e;
static struct cv *cv_w;
static struct cv *cv_s;

// static volatile int passed_cars = 0;
static volatile Direction directions[4];
static volatile int arr_len;
static volatile int north_cars = 0;
static volatile int south_cars = 0;
static volatile int west_cars = 0;
static volatile int east_cars = 0;
static volatile int exited_cars = 0;

#define MAX_ALLOWED = 3;

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
  cv_n = cv_create("north");
  cv_e = cv_create("east");
  cv_w = cv_create("west");
  cv_s = cv_create("south");
  // directions = array_create();
  // array_init(directions);
  //

  if (intersectionLock == NULL || cv_n == NULL || cv_e == NULL || cv_w == NULL || cv_s == NULL) {
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
  KASSERT(cv_n != NULL);
  KASSERT(cv_e != NULL);
  KASSERT(cv_w != NULL);
  KASSERT(cv_s != NULL);
  cv_destroy(cv_n);
  cv_destroy(cv_e);
  cv_destroy(cv_w);
  cv_destroy(cv_s);

  KASSERT(intersectionLock != NULL);
  lock_destroy(intersectionLock);

  // KASSERT(directions != NULL);
  // array_destroy(directions);
}

void remove_element(int index);
void remove_element(int index)
{
   int i;
   for(i = index; i < arr_len - 1; i++) directions[i] = directions[i + 1];
}

void make_signal(Direction origin);
void make_signal(Direction origin) {
    if (origin == north) cv_signal(cv_n, intersectionLock);
    else if (origin == east) cv_signal(cv_e, intersectionLock);
    else if (origin == west) cv_signal(cv_w, intersectionLock);
    else cv_signal(cv_s, intersectionLock);
}

void make_wait(Direction origin);
void make_wait(Direction origin) {
    if (origin == north) cv_wait(cv_n, intersectionLock);
    else if (origin == east) cv_wait(cv_e, intersectionLock);
    else if (origin == west) cv_wait(cv_w, intersectionLock);
    else cv_wait(cv_s, intersectionLock);
}

int prepare_car(Direction origin);
int prepare_car(Direction origin) {
    if (origin == north) return ++north_cars;
    else if (origin == east) return ++east_cars;
    else if (origin == west) return ++west_cars;
    return ++south_cars;
}

int get_cars(Direction origin);
int get_cars(Direction origin) {
    if (origin == north) return north_cars;
    else if (origin == east) return east_cars;
    else if (origin == west) return west_cars;
    return south_cars;
}

void exit_cars(Direction origin, int cars);
void exit_cars(Direction origin, int cars) {
    if (origin == north) north_cars -= cars;
    else if (origin == east) east_cars -= cars;
    else if (origin == west) west_cars -= cars;
    else south_cars -= cars;
}

int waiting_cars(Direction origin);
int waiting_cars(Direction origin) {
    if (origin == north) return east_cars + west_cars + south_cars;
    else if (origin == east)  return north_cars + west_cars + south_cars;
    else if (origin == west)  return north_cars + east_cars + south_cars;
     return north_cars + east_cars + west_cars;
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
  (void)destination; /* avoid compiler complaint about unused parameter */

  KASSERT(intersectionLock != NULL);
  lock_acquire(intersectionLock);
  int index = -1;
  
  for (int i = 0; i < arr_len; i++) {
    if (directions[i] == origin) {
      index = i;
      break;
    }
  }
  if (index != -1) {
    directions[arr_len++] = origin;
  }

  prepare_car(origin);

  while (arr_len > 0 && directions[0] != origin) {
    kprintf("DIRECTION QUEUE NOT EQUAL 0: %d ORIGIN %d\n", directions[0], origin);
    kprintf("SLEEP1 \n");
    make_wait(origin);
  }
  while (arr_len > 0 && directions[0] && exited_cars >= 3){
    kprintf("SLEEP2 \n");
    make_wait(origin);
  }
  for (int i = 0; i < arr_len; i++) {
    kprintf("ARR at %d: %d", i, directions[i]);
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
  exited_cars++;
  exit_cars(origin, 1);
  if (exited_cars == 3 || waiting_cars(origin) >= 3 || get_cars(origin) == 0) {
    remove_element(0);
    if (get_cars(origin) != 0) {
      directions[arr_len - 1] = origin;
    } else arr_len--;
    exited_cars = 0;
  }
  if (arr_len > 0) {
    make_signal(directions[0]);
  } else {
    make_signal(0);
    make_signal(1);
    make_signal(2);
    make_signal(3);
  }
  lock_release(intersectionLock);
}
