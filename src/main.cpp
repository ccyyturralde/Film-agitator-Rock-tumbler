#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <TMCStepper.h>
#include <HardwareSerial.h>

// WiFi credentials - modify these or use Access Point mode
const char* ssid = "FilmAgitator";  // AP mode SSID
const char* password = "agitate123"; // AP mode password

// Motor and driver pins
#define STEP_PIN 26        // Step pin for TMC2209
#define DIR_PIN 25         // Direction pin for TMC2209
#define ENABLE_PIN 33      // Enable pin for TMC2209
#define SERIAL_PORT Serial2 // UART for TMC2209 (GPIO 16/17)
#define DRIVER_ADDRESS 0b00 // TMC2209 Driver address (0-3)

// Motor parameters for NEMA 17
#define STEPS_PER_REV 200  // NEMA 17 has 200 steps per revolution
#define MICROSTEPS 16      // 16 microsteps for smooth operation
#define MAX_RPM 300        // Maximum RPM for high speed operation

// TMC2209 UART configuration
#define R_SENSE 0.11f      // Match to your driver module (typical is 0.11)
HardwareSerial stepperSerial(2); // Use Serial2

// Create stepper driver object
TMC2209Stepper driver(&stepperSerial, R_SENSE, DRIVER_ADDRESS);

WebServer server(80);

// Motor control variables
bool motorRunning = false;
float currentRPM = 0.0;
bool motorDirection = true; // true = forward, false = reverse
unsigned long lastStepTime = 0;
unsigned long stepInterval = 0;

// Function prototypes
void handleRoot();
void handleAPI();
void setupMotor();
void stepMotor();
void stopMotor();
void startMotor(float rpm);
String getStatusJSON();
String getContentType(String filename);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize SPIFFS for web files
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
  }
  
  // Initialize stepper driver UART
  stepperSerial.begin(115200, SERIAL_8N1, 16, 17); // RX=GPIO16, TX=GPIO17
  delay(100);
  
  // Initialize motor pins
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, HIGH); // Disable driver initially (HIGH = disabled for some boards, LOW = enabled for others)
  
  // Setup motor driver
  setupMotor();
  
  // Setup WiFi - create Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  // Web server routes
  server.on("/", handleRoot);
  server.on("/api", HTTP_GET, handleAPI);
  server.on("/api", HTTP_POST, handleAPI);
  server.onNotFound([]() {
    String path = server.uri();
    if (path.endsWith("/")) {
      path += "index.html";
    }
    
    if (SPIFFS.exists(path)) {
      File file = SPIFFS.open(path, "r");
      server.streamFile(file, getContentType(path));
      file.close();
    } else {
      server.send(404, "text/plain", "File not found: " + path);
    }
  });
  
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
  
  // Handle motor stepping
  if (motorRunning && stepInterval > 0) {
    unsigned long currentTime = micros();
    if (currentTime - lastStepTime >= stepInterval) {
      stepMotor();
      lastStepTime = currentTime;
    }
  }
}

void setupMotor() {
  // Configure TMC2209 driver settings
  driver.begin();                    // Initialize driver
  driver.toff(4);                    // Enable driver in software
  driver.rms_current(800);           // Set motor RMS current (mA)
  driver.microsteps(MICROSTEPS);     // Set microsteps
  driver.pwm_autoscale(true);        // Enable automatic current scaling
  driver.en_spreadcycle(false);      // Use StealthChop for quiet operation
  driver.TPOWERDOWN(128);            // Powerdown delay
  driver.semin(5);                   // Lower threshold for spreadCycle
  driver.semax(2);                   // Upper threshold for spreadCycle
  driver.sedn(0b01);                 // Hysteresis for spreadCycle
  
  Serial.println("Motor driver configured");
}

void stepMotor() {
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(2);
  digitalWrite(STEP_PIN, LOW);
}

void startMotor(float rpm) {
  if (rpm <= 0 || rpm > MAX_RPM) {
    Serial.println("Invalid RPM");
    return;
  }
  
  motorRunning = true;
  currentRPM = rpm;
  
  // Set direction
  digitalWrite(DIR_PIN, motorDirection ? HIGH : LOW);
  
  // Calculate step interval based on RPM
  // Steps per second = (RPM / 60) * STEPS_PER_REV * MICROSTEPS
  float stepsPerSecond = (rpm / 60.0) * STEPS_PER_REV * MICROSTEPS;
  stepInterval = (unsigned long)(1000000.0 / stepsPerSecond); // microseconds between steps
  
  digitalWrite(ENABLE_PIN, LOW); // Enable driver (LOW = enabled, adjust if your board is different)
  delayMicroseconds(100); // Small delay for driver to enable
  lastStepTime = micros();
  
  Serial.print("Motor started at ");
  Serial.print(rpm);
  Serial.print(" RPM (");
  Serial.print(stepsPerSecond);
  Serial.println(" steps/sec)");
}

void stopMotor() {
  motorRunning = false;
  currentRPM = 0.0;
  stepInterval = 0;
  digitalWrite(ENABLE_PIN, HIGH); // Disable driver to save power (HIGH = disabled)
  Serial.println("Motor stopped");
}

void handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Could not open index.html");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleAPI() {
  if (server.method() == HTTP_GET) {
    // Return status
    server.send(200, "application/json", getStatusJSON());
    return;
  }
  
  // Handle POST requests
  if (!server.hasArg("action")) {
    server.send(400, "application/json", "{\"error\":\"Missing action parameter\"}");
    return;
  }
  
  String action = server.arg("action");
  
  if (action == "start") {
    if (!server.hasArg("rpm")) {
      server.send(400, "application/json", "{\"error\":\"Missing rpm parameter\"}");
      return;
    }
    float rpm = server.arg("rpm").toFloat();
    startMotor(rpm);
    server.send(200, "application/json", getStatusJSON());
  }
  else if (action == "stop") {
    stopMotor();
    server.send(200, "application/json", getStatusJSON());
  }
  else if (action == "direction") {
    if (!server.hasArg("dir")) {
      server.send(400, "application/json", "{\"error\":\"Missing dir parameter\"}");
      return;
    }
    motorDirection = (server.arg("dir") == "forward");
    if (motorRunning) {
      digitalWrite(DIR_PIN, motorDirection ? HIGH : LOW);
    }
    server.send(200, "application/json", getStatusJSON());
  }
  else {
    server.send(400, "application/json", "{\"error\":\"Unknown action\"}");
  }
}

String getStatusJSON() {
  String json = "{";
  json += "\"running\":" + String(motorRunning ? "true" : "false") + ",";
  json += "\"rpm\":" + String(currentRPM) + ",";
  json += "\"direction\":\"" + String(motorDirection ? "forward" : "reverse") + "\",";
  json += "\"maxRpm\":" + String(MAX_RPM);
  json += "}";
  return json;
}

String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  return "text/plain";
}
