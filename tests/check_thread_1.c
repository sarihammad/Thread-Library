#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>

#include "check.h"
#include "check_thread_util.h"

// Test for functionality when there are two threads (including main)
static int
set_flag(int val)
{
  static int flag_value;
  return __sync_lock_test_and_set(&flag_value, val);
}

void
f_set_flag_and_exit(void)
{
  int const old_flag = set_flag(1);
  ck_assert_int_eq(old_flag, 0);

  ThreadExit(0);
  ck_assert_msg(0, "This thread should have exited.");
}

START_TEST(test_create_thread)
{
  int const tid = ThreadCreate((void (*)(void*))f_do_nothing, NULL);
  ck_assert_int_ge(tid, 1);
  ck_assert_int_lt(tid, MAX_THREADS);

  _exit(TESTS_EXIT_SUCCESS);
}

START_TEST(test_f_do_nothing)
{
  int const tid = ThreadCreate((void (*)(void*))f_do_nothing, NULL);
  ck_assert_int_ge(tid, 1);
  ck_assert_int_lt(tid, MAX_THREADS);

  // Yield until we are back at the main thread
  ck_assert_int_eq(yieldto_till_main_thread(tid), 2);

  _exit(TESTS_EXIT_SUCCESS);
}

START_TEST(test_f_yield_once)
{
  int const tid = ThreadCreate((void (*)(void*))f_yield_once, (void*)0);
  ck_assert_int_ge(tid, 1);
  ck_assert_int_lt(tid, MAX_THREADS);

  // Yield until we are back at the main thread
  ck_assert_int_eq(yieldto_till_main_thread(tid), 3);

  _exit(TESTS_EXIT_SUCCESS);
}

START_TEST(test_f_yield_twice)
{
  int const tid = ThreadCreate((void (*)(void*))f_yield_twice, (void*)0);
  ck_assert_int_ge(tid, 1);
  ck_assert_int_lt(tid, MAX_THREADS);

  // Yield until we are back at the main thread
  ck_assert_int_eq(yieldto_till_main_thread(tid), 4);

  _exit(TESTS_EXIT_SUCCESS);
}

START_TEST(test_f_factorial)
{
  int const tid = ThreadCreate((void (*)(void*))f_factorial, (void*)10);
  ck_assert_int_ge(tid, 1);
  ck_assert_int_lt(tid, MAX_THREADS);

  // Yield until we are back at the main thread
  ck_assert_int_eq(yieldto_till_main_thread(tid), 11);

  _exit(TESTS_EXIT_SUCCESS);
}

START_TEST(test_f_set_flag_and_exit)
{
  set_flag(0);
  int const tid =
      ThreadCreate((void (*)(void*))f_set_flag_and_exit, NULL);
  ck_assert_int_ge(tid, 1);
  ck_assert_int_lt(tid, MAX_THREADS);

  int const yield_tid = ThreadYieldTo(tid);
  ck_assert_int_eq(yield_tid, tid);
  ck_assert_int_eq(set_flag(0), 1);

  int const yield_tid2 = ThreadYieldTo(tid);
  ck_assert_int_eq(yield_tid2, ERROR_THREAD_BAD);

  _exit(TESTS_EXIT_SUCCESS);
}

START_TEST(test_f_0_has_exited)
{
  set_flag(0);
  int const tid = ThreadCreate((void (*)(void*))f_0_has_exited, NULL);
  ck_assert_int_ge(tid, 1);
  ck_assert_int_lt(tid, MAX_THREADS);

  ThreadExit(0);
  ck_assert_msg(0, "This thread should have exited.");
}

START_TEST(test_f_no_exit)
{
  set_flag(0);
  int const tid = ThreadCreate((void (*)(void*))f_no_exit, NULL);
  ck_assert_int_ge(tid, 1);
  ck_assert_int_lt(tid, MAX_THREADS);
  ck_assert_int_eq(ThreadKill(tid), tid);

  _exit(TESTS_EXIT_SUCCESS);
}

