# Upload Instructions - Film Agitator

Follow these steps to upload the firmware and web files to your ESP32.

## Option 1: Using PlatformIO (Recommended)

### Step 1: Install PlatformIO

**If you don't have PlatformIO installed:**

1. **VS Code Extension (Easiest):**
   - Install [VS Code](https://code.visualstudio.com/)
   - Open VS Code
   - Go to Extensions (Ctrl+Shift+X / Cmd+Shift+X)
   - Search for "PlatformIO IDE" and install it
   - Restart VS Code

2. **Or use PlatformIO CLI:**
   ```bash
   # Install via pip
   pip install platformio
   ```

### Step 2: Open the Project

1. Open VS Code
2. Click "File" → "Open Folder"
3. Navigate to your project folder: `Film-agitator-Rock-tumbler`
4. PlatformIO should automatically detect the project

### Step 3: Install Dependencies

1. Click on the PlatformIO icon in the left sidebar (ant icon)
2. Go to "PROJECT TASKS" → "esp32dev" → "General"
3. Click "lib install" (or run `pio lib install` in terminal)

### Step 4: Connect Your ESP32

1. Connect your ESP32 to your computer via USB
2. Note which COM port it's on:
   - **Windows:** Check Device Manager → Ports (COM & LPT)
   - **Mac/Linux:** Run `ls /dev/tty.*` or `ls /dev/ttyUSB*`

### Step 5: Configure COM Port (if needed)

If PlatformIO doesn't auto-detect your port, edit `platformio.ini` and add:
```ini
upload_port = COM3  ; Windows: COM3, COM4, etc.
; or
upload_port = /dev/ttyUSB0  ; Linux
upload_port = /dev/tty.usbserial-*  ; Mac
```

### Step 6: Upload Firmware

1. In PlatformIO sidebar, go to "PROJECT TASKS" → "esp32dev"
2. Click "upload" (or press Ctrl+Alt+U / Cmd+Alt+U)
3. Wait for compilation and upload to complete
4. You should see "SUCCESS" message

### Step 7: Upload Web Files (SPIFFS)

1. In PlatformIO sidebar, go to "PROJECT TASKS" → "esp32dev"
2. Click "Upload Filesystem Image" (or run `pio run --target uploadfs`)
3. Wait for upload to complete

### Step 8: Open Serial Monitor

1. In PlatformIO sidebar, click "Monitor" (or press Ctrl+Alt+S / Cmd+Alt+S)
2. You should see:
   ```
   AP IP address: 192.168.4.1
   Web server started
   ```

**Done!** Your ESP32 is now ready. Connect to WiFi "FilmAgitator" and open `http://192.168.4.1`

---

## Option 2: Using Arduino IDE

### Step 1: Install Arduino IDE and ESP32 Support

1. Download [Arduino IDE](https://www.arduino.cc/en/software)
2. Open Arduino IDE
3. Go to **File** → **Preferences**
4. In "Additional Board Manager URLs", add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
5. Go to **Tools** → **Board** → **Boards Manager**
6. Search for "esp32" and install "esp32 by Espressif Systems"

### Step 2: Install Required Libraries

1. Go to **Sketch** → **Include Library** → **Manage Libraries**
2. Search for and install:
   - **TMCStepper** by Teemu Mäntylä

### Step 3: Install SPIFFS Upload Tool

1. Download: [ESP32 Sketch Data Upload](https://github.com/me-no-dev/arduino-esp32fs-plugin/releases)
2. Extract the ZIP file
3. Copy the extracted folder to:
   - **Windows:** `C:\Users\YourName\Documents\Arduino\tools\`
   - **Mac:** `~/Documents/Arduino/tools/`
   - **Linux:** `~/Arduino/tools/`
4. Create the `tools` folder if it doesn't exist
5. Restart Arduino IDE

### Step 4: Prepare the Sketch

1. In Arduino IDE, go to **File** → **New**
2. Copy the entire contents of `src/main.cpp`
3. Paste into the new sketch
4. Save the sketch (e.g., as "FilmAgitator")

### Step 5: Configure Board Settings

1. Go to **Tools** → **Board** → Select your ESP32 board (e.g., "ESP32 Dev Module")
2. Go to **Tools** → **Port** → Select your ESP32's COM port
3. Set **Upload Speed** to "115200"
4. Set **CPU Frequency** to "240MHz (WiFi/BT)"
5. Set **Flash Frequency** to "80MHz"
6. Set **Flash Size** to "4MB (32Mb)"
7. Set **Partition Scheme** to "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"

### Step 6: Upload Firmware

1. Click the **Upload** button (→) in Arduino IDE
2. Wait for compilation and upload
3. You should see "Hard resetting via RTS pin..." when done

### Step 7: Upload Web Files

1. Make sure your sketch folder has a `data` subfolder
2. Copy all files from `data/` folder to your sketch's `data/` folder:
   - `index.html`
   - `style.css`
   - `script.js`
3. In Arduino IDE, go to **Tools** → **ESP32 Sketch Data Upload**
4. Wait for upload to complete

### Step 8: Open Serial Monitor

1. Click **Tools** → **Serial Monitor**
2. Set baud rate to **115200**
3. You should see:
   ```
   AP IP address: 192.168.4.1
   Web server started
   ```

**Done!** Your ESP32 is now ready.

---

## Troubleshooting

### "Port not found" or "Failed to connect"

- **Check USB cable:** Use a data cable, not just a charging cable
- **Install drivers:** Install CP2102 or CH340 drivers for your ESP32 board
- **Try different USB port:** Some USB ports don't work well
- **Hold BOOT button:** Some boards need BOOT button held during upload

### "SPIFFS Mount Failed"

- Make sure you uploaded the filesystem image (Step 7)
- Try erasing flash: In PlatformIO, run `pio run --target erase`
- Then upload firmware and filesystem again

### "Compilation error: TMCStepper not found"

- Make sure library is installed correctly
- In PlatformIO: Run `pio lib install`
- In Arduino IDE: Reinstall TMCStepper library

### Board keeps resetting

- Check power supply: ESP32 needs stable 5V via USB
- Check wiring: Make sure no shorts
- Try reducing motor current in code

### Can't connect to WiFi

- Make sure ESP32 finished booting (wait 10 seconds after upload)
- Check Serial Monitor for IP address
- Try resetting ESP32 (press RESET button)
- Make sure you're connecting to "FilmAgitator" network, not your home WiFi

---

## Quick Command Reference (PlatformIO CLI)

```bash
# Navigate to project directory
cd Film-agitator-Rock-tumbler

# Install libraries
pio lib install

# Build project
pio run

# Upload firmware
pio run --target upload

# Upload filesystem
pio run --target uploadfs

# Open serial monitor
pio device monitor

# Erase flash (if needed)
pio run --target erase
```

---

## Next Steps

After successful upload:

1. **Connect to WiFi:**
   - On your phone/computer, connect to WiFi network "FilmAgitator"
   - Password: "agitate123"

2. **Open Web Interface:**
   - Open browser and go to: `http://192.168.4.1`
   - You should see the Film Agitator control interface

3. **Test Motor:**
   - Set a low RPM (e.g., 30 RPM)
   - Click "Start"
   - Motor should begin rotating
   - Click "Stop" to stop

4. **Adjust Settings:**
   - If motor doesn't move, check wiring
   - If motor runs wrong direction, swap one coil pair
   - Adjust speed as needed for your application
