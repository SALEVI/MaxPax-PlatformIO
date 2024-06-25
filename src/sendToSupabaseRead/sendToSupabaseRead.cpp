#include "sendToSupabaseRead.h"
#include "../confidential.h"

extern const char *readJSON;
extern Supabase db;
extern String table;

String sendToSupabaseRead(String name, String column)
{
  // Beginning Supabase Connection
  // db.begin(supabase_url, anon_key);
  // db.login_email(email_a, password_a);

  // Allocate the JSON document
  JsonDocument doc;

  // Validate inputs
  if (name.isEmpty() || column.isEmpty())
  {
    // Handle invalid inputs
    return "";
  }

  String read = db.from(table).select(column).eq("name", name).limit(1).doSelect();
  // Serial.println(read);

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, read);

  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return ""; // Return empty string to indicate failure
  }

  readJSON = doc[0][column];
  // Serial.print(name + ": ");
  Serial.println(readJSON);

  db.urlQuery_reset();

  return readJSON;
}