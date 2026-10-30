#include <Arduino.h>
#include "calc.h"

// Part 2: Calculator Application
// Target Platform: Arduino MKR Zero

void setup()
{
    Serial.begin(9600);

    unsigned long startTime = millis();

    while (!Serial && (millis() - startTime < 5000UL))
    {
    }

    Serial.println("Calculator Application");
    Serial.println("----------------------");

    Serial.print("10 + 2 = ");
    Serial.println(add(10, 2));

    Serial.print("10 - 2 = ");
    Serial.println(sub(10, 2));

    Serial.print("10 * 2 = ");
    Serial.println(mul(10, 2));

    Serial.print("10 / 2 = ");
    Serial.println(calc_div(10, 2));

    Serial.print("10 % 2 = ");
    Serial.println(mod(10, 2));

    Serial.print("10 power two = ");
    Serial.println(power_two(10));

    Serial.print("10 power three = ");
    Serial.println(power(10, 3));

    // Demonstrate the new magnitude function
    Serial.print("Magnitude of -10 = ");
    Serial.println(magnitude(-10));

    int arr[10];

    size_t size = sizeof(arr) / sizeof(arr[0]);

    fill_single_digit_positive_number(arr, size);

    Serial.println("Application completed.");
}

void loop()
{
    // No repeated calculations required
}