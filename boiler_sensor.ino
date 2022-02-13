#include <WiFi.h>
#include <ArduinoJson.h>
#include <Thermistor.h>
#include <NTC_Thermistor.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <millisDelay.h>
#include "index.h"

// WiFi parameters
const char* ssid     = "xxxxxxxx";    // your WiFi SSID
const char* password = "xxxxxxxx"; // your WiFi password
const char* hostname = "boiler";        // access the status page with http://<hostname> 


// Open Weather Map API
// get your API key form https://home.openweathermap.org
const char* weatherServer = "api.openweathermap.org";
const char* weatherLocation = "xxxxxxxx"; // your location
const char* weatherAPIKey = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; // your Open Weather Map API key 
const int httpPort = 80;
String weatherURL = String("/data/2.5/weather?q=") + weatherLocation + "&units=metric&APPID=" + weatherAPIKey;
const unsigned long HTTP_TIMEOUT = 10000;  // max response time from server
const size_t MAX_CONTENT_SIZE = 512;       // max size of the HTTP response
// The type of data that we want to extract from the API
struct clientData {
  char temp[8];
};

// Alpha CD32 boiler parameters
#define COMPENSATION_COUNT 10 
float compensation_m[ COMPENSATION_COUNT ] = { 0.0, -0.512820513, -0.786516854, -1.166666667, -1.489361702, -1.739130435, -2.058823529, -2.307692308, -2.5, -2.727272727 }; // Derived from boiler documentation
float compensation_c[ COMPENSATION_COUNT ] = { 25.0, 38.84615385, 46.23595506, 56.5, 65.21276596, 71.95652174, 80.58823529, 87.30769231, 92.5, 98.63636364 }; // Derived from boiler documentation
float boiler_probe_m = 7.7086; // Measured by experiment
float boiler_probe_c = 795.51; // Measured by experiment 

// Temperature mapping
#define MAP_COUNT   16   // 4 bit resolution
int threshold[ MAP_COUNT ] = { -10, -8, -6,-4,-2, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20 }; //external temperature thresholds in Celsius
byte mapping[ MAP_COUNT ] = { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }; 
#define RESISTOR_COUNT 5 // 4 bits + 1 fixed resistor
int resistors[RESISTOR_COUNT] = { 733, 15, 33, 60, 120 }; //resistor values in Ohms

// Web interface
int compensation_curve = 3; //default
const char* PARAM_INPUT_1 = "compensation";
float target_temp = 0;
double measured_temp = 0;
float external_temp = 0;

// Thermistor parameters
#define SENSOR_PIN             36
#define REFERENCE_RESISTANCE   12000 // pull up resistor value
#define NOMINAL_RESISTANCE     10000 // from thermistor data sheet
#define NOMINAL_TEMPERATURE    25    // from thermistor data sheet
#define B_VALUE                3950  // from thermistor data sheet
#define ANALOG_RESOLUTION      4095

// Relay parameters
#define RELAY_1_PIN            16
#define RELAY_2_PIN            17
#define RELAY_3_PIN            18
#define RELAY_4_PIN            19


// Object declarations
millisDelay updateInterval;
Thermistor* thermistor;
WiFiClient client;
AsyncWebServer server(80);


void setup() {

// Start serial console
    Serial.begin(115200);
    delay(10);

// Start 15 minute timer
    updateInterval.start(900000);

// Create WiFi connection
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());


// Start mDNS server so we can connect to http://<hostname>, rather than an IP address
    if(!MDNS.begin(hostname)) {
     Serial.println("Error starting mDNS");
     return;
  }
  MDNS.addService("http", "tcp", 80);


 // Start web server

 // Define root web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, HTMLprocessor);
  });

// Respond to parameter change on web page
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage1;
    // GET new compensation setting on /update?compensation=<inputMessage1>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
      compensation_curve = inputMessage1.toInt();
      CalculateCompensation();
    }
  });
  
  server.begin();
  Serial.print("HTTP server started at http://");
  Serial.println(hostname);

  
// Configure GPIO pins
    pinMode(RELAY_1_PIN, OUTPUT);
    pinMode(RELAY_2_PIN, OUTPUT);
    pinMode(RELAY_3_PIN, OUTPUT);
    pinMode(RELAY_4_PIN, OUTPUT);


// Create thermistor oject
    thermistor = new NTC_Thermistor(
    SENSOR_PIN,
    REFERENCE_RESISTANCE,
    NOMINAL_RESISTANCE,
    NOMINAL_TEMPERATURE,
    B_VALUE,
    ANALOG_RESOLUTION
  );

// First pass
    GetExternalTemp();
    SetRelays(CalculateCompensation());

}

void loop() {
  
if (updateInterval.justFinished()) {
    updateInterval.repeat(); // repeat
    GetExternalTemp();
    SetRelays(CalculateCompensation());
}
  
}

void GetExternalTemp() {
    // Check WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("WiFi connection lost. Reconnecting to ");
    Serial.println(ssid);
    WiFi.disconnect();
    WiFi.reconnect();
    delay(1000);
  }
  // Still no WiFi? Bail.
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

