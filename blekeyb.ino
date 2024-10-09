#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <BleKeyboard.h>
#include "esp_bt.h"
#include "esp_bt_main.h"

#define DEVICE_NAME "niebieskizab"      // BLE Device Name
#define MANUFACTURER "MC"    // BLE Manufacturer

// Access Point credentials
const char* ap_ssid = "EPS";
const char* ap_password = "12345678";

// Create a WiFiServer object on port 80
WiFiServer server(80);

// BLE Keyboard initialization with shortened names
BleKeyboard bleKeyboard(DEVICE_NAME, MANUFACTURER, 69);

// Embedded and minified HTML content
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <body>
    <h1>ESP32 Keyboard</h1>
    <form>
      <textarea id="s"></textarea><br>
      <button>Send</button>
    </form>
    <p id="st"></p>
    <script>
      f = document.forms[0];
      f.addEventListener('submit', function(e) {
        e.preventDefault();
        fetch('/send', {
          method: 'POST',
          headers: {'Content-Type':'application/x-www-form-urlencoded'},
          body: 'sequence=' + encodeURIComponent(document.getElementById('s').value)
        }).then(r => r.text()).then(d => {
          document.getElementById('st').innerText = d;
        });
      });
    </script>
  </body>
</html>
)rawliteral";

// Function prototypes
void parseAndExecuteSequence(const char* sequence);
void executeCommand(const char* command);
void pressKeys(const char* keys);
void pressKey(const char* key);
void typeText(const char* text); // New function to type text with delay
String urlDecode(String input);

bool shiftPressed = false;

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  Serial.println("Starting...");

  // Release memory used by Classic Bluetooth
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  // Initialize BLE Keyboard
  bleKeyboard.begin();
  Serial.println("BLE Keyboard started.");

  // Initialize Wi-Fi Access Point
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("Wi-Fi Access Point started.");

  // Start the server
  server.begin();
  Serial.println("Server started.");
}

void loop() {
  // Handle Wi-Fi client connections
  WiFiClient client = server.available();
  if (client) {
    Serial.println("Client connected.");
    String request = client.readStringUntil('\r');
    client.flush();

    // Handle GET request for "/"
    if (request.indexOf("GET / ") != -1) {
      Serial.println("Serving index.html");
      // Send index.html
      client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
      client.print(index_html);
    }
    // Handle POST request for "/send"
    else if (request.indexOf("POST /send") != -1) {
      Serial.println("Received POST request for /send");
      // Read headers
      while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
          break;
        }
      }

      // Read the body
      String body = client.readString();
      Serial.print("Request body: ");
      Serial.println(body);

      // Parse "sequence" parameter
      int seqIndex = body.indexOf("sequence=");
      if (seqIndex != -1) {
        String sequence = body.substring(seqIndex + 9);
        sequence = urlDecode(sequence);
        Serial.print("Sequence received: ");
        Serial.println(sequence);

        if (bleKeyboard.isConnected()) {
          Serial.println("BLE Keyboard is connected. Executing sequence.");
          parseAndExecuteSequence(sequence.c_str());
          client.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nSequence executed!");
        } else {
          Serial.println("BLE Keyboard not connected.");
          client.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nBluetooth not connected.");
        }
      } else {
        Serial.println("No sequence received.");
        client.print("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nNo sequence received.");
      }
    } else {
      Serial.println("Unknown request.");
      client.print("HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nNot Found");
    }
    client.stop();
    Serial.println("Client disconnected.");
  }
}

void parseAndExecuteSequence(const char* sequence) {
  char seqCopy[512]; // Increased buffer size
  strncpy(seqCopy, sequence, sizeof(seqCopy) - 1);
  seqCopy[sizeof(seqCopy) - 1] = '\0';
  char* command = strtok(seqCopy, ";");
  while (command != NULL) {
    Serial.print("Executing command: ");
    Serial.println(command);
    executeCommand(command);
    command = strtok(NULL, ";");
  }
}

