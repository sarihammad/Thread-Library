#include "check.h"

#include <stdlib.h>

#include "interrupts.h"
#include "thread.h"


// Functions to pass to ThreadCreate
void
f_no_exit(void)
{
  while (1) {
    ck_assert(InterruptsAreEnabled());
  }
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

// Functions to run before/after every test
void
set_up(void)
{
  ck_assert_int_eq(ThreadInit(), 0);
  InterruptsInit();
}

void
tear_down(void)
{}


START_TEST(test_interupts_stay_enabled)
{
  Tid tid = ThreadCreate((void (*)(void*))f_no_exit, NULL);
  ck_assert_int_gt(tid, 0);
  ck_assert_int_lt(tid, MAX_THREADS);
  ck_assert(InterruptsAreEnabled());

  ThreadSpin(INTERRUPTS_SIGNAL_INTERVAL * 2);
  ck_assert(InterruptsAreEnabled());

  ck_assert_int_eq(ThreadKill(tid), tid);
  ck_assert(InterruptsAreEnabled());

  ThreadSpin(INTERRUPTS_SIGNAL_INTERVAL * 2);
  ck_assert(InterruptsAreEnabled());
}
END_TEST

START_TEST(test_join_created_thread)
{
  Tid const tid = ThreadCreate((void (*)(void*)) f_factorial, (void*) 10);
  ck_assert_int_gt(tid, 0);
  ck_assert_int_lt(tid, MAX_THREADS);

  int exit_value;
  ck_assert_int_eq(ThreadJoin(tid, &exit_value), tid);
  ck_assert_int_eq(exit_value, 0); // thread should have exited successfully
}
END_TEST

START_TEST(test_join_self)
{
  ck_assert_int_eq(ThreadId(), 0);

  int exit_value;
  ck_assert_int_eq(ThreadJoin(0, &exit_value), ERROR_THREAD_BAD);
}
END_TEST

START_TEST(test_join_uncreated_tid)
{
  int exit_value;
  ck_assert_int_eq(ThreadJoin(5, &exit_value), ERROR_SYS_THREAD);
}
END_TEST

START_TEST(test_join_previously_killed)
{
  Tid const tid = ThreadCreate((void (*)(void*))f_no_exit, NULL);
  ck_assert_int_gt(tid, 0);
  ck_assert_int_lt(tid, MAX_THREADS);

  // Turn the newly created thread into a zombie
  ck_assert_int_eq(ThreadKill(tid), tid);

  ThreadSpin(INTERRUPTS_SIGNAL_INTERVAL * 2);

  int exit_value;
  ck_assert_int_eq(ThreadJoin(tid, &exit_value), ERROR_SYS_THREAD);
  ck_assert_int_eq(exit_value, EXIT_CODE_KILL);
}
END_TEST


int
main(void)
{
  TCase* test_case = tcase_create("The Test Case");
  tcase_add_checked_fixture(test_case, set_up, tear_down);
  tcase_add_test(test_case, test_interupts_stay_enabled);
  tcase_add_test(test_case, test_join_created_thread);
  tcase_add_test(test_case, test_join_self);
  tcase_add_test(test_case, test_join_uncreated_tid);
  tcase_add_test(test_case, test_join_previously_killed);

  Suite* suite = suite_create("Student Test Suite");
  suite_add_tcase(suite, test_case);

  SRunner* suite_runner = srunner_create(suite);
  srunner_run_all(suite_runner, CK_VERBOSE);

  srunner_ntests_failed(suite_runner);
  srunner_free(suite_runner);

  return EXIT_SUCCESS;
}
