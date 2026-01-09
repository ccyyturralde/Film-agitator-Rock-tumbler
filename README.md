# Film Agitator / Rock Tumbler Controller

A mobile-friendly web application for controlling a stepper motor-powered film agitation machine using an ESP32-WROOM-32, TMC2209 stepper driver, and NEMA 17 motor.

## Hardware Requirements

- ESP32-WROOM-32 Development Kit
- TMC2209 Stepper Motor Driver
- NEMA 17 Stepper Motor (1.8° step angle, 200 steps/revolution)
- 12V Power Supply (for motor driver)
- USB Cable (for ESP32 power and programming)
- Jumper wires and breadboard/protoboard

## Wiring Diagram

### ESP32 to TMC2209 Connections

| ESP32 Pin | TMC2209 Pin | Description |
|-----------|-------------|-------------|
| GPIO 26   | STEP        | Step signal (STEP pin on left side) |
| GPIO 25   | DIR         | Direction signal (DIR pin on left side) |
| GPIO 33   | ENABLE      | Enable/Disable driver (EN pin on left side) |
| GPIO 16   | RX2         | Serial2 RX → TMC2209 USART pin (via small breadboard/perfboard) |
| GPIO 17   | TX2         | Serial2 TX → TMC2209 USART pin via 1kΩ resistor (via small breadboard/perfboard) |
| GPIO 32   | (Optional)   | PDN pin control (only if not using jumper - set LOW to enable UART mode) |
|           |             | **Note:** Both RX2 and TX2 connect to the SINGLE USART pin on TMC2209. You need a small breadboard/perfboard to combine them. See wiring details below. |
| GND       | GND         | Common ground (connect to GND on right side) |
| 3.3V      | VDD         | Logic power (VDD pin on right side - 3.3V) |

### Power Connections

1. **Motor Power (12V Supply):**
   - Connect 12V positive to TMC2209 VM+
   - Connect 12V negative to TMC2209 GND
   - **Important:** Use proper fusing and ensure adequate current capacity (2-3A recommended)

2. **ESP32 Power:**
   - Connect via USB cable to computer or USB power adapter

3. **Common Ground:**
   - Connect ESP32 GND to TMC2209 GND (critical for proper operation)

### TMC2209 to NEMA 17 Motor

| TMC2209 | NEMA 17 | Description |
|---------|---------|-------------|
| 1B      | Coil 1+ | Motor coil 1 positive |
| 1A      | Coil 1- | Motor coil 1 negative |
| 2B      | Coil 2+ | Motor coil 2 positive |
| 2A      | Coil 2- | Motor coil 2 negative |

**Note:** If motor runs in wrong direction, swap one coil pair (e.g., swap 1A and 1B).

### UART Wiring - Crystal Clear Answer

**Your TMC2209 has these pins available: MS1, MS2, PDN, USART**

**Where ESP32 RX2 and TX2 connect:**
- **ESP32 RX2 (GPIO 16) → TMC2209 USART pin** (direct connection)
- **ESP32 TX2 (GPIO 17) → 1kΩ resistor → TMC2209 USART pin** (resistor inline)

**BOTH wires go to the SAME pin: the USART pin on TMC2209.**

This is called half-duplex UART - one pin handles both transmit and receive.

**MS1, MS2:** Not used (we configure microstepping via UART instead)  
**PDN:** Not used if your board has a jumper to enable UART mode (most do)

### The Problem: Two Wires, One Pin

You cannot physically plug two Dupont connectors into one USART pin. You must combine the wires:

**Options to combine the wires:**

1. **Y-connector with Dupont (RECOMMENDED - compact & clean):**
   - Take 3 Dupont wires (female-to-female)
   - Cut one end off two of them (for ESP32 RX2 and TX2)
   - Cut both ends off the third (for the Y junction)
   - Solder a 1kΩ resistor inline on the TX2 wire
   - Solder all three together: RX2 + TX2 (with resistor) + wire to USART
   - Wrap the solder joint with heat shrink
   - Result: Two Dupont connectors for ESP32, one for TMC2209 USART
   ```
   ESP32 RX2 ────────────┐
                          ├─── → TMC2209 USART
   ESP32 TX2 ──[1kΩ]─────┘
   ```

2. **Wire splice connector:**
   - Use a small crimp/splice connector
   - Join: TX2 (with resistor), RX2, and wire to USART

3. **Tiny 3-way terminal block:**
   - Screw terminals to join TX2, RX2, and USART wire
   - About 1cm in size

**All other connections use Dupont connectors directly:**
- STEP, DIR, EN → Direct pin-to-pin
- Power (12V, 3.3V, GND) → Direct connections
- Motor coils → Direct to TMC2209

### TMC2209 Configuration Jumpers

- **MS1, MS2, MS3:** **NOT NEEDED** - We control microsteps via UART (set to 16 microsteps in software). Leave these pins unconnected or set to any state.
- **PDN pin:** Must be set LOW for UART mode. Most boards have a jumper - set it to enable UART mode. If your board doesn't have a jumper, connect PDN pin to GND or use GPIO 32.
- **USART pin:** Both ESP32 RX2 and TX2 connect to THIS pin (half-duplex UART)
  - RX2 → USART (direct)
  - TX2 → 1kΩ resistor → USART (same pin as RX2)
  - You must combine the wires (solder/splice/terminal block) because two Dupont connectors won't fit in one pin
