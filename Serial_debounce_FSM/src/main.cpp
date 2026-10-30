
#include <Arduino.h>

// ECNG3006 - Question 7
// Method: Switch-Case FSM
// Board: Arduino MKR Zero

#define VALID_CHAR 'A'
#define DEBOUNCE_TIME 500UL

// Define the two FSM states
enum FSM_State
{
    STATE_OFF,
    STATE_ON
};

// Initial state
FSM_State currentState = STATE_OFF;

// Time of the last accepted character
unsigned long lastAcceptedTime = 0;

// Tracks whether any valid character was accepted
bool firstCharacter = true;

void setup()
{
    // Configure onboard LED
    pinMode(LED_BUILTIN, OUTPUT);

    // LED initially OFF
    digitalWrite(LED_BUILTIN, LOW);

    // Begin USB serial communication
    Serial.begin(9600);
    Serial1.begin(9600);


    // Wait briefly for Serial Monitor
    unsigned long startTime = millis();

    while (!Serial &&
           (millis() - startTime < 5000UL))
    {
    }

    Serial.println("ECNG3006 Question 7");
    Serial.println("Switch-Case FSM");
    Serial.println("Send A to toggle the LED.");
}

void loop()
{
    // Check whether a character is available
    if (Serial.available() > 0)
    {
        char receivedChar = Serial.read();
        if (receivedChar != '\r' && receivedChar != '\n')
{
    Serial1.write(receivedChar);
}

        // Check for valid character
        if (receivedChar == VALID_CHAR)
        {
            unsigned long currentTime = millis();

            // Check debounce interval
            if (firstCharacter ||
                (currentTime - lastAcceptedTime >= DEBOUNCE_TIME))
            {
                // Switch-case FSM
                switch (currentState)
                {
                    case STATE_OFF:
                        currentState = STATE_ON;
                        digitalWrite(LED_BUILTIN, HIGH);
                        Serial.println("OFF -> ON");
                        break;

                    case STATE_ON:
                        currentState = STATE_OFF;
                        digitalWrite(LED_BUILTIN, LOW);
                        Serial.println("ON -> OFF");
                        break;

                    default:
                        currentState = STATE_OFF;
                        digitalWrite(LED_BUILTIN, LOW);
                        break;
                }

                lastAcceptedTime = currentTime;
                firstCharacter = false;
            }
            else
            {
                Serial.println("Ignored: within 500 ms");
            }
        }
        else if (receivedChar != '\r' &&
                 receivedChar != '\n')
        {
            Serial.println("Invalid character ignored");
        }
    }
}
