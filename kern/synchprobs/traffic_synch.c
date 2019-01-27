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
static struct cv *cv_s;
static struct cv *cv_e;
static struct cv *cv_w;

// static volatile int passed_cars = 0;
static volatile Direction direction_queue[4];
static volatile int arr_len = 0;
// static volatile int passed_cars = 0;
static volatile int exited_cars = 0;
static volatile int entered_cars = 0;
static volatile int north_cars = 0;
static volatile int east_cars = 0;
static volatile int west_cars = 0;
static volatile int south_cars = 0;

void remove_element(int index);
void remove_element(int index)
{
   int i;
   for(i = index; i < arr_len - 1; i++) direction_queue[i] = direction_queue[i + 1];
}

void make_signal(Direction origin);
void make_signal(Direction origin) {
    if (origin == north) cv_broadcast(cv_n, intersectionLock);
    else if (origin == east) cv_broadcast(cv_e, intersectionLock);
    else if (origin == west) cv_broadcast(cv_w, intersectionLock);
    else cv_broadcast(cv_s, intersectionLock);
}

void make_wait(Direction origin);
void make_wait(Direction origin) {
    if (origin == north) cv_wait(cv_n, intersectionLock);
    else if (origin == east) cv_wait(cv_e, intersectionLock);
    else if (origin == west) cv_wait(cv_w, intersectionLock);
    else cv_wait(cv_s, intersectionLock);
}

int pass_car(Direction origin);
int pass_car(Direction origin) {
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

int all_cars_left();
int all_cars_left() {
  return (entered_cars - exited_cars) == 0;
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
  cv_n = cv_create("n");
  cv_e = cv_create("e");
  cv_s = cv_create("s");
  cv_w = cv_create("w");
  //

  if (intersectionLock == NULL || cv_n == NULL || cv_e == NULL ||
    cv_s == NULL || cv_w == NULL) {
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
  KASSERT(cv_s != NULL);
  KASSERT(cv_w != NULL);
  cv_destroy(cv_n);
  cv_destroy(cv_e);
  cv_destroy(cv_s);
  cv_destroy(cv_w);

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
  KASSERT(intersectionLock != NULL);
  lock_acquire(intersectionLock);
  kprintf("B4ENTRY: %d, %d\n", origin, destination);

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

  pass_car(origin);

  if (direction_queue[0] != origin) {
    kprintf("CURRENT DIRECTION: %d, ORIGIN: %d\n", direction_queue[0], origin);
  }

  while (get_cars(origin) > 3) {
    make_wait(origin);
  }

  while (arr_len > 0 && direction_queue[0] != origin) {
    make_wait(origin);
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
  kprintf("AFTEREXIT: %d, %d\n", origin, destination);

  exited_cars += 1;
  int passed_cars = get_cars(origin);
  int int_empty = all_cars_left();
  if (int_empty || (int_empty && waiting_cars(origin) > 2)) {
    remove_element(0);
    exit_cars(origin, exited_cars);
    if (get_cars(origin) > 0) {
      direction_queue[arr_len-1] = origin;
    } else arr_len -= 1;
    exited_cars = 0;
    entered_cars = 0;
    kprintf("ARR_LEN22 %d\n", arr_len);
    if (arr_len > 0) {
      kprintf("OPEN DIRECTION: %d\n", direction_queue[0]);
      make_signal(direction_queue[0]);
    }
  } else {
    kprintf("BROADCAST ORIGIN\n");
    make_signal(origin);
  }

  lock_release(intersectionLock);
}
