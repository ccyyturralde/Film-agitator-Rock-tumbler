#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <TMCStepper.h>
#include <HardwareSerial.h>
#include <Preferences.h>

// AP mode credentials (used when no WiFi configured or connection fails)
const char* ap_ssid = "FilmAgitator";
const char* ap_password = "agitate123";

// Motor and driver pins
#define STEP_PIN 26        // Step pin for TMC2209 (STEP on left side)
#define DIR_PIN 25         // Direction pin for TMC2209 (DIR on left side)
#define ENABLE_PIN 33      // Enable pin for TMC2209 (EN on left side)
#define PDN_UART_PIN 32    // PDN_UART pin - set LOW to enable UART mode (optional, some boards auto-detect)
// Serial2 pins for TMC2209 UART - adjust these to match your ESP32 board
#define UART_RX_PIN 4      // RX2 pin (commonly GPIO 4 on ESP32 dev boards)
#define UART_TX_PIN 2      // TX2 pin (commonly GPIO 2 on ESP32 dev boards)
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
Preferences preferences;

// WiFi state
bool isAPMode = false;
String savedSSID = "";
String savedPassword = "";

// Motor control variables
bool motorRunning = false;
float currentRPM = 0.0;
bool motorDirection = true; // true = forward, false = reverse
unsigned long lastStepTime = 0;
unsigned long stepInterval = 0;

// Time control variables
bool timeControlEnabled = false;
unsigned long runDuration = 0; // Duration in milliseconds
unsigned long startTime = 0;
unsigned long elapsedTime = 0;

// Function prototypes
void handleRoot();
void handleCSS();
void handleJS();
void handleSetup();
void handleWiFiConfig();
void handleAPI();
void setupMotor();
void stepMotor();
void stopMotor();
void startMotor(float rpm, unsigned long durationMs);
bool connectToWiFi(String ssid, String password);
void startAPMode();
String getStatusJSON();
String getContentType(String filename);
String getEmbeddedHTML();
String getEmbeddedCSS();
String getEmbeddedJS();
String getSetupHTML();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize SPIFFS for web files
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
  }
  
  // Initialize Preferences for WiFi storage
  preferences.begin("wifi", false);
  savedSSID = preferences.getString("ssid", "");
  savedPassword = preferences.getString("pass", "");
  preferences.end();
  
  // Initialize stepper driver UART
  stepperSerial.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN); // RX2, TX2 pins
  delay(100);
  
  // Initialize motor pins
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(PDN_UART_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, HIGH); // Disable driver initially
  digitalWrite(PDN_UART_PIN, LOW); // Enable UART mode (LOW = UART enabled)
  
  // Setup motor driver
  setupMotor();
  
  // Try to connect to saved WiFi
  if (savedSSID.length() > 0) {
    Serial.print("Attempting to connect to saved WiFi: ");
    Serial.println(savedSSID);
    if (connectToWiFi(savedSSID, savedPassword)) {
      Serial.println("Connected to WiFi!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      isAPMode = false;
    } else {
      Serial.println("Failed to connect to saved WiFi, starting AP mode");
      startAPMode();
    }
  } else {
    Serial.println("No saved WiFi credentials, starting AP mode");
    startAPMode();
  }
  
  // Web server routes
  server.on("/", handleRoot);
  server.on("/index.html", handleRoot);
  server.on("/style.css", handleCSS);
  server.on("/script.js", handleJS);
  server.on("/setup", handleSetup);
  server.on("/wifi-config", HTTP_POST, handleWiFiConfig);
  server.on("/api", HTTP_GET, handleAPI);
  server.on("/api", HTTP_POST, handleAPI);
  server.onNotFound([]() {
    String path = server.uri();
    if (path.endsWith("/")) {
      path += "index.html";
    }
    server.send(404, "text/plain", "File not found: " + path);
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
    
    // Check time control
    if (timeControlEnabled) {
      elapsedTime = millis() - startTime;
      if (elapsedTime >= runDuration) {
        stopMotor();
        timeControlEnabled = false;
        Serial.println("Motor stopped - time limit reached");
      }
    }
  }
}

bool connectToWiFi(String ssid, String password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  return (WiFi.status() == WL_CONNECTED);
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  isAPMode = true;
}

void setupMotor() {
  // Configure TMC2209 driver settings
  driver.begin();
  driver.toff(4);
  driver.rms_current(800);
  driver.microsteps(MICROSTEPS);
  driver.pwm_autoscale(true);
  driver.en_spreadCycle(false);
  driver.TPOWERDOWN(128);
  driver.semin(5);
  driver.semax(2);
  driver.sedn(0b01);
  
  Serial.println("Motor driver configured");
}

void stepMotor() {
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(2);
  digitalWrite(STEP_PIN, LOW);
}

void startMotor(float rpm, unsigned long durationMs = 0) {
  if (rpm <= 0 || rpm > MAX_RPM) {
    Serial.println("Invalid RPM");
    return;
  }
  
  motorRunning = true;
  currentRPM = rpm;
  
  // Set direction
  digitalWrite(DIR_PIN, motorDirection ? HIGH : LOW);
  
  // Calculate step interval based on RPM
  float stepsPerSecond = (rpm / 60.0) * STEPS_PER_REV * MICROSTEPS;
  stepInterval = (unsigned long)(1000000.0 / stepsPerSecond);
  
  // Setup time control
  if (durationMs > 0) {
    timeControlEnabled = true;
    runDuration = durationMs;
    startTime = millis();
    elapsedTime = 0;
  } else {
    timeControlEnabled = false;
  }
  
  digitalWrite(ENABLE_PIN, LOW);
  delayMicroseconds(100);
  lastStepTime = micros();
  
  Serial.print("Motor started at ");
  Serial.print(rpm);
  Serial.print(" RPM");
  if (timeControlEnabled) {
    Serial.print(" for ");
    Serial.print(runDuration / 1000);
    Serial.print(" seconds");
  }
  Serial.print(" (");
  Serial.print(stepsPerSecond);
  Serial.println(" steps/sec)");
}

void stopMotor() {
  motorRunning = false;
  currentRPM = 0.0;
  stepInterval = 0;
  timeControlEnabled = false;
  elapsedTime = 0;
  digitalWrite(ENABLE_PIN, HIGH);
  Serial.println("Motor stopped");
}

void handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  if (file) {
    server.streamFile(file, "text/html");
    file.close();
    return;
  }
  server.send(200, "text/html", getEmbeddedHTML());
}

