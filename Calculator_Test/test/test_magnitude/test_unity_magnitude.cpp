#include <Arduino.h>
#include <unity.h>
#include "calc.h"

// Unity setup functions
void setUp(void)
{
}

void tearDown(void)
{
}

// Unit test for the magnitude function
void test_magnitude(void)
{
    // Negative number
    TEST_ASSERT_EQUAL_UINT(5U, magnitude(-5));

    // Positive number
    TEST_ASSERT_EQUAL_UINT(8U, magnitude(8));

    // Zero
    TEST_ASSERT_EQUAL_UINT(0U, magnitude(0));
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

    RUN_TEST(test_magnitude);

    UNITY_END();
}

void loop()
{
}