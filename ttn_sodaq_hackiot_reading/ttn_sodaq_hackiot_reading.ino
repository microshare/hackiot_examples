#include "TheThingsNetwork.h"
#include "CayenneLPP.h"

// Board Definitions
#define bleSerial Serial1
#define loraSerial Serial2
#define debugSerial SerialUSB
#define SERIAL_TIMEOUT  10000
enum state {WHITE, RED, GREEN, BLUE, CYAN, ORANGE, PURPLE, OFF};    // List of colours for the RGB LED
byte BUTTON_STATE;                                                  // Tracks the previous status of the button

// LoRa Definitions and constants

// For OTAA. Set your AppEUI and AppKey. DevEUI will be serialized by using  HwEUI (in the RN module)
const char *appEui = "";
const char *appKey = "";

/*
// For ABP. Set your devAddr & Keys
const char *devAddr = "";
const char *nwkSKey = "";
const char *appSKey = "";
*/

const bool CNF   = true;
const bool UNCNF = false;
const byte MyPort = 3;
byte Payload[51];
byte CNT = 0;                                               // Counter for the main loop, to track packets while prototyping
#define freqPlan TTN_FP_EU868                               // Replace with TTN_FP_EU868 or TTN_FP_US915
#define FSB 0                                               // FSB 0 = enable all channels, 1-8 for private networks
#define SF 7                                                // Initial SF

TheThingsNetwork  ExpLoRer (loraSerial, debugSerial, freqPlan, SF, FSB);    // Create an instance from TheThingsNetwork class
CayenneLPP        CayenneRecord (51);                                       // Create an instance of the Cayenne Low Power Payload


void setup()
{ 
  pinMode(BUTTON, INPUT_PULLUP);
  BUTTON_STATE = !digitalRead(BUTTON);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  LED(RED);                             // Start with RED
  pinMode(TEMP_SENSOR, INPUT);
  analogReadResolution(10);             //Set ADC resolution to 10 bits
  
  pinMode(LORA_RESET, OUTPUT);          //Reset the LoRa module to a known state
  digitalWrite(LORA_RESET, LOW);
  delay(100);
  digitalWrite(LORA_RESET, HIGH);
  delay(1000);                          // Wait for RN2483 to reset
  LED(ORANGE);                          // Switch to ORANGE after reset
  
  loraSerial.begin(57600);
  debugSerial.begin(57600);

  // Wait a maximum of 10s for Serial Monitor
  while (!debugSerial && millis() < SERIAL_TIMEOUT);

  // Set callback for incoming messages
  ExpLoRer.onMessage(message);

  //Set up LoRa communications
  debugSerial.println("-- STATUS");
  ExpLoRer.showStatus();

  delay(1000);

  debugSerial.println("-- JOIN");

  // For OTAA. Use the 'join' function 
  if (ExpLoRer.join(appEui, appKey, 6, 5000))   // 6 Re-tries, 3000ms delay in between
    LED(GREEN);               // Switch to GREEN if OTAA join is successful
  else
    LED(RED);                 // Switch to RED if OTAA join fails

  // For ABP. Use the 'personalize' function instead of 'join' above
  //ExpLoRer.personalize(devAddr, nwkSKey, appSKey);

  delay(1000);

  //Hard-coded GPS position, just to create a map display on Cayenne
  CayenneRecord.reset();
  CayenneRecord.addGPS(1, 51.4141391, -0.9412872, 10);  // Thames Valley Science Park, Shinfield, UK
                  
  // Copy out the formatted record
  byte PayloadSize = CayenneRecord.copy(Payload);
  
  ExpLoRer.sendBytes(Payload, PayloadSize, MyPort, UNCNF);
  CayenneRecord.reset();
}                                                       // End of the setup() function


void loop()
{
  CNT++;
  debugSerial.println(CNT, DEC);
  // ExpLoRer.showStatus();                           // Un-comment during LoRa debugging to see status in Serial Monitor

  // Prepare payload of 1 byte to indicate LED status
  // payload[0] = (digitalRead(LED_BUILTIN) == HIGH) ? 1 : 0;

  // Read sensors here for regular transmissions, or triggered by pressing the button
  float MyTemp = getTemperature();                    // Onboard temperature sensor
  CayenneRecord.addTemperature(3, MyTemp);

  float MySound = getSound();                         // Grove sound sensor on pin A8
  CayenneRecord.addAnalogInput(4, MySound);
  
  // When all measurements are done and the complete Cayenne record created, send it off via LoRa
  LED(BLUE);                                          // LED on while transmitting. Green for energy-efficient LoRa
  byte PayloadSize = CayenneRecord.copy(Payload);
  byte response = ExpLoRer.sendBytes(Payload, PayloadSize, MyPort, UNCNF);

  LED(response +1);                                   // Change LED colour depending on module response to uplink success
  delay(100);
  
  CayenneRecord.reset();  // Clear the record buffer for the next loop
  LED(OFF);

  
  // Delay to limit the number of transmissions
  for (byte i=0; i<8; i++)    // 8+2second delay. Additional ~2sec delay during Tx
    {
      // LED(i % 8);          // Light show, to attract attention at tradeshows!
      delay(500); 
      LED(OFF);
      delay(500);
      
      // add code here to check for realtime triggers (button or Grove sensors). Drop out of delay loop by setting i high 
      if (digitalRead(BUTTON) != BUTTON_STATE)   // Check for a change of the button state   
        { 
          BUTTON_STATE = digitalRead(BUTTON);
          LED(GREEN);   // Switch to GREEN 
          // debugSerial.print("Button state = ");
          // debugSerial.println(BUTTON_STATE);
          CayenneRecord.addDigitalOutput(2, !BUTTON_STATE);
          i=99999;   // Bomb out of the delay loop and transmit
        }
    }
}                                                       // End of loop() function


// Helper functions below here                                                

float getTemperature()
{
  //10mV per C, 0C is 500mV
  float mVolts = (float)analogRead(TEMP_SENSOR) * 3300.0 / 1023.0;
  float temp = (mVolts - 500.0) / 10.0;    // Gives value to 0.1degC  
  return temp;
}

float getSound()
{
    long sum = 0;
    for(int i=0; i<32; i++)
    {
        sum += analogRead(A8);
    }

    sum >>= 5;

    debugSerial.println(sum);
    delay(10);
    return sum;
}

void LED(byte state)
{
  switch (state)
  {
  case WHITE:
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW); 
    break;
  case RED:
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH); 
    break;
  case ORANGE:
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, HIGH); 
    break;
  case CYAN:
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW); 
    break;
  case PURPLE:
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, LOW);
    break;
  case BLUE:
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, LOW);
    break;
  case GREEN:
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, HIGH);
    break;
  default:
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    break;
  }
}

// Callback method for receiving downlink messages. Uses ExpLoRer.onMessage(message) in setup()
void message(const uint8_t *payload, size_t size, port_t port)
{
  debugSerial.println("-- MESSAGE");
  debugSerial.print("Received " + String(size) + " bytes on port " + String(port) + ":");

  for (int i = 0; i < size; i++)
  {
    debugSerial.print(" " + String(payload[i]));
  }

  debugSerial.println();
}