- **VIO/VDD:** Can be connected to 3.3V if driver board requires it (most TMC2209 boards support 3.3V logic levels - check your board specs)

## Software Setup

### Prerequisites

1. Install [PlatformIO IDE](https://platformio.org/) (or use PlatformIO CLI)
2. Alternatively, use Arduino IDE with ESP32 board support

### Building and Uploading

1. **Using PlatformIO:**
   ```bash
   # Install dependencies
   pio lib install
   
   # Upload firmware
   pio run --target upload
   
   # Upload web files to SPIFFS
   pio run --target uploadfs
   ```

2. **Using Arduino IDE:**
   - Install ESP32 board support via Board Manager
   - Install libraries: `TMCStepper` (Teemu Mäntylä)
   - Install SPIFFS upload tool: [ESP32 Sketch Data Upload](https://github.com/me-no-dev/arduino-esp32fs-plugin)
   - Upload `src/main.cpp` as a sketch
   - Upload `data/` folder contents to SPIFFS

### Initial Configuration

1. After uploading firmware, open Serial Monitor (115200 baud)
2. ESP32 will create a WiFi Access Point named "FilmAgitator"
3. Password: "agitate123"
4. Note the IP address displayed in Serial Monitor (typically 192.168.4.1)

## Usage

1. Connect your mobile device to the "FilmAgitator" WiFi network
2. Open a web browser and navigate to: `http://192.168.4.1`
3. The web interface will display:
   - Current motor status (Running/Stopped)
   - Current RPM
   - Current direction (Forward/Reverse)

### Controls

- **Speed Slider:** Adjust motor speed from 10-300 RPM
- **Direction Buttons:** Select forward or reverse rotation
- **Start Button:** Start the motor at the selected speed
- **Stop Button:** Stop the motor immediately

### Motor Parameters

- **Steps per Revolution:** 200 (standard NEMA 17)
- **Microsteps:** 16 (configurable in code)
- **Max RPM:** 300 (adjustable in `MAX_RPM` define)
- **Current:** 800mA RMS (adjustable in `setupMotor()` function)

## Customization

### Changing WiFi Credentials

Edit `src/main.cpp`:
```cpp
const char* ssid = "YourNetworkName";
const char* password = "YourPassword";
```

### Adjusting Motor Speed Range

Edit `src/main.cpp`:
```cpp
#define MAX_RPM 300  // Change to desired maximum RPM
```

### Adjusting Motor Current

Edit `setupMotor()` function in `src/main.cpp`:
```cpp
driver.rms_current(800);  // Change to desired current in mA
```

### Changing Pin Assignments

Edit the pin definitions in `src/main.cpp`:
```cpp
#define STEP_PIN 26
#define DIR_PIN 25
#define ENABLE_PIN 33
```

**Note:** If you change UART pins (currently GPIO 16/17), also update:
```cpp
stepperSerial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
```

## Troubleshooting

### Motor Doesn't Move

1. Check all power connections (12V supply, USB power)
2. Verify common ground between ESP32 and TMC2209
3. Check motor coil connections (try swapping one coil pair)
4. Verify ENABLE pin logic (some boards have inverted logic)
5. If your TMC2209 board has a VIO pin, connect it to ESP32 3.3V (not 5V)
6. Check Serial Monitor for error messages

### Motor Runs Erratically

1. Verify TMC2209 UART connection (GPIO 16/17)
2. Check if PDN_UART jumper is set correctly on driver board
3. Adjust motor current if too low
4. Check for loose connections

### Web Interface Not Loading

1. Verify ESP32 WiFi AP is active (check Serial Monitor)
2. Confirm device is connected to "FilmAgitator" network
3. Clear browser cache and try again
4. Check if SPIFFS files uploaded correctly

### Motor Overheating

1. Reduce motor current in `setupMotor()`
2. Reduce RPM if running at maximum speed for extended periods
3. Ensure adequate ventilation

## Safety Considerations

- **Electrical Safety:**
  - Use appropriate fusing for 12V power supply
  - Ensure all connections are secure and insulated
  - Keep liquids away from electrical components

- **Mechanical Safety:**
  - Securely mount the motor and drum
  - Ensure drum is balanced and won't come loose during operation
  - Test at low speeds before running at maximum RPM

- **Overcurrent Protection:**
  - The TMC2209 has built-in overcurrent protection
  - Monitor motor temperature during operation
  - Start with lower current settings and increase gradually

## Technical Specifications

- **ESP32:** Dual-core 240MHz, WiFi 802.11 b/g/n
- **Stepper Driver:** TMC2209 SilentStepStick
- **Motor:** NEMA 17, 1.8° step angle, 200 steps/revolution
- **Power:** 12V for motor, 5V USB for ESP32 (ESP32 uses 3.3V logic levels)
- **Control Method:** UART-based TMC2209 configuration, step/dir control

## License

This project is provided as-is for educational and personal use.

## Additional Resources

- [TMC2209 Datasheet](https://www.trinamic.com/fileadmin/assets/Products/ICs_Documents/TMC2209_datasheet.pdf)
- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [TMCStepper Library](https://github.com/teemuatlut/TMCStepper)