void handleCSS() {
  // CSS is now embedded in HTML, but keep this for compatibility
  server.send(200, "text/css", "");
}

void handleJS() {
  // JS is now embedded in HTML, but keep this for compatibility
  server.send(200, "application/javascript", "");
}

void handleSetup() {
  server.send(200, "text/html", getSetupHTML());
}

void handleWiFiConfig() {
  if (!server.hasArg("ssid") || !server.hasArg("password")) {
    server.send(400, "text/html", "<html><body><h1>Error</h1><p>Missing SSID or password</p><a href='/setup'>Go back</a></body></html>");
    return;
  }
  
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  
  // Save credentials
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", password);
  preferences.end();
  
  savedSSID = ssid;
  savedPassword = password;
  
  // Try to connect
  server.send(200, "text/html", 
    "<html><body><h1>WiFi Configuration</h1>"
    "<p>Credentials saved! Attempting to connect...</p>"
    "<p>If connection fails, the device will restart in AP mode.</p>"
    "<p>Please wait a few seconds and refresh.</p>"
    "<script>setTimeout(function(){window.location.href='/';}, 5000);</script>"
    "</body></html>");
  
  delay(1000);
  
  // Try to connect
  if (connectToWiFi(ssid, password)) {
    Serial.println("Connected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    isAPMode = false;
  } else {
    Serial.println("Connection failed, restarting in AP mode");
    startAPMode();
  }
}

void handleAPI() {
  if (server.method() == HTTP_GET) {
    server.send(200, "application/json", getStatusJSON());
    return;
  }
  
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
    unsigned long duration = 0;
    if (server.hasArg("duration")) {
      duration = server.arg("duration").toInt() * 1000; // Convert seconds to milliseconds
    }
    startMotor(rpm, duration);
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
  json += "\"maxRpm\":" + String(MAX_RPM) + ",";
  json += "\"wifiMode\":\"" + String(isAPMode ? "ap" : "station") + "\",";
  json += "\"ip\":\"" + (isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\",";
  json += "\"timeControl\":" + String(timeControlEnabled ? "true" : "false") + ",";
  if (timeControlEnabled && motorRunning) {
    unsigned long remaining = (runDuration > elapsedTime) ? (runDuration - elapsedTime) : 0;
    json += "\"remainingTime\":" + String(remaining / 1000) + ",";
    json += "\"elapsedTime\":" + String(elapsedTime / 1000);
  } else {
    json += "\"remainingTime\":0,";
    json += "\"elapsedTime\":0";
  }
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

String getEmbeddedHTML() {
  return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <meta name="mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>Film Agitator Control</title>
    <style>
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent}body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,Ubuntu,Cantarell,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;padding:20px;display:flex;justify-content:center;align-items:center}.container{width:100%;max-width:500px;background:white;border-radius:20px;padding:30px;box-shadow:0 20px 60px rgba(0,0,0,0.3)}h1{text-align:center;color:#333;margin-bottom:30px;font-size:28px;font-weight:700}.status-card{background:#f8f9fa;border-radius:15px;padding:20px;margin-bottom:25px}.status-item{display:flex;justify-content:space-between;align-items:center;padding:12px 0;border-bottom:1px solid #e9ecef}.status-item:last-child{border-bottom:none}.label{font-weight:600;color:#666;font-size:16px}.value{font-weight:700;color:#333;font-size:18px}.value.running{color:#28a745}.value.stopped{color:#dc3545}.control-card{background:#f8f9fa;border-radius:15px;padding:25px;margin-bottom:25px}.control-card label{display:block;font-weight:600;color:#333;margin-bottom:15px;font-size:16px}input[type="range"],input[type="number"]{width:100%;height:8px;border-radius:5px;background:#ddd;outline:none;-webkit-appearance:none;margin-bottom:15px}input[type="number"]{height:45px;padding:12px;border:2px solid #ddd;border-radius:8px;font-size:16px;text-align:center}input[type="number"]:focus{border-color:#667eea}input[type="range"]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:28px;height:28px;border-radius:50%;background:#667eea;cursor:pointer;box-shadow:0 2px 6px rgba(0,0,0,0.2);transition:all 0.2s}input[type="range"]::-webkit-slider-thumb:active{transform:scale(1.1);background:#764ba2}input[type="range"]::-moz-range-thumb{width:28px;height:28px;border-radius:50%;background:#667eea;cursor:pointer;border:none;box-shadow:0 2px 6px rgba(0,0,0,0.2)}.rpm-display{text-align:center;font-size:32px;font-weight:700;color:#667eea;margin-top:10px}.time-display{text-align:center;font-size:24px;font-weight:700;color:#667eea;margin-top:10px}.direction-buttons{display:flex;gap:15px;margin-bottom:25px}.dir-btn{flex:1;padding:15px 20px;font-size:18px;font-weight:600;border:2px solid #ddd;border-radius:12px;background:white;color:#666;cursor:pointer;transition:all 0.2s;touch-action:manipulation}.dir-btn:active{transform:scale(0.98)}.dir-btn.active{background:#667eea;color:white;border-color:#667eea}.control-buttons{display:flex;gap:15px;margin-bottom:20px}.btn{flex:1;padding:18px 25px;font-size:20px;font-weight:700;border:none;border-radius:12px;cursor:pointer;transition:all 0.2s;touch-action:manipulation;text-transform:uppercase;letter-spacing:1px}.btn:active{transform:scale(0.98)}.btn-start{background:linear-gradient(135deg,#28a745,#20c997);color:white;box-shadow:0 4px 15px rgba(40,167,69,0.4)}.btn-start:active{box-shadow:0 2px 8px rgba(40,167,69,0.3)}.btn-stop{background:linear-gradient(135deg,#dc3545,#c82333);color:white;box-shadow:0 4px 15px rgba(220,53,69,0.4)}.btn-stop:active{box-shadow:0 2px 8px rgba(220,53,69,0.3)}.info-text{text-align:center;font-size:14px;color:#666;line-height:1.6;padding-top:15px;border-top:1px solid #e9ecef}.time-toggle{display:flex;align-items:center;gap:10px;margin-bottom:15px}.time-toggle input[type="checkbox"]{width:24px;height:24px;cursor:pointer}@media (max-width:480px){.container{padding:20px;border-radius:15px}h1{font-size:24px;margin-bottom:20px}.rpm-display{font-size:28px}.btn{padding:16px 20px;font-size:18px}}
    </style>
</head>
<body>
    <div class="container">
        <h1>🎬 Film Agitator</h1>
        <div class="status-card">
            <div class="status-item">
                <span class="label">Status:</span>
                <span id="status" class="value stopped">Stopped</span>
            </div>
            <div class="status-item">
                <span class="label">RPM:</span>
                <span id="rpm" class="value">0</span>
            </div>
            <div class="status-item">
                <span class="label">Direction:</span>
                <span id="direction" class="value">Forward</span>
            </div>
            <div class="status-item" id="timeStatus" style="display:none">
                <span class="label">Time Remaining:</span>
                <span id="timeRemaining" class="value">-</span>
            </div>
            <div class="status-item">
                <span class="label">WiFi:</span>
                <span id="wifiInfo" class="value">-</span>
            </div>
        </div>
        <div class="control-card">
            <label for="rpmSlider">Speed (RPM)</label>
            <input type="range" id="rpmSlider" min="10" max="300" value="60" step="10">
            <div class="rpm-display">
                <span id="rpmValue">60</span> RPM
            </div>
        </div>
        <div class="control-card">
            <div class="time-toggle">
                <input type="checkbox" id="timeControlCheck">
                <label for="timeControlCheck" style="margin:0;cursor:pointer">Enable Time Control</label>
            </div>
            <div id="timeControlGroup" style="display:none">
                <label for="durationInput">Duration (seconds)</label>
                <input type="number" id="durationInput" min="1" max="86400" value="300" placeholder="Enter duration">
            </div>
            <div class="time-display" id="timeDisplay" style="display:none">
                <span id="timeValue">0</span>s remaining
            </div>
        </div>
        <div class="direction-buttons">
            <button id="dirForward" class="dir-btn active">Forward</button>
            <button id="dirReverse" class="dir-btn">Reverse</button>
        </div>
        <div class="control-buttons">
            <button id="startBtn" class="btn btn-start">▶ Start</button>
            <button id="stopBtn" class="btn btn-stop">■ Stop</button>
        </div>
        <div class="info-text">
            <a href="/setup" style="color: #667eea; text-decoration: none;">⚙️ WiFi Settings</a>
        </div>
    </div>
    <script>
const API_BASE='/api';
const statusEl=document.getElementById('status');
const rpmEl=document.getElementById('rpm');
const directionEl=document.getElementById('direction');
const wifiInfoEl=document.getElementById('wifiInfo');
const timeStatusEl=document.getElementById('timeStatus');
const timeRemainingEl=document.getElementById('timeRemaining');
const timeDisplayEl=document.getElementById('timeDisplay');
const timeValueEl=document.getElementById('timeValue');
const rpmSlider=document.getElementById('rpmSlider');
const rpmValue=document.getElementById('rpmValue');
const dirForwardBtn=document.getElementById('dirForward');
const dirReverseBtn=document.getElementById('dirReverse');
const startBtn=document.getElementById('startBtn');
const stopBtn=document.getElementById('stopBtn');
const timeControlCheck=document.getElementById('timeControlCheck');
const timeControlGroup=document.getElementById('timeControlGroup');
const durationInput=document.getElementById('durationInput');
let currentRPM=60;
let isRunning=false;
let direction='forward';
let statusCheckInterval=null;
document.addEventListener('DOMContentLoaded',()=>{
    rpmSlider.addEventListener('input',(e)=>{currentRPM=parseInt(e.target.value);rpmValue.textContent=currentRPM});
    timeControlCheck.addEventListener('change',(e)=>{timeControlGroup.style.display=e.target.checked?'block':'none'});
    dirForwardBtn.addEventListener('click',()=>{setDirection('forward')});
    dirReverseBtn.addEventListener('click',()=>{setDirection('reverse')});
    startBtn.addEventListener('click',()=>{startMotor(currentRPM)});
    stopBtn.addEventListener('click',()=>{stopMotor()});
    startStatusPolling();
    updateStatus();
});
async function updateStatus(){
    try{
        const response=await fetch(`${API_BASE}`);
        const data=await response.json();
        isRunning=data.running;
        direction=data.direction;
        statusEl.textContent=isRunning?'Running':'Stopped';
        statusEl.className=`value ${isRunning?'running':'stopped'}`;
        rpmEl.textContent=Math.round(data.rpm);
        directionEl.textContent=data.direction.charAt(0).toUpperCase()+data.direction.slice(1);
        if(data.wifiMode)wifiInfoEl.textContent=data.wifiMode==='ap'?'AP Mode':data.ip||'Connected';
        if(data.timeControl&&data.remainingTime>0){
            timeStatusEl.style.display='flex';
            timeDisplayEl.style.display='block';
            const mins=Math.floor(data.remainingTime/60);
            const secs=data.remainingTime%60;
            timeRemainingEl.textContent=mins>0?`${mins}m ${secs}s`:`${secs}s`;
            timeValueEl.textContent=data.remainingTime;
        }else{
            timeStatusEl.style.display='none';
            timeDisplayEl.style.display='none';
        }
        if(direction==='forward'){
            dirForwardBtn.classList.add('active');
            dirReverseBtn.classList.remove('active');
        }else{
            dirForwardBtn.classList.remove('active');
            dirReverseBtn.classList.add('active');
        }
        if(isRunning&&data.rpm>0){
            currentRPM=data.rpm;
            rpmSlider.value=currentRPM;
            rpmValue.textContent=Math.round(currentRPM);
        }
    }catch(error){
        console.error('Error updating status:',error);
        statusEl.textContent='Error';
        statusEl.className='value stopped';
    }
}
async function startMotor(rpm){
    try{
        const formData=new URLSearchParams();
        formData.append('action','start');
        formData.append('rpm',rpm.toString());
        if(timeControlCheck.checked&&durationInput.value){
            formData.append('duration',durationInput.value);
        }
        const response=await fetch(`${API_BASE}`,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData.toString()});
        if(response.ok){
            const data=await response.json();
            updateUIFromResponse(data);
            console.log('Motor started at',rpm,'RPM');
        }else{
            console.error('Failed to start motor');
            alert('Failed to start motor. Please try again.');
        }
    }catch(error){
        console.error('Error starting motor:',error);
        alert('Error starting motor. Check connection.');
    }
}
async function stopMotor(){
    try{
        const formData=new URLSearchParams();
        formData.append('action','stop');
        const response=await fetch(`${API_BASE}`,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData.toString()});
        if(response.ok){
            const data=await response.json();
            updateUIFromResponse(data);
            console.log('Motor stopped');
        }else{
            console.error('Failed to stop motor');
            alert('Failed to stop motor. Please try again.');
        }
    }catch(error){
        console.error('Error stopping motor:',error);
        alert('Error stopping motor. Check connection.');
    }
}
async function setDirection(dir){
    try{
        const formData=new URLSearchParams();
        formData.append('action','direction');
        formData.append('dir',dir);
        const response=await fetch(`${API_BASE}`,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData.toString()});
        if(response.ok){
            const data=await response.json();
            updateUIFromResponse(data);
            console.log('Direction set to',dir);
        }else{
            console.error('Failed to set direction');
        }
    }catch(error){
        console.error('Error setting direction:',error);
    }
}
function updateUIFromResponse(data){
    isRunning=data.running;
    direction=data.direction;
    statusEl.textContent=isRunning?'Running':'Stopped';
    statusEl.className=`value ${isRunning?'running':'stopped'}`;
    rpmEl.textContent=Math.round(data.rpm);
    directionEl.textContent=data.direction.charAt(0).toUpperCase()+data.direction.slice(1);
    if(data.wifiMode)wifiInfoEl.textContent=data.wifiMode==='ap'?'AP Mode':data.ip||'Connected';
    if(direction==='forward'){
        dirForwardBtn.classList.add('active');
        dirReverseBtn.classList.remove('active');
    }else{
        dirForwardBtn.classList.remove('active');
        dirReverseBtn.classList.add('active');
    }
}
function startStatusPolling(){
    statusCheckInterval=setInterval(updateStatus,500);
}
window.addEventListener('beforeunload',(e)=>{if(isRunning){e.preventDefault();e.returnValue=''}});
    </script>
