/*
  Dual Flowmeter Pulse Generator and Counter
  ------------------------------------------
  Description:
    This Wokwi code simulates the behavior of two flowmeters by generating
    square wave pulses on digital pins 9 and 10. The frequency of the pulses is
    controlled using two potentiometers connected to analog pins A1 and A2.

    Additionally we have 4 switches to simulate the control of the following: 
  Pump
  Horizontal Float Switch for Reservoir water level warning
  Horizontal Float Switch for Reservoir water level critical
  Verical Float Switch for Pipe Oveflow Warning 

    The pulses are looped back into digital pins 2 and 3 (acting as input counters),
    where they are counted using hardware interrupts. The code calculates and prints:
      - The number of pulses generated per minute for each flowmeter
      - The estimated flow rate in litres per minute
      - The total cumulative pulses and total litres measured since startup

    Each pulse is assumed to represent a fixed volume of water, defined by
    `litrseperpulse`.

  Hardware Connections: (D = Digital, A = Analog)
    - D9: LED Pump Status Output
    - D10: Flowmeter 1 simulated output
    - D11: Flowmeter 2 simulated output
    - D12: LED Reservoir Warning output
    - D13: LED Reservoir Cutoff output
    - D14: LED Reservoir Pipe Overflow output
    - A1: Potentiometer 1 (controls Flowmeter 1 frequency)
    - A2: Potentiometer 2 (controls Flowmeter 2 frequency)
    - D3: Flowmeter 1 pulse input (interrupt pin)
    - D4: Flowmeter 2 pulse input (interrupt pin)
    - D5: Pump Switch input
    - D6: H Float Switch Reservoir Warning input
    - D7: H Float Switch Reservoir Critical input
    - D8: V Float Switch Pipe Overflow input

  Author: Hiten
  Copyright (c) 2025
  All rights reserved.

  License:
    This code is free to use and modify for non-commercial and educational
    purposes, provided proper attribution is given. No warranty is provided.

*/

#include <WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#define WLAN_SSID       "Wokwi-GUEST"
#define WLAN_PASS       ""

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883 // Use 8883 for SSL

#define AIO_USERNAME    "<Enter your username>"
#define AIO_KEY         "<Enter your AIO Key>"

// WiFi and MQTT clients
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Publishers for feeds
Adafruit_MQTT_Publish pumpInFlowFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/pump-in-flow-rate");
Adafruit_MQTT_Publish pumpReturnFlowFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/pump-return-flow-rate");
Adafruit_MQTT_Publish reservoirWarn = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/reservoir-warning-alert");
Adafruit_MQTT_Publish reservoirCritical = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/reservoir-critical-alert");
Adafruit_MQTT_Publish pipeOverflowWarning = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/pipe-overflow-warning");
Adafruit_MQTT_Publish pumpOverride = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/pump-override");

// Subscribers for the feed
Adafruit_MQTT_Subscribe pumpOverrideSub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/pump-override");

const int ledPump = 9;    // LED Pump Status (On or Off) output
const int flowPin1 = 10;     // Flowmeter 1 output
const int flowPin2 = 11;    // Flowmeter 2 output
const int ledRW = 12;   // LED Pipe overflow warning output
const int ledRC = 13;   // LED Reservoir cuttoff output
const int ledPOW = 14;    // LED Reservior warning output
const int potPin1 = 1;     // Potentiometer 1 Analog Input control
const int potPin2 = 2;     // Potentiometer 2 Analog Input control
const int flowMeterPulsePin1 = 3;   // Flowmeter 1 pulse input (receiver)
const int flowMeterPulsePin2 = 4;   // Flowmeter 2 pulse input (receiver)
const int pumpOverrideSwitch = 5;   // Pump Override switch input
const int hFloatSwitchWarn = 6;   // Horizontal Float Switch Reservoir Warning input
const int hFloatSwitchCritical = 7; // Horizontal Float Switch Reservoir Critical input
const int vFloatSwitchPipe = 8;   // Vertical Float Switch Pipe Overflow Warning input
const unsigned long pulseWidth = 1000; // HIGH duration in microseconds

// Timing control
unsigned long nextToggle1 = 0;
unsigned long nextToggle2 = 0;
bool state1 = false;
bool state2 = false;

// Counting logic
 unsigned long count1 = 0;
volatile unsigned long count2 = 0;
unsigned long totalPulse1 = 0;
unsigned long totalPulse2 = 0;

// Flowmeter volume 
float litresperpulse = 7.5;

// Last stored State of Button 
int statePumpOverride = 1;  //Pump override switch
int stateHFSWarn = 0;       //HFS Water Level warning
int stateHFSCritical = 0;     //HFS Water Level critical
int stateVFSPipe = 0;       //VFS Pipe overflow warning

