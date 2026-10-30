#include <Arduino.h>
#include "calc.h"

void setup()
{
	Serial.begin(115200);
	while (!Serial) {
	}

	Serial.print("10 + 2 = ");          Serial.println(add(10, 2));
	Serial.print("10 - 2 = ");          Serial.println(sub(10, 2));
	Serial.print("10 * 2 = ");          Serial.println(mul(10, 2));
	Serial.print("10 / 2 = ");          Serial.println(divide(10, 2));
	Serial.print("10 % 2 = ");          Serial.println(mod(10, 2));
	Serial.print("10 power two = ");    Serial.println(power_two(10));
	Serial.print("10 power three = ");  Serial.println(power(10, 3));
	Serial.print("posnum(10) = ");      Serial.println(posnum(10) ? "TRUE" : "FALSE");
	Serial.print("posnum(-10) = ");     Serial.println(posnum(-10) ? "TRUE" : "FALSE");

	int arr[10];
	size_t size = sizeof(arr) / sizeof(arr[0]);
	fill_single_digit_positive_number(arr, size);
}

void loop()
{
}
