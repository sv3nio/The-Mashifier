#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "BluefruitConfig.h"
#include "keycode.h"

// ###### DECLARATIONS ######
#define R_LED 13      // RGB LED pins
#define G_LED 5
#define B_LED 1
#define VBATPIN A9
bool FACTORYRESET_ENABLE = 0;
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

typedef struct
{
  uint8_t modifier;   // Keyboard modifier keys
  uint8_t reserved;   // Reserved for OEM use, always set to 0.
  uint8_t keycode[6]; // Key codes of the currently pressed keys. <-- IMPORTANT!
} hid_keyboard_report_t;

// Report that send to Central every scanning period
hid_keyboard_report_t keyReport = { 0, 0, { 0 } };

// Report sent previously. This is used to prevent sending the same report over time.
// Notes: HID Central intepretes no new report as no changes, which is the same as
// sending very same report multiple times. This will help to reduce traffic especially
// when most of the time there are no keys pressed. This is initialized with different
// data from keyReport.
hid_keyboard_report_t previousReport = { 0, 0, { 1 } };

// GPIO Assignments
/*
    NOTE: The order in which inputPins is constructed is the order in which the pins
    are polled. Given that we can send only 6 keys and the rest are ignored, the
    order of the array constitutues the priority of the controls. Therefore, since
    this is a gaming controller, the joystick and 4 main buttons come first.)
*/
char controls[10][12] = { "Joy Up"        , "Joy Down"      , "Joy Left"      , "Joy Right"     , "Btn 1"         , "Btn 2"         , "Btn 3"         , "Btn 4"         , "Btn Start"         , "Btn Select"      };
int inputPins[10]     = { 18              , 19              , 20              , 21              , 12              , 11              , 10              , 6               , 2                   , 3                 };
int inputKeycodes[10] = { HID_KEY_KEYPAD_8, HID_KEY_KEYPAD_2, HID_KEY_KEYPAD_4, HID_KEY_KEYPAD_6, HID_KEY_KEYPAD_7, HID_KEY_KEYPAD_9, HID_KEY_KEYPAD_1, HID_KEY_KEYPAD_3, HID_KEY_KEYPAD_ENTER, HID_KEY_KEYPAD_DECIMAL};

// BLE HID can only send up to 6 simultaneous key presses. Therefore, since the
// controller has more than 6 buttons, we need a way to enforce this limit.
// This counter will be used in the main loop to collect only the first 6 active
// controls for transmission.
int keyCount = 0;

// ###### FUNCTIONS ######
// A small helper
void error(const __FlashStringHelper*err) {
  setLED(5);
  Serial.println(err);
  while (1);
}

// Status LED
//    1 = Low Battery          = Blink RED
//    2 = Error                = Steady RED
//    3 = Ready & Disconnected = Blink GREEN
//    4 = Ready & Connected    = Steady GREEN
//    5 = Pairing mode         = Blink BLUE
//    6 = Booting              = Steady BLUE
void setLED(int cond) {
  // Blink RED for low battery
  if ( cond == 1 ) {
    digitalWrite(R_LED, HIGH);
    digitalWrite(G_LED, LOW);
    digitalWrite(B_LED, LOW);
    delay(150);
    digitalWrite(R_LED, LOW);
  }

  // Solid RED for error
  if ( cond == 2 ) {
    digitalWrite(R_LED, HIGH);
    digitalWrite(G_LED, LOW);
    digitalWrite(B_LED, LOW);
  }

  // Blink GREEN for ready, but disconnected
  if ( cond == 3 ) {
    digitalWrite(R_LED, LOW);
    digitalWrite(G_LED, HIGH);
    digitalWrite(B_LED, LOW);
    delay(250);
    digitalWrite(G_LED, LOW);
    delay(250);
  }

  // Steady GREEN for ready and connected
  if ( cond == 4 ) {
    digitalWrite(R_LED, LOW);
    digitalWrite(G_LED, HIGH);
    digitalWrite(B_LED, LOW);
  }

  // Blink BLUE for pairing mode
  if ( cond == 5 ) {
    digitalWrite(R_LED, LOW);
    digitalWrite(G_LED, LOW);
    digitalWrite(B_LED, HIGH);
    delay(250);
    digitalWrite(B_LED, LOW);
    delay(250);
  }

  // Steady BLUE for booting
  if ( cond == 6 ) {
    digitalWrite(R_LED, LOW);
    digitalWrite(G_LED, LOW);
    digitalWrite(B_LED, HIGH);
  }
}