// Last read State of ButtonPressed
int lastPumpState = 0;
int lastHFSWarnState = 1;
int lastHFSCriticalState = 1;
int lastVFSPipeState = 1;

// Record the last debounce for each button
unsigned long lastPumpDebounce = 0;
unsigned long lastWarnDebounce = 0;
unsigned long lastCriticalDebounce = 0;
unsigned long lastPipeDebounce = 0;
const unsigned long debounceDelay = 100;


// Monitoring Triggers
unsigned long countUpdate = 0;
unsigned long lastUpdateCount = 0;
bool triggered = false;
bool trgPump = false;
bool trgHFSWarn = false;
bool trgHFSCritical = false;
bool trgVFSPipe = false;
volatile bool pumpBtnPressed = false;
volatile bool warnBtnPressed = false;
volatile bool criticalBtnPressed = false;
volatile bool pipeBtnPressed = false;

// Update feeds
bool firstUpdate = true;

void countPulse1() 
{
  count1++;
}

void countPulse2() 
{
  count2++;
}

void PumpBtn_Pressed()
{
  pumpBtnPressed = true;
}

void WarnBtn_Pressed()
{
  warnBtnPressed = true;
}

void CriticalBtn_Pressed()
{
  criticalBtnPressed = true;
}

void PipeBtn_Pressed()
{
  pipeBtnPressed = true;
}

void Action_Btn_Pressed()
{
  if (pumpBtnPressed)
  {
    ActionPumpOverride();
    pumpBtnPressed = false;
  }

  if (warnBtnPressed)
  {
    ActionHFSWarn();
    warnBtnPressed = false;
  }

  if (criticalBtnPressed)
  {
    ActionHFSCritical();
    criticalBtnPressed = false;
  }

  if (pipeBtnPressed)
  {
    ActionVFSPipe();
    pipeBtnPressed = false;
  }
}

void ActionPumpOverride() 
{
  if ((millis() - lastPumpDebounce) > debounceDelay) 
  {
    if (lastPumpState != statePumpOverride) 
    {
      lastPumpState = statePumpOverride;
      statePumpOverride = !statePumpOverride;
      trgPump = true;
      triggered = true;
      digitalWrite(ledPump, statePumpOverride ? HIGH : LOW);
    }
  }
  lastPumpDebounce = millis();
}

void ActionHFSWarn() {
  if ((millis() - lastWarnDebounce) > debounceDelay) 
  {
    if (lastHFSWarnState != stateHFSWarn) 
    {
      lastHFSWarnState = stateHFSWarn;
      stateHFSWarn = !stateHFSWarn;
      trgHFSWarn = true;
      triggered = true;
      digitalWrite(ledRW, stateHFSWarn ? HIGH : LOW);
      //Serial.println("Trigged Warn - " + String(stateHFSWarn));
    }
  }
  lastWarnDebounce = millis();
}

void ActionHFSCritical() {
  if ((millis() - lastCriticalDebounce) > debounceDelay)
  {
    if (lastHFSCriticalState != stateHFSCritical)
    {
      lastHFSCriticalState = stateHFSCritical;
      stateHFSCritical = !stateHFSCritical;
      trgHFSCritical = true;
      triggered = true;
      digitalWrite(ledRC, stateHFSCritical ? HIGH : LOW);
      //Serial.println("Triggered Critical - " + String(stateHFSCritical));
    }
  }
  lastCriticalDebounce = millis();
}

void ActionVFSPipe() {
  if ((millis() - lastPipeDebounce) > debounceDelay)
  {
    if (lastVFSPipeState != stateVFSPipe)
    {
      lastVFSPipeState = stateVFSPipe;
      stateVFSPipe = !stateVFSPipe;
      trgVFSPipe = true;
      triggered = true;
      digitalWrite(ledPOW, stateVFSPipe ? HIGH : LOW);
      //Serial.println("Trigger Pipe - " + String(stateVFSPipe));
    }
  }
  lastPipeDebounce = millis();
}

void ActionPumpControl() {
  // Control Pump Function - Stop pump on the following conditions
  // 1. If the overflow pipe warning has been triggered
  // 2. If the Reservoir tank water is at critical level
  
  //Serial.println("Inside actionPumpControl");
  if (stateVFSPipe == HIGH || stateHFSCritical == HIGH)
  { // checking for condition 1 or 2
    lastPumpState = 1; //Reset pump last state to HIGH as pump is switched off
    statePumpOverride = 0; //Reset pump state to LOW as pump is switched off    
    digitalWrite(ledPump, statePumpOverride ? HIGH : LOW); //set ledPump OFF
    //Serial.println("statePumpOverride = " + String(statePumpOverride ? HIGH : LOW) + ": lastPumpState = " + String(lastPumpState ? HIGH : LOW));
  }
  
}