void executeCommand(const char* command) {
  char cmd[512]; // Increased buffer size
  strncpy(cmd, command, sizeof(cmd) - 1);
  cmd[sizeof(cmd) - 1] = '\0';

  char* token = strtok(cmd, " ");
  if (token == NULL) return;

  if (strcasecmp(token, "press") == 0) {
    char* keys = strtok(NULL, "");
    if (keys != NULL) {
      Serial.print("Pressing keys: ");
      Serial.println(keys);
      pressKeys(keys);
    }
  } else if (strcasecmp(token, "type") == 0) {
    char* text = strtok(NULL, "");
    if (text != NULL) {
      size_t len = strlen(text);
      if ((text[0] == '\'' && text[len - 1] == '\'') || (text[0] == '\"' && text[len - 1] == '\"')) {
        text[len - 1] = '\0';
        text++;
      }
      Serial.print("Typing text: ");
      Serial.println(text);
      typeText(text); // Use the new function
    }
  } else {
    // Treat any other input as text to type
    Serial.print("Typing text: ");
    Serial.println(command);
    typeText(command); // Use the new function
  }
}

void pressKeys(const char* keys) {
  shiftPressed = false;
  char keysCopy[256];
  strncpy(keysCopy, keys, sizeof(keysCopy) - 1);
  keysCopy[sizeof(keysCopy) - 1] = '\0';
  char* key = strtok(keysCopy, "+");
  while (key != NULL) {
    Serial.print("Pressing key: ");
    Serial.println(key);
    pressKey(key);
    key = strtok(NULL, "+");
  }
  delay(20); // Added delay between key combinations
  bleKeyboard.releaseAll();
}

void pressKey(const char* key) {
  if (strcasecmp(key, "ctrl") == 0 || strcasecmp(key, "control") == 0) {
    bleKeyboard.press(KEY_LEFT_CTRL);
  } else if (strcasecmp(key, "alt") == 0) {
    bleKeyboard.press(KEY_LEFT_ALT);
  } else if (strcasecmp(key, "shift") == 0) {
    if (!shiftPressed) {
      bleKeyboard.press(KEY_LEFT_SHIFT);
      shiftPressed = true;
    }
  } else if (strcasecmp(key, "gui") == 0 || strcasecmp(key, "win") == 0 || strcasecmp(key, "windows") == 0) {
    bleKeyboard.press(KEY_LEFT_GUI);
  } else if (strcasecmp(key, "enter") == 0 || strcasecmp(key, "return") == 0) {
    bleKeyboard.press(KEY_RETURN);
  } else if (strlen(key) == 1) {
    // Handle single-character keys
    char c = key[0];
    if (c >= 'A' && c <= 'Z') {
      if (!shiftPressed) {
        bleKeyboard.press(KEY_LEFT_SHIFT);
        shiftPressed = true;
      }
      bleKeyboard.press(c + 32); // Convert to lowercase ASCII
    } else {
      bleKeyboard.press(c);
    }
  } else {
    Serial.print("Unknown key: ");
    Serial.println(key);
  }
}

void typeText(const char* text) {
  size_t len = strlen(text);
  for (size_t i = 0; i < len; i++) {
    char c = text[i];
    // Handle uppercase letters
    if (c >= 'A' && c <= 'Z') {
      bleKeyboard.press(KEY_LEFT_SHIFT);
      bleKeyboard.press(c + 32); // Convert to lowercase ASCII
      delay(20);
      bleKeyboard.releaseAll();
    } else {
      bleKeyboard.write(c);
      delay(20);
    }
  }
}

String urlDecode(String input) {
  String decoded = "";
  char temp[] = "0x00";
  unsigned int len = input.length();
  unsigned int i = 0;
  while (i < len) {
    char c = input.charAt(i++);
    if (c == '+') {
      decoded += ' ';
    } else if (c == '%') {
      if (i + 1 < len) {
        temp[2] = input.charAt(i++);
        temp[3] = input.charAt(i++);
        decoded += (char) strtol(temp, NULL, 16);
      }
    } else {
      decoded += c;
    }
  }
  return decoded;
}