TCase* CreateTwoThreadsCase(void)
{
  TCase* test_case = tcase_create("Two Threads Case");
  tcase_add_checked_fixture(test_case, set_up, tear_down);
  tcase_add_exit_test(test_case, test_create_thread, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_f_do_nothing, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_f_yield_once, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_f_yield_twice, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_f_factorial, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_f_set_flag_and_exit, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_f_0_has_exited, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_f_no_exit, TESTS_EXIT_SUCCESS);

  return test_case;
}


// Test for issues related to memory
START_TEST(test_dynamically_allocates_stack)
{
  struct mallinfo info = mallinfo();
  int const allocated_space = info.uordblks;

  int new_tid = ThreadCreate((void (*)(void*))f_do_nothing, NULL);
  ck_assert_int_gt(new_tid, 0);
  ck_assert_int_lt(new_tid, MAX_THREADS);

  info = mallinfo();
  ck_assert_int_gt(info.uordblks, allocated_space);

  _exit(TESTS_EXIT_SUCCESS);
}
END_TEST

START_TEST(test_stacks_sufficiently_apart)
{
  // Create a variable on the stack
  int x = 5;
  // Save the address of that variable
  array[ThreadId()] = (long*)&x;

  for (int i = 0; i < MAX_THREADS - 1; i++) {
    Tid tid = ThreadCreate((void (*)(void*))f_save_to_array, (void*)4);

    ck_assert_int_gt(tid, 0);
    ck_assert_int_lt(tid, MAX_THREADS);
  }

  yield_till_main_thread();

  // Pairwise comparison of stack addresses to make sure they are sufficiently
  // far apart
  for (int tid_a = 0; tid_a < MAX_THREADS; tid_a++) {
    for (int tid_b = 0; tid_b < MAX_THREADS; tid_b++) {
      if (tid_a != tid_b) {
        long const stack_sep = (long)(array[tid_a]) - (long)(array[tid_b]);
        ck_assert_int_ge(labs(stack_sep), THREAD_STACK_SIZE);
      }
    }
  }

  _exit(TESTS_EXIT_SUCCESS);
}
END_TEST

START_TEST(test_fp_alignment)
{
  int new_tid = ThreadCreate((void (*)(void*))f_fp_alignment, NULL);
  ck_assert_int_gt(new_tid, 0);
  ck_assert_int_lt(new_tid, MAX_THREADS);

  int yield_tid = ThreadYieldTo(new_tid);
  ck_assert_int_eq(new_tid, yield_tid);

  _exit(TESTS_EXIT_SUCCESS);
}
END_TEST


// Public Interface
TCase* CreateMemoryCase(void)
{
  TCase* test_case = tcase_create("Memory Case");
  tcase_add_checked_fixture(test_case, set_up, tear_down);
  tcase_add_exit_test(test_case, test_dynamically_allocates_stack, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_stacks_sufficiently_apart, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_fp_alignment, TESTS_EXIT_SUCCESS);

  return test_case;
}


// Test behaviour when the maximum number of threads are created
START_TEST(test_create) {
  for (int i = 0; i < MAX_THREADS - 1; i++) {
    Tid tid = ThreadCreate((void (*)(void *)) f_yield_once, (void *) 0);

    ck_assert_int_gt(tid, 0);
    ck_assert_int_lt(tid, MAX_THREADS);
  }

  _exit(TESTS_EXIT_SUCCESS);
}

START_TEST(test_create_more_than_max)
{
  for (int i = 0; i < MAX_THREADS - 1; i++) {
    Tid tid = ThreadCreate((void (*)(void*))f_yield_once, (void*)0);

    ck_assert_int_gt(tid, 0);
    ck_assert_int_lt(tid, MAX_THREADS);
  }

  // Now we are out of threads. Next create should fail.
  Tid tid = ThreadCreate((void (*)(void*))f_yield_once, (void*)0);
  ck_assert_int_eq(tid, ERROR_SYS_THREAD);

  _exit(TESTS_EXIT_SUCCESS);
}
END_TEST

START_TEST(test_create_yield) {
  for (int i = 0; i < MAX_THREADS - 1; i++) {
    Tid tid = ThreadCreate((void (*)(void *)) f_yield_once, (void *) 0);

    ck_assert_int_gt(tid, 0);
    ck_assert_int_lt(tid, MAX_THREADS);
  }

  // Yield until we are back at the main thread
  yield_till_main_thread();

  _exit(TESTS_EXIT_SUCCESS);
}