// ###### SETUP ######
void setup(void)
{
  // Status LED pins
  pinMode(R_LED, OUTPUT);
  pinMode(G_LED, OUTPUT);
  pinMode(B_LED, OUTPUT);

  // Set boot LED
  setLED(6);

  Serial.begin(115200);
  delay(1000);
  Serial.println( F("Setting up...") );

  // Configure GPIO input Pins
  for(int i=0; i<12; i++)
  {
    pinMode(inputPins[i], INPUT_PULLUP);
  }

  // Initialize BLE module
  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("ERROR: Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("  --> BLE initialized.") );

  /*

  Pairing Mode / Reset

  This is a hack. The BLE module does have AT commands for disconnecting and
  forgetting devices, however it is simpler to intercept a keypress on bootup
  and initiate a reset to wipe the pairing history (major performance boost
  during the main loop). The reset leaves the unit ready for pairing. To
  initiate, the user must hold the SELECT and START buttons for 3 seconds while
  powering on the controller. The LED will blink BLUE to acknowledge the
  reset and ready for pairing.

  */
  if ( digitalRead(2) == LOW && digitalRead(3) == LOW ) {
    delay(3000);
    if ( digitalRead(2) == LOW && digitalRead(3) == LOW ) {
      FACTORYRESET_ENABLE = 1;
      for(int i=0; i<6; i++) {
        setLED(5);
      }
    }
  }

  // Factory Reset. Used pairing, dev or troubleshooting.
  if ( FACTORYRESET_ENABLE )
  {
    ble.factoryReset();
    ble.sendCommandCheckOK(F( "AT+GAPDEVNAME=The Mashifier"  ));
    ble.reset();
    setLED(6);  // Boot LED
    Serial.println( F("  --> Factory reset complete. Ready to pair.") );
  }

  // Disable command echo from Bluefruit
  ble.echo(false);

  // Enable HID Service (if not already running)
  int32_t hid_en = 0;
  ble.sendCommandWithIntReply( F("AT+BleHIDEn"), &hid_en);

  if ( !hid_en )
  {
    ble.sendCommandCheckOK(F( "AT+BleHIDEn=On" ));
    !ble.reset();
    Serial.println(F("  --> HID service enabled."));
  }

  Serial.println(F("  --> Controller configured."));
  Serial.println(F("  --> READY!"));
}

// ###### LOOP ######
void loop(void)
{

  // Get BATT voltage
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // Divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V (reference voltage)
  measuredvbat /= 1024; // Convert to voltage

  if ( ble.isConnected() )
  {
    // Set LED
    if (measuredvbat < 3.37) {
      setLED(1);    // Low battery
    } else {
      setLED(4);    // Ready & connected
    }

    // Once connected, we need to make sure that the LED doesn't accidentally
    // show pairing mode in the event of a lost signal after a successful
    // pairing.
    if ( FACTORYRESET_ENABLE ) {
      FACTORYRESET_ENABLE = 0;
    }

    // Reset the keyCount to 0 at the start of every new loop.
    keyCount = 0;

    // Loop through the GPIO pins...
    for(int i=0; i<10; i++)
    {
      // If the pin is LOW (button is pressed)...
      if ( digitalRead(inputPins[i]) == LOW )
      {
        // And we have <6 active controls...
        // (NOTE: this condition is not included alongside the above IF statement
        // because we need to break the FOR-loop after 6 keys have been collected
        // even if there are still more keys being pressed.)
        if (keyCount <= 5)
        {
          // Verbose output to serial
          Serial.print("keyReport: ");  Serial.print(keyCount);
          Serial.print(" Pin: ");       Serial.print(inputPins[i]);
          Serial.print(" Ctrl: ");      Serial.println(controls[i]);

          // Put the current keyCode into the next slot in the keyReport.
          keyReport.keycode[keyCount] = inputKeycodes[i];
          keyCount += 1;
        } else
        {
          // Break the FOR-loop once we have our 6 keys.
          break;
        }
      // Otherwise, if the pin is HIGH (button NOT pressed)...
      } else
      {
        // Put zeros into the rest of the keyReport. This also prepares
        // the "release" signal once a button is no longer pressed.
        keyReport.keycode[i] = 0;
      }
    }

    // Compare the new keyReport with the previousReport so that we don't
    // send the same report twice. This prevents restarting key presses that
    // should be continious and saves on airtime, battery and the things.
    if ( memcmp(&previousReport, &keyReport, 8) ) // Returns "0" if they match.
    {
      // Send the keyReport
      ble.atcommand("AT+BLEKEYBOARDCODE", (uint8_t*) &keyReport, 8);

      // Copy to previousReport
      memcpy(&previousReport, &keyReport, 8);
    }
  } else {                          // BLE not connected...
    if ( FACTORYRESET_ENABLE ) {    // ...because of pairing mode
      setLED(5);
    } else {                        // ...or some other reason
      setLED(3);
    }
  }
}
