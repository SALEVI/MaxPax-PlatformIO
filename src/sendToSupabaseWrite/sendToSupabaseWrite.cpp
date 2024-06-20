#include "sendToSupabaseWrite.h"
#include "../confidential.h"

extern const char *readJSON;
extern Supabase db;
extern String table;

// Program Initialization
// void initializeProgram()
// {
//   db.begin(supabase_url, anon_key);
//   db.login_email(email_a, password_a);
// }

void sendToSupabaseWrite(String name, String column, int value)
{
  // Create JSON payload
  JsonDocument doc;
  doc[column] = value;

  String writtenJSON;
  serializeJson(doc, writtenJSON);

  int code = db.update(table).eq("name", name).doUpdate(writtenJSON);
  Serial.println((String) "SupabaseWrite int result: " + code);
  db.urlQuery_reset();
}

void sendToSupabaseWrite(String name, String column, String value)
{
  // Create JSON payload
  JsonDocument doc;
  doc[column] = value;

  String writtenJSON;
  serializeJson(doc, writtenJSON);

  int code = db.update(table).eq("name", name).doUpdate(writtenJSON);
  Serial.println((String) "SupabaseWrite string result: " + code);
  db.urlQuery_reset();
}