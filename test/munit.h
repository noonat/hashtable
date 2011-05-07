/* this comes from: https://gist.github.com/236001 */

#ifndef __munit_h__
#define __munit_h__

#include <stdio.h>

#define mu_color_code 0x1b
#define mu_color_red "[1;31m"
#define mu_color_green "[1;32m"
#define mu_color_reset "[0m"

#define mu_print_fail(file, line, expr) \
  printf("%s:%u: failed assertion `%s'\n", file, line, expr)

#define mu_print_test(test, passed) \
  printf("test: %-10s ... %c%s%c%s\n", test, mu_color_code, \
    (passed) ? mu_color_green "pass" : mu_color_red "fail", \
    mu_color_code, mu_color_reset)

#define mu_print_results() \
  if (mu_num_failures) { \
    printf("%c%sFAILED%c%s (failed: %d, passed: %d, total: %d)\n", \
      mu_color_code, mu_color_red, mu_color_code, mu_color_reset, \
      mu_num_asserts - mu_num_failures, mu_num_failures, mu_num_asserts); \
  } else { \
    printf("%c%sPASSED%c%s (total asserts: %d)\n", \
      mu_color_code, mu_color_green, mu_color_code, mu_color_reset, mu_num_asserts); \
  }

#define mu_assert(expr) \
  { \
    mu_num_asserts++; \
    if(!(expr)) { \
      ++mu_num_failures; \
      mu_print_fail(__FILE__, __LINE__, #expr); \
    } \
  }

#define mu_test(test) \
  { \
    int f = mu_num_failures; \
    ++mu_num_tests; \
    test(); \
    mu_print_test(#test, (f == mu_num_failures)); \
  }

extern int mu_num_asserts;
extern int mu_num_failures;
extern int mu_num_tests;

#endif /* __munit_h__ */
