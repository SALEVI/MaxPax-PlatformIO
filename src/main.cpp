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
#include "confidential.h"

// Constants
#define WIFI_TIMEOUT_MS 20000
#define SS_PIN 5
#define RST_PIN 17
#define KEYPAD_ADDR 0x20
#define LCD_ADDR 0x27
#define BAUD_RATE 115200
#define VIBRATION_THRESHOLD 4000
#define MAX_PASSWORD_LENGTH 8
#define LCD_COLUMNS 16
#define LCD_ROWS 2

// Pins
const int MOTION_PIN = 16;
const int VIBRATION_PIN = 35;
const int MAGNETIC_PIN = 14;
const int BUZZER_PIN = 4;

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte rowPins[ROWS] = {0, 1, 2, 3};
byte colPins[COLS] = {4, 5, 6, 7};

// Global Variables
long motionValue;
int vibrationValue;
int magneticValue;
int wifiStatus = 1;
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
String keypadPassword = "";
const String correctPassword = "123456";

// Supabase
Supabase db;
String table = "sensor_data"; // Target table
const char *readJSON;

// Task handles
TaskHandle_t task1Handle = NULL;
TaskHandle_t task2Handle = NULL;

// Mutex handle
SemaphoreHandle_t xSupabaseMutex;

// Class Instances
MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLUMNS, LCD_ROWS);
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, KEYPAD_ADDR);

// Function prototypes
void initializePins();
void handleKeypadInput(void *pvParameters);
void handleSensors(void *pvParameters);
void playWelcomeMelody();
void onCorrectKeypadCode();
void onCorrectRFIDRead();
void checkSupabaseStatus();

void initializePins()
{
  pinMode(MOTION_PIN, INPUT);
  pinMode(VIBRATION_PIN, INPUT);
  pinMode(MAGNETIC_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
}

void playWelcomeMelody()
{
  int melody[] = {262, 294, 330, 349, 392, 440, 494, 523};
  int noteDurations[] = {500, 500, 500, 500, 500, 500, 500, 500};

  for (int i = 0; i < 8; i++)
  {
    tone(BUZZER_PIN, melody[i], noteDurations[i]);
    delay(noteDurations[i] * 1.3);
  }
  noTone(BUZZER_PIN);
}

void onCorrectKeypadCode()
{
  keypadAccess = 1;
  keypadAccessTimestamp = millis();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Access granted");
  isAccessGranted = true;
  playWelcomeMelody();
}

void onCorrectRFIDRead()
{
  rfidAccess = 1;
  rfidAccessTimestamp = millis();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Access granted");
  isAccessGranted = true;
  playWelcomeMelody();
}

void lcdReset()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter password:");
  lcd.setCursor(0, 1);
  for (int i = 0; i < keypadPassword.length(); i++)
  {
    lcd.print('*'); // Display '*' for each key pressed
  }
}

void resetAccess()
{
  if (isAccessGranted)
  {
    keypadPassword = "";
    lcdReset();
    isAccessGranted = false;
  }
}

void semaphoreSendToSupabase(String name, int value)
{
  if (wifiStatus && xSemaphoreTake(xSupabaseMutex, portMAX_DELAY) == pdTRUE)
  {
    Serial.printf("Sending to Supabase: %s = %d\n", name.c_str(), value);
    sendToSupabaseWrite(name, "value", value);
    xSemaphoreGive(xSupabaseMutex);
  }
  else
  {
    Serial.println("Failed to acquire semaphore or WiFi not connected");
  }
}

String semaphoreReadFromSupabase(String name)
{
  String read = "";
  if (wifiStatus && xSemaphoreTake(xSupabaseMutex, portMAX_DELAY) == pdTRUE)
  {
    read = sendToSupabaseRead(name, "status");
    xSemaphoreGive(xSupabaseMutex);
  }
  return read;
}

// Define states for the status check
enum SupabaseStatusCheckState
{
  CHECK_WIFI,
  CHECK_RFID,
  CHECK_SIREN,
  CHECK_KEYPAD,
  CHECK_VIBRATION,
  CHECK_MAGNETIC,
  CHECK_MOTION,
  DONE_CHECKING
};

SupabaseStatusCheckState currentSupabaseState = CHECK_WIFI;
unsigned long lastSupabaseCheckTime = 0;
const unsigned long SUPABASE_CHECK_INTERVAL = 1000; // Interval in milliseconds between checks

