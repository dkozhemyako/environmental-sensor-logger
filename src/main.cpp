#include <Arduino.h>
#include <Wire.h>

#include <BH1750.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <time.h>

#include "secrets.h"


// ----------------------------------------------------
// Sensors
// ----------------------------------------------------

BH1750 light;
Adafruit_AHTX0 temp_hum;
Adafruit_BMP280 atmos;


// ----------------------------------------------------
// Device state machine
// ----------------------------------------------------

enum class DeviceState {
  IDLE,
  RUNNING,
  ERROR
};

DeviceState state = DeviceState::IDLE;


// ----------------------------------------------------
// Configuration
// ----------------------------------------------------

const unsigned long DEFAULT_INTERVAL_MS = 1000;
const unsigned long MIN_INTERVAL_MS = 100;
const unsigned long MAX_INTERVAL_MS = 60000;

unsigned long intervalMs = DEFAULT_INTERVAL_MS;
unsigned long lastReadMs = 0;

const char* LOG_FILE = "/sensor_log.csv";


// ----------------------------------------------------
// Runtime status
// ----------------------------------------------------

bool sensorsReady = false;
bool storageReady = false;
bool clockSynced = false;

unsigned int consecutiveSensorErrors = 0;
const unsigned int MAX_SENSOR_ERRORS = 3;


// ----------------------------------------------------
// Serial command buffer
// ----------------------------------------------------

String commandBuffer = "";

const unsigned long COMMAND_TIMEOUT_MS = 3000;
unsigned long lastCommandByteMs = 0;

bool commandOverflow = false;


// ----------------------------------------------------
// State name
// ----------------------------------------------------

const char* stateName(DeviceState currentState) {

  switch (currentState) {

    case DeviceState::IDLE:
      return "IDLE";

    case DeviceState::RUNNING:
      return "RUNNING";

    case DeviceState::ERROR:
      return "ERROR";
  }

  return "UNKNOWN";
}


// ----------------------------------------------------
// UTC time synchronization
// ----------------------------------------------------

bool synchronizeClock() {

  Serial.println("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startTime < 15000
  ) {
    yield();
  }


  Serial.print("WiFi status: ");
  Serial.println(WiFi.status());

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WiFi connection failed");

    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);

    return false;
  }


  Serial.println("WiFi connected");

  Serial.println("Synchronizing UTC time...");


  configTime(
    0,
    0,
    "pool.ntp.org",
    "time.nist.gov"
  );


  startTime = millis();

  while (
    time(nullptr) < 1700000000 &&
    millis() - startTime < 15000
  ) {
    yield();
  }


  bool synchronized =
    time(nullptr) >= 1700000000;


  if (synchronized) {

    Serial.println("UTC time synchronized");

  } else {

    Serial.println("UTC synchronization failed");
  }


  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);

  return synchronized;
}


// ----------------------------------------------------
// Get timestamp
// ----------------------------------------------------

String getUtcTimestamp() {

  time_t currentTime;

  if (clockSynced) {

    currentTime = time(nullptr);

  } else {

    currentTime = millis() / 1000;
  }


  struct tm utcTime;

  gmtime_r(
    &currentTime,
    &utcTime
  );


  char buffer[25];

  strftime(
    buffer,
    sizeof(buffer),
    "%Y-%m-%dT%H:%M:%SZ",
    &utcTime
  );


  return String(buffer);
}


// ----------------------------------------------------
// Flash storage initialization
// ----------------------------------------------------

bool initializeStorage() {

  if (!LittleFS.begin()) {
    return false;
  }


  File file =
    LittleFS.open(LOG_FILE, "a");


  if (!file) {
    return false;
  }


  if (file.size() == 0) {

    file.println(
      "timestamp,"
      "time_synced,"
      "temperature_c,"
      "humidity_pct,"
      "pressure_hpa,"
      "light_lx,"
      "status"
    );
  }


  file.close();

  return true;
}


// ----------------------------------------------------
// Sensor validation
// ----------------------------------------------------