</body>
</html>)";
}

String getEmbeddedCSS() {
  return R"(* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
    -webkit-tap-highlight-color: transparent;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    min-height: 100vh;
    padding: 20px;
    display: flex;
    justify-content: center;
    align-items: center;
}

.container {
    width: 100%;
    max-width: 500px;
    background: white;
    border-radius: 20px;
    padding: 30px;
    box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
}

h1 {
    text-align: center;
    color: #333;
    margin-bottom: 30px;
    font-size: 28px;
    font-weight: 700;
}

.status-card {
    background: #f8f9fa;
    border-radius: 15px;
    padding: 20px;
    margin-bottom: 25px;
}

.status-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 12px 0;
    border-bottom: 1px solid #e9ecef;
}

.status-item:last-child {
    border-bottom: none;
}

.label {
    font-weight: 600;
    color: #666;
    font-size: 16px;
}

.value {
    font-weight: 700;
    color: #333;
    font-size: 18px;
}

.value.running {
    color: #28a745;
}

.value.stopped {
    color: #dc3545;
}

.control-card {
    background: #f8f9fa;
    border-radius: 15px;
    padding: 25px;
    margin-bottom: 25px;
}

.control-card label {
    display: block;
    font-weight: 600;
    color: #333;
    margin-bottom: 15px;
    font-size: 16px;
}