void checkSupabaseStatusAndWiFi()
{
  switch (currentSupabaseState)
  {
  case CHECK_WIFI:
    if (millis() - lastSupabaseCheckTime >= SUPABASE_CHECK_INTERVAL)
    {
      wifiStatus = WiFi.status() == WL_CONNECTED ? 1 : 0;
      lastSupabaseCheckTime = millis();
      currentSupabaseState = CHECK_RFID; // Move to next state
      Serial.println("Checked WiFi status");
    }
    break;

  case CHECK_RFID:
    if (millis() - lastSupabaseCheckTime >= SUPABASE_CHECK_INTERVAL)
    {
      rfidStatus = (semaphoreReadFromSupabase("rfid") == "on") ? 1 : 0;
      lastSupabaseCheckTime = millis();
      currentSupabaseState = CHECK_SIREN; // Move to next state
      Serial.println("Checked RFID status");
    }
    break;

  case CHECK_SIREN:
    if (millis() - lastSupabaseCheckTime >= SUPABASE_CHECK_INTERVAL)
    {
      sirenStatus = (semaphoreReadFromSupabase("siren") == "on") ? 1 : 0;
      lastSupabaseCheckTime = millis();
      currentSupabaseState = CHECK_KEYPAD; // Move to next state
      Serial.println("Checked SIREN status");
    }
    break;

  case CHECK_KEYPAD:
    if (millis() - lastSupabaseCheckTime >= SUPABASE_CHECK_INTERVAL)
    {
      keypadStatus = (semaphoreReadFromSupabase("keypad") == "on") ? 1 : 0;
      lastSupabaseCheckTime = millis();
      currentSupabaseState = CHECK_VIBRATION; // Move to next state
      Serial.println("Checked KEYPAD status");
    }
    break;

  case CHECK_VIBRATION:
    if (millis() - lastSupabaseCheckTime >= SUPABASE_CHECK_INTERVAL)
    {
      vibrationStatus = (semaphoreReadFromSupabase("vibration") == "on") ? 1 : 0;
      lastSupabaseCheckTime = millis();
      currentSupabaseState = CHECK_MAGNETIC; // Move to next state
      Serial.println("Checked VIBRATION status");
    }
    break;

  case CHECK_MAGNETIC:
    if (millis() - lastSupabaseCheckTime >= SUPABASE_CHECK_INTERVAL)
    {
      magneticStatus = (semaphoreReadFromSupabase("magnetic") == "on") ? 1 : 0;
      lastSupabaseCheckTime = millis();
      currentSupabaseState = CHECK_MOTION; // Move to next state
      Serial.println("Checked MAGNETIC status");
    }
    break;

  case CHECK_MOTION:
    if (millis() - lastSupabaseCheckTime >= SUPABASE_CHECK_INTERVAL)
    {
      motionStatus = (semaphoreReadFromSupabase("motion") == "on") ? 1 : 0;
      lastSupabaseCheckTime = millis();
      currentSupabaseState = DONE_CHECKING; // Move to done state
      Serial.println("Checked MOTION status");
    }
    break;

  case DONE_CHECKING:
    // Reset
    currentSupabaseState = CHECK_WIFI;
    break;
  }
}

void setup()
{
  Serial.begin(BAUD_RATE);
  connectToWifi();
  initializePins();
  SPI.begin();
  Wire.begin();
  mfrc522.PCD_Init();
  lcd.init();
  lcd.backlight();
  keypad.begin();
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

  // Initialize LCD display
  lcd.setCursor(0, 0);
  lcd.print("Enter password:");
}

void loop()
{
  static unsigned long lastStatusCheckTime = 0;
  unsigned long currentMillis = millis();

  // Check Supabase status
  if (currentMillis - lastStatusCheckTime >= 5000)
  {
    lastStatusCheckTime = currentMillis;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Please wait...");
    checkSupabaseStatusAndWiFi();
    lcdReset();
  }

  // Give control back to the FreeRTOS scheduler
  vTaskDelay(1 / portTICK_PERIOD_MS);
}