bool validateReadings(
  float temperature,
  float humidity,
  float pressure,
  float lightLevel
) {

  if (
    isnan(temperature) ||
    isnan(humidity) ||
    isnan(pressure) ||
    isnan(lightLevel)
  ) {
    return false;
  }


  if (
    temperature < -40.0 ||
    temperature > 85.0
  ) {
    return false;
  }


  if (
    humidity < 0.0 ||
    humidity > 100.0
  ) {
    return false;
  }


  if (
    pressure < 300.0 ||
    pressure > 1100.0
  ) {
    return false;
  }


  if (
    lightLevel < 0.0 ||
    lightLevel > 120000.0
  ) {
    return false;
  }


  return true;
}


// ----------------------------------------------------
// JSON number output
// ----------------------------------------------------

void printJsonNumber(float value) {

  if (
    isnan(value) ||
    isinf(value)
  ) {

    Serial.print("null");

  } else {

    Serial.print(value, 2);
  }
}


// ----------------------------------------------------
// JSON sensor message
// ----------------------------------------------------

void printReading(
  const String& timestamp,
  float temperature,
  float humidity,
  float pressure,
  float lightLevel,
  bool valid
) {

  Serial.print("{");

  Serial.print("\"type\":\"reading\",");

  Serial.print("\"timestamp\":\"");
  Serial.print(timestamp);
  Serial.print("\",");

  Serial.print("\"time_synced\":");
  Serial.print(
    clockSynced ? "true" : "false"
  );
  Serial.print(",");

  Serial.print("\"temperature_c\":");
  printJsonNumber(temperature);
  Serial.print(",");

  Serial.print("\"humidity_pct\":");
  printJsonNumber(humidity);
  Serial.print(",");

  Serial.print("\"pressure_hpa\":");
  printJsonNumber(pressure);
  Serial.print(",");

  Serial.print("\"light_lx\":");
  printJsonNumber(lightLevel);
  Serial.print(",");

  Serial.print("\"status\":\"");
  Serial.print(
    valid ? "OK" : "INVALID"
  );
  Serial.print("\"");

  Serial.println("}");
}


// ----------------------------------------------------
// Save CSV to flash
// ----------------------------------------------------

bool writeReadingToFlash(
  const String& timestamp,
  float temperature,
  float humidity,
  float pressure,
  float lightLevel,
  bool valid
) {

  File file =
    LittleFS.open(LOG_FILE, "a");


  if (!file) {
    return false;
  }


  file.print(timestamp);
  file.print(",");

  file.print(
    clockSynced ? "true" : "false"
  );
  file.print(",");

  file.print(temperature, 2);
  file.print(",");

  file.print(humidity, 2);
  file.print(",");

  file.print(pressure, 2);
  file.print(",");

  file.print(lightLevel, 2);
  file.print(",");

  file.println(
    valid ? "OK" : "INVALID"
  );


  file.flush();
  file.close();

  return true;
}


// ----------------------------------------------------
// Read sensors
// ----------------------------------------------------

void readSensors() {

  sensors_event_t humidityEvent;
  sensors_event_t temperatureEvent;


  temp_hum.getEvent(
    &humidityEvent,
    &temperatureEvent
  );


  float temperature =
    temperatureEvent.temperature;


  float humidity =
    humidityEvent.relative_humidity;


  float pressure =
    atmos.readPressure() / 100.0F;


  float lightLevel =
    light.readLightLevel();


  bool valid =
    validateReadings(
      temperature,
      humidity,
      pressure,
      lightLevel
    );


  String timestamp =
    getUtcTimestamp();


  printReading(
    timestamp,
    temperature,
    humidity,
    pressure,
    lightLevel,
    valid
  );


  bool logResult =
    writeReadingToFlash(
      timestamp,
      temperature,
      humidity,
      pressure,
      lightLevel,
      valid
    );


  if (!logResult) {

    storageReady = false;
    state = DeviceState::ERROR;

    Serial.println(
      "{\"type\":\"error\","
      "\"source\":\"storage\","
      "\"message\":\"Flash write failed\"}"
    );

    return;
  }


  if (!valid) {

    consecutiveSensorErrors++;


    if (
      consecutiveSensorErrors >=
      MAX_SENSOR_ERRORS
    ) {

      state = DeviceState::ERROR;

      Serial.println(
        "{\"type\":\"error\","
        "\"source\":\"sensor\","
        "\"message\":\"Three consecutive invalid readings\"}"
      );
    }

  } else {

    consecutiveSensorErrors = 0;
  }
}


