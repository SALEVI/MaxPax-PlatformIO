#include "physButton.h"
#include "../sendToSupabaseWrite/sendToSupabaseWrite.h"

int buttonState = LOW;              // the current reading from the input pin
int lastButtonState = LOW;          // the previous reading from the input pin
unsigned long lastDebounceTime = 0; // the last time the output pin was toggled
unsigned long debounceDelay = 100;  // the debounce time; increase if the output flickers
bool buttonPressed = false;         // indicates if the button was pressed
bool outputState = false;           // the current state of the output

void physButton()
{
  buttonState = digitalRead(buttonPin);

  if (buttonState == HIGH && lastButtonState == LOW)
  {
    buttonPressed = true;
  }

  lastButtonState = buttonState;

  if (buttonPressed && millis() - lastDebounceTime > debounceDelay)
  {
    outputState = !outputState;
    buttonPressed = false;
    lastDebounceTime = millis();

    if (outputState)
    {
      sendToSupabaseWrite("led1", "status", "on");
      sendToSupabaseWrite("ldr", "status", "on");
    }
    else
    {
      sendToSupabaseWrite("led1", "status", "off");
      sendToSupabaseWrite("ldr", "status", "off");
    }
  }
}