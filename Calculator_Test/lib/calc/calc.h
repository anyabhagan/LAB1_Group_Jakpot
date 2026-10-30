#ifndef CALC_H
#define CALC_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// New magnitude function
unsigned int magnitude(int x);

// Calculator functions
int add(int x, int y);
int sub(int x, int y);
int mul(int x, int y);
int calc_div(int x, int y);
int mod(int x, int y);
int power_two(int x);
int power(int x, int n);

// Comparison functions
bool is_equal(int x, int y);
bool is_greater(int x, int y);
bool is_smaller(int x, int y);

// Array function
void fill_single_digit_positive_number(int arr[], size_t size);

// String functions
const char *true_as_string(void);
const char *false_as_string(void);

#ifdef __cplusplus
}
#endif

#endif