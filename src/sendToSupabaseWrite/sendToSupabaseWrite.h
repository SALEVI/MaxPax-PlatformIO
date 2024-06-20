#ifndef SEND_TO_SUPABASE_WRITE_H
#define SEND_TO_SUPABASE_WRITE_H

#include <Arduino.h>
#include <ESP32_Supabase.h>
#include <ArduinoJson.h>

void sendToSupabaseWrite(String name, String column, int value);
void sendToSupabaseWrite(String name, String column, String value);

#endif