void handleKeypadInput(void *pvParameters)
{
  unsigned long lastKeypadAccessTime = 0;
  unsigned long lastRFIDAccessTime = 0;

  while (true)
  {
    if (!keypadStatus)
    {
      Serial.println("KEYPAD is turned OFF");
    }
    if (keypadStatus)
    {
      char key = keypad.getKey();
      if (key)
      {
        Serial.print("Key Pressed: ");
        Serial.println(key);

        if (key == '#') // Submit password
        {
          Serial.println((String) "Entered Password: " + keypadPassword);
          if (keypadPassword == correctPassword) // Verify password
          {
            onCorrectKeypadCode();
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Access granted");
            semaphoreSendToSupabase("keypad", 1);
            lastKeypadAccessTime = millis();
          }
          else
          {
            keypadAccess = 1;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Incorrect");
            lcd.setCursor(0, 1);
            lcd.print("password");
            vTaskDelay(2000 / portTICK_PERIOD_MS); // Display the message for 2 seconds
            lcdReset();
          }
          keypadPassword = "";
        }
        else if (key == '*')
        {
          keypadPassword = "";
          lcdReset();
        }
        else
        {
          if (keypadPassword.length() < MAX_PASSWORD_LENGTH)
          {
            keypadPassword += key;
            lcd.setCursor(keypadPassword.length() - 1, 1);
            lcd.print('*');
          }
        }
      }
    }

    if (!rfidStatus)
    {
      Serial.println("RFID is turned OFF");
    }
    if (rfidStatus && mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())
    {
      Serial.print("UID tag: ");
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
      Serial.print("Message: ");
      content.toUpperCase();
      if ((content.substring(1) == "7A 77 C7 B2") || (content.substring(1) == "43 10 73 0E"))
      {
        Serial.println("Authorized access");
        onCorrectRFIDRead();
        semaphoreSendToSupabase("rfid", 1);
        lastRFIDAccessTime = millis();
      }
      else
      {
        Serial.println("Access denied");
        rfidAccess = 1;
        semaphoreSendToSupabase("rfid", 0);
      }
    }

    // Check and reset keypad access after 5 seconds
    if (keypadAccess && millis() - lastKeypadAccessTime >= 5000)
    {
      keypadAccess = 0;
      semaphoreSendToSupabase("keypad", 0);
      resetAccess();
    }

    // Handle RFID access timeout similarly using millis() approach
    if (rfidAccess && millis() - lastRFIDAccessTime >= 5000)
    {
      rfidAccess = 0;
      semaphoreSendToSupabase("rfid", 0);
      resetAccess();
    }

    vTaskDelay(50 / portTICK_PERIOD_MS); // Short delay to allow other tasks to run
  }
}

void handleSensors(void *pvParameters)
{
  while (true)
  {
    if (motionStatus)
    {
      motionValue = digitalRead(MOTION_PIN);
      Serial.println((motionValue == HIGH) ? "Motion detected" : "Motion stopped");
      semaphoreSendToSupabase("motion", motionValue == HIGH ? 1 : 0);
      // Delay after reading motion sensor 0.3sec is the minimum time interval
      vTaskDelay(250 / portTICK_PERIOD_MS);
    }
    else
    {
      Serial.println("MOTION is turned OFF");
    }

    if (vibrationStatus)
    {
      vibrationValue = analogRead(VIBRATION_PIN);
      Serial.println(vibrationValue >= VIBRATION_THRESHOLD ? (String) "Vibration amplitude: " + vibrationValue + " - that's a hit!"
                                                           : (String) "Vibration amplitude: " + vibrationValue);
      if (sirenStatus)
      {
        digitalWrite(BUZZER_PIN, vibrationValue >= (VIBRATION_THRESHOLD - 500) ? HIGH : LOW);
      }
      semaphoreSendToSupabase("vibration", vibrationValue >= VIBRATION_THRESHOLD ? 1 : 0);
    }
    else
    {
      Serial.println("VIBRATION is turned OFF");
    }

    if (!magneticStatus)
    {
      Serial.println("MAGNETIC is turned OFF");
    }
    if (magneticStatus && !rfidAccess && !keypadAccess)
    {
      magneticValue = digitalRead(MAGNETIC_PIN);
      Serial.println(magneticValue == HIGH ? (String) "Magnetic value: " + magneticValue + " - Door is open!"
                                           : (String) "Magnetic value: " + magneticValue + " - Door is closed!");
      if (sirenStatus)
      {
        digitalWrite(BUZZER_PIN, magneticValue == HIGH ? HIGH : LOW);
      }
      semaphoreSendToSupabase("magnetic", magneticValue == HIGH ? 1 : 0);
    }

    // Delay before the next iteration of the while loop
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}