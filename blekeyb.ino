#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <BleKeyboard.h>
#include "esp_bt.h"
#include "esp_bt_main.h"

#define DEVICE_NAME "niebieskizab"
#define MANUFACTURER "MC"

const char* ap_ssid = "EPS";
const char* ap_password = "12345678";

WiFiServer server(80);

BleKeyboard bleKeyboard(DEVICE_NAME, MANUFACTURER, 69);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><body><h1>ESP32 Keyboard</h1><div><input type="text" id="command" placeholder="Enter command"><input type="number" id="loopCount" placeholder="Loop count" min="1" value="1"><button id="addStep">Add Step</button></div><h2>Steps:</h2><ul id="stepsList"></ul><button id="executeSteps">Execute Steps</button><p id="status"></p><script>
var steps=[];
document.getElementById('addStep').addEventListener('click',function(){
  var cmd=document.getElementById('command').value.trim();
  var loopCount=document.getElementById('loopCount').value.trim();
  if(cmd && loopCount > 0){
    var step={command: cmd, loop: loopCount};
    steps.push(step);
    var li=document.createElement('li');
    li.textContent=cmd+' (x'+loopCount+') ';
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
    document.getElementById('loopCount').value='1';
  }
});
document.getElementById('executeSteps').addEventListener('click',function(){
  if(steps.length>0){
    var sequence=steps.map(step => `${step.command}:${step.loop}`).join(';');
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

void parseAndExecuteSequence(const char* sequence);
void executeCommand(const char* command);
void pressKeys(const char* keys);
void pressKey(const char* key);
void typeText(const char* text);
String urlDecode(String input);

bool shiftPressed = false;

void setup() {
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  bleKeyboard.begin();
  WiFi.softAP(ap_ssid, ap_password);
  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    client.flush();

    if (request.indexOf("GET / ") != -1) {
      client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
      client.print(index_html);
    }
    else if (request.indexOf("POST /send") != -1) {
      while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
          break;
        }
      }

      String body = client.readString();
      int seqIndex = body.indexOf("sequence=");
      if (seqIndex != -1) {
        String sequence = body.substring(seqIndex + 9);
        sequence = urlDecode(sequence);

        if (bleKeyboard.isConnected()) {
          parseAndExecuteSequence(sequence.c_str());
          client.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nSequence executed!");
        } else {
          client.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nBluetooth not connected.");
        }
      } else {
        client.print("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nNo sequence received.");
      }
    } else {
      client.print("HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nNot Found");
    }
    client.stop();
  }
}

void parseAndExecuteSequence(const char* sequence) {
  char seqCopy[1024];
  strncpy(seqCopy, sequence, sizeof(seqCopy) - 1);
  seqCopy[sizeof(seqCopy) - 1] = '\0';
  char* saveptr1;
  char* command = strtok_r(seqCopy, ";", &saveptr1);

  while (command != NULL) {
    char* loopCountStr = strchr(command, ':');
    int loopCount = 1;
    if (loopCountStr != NULL) {
      *loopCountStr = '\0';
      loopCount = atoi(loopCountStr + 1);
      if (loopCount < 1) loopCount = 1;
    }

    for (int i = 0; i < loopCount; i++) {
      executeCommand(command);
    }

    command = strtok_r(NULL, ";", &saveptr1);
  }
}

void executeCommand(const char* command) {
  char cmd[1024];
  strncpy(cmd, command, sizeof(cmd) - 1);
  cmd[sizeof(cmd) - 1] = '\0';

  char* saveptr2;
  char* token = strtok_r(cmd, " ", &saveptr2);
  if (token == NULL) return;

  char* restOfCommand = strtok_r(NULL, "", &saveptr2);
  if (restOfCommand != NULL) {
    while (*restOfCommand == ' ') restOfCommand++;
  }

  if (strcasecmp(token, "press") == 0) {
    if (restOfCommand != NULL && *restOfCommand != '\0') {
      pressKeys(restOfCommand);
    }
  } else if (strcasecmp(token, "type") == 0) {
    if (restOfCommand != NULL && *restOfCommand != '\0') {
      size_t len = strlen(restOfCommand);
      if ((restOfCommand[0] == '\'' && restOfCommand[len - 1] == '\'') || (restOfCommand[0] == '\"' && restOfCommand[len - 1] == '\"')) {
        restOfCommand++;
        len -= 2;
      }
      typeText(restOfCommand);
    }
  } else {
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
    pressKey(key);
    key = strtok_r(NULL, "+", &saveptr3);
  }
  delay(120);
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
    char c = key[0];
    if (c >= 'A' && c <= 'Z') {
      if (!shiftPressed) {
        bleKeyboard.press(KEY_LEFT_SHIFT);
        shiftPressed = true;
      }
      bleKeyboard.press(c + 32);
    } else {
      bleKeyboard.press(c);
    }
  }
}

void typeText(const char* text) {
  size_t len = strlen(text);
  for (size_t i = 0; i < len; i++) {
    char c = text[i];
    if (c >= 'A' && c <= 'Z') {
      bleKeyboard.press(KEY_LEFT_SHIFT);
      bleKeyboard.press(c + 32);
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