input[type="range"] {
    width: 100%;
    height: 8px;
    border-radius: 5px;
    background: #ddd;
    outline: none;
    -webkit-appearance: none;
    margin-bottom: 15px;
}

input[type="range"]::-webkit-slider-thumb {
    -webkit-appearance: none;
    appearance: none;
    width: 28px;
    height: 28px;
    border-radius: 50%;
    background: #667eea;
    cursor: pointer;
    box-shadow: 0 2px 6px rgba(0, 0, 0, 0.2);
    transition: all 0.2s;
}

input[type="range"]::-webkit-slider-thumb:active {
    transform: scale(1.1);
    background: #764ba2;
}

input[type="range"]::-moz-range-thumb {
    width: 28px;
    height: 28px;
    border-radius: 50%;
    background: #667eea;
    cursor: pointer;
    border: none;
    box-shadow: 0 2px 6px rgba(0, 0, 0, 0.2);
}

.rpm-display {
    text-align: center;
    font-size: 32px;
    font-weight: 700;
    color: #667eea;
    margin-top: 10px;
}

.direction-buttons {
    display: flex;
    gap: 15px;
    margin-bottom: 25px;
}

.dir-btn {
    flex: 1;
    padding: 15px 20px;
    font-size: 18px;
    font-weight: 600;
    border: 2px solid #ddd;
    border-radius: 12px;
    background: white;
    color: #666;
    cursor: pointer;
    transition: all 0.2s;
    touch-action: manipulation;
}