// Get weather data
 if (!client.connect(weatherServer, httpPort))
  {
    return;
  }

  Serial.println("Connected!");

  // Send the HTTP GET request to the server
  Serial.println("Requesting URL: " + weatherURL);
  // This will send the request to the server
  client.print(String("GET ") + weatherURL + " HTTP/1.1\r\n");
  client.print(String("Host:") + weatherServer + "\r\n");
  client.print("Connection: close\r\n\r\n");

  delay(10);

  if (client.println() == 0)
  {
    Serial.println("Failed to send request");
    return;
  }

  // Check HTTP status
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  // It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK"
  if (strcmp(status + 9, "200 OK") != 0)
  {
    Serial.print("Unexpected response: ");
    Serial.println(status);
    return;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders))
  {
    Serial.println("Invalid response");
    return;
  }

  // Allocate the JSON document as doc
  // Use arduinojson.org/v6/assistant to compute the capacity.  The pad came from the recommended "Additional bytes for strings duplication" size in the assistant.
  int PAD = 1333;

  //************************ from https://arduinojson.org/v6/assistant/ ****************************************************************
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + 2 * JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(13) + 270 + PAD;
  DynamicJsonDocument doc(capacity);
  //deserializeJson(doc, client);  // no error checking - never seems to fail or crash with propper PAD size, but also is a little risky
  //************************************************************************************************************************************

  // Instead of directly using the deserializeJson line, this modifies Parse JSON object as doc from client with error checking - from an example given at the ArduinoJson.org website
  DeserializationError error = deserializeJson(doc, client);
  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  //*********************** from https://arduinojson.org/v6/assistant/ *****************************************************************
  // Extract values
  JsonObject main = doc["main"];
  external_temp = main["temp"];         // in degrees Celsius
  //***************** This is the end of the section cut/pasted from the assistant ******************************************
}

byte CalculateCompensation() {

// Calculate compensation resistance and target flow temperature

  // Find nearest compensation threshold
  int i = 0;
  while (( i < MAP_COUNT ) && ( external_temp > threshold[ i ] )) {
    i++;
  }
  byte output = mapping[ i ];

  target_temp = ResistanceToTemperature(CompensationToResistance(output));
  measured_temp = thermistor->readCelsius();

  Serial.print("Ambient temperature: ");
  Serial.println(external_temp);
  Serial.print("Equivalent resistance: ");
  Serial.println(TemperatureToResistance(external_temp));
  Serial.print("Selected compensation curve: ");
  Serial.println(compensation_curve);  
  Serial.print("Output: ");  
  Serial.println(output);  
  Serial.print("Set resistance: ");
  Serial.println(CompensationToResistance(output));  
  Serial.print("Target flow temperature: ");
  Serial.println(target_temp);  
  Serial.print("Measured flow temperature: ");
  Serial.println(measured_temp);  

  return output;
}


// Calculate sensor resistance
float TemperatureToResistance(float temperature)
{

return boiler_probe_m * temperature + boiler_probe_c;
  
}

// Calculate set temperature
float ResistanceToTemperature(float resistance)
{

float effective_temp = (resistance - boiler_probe_c) / boiler_probe_m;

return compensation_m[compensation_curve] * effective_temp + compensation_c[compensation_curve];
  
}

// Calculate actual resistance
float CompensationToResistance(byte compensation)
{

int resistance = resistors[0] + resistors[1] + resistors[2] + resistors[3] + resistors[4];

if ( compensation & (1 << 0) ) {
  resistance = resistance - resistors[1];
}

if ( compensation & (1 << 1) ) {
  resistance = resistance - resistors[2];
}

if ( compensation & (1 << 2) ) {
  resistance = resistance - resistors[3];
}

if ( compensation & (1 << 3) ) {
  resistance = resistance - resistors[4];
}

return resistance;
  
}

// Set relays
void SetRelays(byte compensation)
{

boolean Relay1 = LOW;
boolean Relay2 = LOW;
boolean Relay3 = LOW;
boolean Relay4 = LOW;

if ( compensation & (1 << 0) ) {
  Relay1 = HIGH;
}

if ( compensation & (1 << 1) ) {
  Relay2 = HIGH;
}

if ( compensation & (1 << 2) ) {
  Relay3 = HIGH;
}

if ( compensation & (1 << 3) ) {
  Relay4 = HIGH;
}

  digitalWrite(RELAY_1_PIN, Relay1);
  digitalWrite(RELAY_2_PIN, Relay2);
  digitalWrite(RELAY_3_PIN, Relay3);
  digitalWrite(RELAY_4_PIN, Relay4);

}

String HTMLprocessor(const String& var) {
  if(var == "EXTERNAL_TEMP")
    return String(external_temp);
  if(var == "TARGET_TEMP")
    return String(target_temp);
  if(var == "MEASURED_TEMP")
    return String(measured_temp);  
   if(var == "COMPENSATION_CURVE"){
    String selector = "";
    selector += String("<input type=\"number\" min=\"0\" max=\"" + String(COMPENSATION_COUNT - 1));
    selector += String("\" onchange=\"updateInput(this)\" id=\"compensation_curve\" value=\"") + compensation_curve + "\">";
    return selector;
  }  
  return String();
}
