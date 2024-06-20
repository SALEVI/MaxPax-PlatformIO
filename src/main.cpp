#include <Arduino.h>
#include <WiFi.h>
#include <ESP32_Supabase.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include "connectToWifi/connectToWifi.h"
#include "sendToSupabaseRead/sendToSupabaseRead.h"
#include "sendToSupabaseWrite/sendToSupabaseWrite.h"
#include "physButton/physButton.h"
#include "confidential.h"

#define WIFI_TIMEOUT_MS 20000
// RFID
#define SS_PIN 5
#define RST_PIN 17
#define KEYPAD_ADDR 0x20
#define LCD_ADDR 0x27

const int BAUD_RATE = 115200;
const char *readJSON;
const int motionPin = 16;
const int vibrationPin = 35;
const int magneticPin = 14;
const int buzzerPin = 4;
const int lcdColumns = 16;
const int lcdRows = 2;
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte rowPins[ROWS] = {0, 1, 2, 3};
byte colPins[COLS] = {4, 5, 6, 7};
int vibrationValue;
int vibrationThreshold = 0;
int magneticValue;
int sirenStatus = 1;
int rfidStatus = 1;
int keypadStatus = 1;
int vibrationStatus = 1;
int magneticStatus = 1;
int motionStatus = 1;
int rfidAccess = 0;
int keypadAccess = 0;
bool isAccessGranted = false;
unsigned long rfidAccessTimestamp = 0;
unsigned long keypadAccessTimestamp = 0;
Supabase db;                  // Rest of supabase data confidential
String table = "sensor_data"; // Target table

// Instance of the classes
MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(LCD_ADDR, lcdColumns, lcdRows);
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, KEYPAD_ADDR);

// Keypad handling
String keypadPassword = "";
const int maxPasswordLength = 8;
const String correctPassword = "123456";

// Task handles
TaskHandle_t task1Handle = NULL;
TaskHandle_t task2Handle = NULL;

// Mutex handle
SemaphoreHandle_t xSupabaseMutex;

// Function prototypes
void initializePins();
void handleKeypadInput(void *pvParameters);
void handleSensors(void *pvParameters);

