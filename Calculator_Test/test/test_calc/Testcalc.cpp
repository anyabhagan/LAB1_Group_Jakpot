#include <Arduino.h>
#include <stddef.h>
#include <unity.h>
#include "calc.h"

// Test addition
void test_add(void)
{
    int output = add(10, 20);
    TEST_ASSERT_EQUAL(30, output);
}

// Test subtraction
void test_sub(void)
{
    int output = sub(20, 10);
    TEST_ASSERT_EQUAL(10, output);
}

// Test multiplication
void test_mul(void)
{
    int output = mul(10, 2);
    TEST_ASSERT_EQUAL(20, output);
}

// Test multiplication overflow
void test_mul_overflow(void)
{
    int output = mul(10000000, 999999999);
    TEST_ASSERT_EQUAL(-1, output);
}

// Test multiplication underflow
void test_mul_underflow(void)
{
    int output = mul(-10000000, 99999999);
    TEST_ASSERT_EQUAL(-1, output);
}

// Test division
void test_div(void)
{
    int output = calc_div(10, 0);
    TEST_ASSERT_EQUAL(0, output);
}

// Test modulus
void test_mod(void)
{
    TEST_ASSERT_EQUAL(0, mod(10, 2));
    TEST_ASSERT_EQUAL(1, mod(10, 3));
}

// Test power of two
void test_power_two(void)
{
    TEST_ASSERT_EQUAL(9, power_two(3));
}

// Test power overflow
void test_power_two_overflow(void)
{
    TEST_ASSERT_EQUAL(-1, power_two(1000000));
}

// Modified test_power()
// Demonstrates an integer value exceeding
// the range of an 8-bit signed integer
void test_power(void)
{
    TEST_ASSERT_EQUAL(8, power(2, 3));
    TEST_ASSERT_EQUAL_INT(256, power(2, 8));
}

// Test greater than
void test_is_greater(void)
{
    TEST_ASSERT_TRUE(is_greater(20, 10));
    TEST_ASSERT_FALSE(is_greater(10, 20));
}

// Test equality
void test_is_equal(void)
{
    TEST_ASSERT_TRUE(is_equal(10, 10));
    TEST_ASSERT_FALSE(is_equal(10, 9));
}

// Test smaller than
void test_is_smaller(void)
{
    TEST_ASSERT_TRUE(is_smaller(9, 10));
    TEST_ASSERT_FALSE(is_smaller(10, 9));
}

// Test array filling
void test_fill_single_digit_positive_number(void)
{
    int expected_arr[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

    size_t expected_size =
        sizeof(expected_arr) / sizeof(expected_arr[0]);

    int arr[9];

    size_t arr_size = sizeof(arr) / sizeof(arr[0]);

    fill_single_digit_positive_number(arr, arr_size);

    TEST_ASSERT_EQUAL_INT_ARRAY(
        expected_arr,
        arr,
        expected_size
    );

    TEST_ASSERT_EQUAL_MEMORY(
        expected_arr,
        arr,
        sizeof(expected_arr)
    );
}

// Test true string
void test_true_as_string(void)
{
    TEST_ASSERT_EQUAL_STRING("true", true_as_string());

    TEST_ASSERT_EQUAL_STRING_LEN(
        "truz",
        true_as_string(),
        3
    );

    TEST_ASSERT_NOT_NULL(true_as_string());

    TEST_ASSERT_EQUAL_MEMORY(
        "true",
        true_as_string(),
        4
    );
}

// Test false string
void test_false_as_string(void)
{
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        "false",
        false_as_string(),
        "The output should be 'false'"
    );

    TEST_ASSERT_EQUAL_STRING_LEN_MESSAGE(
        "falze",
        false_as_string(),
        3,
        "First three characters should be 'fal'"
    );
}

// Unity setup functions
void setUp(void)
{
}

void tearDown(void)
{
}

// Arduino test runner
void setup()
{
    Serial.begin(9600);

    unsigned long startTime = millis();

    while (!Serial && (millis() - startTime < 5000UL))
    {
    }

    delay(200);

    UNITY_BEGIN();

    RUN_TEST(test_add);
    RUN_TEST(test_sub);
    RUN_TEST(test_mul);
    RUN_TEST(test_mul_overflow);
    RUN_TEST(test_mul_underflow);
    RUN_TEST(test_div);
    RUN_TEST(test_mod);
    RUN_TEST(test_power_two);
    RUN_TEST(test_power_two_overflow);
    RUN_TEST(test_power);
    RUN_TEST(test_is_greater);
    RUN_TEST(test_is_equal);
    RUN_TEST(test_is_smaller);
    RUN_TEST(test_fill_single_digit_positive_number);
    RUN_TEST(test_true_as_string);
    RUN_TEST(test_false_as_string);

    UNITY_END();
}

void loop()
{
}