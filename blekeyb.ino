#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <BleKeyboard.h>
#include "esp_bt.h"
#include "esp_bt_main.h"

#define DEVICE_NAME "blutuf klawa" // BLE Device Name
#define MANUFACTURER "MC"          // BLE Manufacturer

// Access Point credentials
const char* ap_ssid = "EPS";
const char* ap_password = "12345678";

// Create a WiFiServer object on port 80
WiFiServer server(80);

// BLE Keyboard initialization with shortened names
BleKeyboard bleKeyboard(DEVICE_NAME, MANUFACTURER, 69);

// Embedded and minified HTML content
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><body><h1>ESP32 Keyboard</h1><div><input type="text" id="command" placeholder="Enter command"><button id="addStep">Add Step</button></div><h2>Steps:</h2><ul id="stepsList"></ul><button id="executeSteps">Execute Steps</button><p id="status"></p><script>
var steps=[];
document.getElementById('addStep').addEventListener('click',function(){
  var cmd=document.getElementById('command').value.trim();
  if(cmd){
    steps.push(cmd);
    var li=document.createElement('li');
    li.textContent=cmd+' ';
    var removeBtn=document.createElement('button');
    removeBtn.textContent='Remove';
    removeBtn.addEventListener('click',function(){
      var index=Array.from(document.getElementById('stepsList').children).indexOf(li);
      steps.splice(index,1);
      li.remove();
    });
    li.appendChild(removeBtn);
    document.getElementById('stepsList').appendChild(li);
    document.getElementById('command').value='';
  }
});
document.getElementById('executeSteps').addEventListener('click',function(){
  if(steps.length>0){
    var sequence=steps.join(';');
    fetch('/send',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:'sequence='+encodeURIComponent(sequence)
    }).then(r=>r.text()).then(d=>{
      document.getElementById('status').innerText=d;
      steps=[];
      document.getElementById('stepsList').innerHTML='';
    });
  }else{
    alert('No steps to execute');
  }
});
</script></body></html>
)rawliteral";

// Function prototypes
void parseAndExecuteSequence(const char* sequence);
void executeCommand(const char* command);
void pressKeys(const char* keys);
void pressKey(const char* key);
void typeText(const char* text); // Function to type text with delay
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
  char seqCopy[1024]; // Increased buffer size
  strncpy(seqCopy, sequence, sizeof(seqCopy) - 1);
  seqCopy[sizeof(seqCopy) - 1] = '\0';
  char* saveptr1;
  char* command = strtok_r(seqCopy, ";", &saveptr1);
  while (command != NULL) {
    Serial.print("Executing command: ");
    Serial.println(command);
    executeCommand(command);
    command = strtok_r(NULL, ";", &saveptr1);
  }
}

void executeCommand(const char* command) {
  char cmd[1024]; // Increased buffer size
  strncpy(cmd, command, sizeof(cmd) - 1);
  cmd[sizeof(cmd) - 1] = '\0';

  char* saveptr2;
  // Get the first token (the command keyword)
  char* token = strtok_r(cmd, " ", &saveptr2);
  if (token == NULL) return;

  // Get the rest of the command
  char* restOfCommand = strtok_r(NULL, "", &saveptr2);
  if (restOfCommand != NULL) {
    // Remove leading spaces
    while (*restOfCommand == ' ') restOfCommand++;
  }

  if (strcasecmp(token, "press") == 0) {
    if (restOfCommand != NULL && *restOfCommand != '\0') {
      Serial.print("Pressing keys: ");
      Serial.println(restOfCommand);
      pressKeys(restOfCommand);
    } else {
      Serial.println("No keys specified for press command.");
    }
  } else if (strcasecmp(token, "type") == 0) {
    if (restOfCommand != NULL && *restOfCommand != '\0') {
      size_t len = strlen(restOfCommand);
      if ((restOfCommand[0] == '\'' && restOfCommand[len - 1] == '\'') || (restOfCommand[0] == '\"' && restOfCommand[len - 1] == '\"')) {
        restOfCommand++;
        len -= 2;
      }
      Serial.print("Typing text: ");
      Serial.println(restOfCommand);
      typeText(restOfCommand);
    } else {
      Serial.println("No text specified for type command.");
    }
  } else {
    // Treat any other input as text to type
    Serial.print("Typing text: ");
    Serial.println(command);
    typeText(command);
  }
}

void pressKeys(const char* keys) {
  shiftPressed = false;
  char keysCopy[256];
  strncpy(keysCopy, keys, sizeof(keysCopy) - 1);
  keysCopy[sizeof(keysCopy) - 1] = '\0';
  char* saveptr3;
  char* key = strtok_r(keysCopy, "+", &saveptr3);
  while (key != NULL) {
    Serial.print("Pressing key: ");
    Serial.println(key);
    pressKey(key);
    key = strtok_r(NULL, "+", &saveptr3);
  }
  delay(125); // Adjust delay as needed
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
  } else if (strcasecmp(key, "esc") == 0) {
    bleKeyboard.press(KEY_ESC);
  } else if (strcasecmp(key, "tab") == 0) {
    bleKeyboard.press(KEY_TAB);
  } else if (strcasecmp(key, "f4") == 0) {
    bleKeyboard.press(KEY_F4);
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