void initializePins()
{
  pinMode(motionPin, INPUT);
  pinMode(vibrationPin, INPUT);
  pinMode(magneticPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
}

void playWelcomeMelody(int buzzerPin)
{
  // Define the notes and their durations
  int melody[] = {262, 294, 330, 349, 392, 440, 494, 523};        // C4, D4, E4, F4, G4, A4, B4, C5
  int noteDurations[] = {500, 500, 500, 500, 500, 500, 500, 500}; // Each note plays for 500ms

  for (int i = 0; i < 8; i++)
  {
    tone(buzzerPin, melody[i], noteDurations[i]); // Play note
    delay(noteDurations[i] * 1.3);                // Add delay between notes
  }

  noTone(buzzerPin); // Turn off the buzzer
}

void onCorrectKeypadCode()
{
  keypadAccess = 1;
  keypadAccessTimestamp = millis();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Access granted");
  isAccessGranted = true;
  playWelcomeMelody(buzzerPin);
}

void onCorrectRFIDRead()
{
  rfidAccess = 1;
  rfidAccessTimestamp = millis();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Access granted");
  isAccessGranted = true;
  playWelcomeMelody(buzzerPin);
}

void setup()
{
  Serial.begin(BAUD_RATE);
  connectToWifi();
  initializePins();
  SPI.begin();        // Init SPI communication
  Wire.begin();       // Init I2C communication
  mfrc522.PCD_Init(); // Init MFRC522
  lcd.init();         // Init LCD
  lcd.backlight();    // Turn on LCD backlight
  keypad.begin();     // Init keypad

  db.begin(supabase_url, anon_key);
  db.login_email(email_a, password_a);

  // Create mutex
  xSupabaseMutex = xSemaphoreCreateMutex();

  Serial.printf("Free heap before tasks: %d\n", xPortGetFreeHeapSize());

  // Create tasks
  if (xTaskCreate(handleKeypadInput, "Task 1", 10000, NULL, 2, &task1Handle) == pdPASS)
  {
    Serial.printf("Task 1 created. Free heap: %d\n", xPortGetFreeHeapSize());
  }
  else
  {
    Serial.println("Failed to create Task 1");
  }

  if (xTaskCreate(handleSensors, "Task 2", 10000, NULL, 1, &task2Handle) == pdPASS)
  {
    Serial.printf("Task 2 created. Free heap: %d\n", xPortGetFreeHeapSize());
  }
  else
  {
    Serial.println("Failed to create Task 2");
  }

  Serial.printf("Task 1 handle: %p\n", task1Handle);
  Serial.printf("Task 2 handle: %p\n", task2Handle);

  // Initialize LCD display
  lcd.setCursor(0, 0);
  lcd.print("Enter password:");
}

void checkSupabaseStatus()
{
  // Acquire mutex semaphore before accessing shared resources
  // Example: Fetch status from Supabase or other external source
  // Update global variables accordingly
  delay(100);
  Serial.println("Reading RFID status");
  if (sendToSupabaseRead("rfid", "status") == "on")
  {
    rfidStatus = 1;
  }
  else
  {
    rfidStatus = 0;
  }
  delay(100);
  Serial.println("Reading SIREN status");
  if (sendToSupabaseRead("siren", "status") == "on")
  {
    sirenStatus = 1;
  }
  else
  {
    sirenStatus = 0;
  }
  delay(100);
  Serial.println("Reading KEYPAD status");
  if (sendToSupabaseRead("keypad", "status") == "on")
  {
    keypadStatus = 1;
  }
  else
  {
    keypadStatus = 0;
  }
  delay(100);
  Serial.println("Reading VIBRATION status");
  if (sendToSupabaseRead("vibration", "status") == "on")
  {
    vibrationStatus = 1;
  }
  else
  {
    vibrationStatus = 0;
  }
  delay(100);
  Serial.println("Reading MAGNETIC status");
  if (sendToSupabaseRead("magnetic", "status") == "on")
  {
    magneticStatus = 1;
  }
  else
  {
    magneticStatus = 0;
  }
  delay(100);
  Serial.println("Reading MOTION status");
  if (sendToSupabaseRead("motion", "status") == "on")
  {
    motionStatus = 1;
  }
  else
  {
    motionStatus = 0;
  }
  // Release mutex semaphore after updating shared resources
}

void loop()
{
  // Check status every 20 seconds
  static unsigned long lastStatusCheckTime = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - lastStatusCheckTime >= 30000)
  {
    Serial.println("Checking Supabase status");
    // Acquire mutex semaphore before accessing shared resources
    if (xSemaphoreTake(xSupabaseMutex, portMAX_DELAY) == pdTRUE)
    {
      Serial.println("Mutex acquired");
      // Update LCD to show "Please wait..."
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Please wait...");
      // Update status variables from Supabase or other source
      checkSupabaseStatus();
      // Revert LCD to "Enter password:"
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter password:");
      lcd.setCursor(0, 1);
      for (int i = 0; i < keypadPassword.length(); i++)
      {
        lcd.print('*'); // Display '*' for each key pressed
      }
      // Release mutex semaphore after updating shared resources
      xSemaphoreGive(xSupabaseMutex);
      Serial.println("Mutex released");
    }
    else
    {
      Serial.println("Failed to acquire mutex");
    }
    lastStatusCheckTime = currentMillis;
  }

  // Check if 5 seconds have passed since keypad access was granted
  if (keypadAccess && currentMillis - keypadAccessTimestamp >= 5000)
  {
    keypadAccess = 0;
    if (xSemaphoreTake(xSupabaseMutex, portMAX_DELAY) == pdTRUE)
    {
      sendToSupabaseWrite("keypad", "value", 0);
      xSemaphoreGive(xSupabaseMutex);
    }
    if (isAccessGranted)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter password:");
      keypadPassword = "";
      lcd.setCursor(0, 1);
      for (int i = 0; i < keypadPassword.length(); i++)
      {
        lcd.print('*'); // Display '*' for each key pressed
      }
      isAccessGranted = false;
    }
  }

  // Check if 5 seconds have passed since RFID access was granted
  if (rfidAccess && currentMillis - rfidAccessTimestamp >= 5000)
  {
    rfidAccess = 0;
    if (xSemaphoreTake(xSupabaseMutex, portMAX_DELAY) == pdTRUE)
    {
      sendToSupabaseWrite("rfid", "value", 0);
      xSemaphoreGive(xSupabaseMutex);
    }
    if (isAccessGranted)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter password:");
      keypadPassword = "";
      lcd.setCursor(0, 1);
      for (int i = 0; i < keypadPassword.length(); i++)
      {
        lcd.print('*'); // Display '*' for each key pressed
      }
      isAccessGranted = false;
    }
  }
}

