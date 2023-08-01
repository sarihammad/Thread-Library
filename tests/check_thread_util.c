#include "check_thread_util.h"

#include <stdio.h>
#include <unistd.h>

#include "check.h"

// Functions to run before/after every test
void
set_up(void)
{
  ck_assert_int_eq(ThreadInit(), 0);
}

void
tear_down(void)
{}

// Other definitions
long* array[MAX_THREADS];

int
yield_till_main_thread(void)
{
  int num_yields = 0;

  // Yield until we are back at the main thread
  int result;
  do {
    result = ThreadYield();
    ck_assert_int_ge(result, 0);
    ck_assert_int_lt(result, MAX_THREADS);

    num_yields++;
  } while (result != 0);

  return num_yields;
}

int
yieldto_till_main_thread(int tid)
{
  int num_yields = 0;

  // Yield until we are back at the main thread
  int result;
  do {
    result = ThreadYieldTo(tid);
    num_yields++;
  } while (result != ERROR_THREAD_BAD);

  return num_yields;
}


// Functions to pass to ThreadCreate
void
f_do_nothing(void)
{}

void
f_yield_once(int tid)
{
  ThreadYieldTo(tid);
}

void
f_yield_twice(int tid)
{
  ThreadYieldTo(tid);
  ThreadYieldTo(tid);
}

void
f_no_exit(void)
{
  while (1) {
    ThreadYield();
  }
}

void
f_save_to_array(int x)
{
  array[ThreadId()] = (long*)&x;
}

void
f_fp_alignment(void)
{
  Tid tid = ThreadYieldTo(ThreadId());
  ck_assert_int_gt(tid, 0);
  ck_assert_int_lt(tid, MAX_THREADS);

  // We cast the return value to a float because that helps to check whether the
  // stack alignment of the frame pointer is correct
  char str[20];
  sprintf(str, "%3.0f\n", (float)tid);
  // A failure here would be something like a segmentation fault
}

int
f_factorial(int n)
{
  if (n == 1) {
    return 1;
  }

  ThreadYield();
  return n * f_factorial(n - 1);
}

void
f_0_has_exited(void)
{
  Tid const self = ThreadId();

  int const yield1 = ThreadYield();
  ck_assert_int_eq(yield1, self);

  int const yield2 = ThreadYieldTo(0);
  ck_assert_int_eq(yield2, ERROR_THREAD_BAD);

  // Make sure the whole process did not exit prematurely
  // (may result in leaks)
  _exit(TESTS_EXIT_SUCCESS);
}