void Update_Feeds()
{
  if (firstUpdate)
  {
    Serial.println("First update!");
  }
  UpdateFlowMeters();
  UpdateSensors();
  firstUpdate = false;
}

void UpdateFlowMeters()
{
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 30000 || firstUpdate) 
  {
    noInterrupts();
    unsigned long c1 = count1;
    unsigned long c2 = count2;
    unsigned long c1lpm = c1 * litresperpulse;
    unsigned long c2lpm = c2 * litresperpulse; 
    totalPulse1 = totalPulse1 + c1;
    totalPulse2 = totalPulse2 + c2;
    count1 = 0;
    count2 = 0;
    interrupts();

    countUpdate++;
    if (!pumpInFlowFeed.publish(c1lpm)) {
      Serial.println("Failed to publish Inflow feed");
    } else {
      //Serial.println("Inflow feed published");
    }
    
    countUpdate++;
    if(!pumpReturnFlowFeed.publish(c2lpm)) {
      Serial.println("Failed to publish Return flow feed");
    } else {
      //Serial.println("Return flow feed published");
    }

    Serial.print(".");
    lastPrint = millis();
  }
}

void UpdateSensors()
{
  noInterrupts();
  if (firstUpdate == true || triggered == true) 
  {
    triggered = false;
    countUpdate++;
    //Serial.println("firstUpdate or Triggered");
    if (!pumpOverride.publish((int32_t)(statePumpOverride ? 1 : 0))) {
        Serial.println("Failed to publish pump status");
      }
    countUpdate++;
    if (!reservoirWarn.publish((int32_t)(stateHFSWarn ? 1 : 0))) {
        Serial.println("Failed to publish HFSWarn");
      }
    countUpdate++;
    //Serial.println("Critical LED " + stateHFSCritical );
    if (!reservoirCritical.publish((int32_t)(stateHFSCritical ? 1 : 0))) {
        Serial.println("Failed to pubish HFSCritical");
      }
    countUpdate++;
    if (!pipeOverflowWarning.publish((int32_t)(stateVFSPipe ? 1 : 0))) {
        Serial.println("Failed to publish VFSPipe Overflow");
      }
    
  }
  interrupts();

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 5000 && triggered == false) 
  {
    
    //Serial.println("Updating the feed data");


    if (trgHFSWarn) {
      countUpdate++;
      if (!reservoirWarn.publish((int32_t)(stateHFSWarn ? 1 : 0))) {
        Serial.println("Failed to publish HFSWarn");
      } else {
        //Serial.println("HFSWarn published");
      }
      trgHFSWarn = false;
    }

    if (trgHFSCritical) {
      countUpdate++;
      if (!reservoirCritical.publish((int32_t)(stateHFSCritical ? 1 : 0))) {
        Serial.println("Failed to pubish HFSCritical");
      } else {
        //Serial.println("HFSCritical published");
      }

      if (stateHFSCritical == 1) 
      {
        trgPump = true;
      }
      trgHFSCritical = false;
    }

    if (trgVFSPipe) {
      countUpdate++;
      if (!pipeOverflowWarning.publish((int32_t)(stateVFSPipe ? 1 : 0))) {
        Serial.println("Failed to publish VFSPipe Overflow");
      } else {
        //Serial.println("VFSPipe overflow published");
      }
      if (stateVFSPipe == 1) 
      {
        trgPump = true;
      }
      trgVFSPipe = false;
    }

    if (trgPump) {
      countUpdate++;
      if (!pumpOverride.publish((int32_t)(statePumpOverride ? 1 : 0))) {
        Serial.println("Failed to publish pump status");
      } else {
        //Serial.println("Pump status published");
      }
      trgPump = false;
    }

    Serial.print(":");
    lastUpdate = millis();
  }  
}

void Flowmeter_Trigger()
{
    unsigned long now = micros();

  // Flowmeter 1
  if (now >= nextToggle1) {
    state1 = !state1;
    digitalWrite(flowPin1, state1 ? HIGH : LOW);

    if (state1) {
      nextToggle1 = now + pulseWidth;
    } else {
      int potVal1 = analogRead(potPin1);
      float freq1 = map(potVal1, 0, 8192, 1, 50);
      unsigned long period1 = 10000000UL / freq1;
      nextToggle1 = now + (period1 - pulseWidth);
      //Serial.println(potVal1);
    }
  }

  // Flowmeter 2
  if (now >= nextToggle2) {
    state2 = !state2;
    digitalWrite(flowPin2, state2 ? HIGH : LOW);

    if (state2) {
      nextToggle2 = now + pulseWidth;
    } else {
      int potVal2 = analogRead(potPin2);
      float freq2 = map(potVal2, 0, 8192, 1, 50);
      unsigned long period2 = 10000000UL / freq2;
      nextToggle2 = now + (period2 - pulseWidth);
      //Serial.print(" - "); Serial.print(freq2);
    }
  }

}