.dir-btn:active {
    transform: scale(0.98);
}

.dir-btn.active {
    background: #667eea;
    color: white;
    border-color: #667eea;
}

.control-buttons {
    display: flex;
    gap: 15px;
    margin-bottom: 20px;
}

.btn {
    flex: 1;
    padding: 18px 25px;
    font-size: 20px;
    font-weight: 700;
    border: none;
    border-radius: 12px;
    cursor: pointer;
    transition: all 0.2s;
    touch-action: manipulation;
    text-transform: uppercase;
    letter-spacing: 1px;
}

.btn:active {
    transform: scale(0.98);
}

.btn-start {
    background: linear-gradient(135deg, #28a745, #20c997);
    color: white;
    box-shadow: 0 4px 15px rgba(40, 167, 69, 0.4);
}

.btn-start:active {
    box-shadow: 0 2px 8px rgba(40, 167, 69, 0.3);
}

.btn-stop {
    background: linear-gradient(135deg, #dc3545, #c82333);
    color: white;
    box-shadow: 0 4px 15px rgba(220, 53, 69, 0.4);
}

.btn-stop:active {
    box-shadow: 0 2px 8px rgba(220, 53, 69, 0.3);
}

.info-text {
    text-align: center;
    font-size: 14px;
    color: #666;
    line-height: 1.6;
    padding-top: 15px;
    border-top: 1px solid #e9ecef;
}

@media (max-width: 480px) {
    .container {
        padding: 20px;
        border-radius: 15px;
    }
    
    h1 {
        font-size: 24px;
        margin-bottom: 20px;
    }
    
    .rpm-display {
        font-size: 28px;
    }
    
    .btn {
        padding: 16px 20px;
        font-size: 18px;
    }
})";
}