// ----------------------------------------------------
// Status
// ----------------------------------------------------

void printStatus() {

  Serial.print("{");

  Serial.print("\"type\":\"status\",");

  Serial.print("\"state\":\"");
  Serial.print(stateName(state));
  Serial.print("\",");

  Serial.print("\"interval_ms\":");
  Serial.print(intervalMs);
  Serial.print(",");

  Serial.print("\"sensors_ready\":");
  Serial.print(
    sensorsReady ? "true" : "false"
  );
  Serial.print(",");

  Serial.print("\"storage_ready\":");
  Serial.print(
    storageReady ? "true" : "false"
  );
  Serial.print(",");

  Serial.print("\"time_synced\":");
  Serial.print(
    clockSynced ? "true" : "false"
  );

  Serial.println("}");
}


// ----------------------------------------------------
// Command response
// ----------------------------------------------------

void printCommandResponse(
  const char* command,
  const char* result,
  const char* message
) {

  Serial.print("{");

  Serial.print("\"type\":\"response\",");

  Serial.print("\"command\":\"");
  Serial.print(command);
  Serial.print("\",");

  Serial.print("\"result\":\"");
  Serial.print(result);
  Serial.print("\",");

  Serial.print("\"message\":\"");
  Serial.print(message);
  Serial.print("\"");

  Serial.println("}");
}


// ----------------------------------------------------
// Dump flash log
// ----------------------------------------------------

void dumpLog() {

  File file =
    LittleFS.open(LOG_FILE, "r");


  if (!file) {

    printCommandResponse(
      "DUMP_LOG",
      "ERR",
      "Cannot open log file"
    );

    return;
  }


  Serial.println(
    "{\"type\":\"log_dump\","
    "\"event\":\"BEGIN\"}"
  );


  while (file.available()) {

    Serial.write(file.read());
  }


  file.close();


  Serial.println(
    "{\"type\":\"log_dump\","
    "\"event\":\"END\"}"
  );
}


// ----------------------------------------------------
// Clear flash log
// ----------------------------------------------------

void clearLog() {

  File file =
    LittleFS.open(LOG_FILE, "w");


  if (!file) {

    printCommandResponse(
      "CLEAR_LOG",
      "ERR",
      "Cannot clear log file"
    );

    return;
  }


  file.println(
    "timestamp,"
    "time_synced,"
    "temperature_c,"
    "humidity_pct,"
    "pressure_hpa,"
    "light_lx,"
    "status"
  );


  file.close();


  printCommandResponse(
    "CLEAR_LOG",
    "OK",
    "Log cleared"
  );
}


// ----------------------------------------------------
// Command parser
// ----------------------------------------------------

void processCommand(String command) {

  command.trim();


  String upperCommand =
    command;


  upperCommand.toUpperCase();


  // START
  if (upperCommand == "START") {

    if (!sensorsReady) {

      printCommandResponse(
        "START",
        "ERR",
        "Sensors are not ready"
      );

      return;
    }


    if (!storageReady) {

      printCommandResponse(
        "START",
        "ERR",
        "Flash storage is not ready"
      );

      return;
    }


    state =
      DeviceState::RUNNING;


    lastReadMs =
      millis();


    printCommandResponse(
      "START",
      "OK",
      "Logger started"
    );

    return;
  }


  // STOP
  if (upperCommand == "STOP") {

    state =
      DeviceState::IDLE;


    printCommandResponse(
      "STOP",
      "OK",
      "Logger stopped"
    );

    return;
  }


  // STATUS
  if (upperCommand == "STATUS") {

    printStatus();

    return;
  }


  // SET_INTERVAL
  const String intervalCommand =
    "SET_INTERVAL ";


  if (
    upperCommand.startsWith(
      intervalCommand
    )
  ) {

    String value =
      upperCommand.substring(
        intervalCommand.length()
      );


    value.trim();


    if (value.length() == 0) {

      printCommandResponse(
        "SET_INTERVAL",
        "ERR",
        "Missing interval"
      );

      return;
    }


    for (
      unsigned int i = 0;
      i < value.length();
      i++
    ) {

      if (!isDigit(value[i])) {

        printCommandResponse(
          "SET_INTERVAL",
          "ERR",
          "Interval must be numeric"
        );

        return;
      }
    }


    unsigned long newInterval =
      value.toInt();


    if (
      newInterval < MIN_INTERVAL_MS ||
      newInterval > MAX_INTERVAL_MS
    ) {

      printCommandResponse(
        "SET_INTERVAL",
        "ERR",
        "Interval must be 100-60000 ms"
      );

      return;
    }


    intervalMs =
      newInterval;


    printCommandResponse(
      "SET_INTERVAL",
      "OK",
      "Interval updated"
    );

    return;
  }


  // DUMP_LOG
  if (upperCommand == "DUMP_LOG") {

    dumpLog();

    return;
  }


  // CLEAR_LOG
  if (upperCommand == "CLEAR_LOG") {

    clearLog();

    return;
  }


  // Unknown command
  printCommandResponse(
    "UNKNOWN",
    "ERR",
    "Unknown command"
  );
}