START_TEST(test_create_yield_recreate)
{
  for (int i = 0; i < MAX_THREADS - 1; i++) {
    Tid tid = ThreadCreate((void (*)(void*))f_yield_once, (void*)0);

    ck_assert_int_gt(tid, 0);
    ck_assert_int_lt(tid, MAX_THREADS);
  }

  // Yield until we are back at the main thread
  yield_till_main_thread();

  // Recreate the maximum number of threads, minus one
  for (int i = 0; i < MAX_THREADS - 1; i++) {
    int new_tid = ThreadCreate((void (*)(void*))f_yield_once, (void*)0);

    ck_assert_int_gt(new_tid, 0);
    ck_assert_int_lt(new_tid, MAX_THREADS);
  }

  _exit(TESTS_EXIT_SUCCESS);
}
END_TEST

START_TEST(test_create_yield_kill)
{
  Tid children[MAX_THREADS - 1];

  for (int i = 0; i < MAX_THREADS - 1; i++) {
    // Create a thread that yield back to the main thread (TID 0)
    children[i] = ThreadCreate((void (*)(void*))f_yield_twice, (void*)0);

    ck_assert_int_gt(children[i], 0);
    ck_assert_int_lt(children[i], MAX_THREADS);
  }

  // Let all threads yield back to the main thread
  for (int i = 0; i < MAX_THREADS; i++) {
    int result = ThreadYield();

    ck_assert_int_ge(result, 0);
    ck_assert_int_lt(result, MAX_THREADS);
  }

  // Kill all non-main threads
  for (int i = 0; i < MAX_THREADS - 1; i++) {
    int const tid = children[i];
    ThreadKill(tid);
  }

  // Yield until we are back at the main  thread, in case killed threads need to
  // run and exit
  int i = yield_till_main_thread();

  // Should only need to yield at most MAX_THREADS - 1 + 2 times.
  ck_assert_int_le(i, MAX_THREADS + 1);

  _exit(TESTS_EXIT_SUCCESS);
}
END_TEST

START_TEST(test_create_kill_yieldto)
{
  Tid children[MAX_THREADS - 1];

  for (int i = 0; i < MAX_THREADS - 1; i++) {
    // Create a thread that yield back to the main thread (TID 0)
    children[i] = ThreadCreate((void (*)(void*))f_yield_twice, (void*)0);

    ck_assert_int_gt(children[i], 0);
    ck_assert_int_lt(children[i], MAX_THREADS);
  }

  // Kill all non-main threads
  for (int i = 0; i < MAX_THREADS - 1; i++) {
    int const tid_to_kill = children[i];
    ThreadKill(tid_to_kill);
  }

  // Let all threads yield back to the main thread
  for (int i = 0; i < MAX_THREADS - 1; i++) {
    int yield_to_result = ThreadYieldTo(children[i]);
    ck_assert(yield_to_result == children[i] || yield_to_result == ERROR_THREAD_BAD);
  }

  _exit(TESTS_EXIT_SUCCESS);
}
END_TEST


TCase* CreateMaxThreadsCase(void)
{
  TCase* test_case = tcase_create("Maximum Threads Case");
  tcase_add_checked_fixture(test_case, set_up, tear_down);
  tcase_add_exit_test(test_case, test_create, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_create_more_than_max, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_create_yield, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_create_yield_recreate, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_create_yield_kill, TESTS_EXIT_SUCCESS);
  tcase_add_exit_test(test_case, test_create_kill_yieldto, TESTS_EXIT_SUCCESS);

  return test_case;
}


int
main(void)
{
  Suite* suite = suite_create("Test Suite 1");
  suite_add_tcase(suite, CreateTwoThreadsCase());
  suite_add_tcase(suite, CreateMemoryCase());
  suite_add_tcase(suite, CreateMaxThreadsCase());

  SRunner* suite_runner = srunner_create(suite);
  srunner_run_all(suite_runner, CK_VERBOSE);

  srunner_ntests_failed(suite_runner);
  srunner_free(suite_runner);

  return EXIT_SUCCESS;
}
