# Virtual ESP32 Keyboard

Program which allows you to connect one device via Bluetooth and another via WiFi to ESP32 and then send keystrokes using a web interface hosted on `192.168.4.1`.

I wanted to create something similar to Rubber Ducky, but I’m still waiting on my USB connectors to arrive so I decided to make something like this lol.

## List of keys:

- ctrl
- alt
- shift
- win
- enter
- esc
- tab
- f4

## Steps files:

A Steps file is essentially a `.txt` file that you import steps from. For an example, see `example.txt`.

## How It Works:

1. **WiFi Access Point:**
   - Connect to the device via WiFi (SSID: `EPS`, Password: `12345678`).
   
2. **Web Interface:**
   - Open a browser and navigate to the device’s IP (default: `192.168.4.1`).
   - Use the web interface to input keystroke sequences or upload steps from a `.txt` file.

3. **Bluetooth Connection:**
   - The device emulates a Bluetooth keyboard using the name `blutuf klawa`.

4. **Execute Commands:**
   - Create a sequence of key presses via the web interface or upload a steps file, and the device will execute the sequence on the connected Bluetooth device.

## Getting Started:

1. Flash the firmware to your ESP32 using your preferred method.
2. Power up the device, connect to its WiFi, and access the web interface.
3. Pair the ESP32 to your Bluetooth device as a keyboard.
4. Start sending keystrokes using the web interface or by uploading a steps file!