// ----------------------------------------------------
// Serial input
// ----------------------------------------------------

void handleSerial() {

  while (Serial.available() > 0) {

    char received =
      Serial.read();


    lastCommandByteMs =
      millis();


    if (received == '\r') {
      continue;
    }


    if (received == '\n') {

      if (commandOverflow) {

        printCommandResponse(
          "UNKNOWN",
          "ERR",
          "Command too long"
        );

      } else if (
        commandBuffer.length() > 0
      ) {

        processCommand(
          commandBuffer
        );
      }


      commandBuffer = "";
      commandOverflow = false;

      continue;
    }


    if (
      commandBuffer.length() < 80
    ) {

      commandBuffer += received;

    } else {

      commandOverflow = true;
    }
  }


  if (
    commandBuffer.length() > 0 &&
    millis() - lastCommandByteMs >
    COMMAND_TIMEOUT_MS
  ) {

    commandBuffer = "";
    commandOverflow = false;


    printCommandResponse(
      "UNKNOWN",
      "ERR",
      "Command timeout"
    );
  }
}


// ----------------------------------------------------
// Setup
// ----------------------------------------------------

void setup() {

  Serial.begin(115200);


  Wire.begin(4, 5);


  // Sensors
  bool lightOk =
    light.begin(
      BH1750::CONTINUOUS_HIGH_RES_MODE
    );


  bool tempHumOk =
    temp_hum.begin();


  bool atmosOk =
    atmos.begin(0x77);


  sensorsReady =
    lightOk &&
    tempHumOk &&
    atmosOk;


  // Flash
  storageReady =
    initializeStorage();


  // UTC
  clockSynced =
    synchronizeClock();


  if (
    sensorsReady &&
    storageReady
  ) {

    state =
      DeviceState::IDLE;

  } else {

    state =
      DeviceState::ERROR;
  }


  // Boot message
  Serial.print("{");

  Serial.print("\"type\":\"boot\",");

  Serial.print("\"bh1750\":");
  Serial.print(
    lightOk ? "true" : "false"
  );
  Serial.print(",");

  Serial.print("\"aht20\":");
  Serial.print(
    tempHumOk ? "true" : "false"
  );
  Serial.print(",");

  Serial.print("\"bmp280\":");
  Serial.print(
    atmosOk ? "true" : "false"
  );
  Serial.print(",");

  Serial.print("\"storage\":");
  Serial.print(
    storageReady ? "true" : "false"
  );
  Serial.print(",");

  Serial.print("\"time_synced\":");
  Serial.print(
    clockSynced ? "true" : "false"
  );

  Serial.println("}");


  printStatus();


  Serial.println(
    "{\"type\":\"info\","
    "\"commands\":\"START, STOP, STATUS, "
    "SET_INTERVAL <ms>, DUMP_LOG, CLEAR_LOG\"}"
  );
}


// ----------------------------------------------------
// Main loop
// ----------------------------------------------------

void loop() {

  handleSerial();


  if (
    state ==
    DeviceState::RUNNING
  ) {

    unsigned long currentTime =
      millis();


    if (
      currentTime - lastReadMs >=
      intervalMs
    ) {

      lastReadMs =
        currentTime;


      readSensors();
    }
  }


  yield();
}