String getEmbeddedJS() {
  return R"(const API_BASE='/api';
const statusEl=document.getElementById('status');
const rpmEl=document.getElementById('rpm');
const directionEl=document.getElementById('direction');
const wifiInfoEl=document.getElementById('wifiInfo');
const rpmSlider=document.getElementById('rpmSlider');
const rpmValue=document.getElementById('rpmValue');
const dirForwardBtn=document.getElementById('dirForward');
const dirReverseBtn=document.getElementById('dirReverse');
const startBtn=document.getElementById('startBtn');
const stopBtn=document.getElementById('stopBtn');
let currentRPM=60;
let isRunning=false;
let direction='forward';
let statusCheckInterval=null;
document.addEventListener('DOMContentLoaded',()=>{
    rpmSlider.addEventListener('input',(e)=>{currentRPM=parseInt(e.target.value);rpmValue.textContent=currentRPM});
    dirForwardBtn.addEventListener('click',()=>{setDirection('forward')});
    dirReverseBtn.addEventListener('click',()=>{setDirection('reverse')});
    startBtn.addEventListener('click',()=>{startMotor(currentRPM)});
    stopBtn.addEventListener('click',()=>{stopMotor()});
    startStatusPolling();
    updateStatus();
});
async function updateStatus(){
    try{
        const response=await fetch(`${API_BASE}`);
        const data=await response.json();
        isRunning=data.running;
        direction=data.direction;
        statusEl.textContent=isRunning?'Running':'Stopped';
        statusEl.className=`value ${isRunning?'running':'stopped'}`;
        rpmEl.textContent=Math.round(data.rpm);
        directionEl.textContent=data.direction.charAt(0).toUpperCase()+data.direction.slice(1);
        if(data.wifiMode)wifiInfoEl.textContent=data.wifiMode==='ap'?'AP Mode':data.ip||'Connected';
        if(direction==='forward'){
            dirForwardBtn.classList.add('active');
            dirReverseBtn.classList.remove('active');
        }else{
            dirForwardBtn.classList.remove('active');
            dirReverseBtn.classList.add('active');
        }
        if(isRunning&&data.rpm>0){
            currentRPM=data.rpm;
            rpmSlider.value=currentRPM;
            rpmValue.textContent=Math.round(currentRPM);
        }
    }catch(error){
        console.error('Error updating status:',error);
        statusEl.textContent='Error';
        statusEl.className='value stopped';
    }
}
async function startMotor(rpm){
    try{
        const formData=new URLSearchParams();
        formData.append('action','start');
        formData.append('rpm',rpm.toString());
        const response=await fetch(`${API_BASE}`,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData.toString()});
        if(response.ok){
            const data=await response.json();
            updateUIFromResponse(data);
            console.log('Motor started at',rpm,'RPM');
        }else{
            console.error('Failed to start motor');
            alert('Failed to start motor. Please try again.');
        }
    }catch(error){
        console.error('Error starting motor:',error);
        alert('Error starting motor. Check connection.');
    }
}
async function stopMotor(){
    try{
        const formData=new URLSearchParams();
        formData.append('action','stop');
        const response=await fetch(`${API_BASE}`,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData.toString()});
        if(response.ok){
            const data=await response.json();
            updateUIFromResponse(data);
            console.log('Motor stopped');
        }else{
            console.error('Failed to stop motor');
            alert('Failed to stop motor. Please try again.');
        }
    }catch(error){
        console.error('Error stopping motor:',error);
        alert('Error stopping motor. Check connection.');
    }
}
async function setDirection(dir){
    try{
        const formData=new URLSearchParams();
        formData.append('action','direction');
        formData.append('dir',dir);
        const response=await fetch(`${API_BASE}`,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formData.toString()});
        if(response.ok){
            const data=await response.json();
            updateUIFromResponse(data);
            console.log('Direction set to',dir);
        }else{
            console.error('Failed to set direction');
        }
    }catch(error){
        console.error('Error setting direction:',error);
    }
}
function updateUIFromResponse(data){
    isRunning=data.running;
    direction=data.direction;
    statusEl.textContent=isRunning?'Running':'Stopped';
    statusEl.className=`value ${isRunning?'running':'stopped'}`;
    rpmEl.textContent=Math.round(data.rpm);
    directionEl.textContent=data.direction.charAt(0).toUpperCase()+data.direction.slice(1);
    if(data.wifiMode)wifiInfoEl.textContent=data.wifiMode==='ap'?'AP Mode':data.ip||'Connected';
    if(direction==='forward'){
        dirForwardBtn.classList.add('active');
        dirReverseBtn.classList.remove('active');
    }else{
        dirForwardBtn.classList.remove('active');
        dirReverseBtn.classList.add('active');
    }
}
function startStatusPolling(){
    statusCheckInterval=setInterval(updateStatus,500);
}
window.addEventListener('beforeunload',(e)=>{if(isRunning){e.preventDefault();e.returnValue=''}});)";
}

