#ifndef SEND_TO_SUPABASE_READ_H
#define SEND_TO_SUPABASE_READ_H

#include <Arduino.h>
#include <ESP32_Supabase.h>
#include <ArduinoJson.h>

String sendToSupabaseRead(String name, String column);

#endif