#ifndef CALC_H
#define CALC_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int add(int x, int y);
int sub(int x, int y);
int mul(int x, int y);
int divide(int x, int y);
int mod(int x, int y);
int power_two(int x);
int power(int x, int n);
bool is_equal(int x, int y);
bool is_greater(int x, int y);
bool is_smaller(int x, int y);
void fill_single_digit_positive_number(int arr[], size_t size);
const char *true_as_string();
const char *false_as_string();
bool posnum(int x);

#ifdef __cplusplus
}
#endif

#endif