void handleKeypadInput(void *pvParameters)
{
  while (true)
  {
    // if (xSemaphoreTake(xSupabaseMutex, portMAX_DELAY) == pdTRUE)
    // {
    // if (sendToSupabaseRead("keypad", "status") == "on")
    // {
    char key = keypad.getKey(); // Get the key pressed
    if (key)
    {
      Serial.print("Key Pressed: ");
      Serial.println(key);

      if (key == '#') // Submit password
      {
        Serial.print("Entered Password: ");
        Serial.println(keypadPassword);
        // Add logic to verify password here
        if (keypadPassword == correctPassword)
        {
          onCorrectKeypadCode();
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Access granted");
          if (xSemaphoreTake(xSupabaseMutex, portMAX_DELAY) == pdTRUE)
          {
            sendToSupabaseWrite("keypad", "value", 1);
            xSemaphoreGive(xSupabaseMutex);
          }
          vTaskDelay(100 / portTICK_PERIOD_MS); // Display "Access granted" for 5 seconds
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Enter password:");
          lcd.setCursor(0, 1);
          for (int i = 0; i < keypadPassword.length(); i++)
          {
            lcd.print('*'); // Display '*' for each key pressed
          }
        }
        else
        {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Incorrect");
          lcd.setCursor(0, 1);
          lcd.print("password");
          vTaskDelay(2000 / portTICK_PERIOD_MS); // Display the message for 2 seconds
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Enter password:");
          lcd.setCursor(0, 1);
          for (int i = 0; i < keypadPassword.length(); i++)
          {
            lcd.print('*'); // Display '*' for each key pressed
          }
        }
        keypadPassword = "";
        lcd.setCursor(0, 1);
        lcd.print("                "); // Clear the second row
      }
      else if (key == '*') // Clear password
      {
        keypadPassword = ""; // Clear the password
        lcd.setCursor(0, 1);
        lcd.print("                "); // Clear the second row
      }
      else if (keypadPassword.length() < maxPasswordLength)
      {
        // lcd.setCursor(0, 1);           // Start from the beginning of the second row
        // lcd.print("                "); // Clear the second row before writing the new password
        keypadPassword += key; // Append key to password
        lcd.setCursor(0, 1);
        for (int i = 0; i < keypadPassword.length(); i++)
        {
          lcd.print('*'); // Display '*' for each key pressed
        }
      }
    }
    // }
    // xSemaphoreGive(xSupabaseMutex);
    // }
    // vTaskDelay(50 / portTICK_PERIOD_MS); // Delay to allow other tasks to run

    if (rfidStatus)
    {
      if (mfrc522.PICC_IsNewCardPresent())
      {
        if (mfrc522.PICC_ReadCardSerial())
        {
          Serial.print("UID tag :");
          String content = "";
          byte letter;
          for (byte i = 0; i < mfrc522.uid.size; i++)
          {
            Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
            Serial.print(mfrc522.uid.uidByte[i], HEX);
            content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
            content.concat(String(mfrc522.uid.uidByte[i], HEX));
          }
          Serial.println();
          Serial.print("Message : ");
          content.toUpperCase();
          if (xSemaphoreTake(xSupabaseMutex, portMAX_DELAY) == pdTRUE)
          {
            if ((content.substring(1) == "7A 77 C7 B2") || (content.substring(1) == "43 10 73 0E"))
            {
              Serial.println("Authorized access");
              onCorrectRFIDRead();
              sendToSupabaseWrite("rfid", "value", 1);
            }
            else
            {
              Serial.println("Access denied");
              rfidAccess = 0;
              sendToSupabaseWrite("rfid", "value", 0);
            }
            xSemaphoreGive(xSupabaseMutex);
          }
        }
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS); // Delay to allow other tasks to run
  }
}

void handleSensors(void *pvParameters)
{
  while (true)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      if (!rfidAccess && !keypadAccess)
      {
        if (xSemaphoreTake(xSupabaseMutex, portMAX_DELAY) == pdTRUE)
        {
          if (magneticStatus)
          {
            magneticValue = digitalRead(magneticPin);

            Serial.print((String) "Magnetic value: " + magneticValue);
            if (magneticValue == HIGH)
            {
              Serial.println(" - Door is open!");
              if (sirenStatus == 1)
              {
                digitalWrite(buzzerPin, HIGH); // Turn on the buzzer
              }
              sendToSupabaseWrite("magnetic", "value", 1);
            }
            else
            {
              Serial.println(" - Door is closed!");
              if (sirenStatus == 1)
              {
                digitalWrite(buzzerPin, LOW); // Turn off the buzzer
              }
              sendToSupabaseWrite("magnetic", "value", 0);
            }

            vTaskDelay(50 / portTICK_PERIOD_MS); // Delay before taking another reading
          }
          xSemaphoreGive(xSupabaseMutex);
        }
      }

      if (xSemaphoreTake(xSupabaseMutex, portMAX_DELAY) == pdTRUE)
      {
        if (motionStatus)
        {
          long motionValue = digitalRead(motionPin);
          if (motionValue == HIGH)
          {
            Serial.println("Motion detected!");
          }
          else
          {
            Serial.println("Motion absent!");
          }
          sendToSupabaseWrite("motion", "value", motionValue);
          // vTaskDelay(250 / portTICK_PERIOD_MS); // Delay before taking another reading
        }
        xSemaphoreGive(xSupabaseMutex);
      }

      if (xSemaphoreTake(xSupabaseMutex, portMAX_DELAY) == pdTRUE)
      {
        if (vibrationStatus)
        {
          vibrationValue = analogRead(vibrationPin);
          Serial.println((String) "Amplitudine vibratie: " + vibrationValue);
          sendToSupabaseWrite("vibration", "value", vibrationValue);
        }
        xSemaphoreGive(xSupabaseMutex);
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS); // Delay before taking another reading
  }
}