void MQTT_connect() {

  //Serial.println("Inside MQTT_Connect function");

  mqtt.subscribe(&pumpOverrideSub);

  int8_t ret;
  if (mqtt.connected()) {
    return;
  }
  Serial.print("Connecting to MQTT... ");
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);
  }
  Serial.println("MQTT Connected!");
  
}

void readbackMQTT() {
  //Serial.println("Inside readbackMQTT");
  //(uintptr_t)&pumpOverrideSub) = 1073515108

  if (!mqtt.connected())
  {
    //Serial.println("MQTT disconnected ... Attempting to reconnect");
    MQTT_connect(); //Ensuring connection is active
  }

  mqtt.processPackets(10); // << REQUIRED to process incoming messages

  Adafruit_MQTT_Subscribe *subscription;
  subscription = mqtt.readSubscription(500);
  //Serial.print("subscription = ");
  //Serial.println((uintptr_t)subscription);
  //Serial.print("&pumpOverrideSub = ");
  //Serial.println((uintptr_t)&pumpOverrideSub);

  if (subscription) {
    Serial.println("Received something via MQTT.");

    if (subscription == &pumpOverrideSub) {
      Serial.println("pumpOverrideSub triggered");

      const char* payload = (char *)pumpOverrideSub.lastread;
      Serial.print("Payload: ");
      Serial.println(payload);

      int val = atoi(payload);

      if (statePumpOverride != val) {
        statePumpOverride = val;
        lastPumpState = !val;
        digitalWrite(ledPump, val ? HIGH : LOW);
        trgPump = true;
        triggered = true;
      }
    } else {
      Serial.println("Unknown subscription received.");
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Hello, ESP32-S2!");

  //Attempting to connect to WiFi
  delay(500);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  delay(500);
  
  while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.print(".");
  }
  Serial.println("WiFi connected");

  Serial.println("Calling MQTT_Connect");
  MQTT_connect();
  
  pinMode(ledPump, OUTPUT);
  pinMode(flowPin1, OUTPUT);
  pinMode(flowPin2, OUTPUT);
  pinMode(ledPOW, OUTPUT);
  pinMode(ledRC, OUTPUT);
  pinMode(ledRW, OUTPUT);
  pinMode(flowMeterPulsePin1, INPUT);
  pinMode(flowMeterPulsePin2, INPUT);
  pinMode(pumpOverrideSwitch, INPUT);
  pinMode(hFloatSwitchWarn, INPUT);
  pinMode(hFloatSwitchCritical, INPUT);
  pinMode(vFloatSwitchPipe, INPUT);

  digitalWrite(flowPin1, LOW);
  digitalWrite(flowPin2, LOW);
  digitalWrite(ledPump, HIGH);
  digitalWrite(ledRW, LOW);
  digitalWrite(ledRC, LOW);
  digitalWrite(ledPOW, LOW);

  //Interrupt to count the pulse trigger by the flowmeters
  attachInterrupt(digitalPinToInterrupt(flowMeterPulsePin1), countPulse1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(flowMeterPulsePin2), countPulse2, CHANGE);

  //Interrupts set to action the button press to simulate the pump override switch and float switches
  attachInterrupt(digitalPinToInterrupt(pumpOverrideSwitch), PumpBtn_Pressed, RISING);
  attachInterrupt(digitalPinToInterrupt(hFloatSwitchWarn), WarnBtn_Pressed, RISING);
  attachInterrupt(digitalPinToInterrupt(hFloatSwitchCritical), CriticalBtn_Pressed, RISING);
  attachInterrupt(digitalPinToInterrupt(vFloatSwitchPipe), PipeBtn_Pressed, RISING);

  //Interrupts set to stop the pump on either the critical switch or pipe overflow is triggered
  attachInterrupt(digitalPinToInterrupt(ledRC), ActionPumpControl, RISING);
  attachInterrupt(digitalPinToInterrupt(ledPOW), ActionPumpControl, RISING);
  //attachInterrupt(digitalPinToInterrupt(ledPump), ActionPumpControl, RISING);
}

void loop() {
  // Read back Pump status from Adafruit Subscriber
  readbackMQTT();
  // Flowmeter trigger simulator based on the potentiometer value
  Flowmeter_Trigger();

  Action_Btn_Pressed();

  Update_Feeds();

  if (lastUpdateCount < countUpdate)
  {
    //Serial.println("Update Count = " + String(countUpdate));
    lastUpdateCount = countUpdate;
  }
}
