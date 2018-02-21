#include "config.h"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NewRemoteReceiver.h>
#include <Ticker.h>

WiFiClient wClient;
PubSubClient mqttClient(wClient);
Ticker tick;
String data = "";
bool published = true;
unsigned long lastMsgTime;
int lightState = 0;
int newStatus = 0;
int switchesToDo = 0;

// 00 -> 10 -> 11 -> 01 -> 00 -> 10 -> 11 -> 01
/*
 * Ist    00  01  10  11
 * soll
 * 00     0   1   3   2
 * 01     3   0   2   1
 * 10     1   2   0   3
 * 11     2   3   1   0
 * 
 */
int switchNumbers [4][4] = {
  {0,1,3,2},
  {3,0,2,1},
  {1,2,0,3},
  {2,3,1,0}
};

void setup() {
  Serial.begin(115200);

  pinMode(relais_pin, OUTPUT);

  setup_wifi();
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);

  tick.attach_ms(relais_toggle_time, checkSwitches);
  
}

void loop() {
 if (!mqttClient.loop()) {
  reconnect();
 }

 if(published == false) {
   mqttClient.publish(sendTopic,(char *)data.c_str());
   published = true;
 }


 if(newStatus != lightState) {
  // go from status to newStatus
    int switches = switchNumbers[lightState][newStatus];
    Serial.print("Doing x switches: ");
    Serial.println(switches);
    switchesToDo += switches;

    lightState = newStatus;
 }


 
}

void checkSwitches() {
  Serial.println("Checking...");
  if(switchesToDo > 0) {
    switchesToDo--;
    digitalWrite(relais_pin, HIGH);
    tick.once_ms(relais_toggle_time, realisPinOff);
  }
}

void realisPinOff() {
  digitalWrite(relais_pin, LOW);
  tick.attach_ms(relais_toggle_time, checkSwitches);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String top = String(topic);
  byte* p = (byte*)malloc(length + 1);
  memcpy(p,payload,length);
  p[length] = '\0';
  String msg = String((char*) p);

  Serial.print("Topic: ");
  Serial.println(top);
  Serial.print("Message: ");
  Serial.println(msg);

  String subtopic = top.substring(top.lastIndexOf("/")+1);

  if(subtopic == "relais") {
    
    
    if(msg == "toggle") {

      switchesToDo++;
    } else if(msg == "1 on") {
      newStatus = lightState;
      bitSet(newStatus, 0);
    } else if(msg == "1 off") {
      newStatus = lightState;
      bitClear(newStatus, 0);
    } else if(msg == "2 on") {
      newStatus = lightState;
      bitSet(newStatus, 1);
    } else if(msg == "2 off") {
      newStatus = lightState;
      bitClear(newStatus, 1);
    }
  }
  Serial.println(newStatus);
//  lightState = newStatus;
}



void setup_wifi() {
  delay(10);
  WiFi.mode(WIFI_STA);
  
  WiFi.begin(wifi_ssid, wifi_password);
    
  Serial.println(WiFi.macAddress()); 
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println(WiFi.localIP());
  
}

boolean reconnect() {
  Serial.println("Reconnecting");
  // Loop until we're reconnected
  while (!mqttClient.connected()) {

      if (!mqttClient.connect(device_name, mqtt_user, mqtt_password, statusTopic, 0, true, "down")) {
        Serial.println("Connection to mqtt failed");
        // Wait 5 seconds before retrying
        delay(5000);
    }
  }
  Serial.println("Connected");
  // send up to where we also send last will
  mqttClient.publish(statusTopic, "up", true);
  mqttClient.subscribe(reciveTopic);
  
  return mqttClient.connected();
}