String getSetupHTML() {
  return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Setup - Film Agitator</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
            display: flex;
            justify-content: center;
            align-items: center;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 30px;
            max-width: 500px;
            width: 100%;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        }
        h1 { text-align: center; color: #333; margin-bottom: 30px; }
        .form-group { margin-bottom: 20px; }
        label { display: block; font-weight: 600; color: #333; margin-bottom: 8px; }
        input[type="text"], input[type="password"] {
            width: 100%;
            padding: 12px;
            border: 2px solid #ddd;
            border-radius: 8px;
            font-size: 16px;
        }
        input:focus { outline: none; border-color: #667eea; }
        .btn {
            width: 100%;
            padding: 14px;
            background: linear-gradient(135deg, #667eea, #764ba2);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 18px;
            font-weight: 700;
            cursor: pointer;
            margin-top: 10px;
        }
        .btn:hover { opacity: 0.9; }
        .btn:active { transform: scale(0.98); }
        .back-link {
            display: block;
            text-align: center;
            margin-top: 20px;
            color: #667eea;
            text-decoration: none;
        }
        .info {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
            font-size: 14px;
            color: #666;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>⚙️ WiFi Configuration</h1>
        <div class="info">
            Enter your home WiFi network credentials. The device will connect to your network and you can access it from any device on your network while keeping internet access.
        </div>
        <form action="/wifi-config" method="POST">
            <div class="form-group">
                <label for="ssid">WiFi Network Name (SSID):</label>
                <input type="text" id="ssid" name="ssid" required placeholder="Your WiFi Network Name">
            </div>
            <div class="form-group">
                <label for="password">WiFi Password:</label>
                <input type="password" id="password" name="password" required placeholder="Your WiFi Password">
            </div>
            <button type="submit" class="btn">Save & Connect</button>
        </form>
        <a href="/" class="back-link">← Back to Control Panel</a>
    </div>
</body>
</html>)";
}
