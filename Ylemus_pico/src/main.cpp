#include <Arduino.h>
#include <RF24.h>
// #include <DFRobotDFPlayerMini.h>
// #include <SoftwareSerial.h>

//   2 -> White LED to GND  (Status)

//   3 -> RED LED to GND    (Button Status)
//   4 -> YELLOW LED to GND (Button Status)
//   5 -> GREEN LED to GND  (Button Status)
//   6 -> BLUE LED to GND   (Button Status)

//   7 -> RESET btn to GND
//   8 -> READY btn to GND

// Aarduino nano
//   9 -> CE  (nRF24)
//  10 -> CSN (nRF24)
//  11 -> MO  (nRF24)
//  12 -> MI  (nRF24)
//  13 -> SCK (nRF24)

//  A0 -> 1K Reststor -> RX on DFPlayerMini
//  A1 -> TX on DFPlayerMini

#define NUM_OF_NUPPs 4
#define NUPP_1_LED 3
#define NUPP_2_LED 4
#define NUPP_3_LED 5
#define NUPP_4_LED 6
//#define NUPP_5_LED 3

#define PIN_STATUS_LED 2
#define PIN_RESET_BTN 7
#define PIN_READY_BTN 8

// Pi Pico
// CSN  -> GPIO16
// CE -  > GPIO17
// SCK  -> GPIO18
// MOSI -> GPIO19
// MISO -> GPIO20
// IRQ  -> x

#define PIN_CSN_nRF24 16
#define PIN_CE_nRF24 17

RF24 radio(PIN_CE_nRF24, PIN_CSN_nRF24);



/*
#define DFMINI_TX A0 // connect to pin 2 on the DFPlayer via a 1K resistor
#define DFMINI_RX A1 // connect to pin 3 on the DFPlayer
SoftwareSerial softwareSerial(DFMINI_RX, DFMINI_TX);

// Player
// Tip: If you have any problems with the DFPlayerMini, power it from the Arduino's 3.3v pin rather than 5v.
DFRobotDFPlayerMini player;
*/

// NUPP LED pins
uint8_t BTN_LEDS[NUM_OF_NUPPs] = {
  NUPP_1_LED,
  NUPP_2_LED,
  NUPP_3_LED,
  NUPP_4_LED
};


enum LED_Status : uint8_t
{
  LED_off = 0,
  LED_on = 1,
  LED_flashing = 2
};

// Status we want to share with the buttons
LED_Status led_status[4] = {LED_off, LED_off, LED_off, LED_off};
bool btn_enabled[4] = {false, false, false, false};
bool btn_connected[4] = {false, false, false, false};
bool has_answered[4] = {false, false, false, false};
uint32_t last_contact_time[4] = {0, 0, 0, 0};

// Last loop time
uint32_t now = 0;

// System status
bool is_ready = false;

// Is audio playing?
bool is_playing = false;
bool dfPlayerReady = false;

/*************************************************************/

bool find_empty_channel();
void setup_ACK_payload();
void check_radio_message_received();

/*************************************************************/


void setup()
{

  Serial.begin(57600);
  /*while (!Serial)
  {
  };*/

  // small delay to allow the DFPlayerMini to boot
  delay(1000);

  // For the DFPlayerMini
  /*softwareSerial.begin(9600);
  if (player.begin(softwareSerial))
  {
    player.volume(30);
    dfPlayerReady = true;
  }*/

  // Setup the radio device
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.enableDynamicPayloads();
  radio.enableAckPayload();
  radio.setDataRate(RF24_250KBPS);
  radio.setRetries(4, 8);
  radio.maskIRQ(false, false, false); // not using the IRQs

  // Setup our I/O
  pinMode(PIN_STATUS_LED, OUTPUT);
  pinMode(PIN_RESET_BTN, INPUT_PULLUP);
  pinMode(PIN_READY_BTN, INPUT_PULLUP);

  if (!radio.isChipConnected())
  {
    Serial.write("RF24 device not detected.\n");
  }
  else
  {
    Serial.write("RF24 detected.\n");

    // Trun off the LED
    digitalWrite(PIN_STATUS_LED, LOW);

    // Now setup the pipes for the four buttons
    char pipe[6] = "0QBTN";
    radio.openWritingPipe((uint8_t *)pipe);
    pipe[0] = '1';
    radio.openReadingPipe(1, (uint8_t *)pipe);
    for (uint8_t channel = 0; channel < 4; channel++)
    {
      pinMode(BTN_LEDS[channel], OUTPUT);
      digitalWrite(BTN_LEDS[channel], LOW);
    }

    // Start listening for messages
    radio.startListening();

    // Find an empty channel to run on
    while (!(find_empty_channel()))
    {
    };

    // Start listening for messages
    radio.startListening();

    // Ready
    digitalWrite(PIN_STATUS_LED, LOW);

    setup_ACK_payload();
  }
}


void loop()
{
  now = millis();

  if (digitalRead(PIN_RESET_BTN) == LOW)
  { // Reset button pressed?
    // Turn all buttons off
    for (unsigned char button = 0; button < 4; button++)
    {
      led_status[button] = LED_off;
      btn_enabled[button] = false;
      has_answered[button] = false;
      if (is_playing)
      {
        //player.stop();
        is_playing = false;
      }
    }
    is_ready = false;
    digitalWrite(PIN_STATUS_LED, LOW);
  }
  else if (digitalRead(PIN_READY_BTN) == LOW)
  { // Ready button pressed
    // Make the buttons flash that havent answered yet
    for (unsigned char button = 0; button < 4; button++)
    {
      btn_enabled[button] = !has_answered[button];
      led_status[button] = has_answered[button] ? LED_off : LED_flashing;
    }
    is_ready = true;
    if (is_playing)
    {
      //player.stop();
      is_playing = false;
    }
    digitalWrite(PIN_STATUS_LED, HIGH);
  }

  // Update our LEDs and monitor for ones that are out of contact
  for (unsigned char button = 0; button < 4; button++)
  {
    // If the button is connected
    if (btn_connected[button])
    {
      // If its been 1 second since we heard from it
      if (now - last_contact_time[button] > 1000)
      {
        // Disconnect it
        btn_connected[button] = false;
        digitalWrite(BTN_LEDS[button], LOW);
      }
      else
      {
        // Set the LED to match the state we have it in
        digitalWrite(BTN_LEDS[button], (led_status[button] == LED_on) || ((led_status[button] == LED_flashing) && (now & 255) > 128));
      }
    }
    else
    {
      // For disconnected ones we just give a short 'blip' once per few second
      digitalWrite(BTN_LEDS[button], (now & 2047) > 2000);
    }
  }

  // Check for messages on the 'network'
  check_radio_message_received();
}


/***************************************************/
// searches the radio spectrum for a quiet channel
bool find_empty_channel()
{
  Serial.write("Scanning for empty channel...\n");
  char buffer[10];

  // Scan all channels looking for a quiet one.  We skip every 10
  for (int channel = 125; channel > 0; channel -= 10)
  {
    radio.setChannel(channel);
    delay(20);

    unsigned int inUse = 0;
    unsigned long testStart = millis();
    // Check for 400 ms per channel
    while (millis() - testStart < 400)
    {
      digitalWrite(PIN_STATUS_LED, millis() % 500 > 400);
      if ((radio.testCarrier()) || (radio.testRPD()))
        inUse++;
      delay(1);
    }

    // Low usage?
    if (inUse < 10)
    {
      itoa(channel, buffer, 10);
      Serial.write("Channel ");
      Serial.write(buffer);
      Serial.write(" selected\n");
      return true;
    }
  }
  return false;
}

/***********************************************************/

// Sends a new ACK payload to the transmitter
void setup_ACK_payload()
{
  // Update the ACK for the next payload
  unsigned char payload[4];
  for (unsigned char button = 0; button < 4; button++)
    payload[button] = (btn_enabled[button] ? 128 : 0) | led_status[button];
  radio.writeAckPayload(1, &payload, 4);
}

/********************************************************** */

// Check for messages from the buttons
void check_radio_message_received()
{
  // Check if data is available
  if (radio.available())
  {
    unsigned char buffer;
    radio.read(&buffer, 1);

    // Grab the button number from the data
    unsigned char buttonNumber = buffer & 0x7F; // Get the button number
    if ((buttonNumber >= 1) && (buttonNumber <= 4))
    {
      buttonNumber--;

      // Update the last contact time for this button
      last_contact_time[buttonNumber] = now;

      // And that it's connected
      btn_connected[buttonNumber] = true;

      // If the button was pressed, was enabled, hasn't answered and the system is ready for button presses
      if ((buffer & 128) && (btn_enabled[buttonNumber]) && (!has_answered[buttonNumber]) && (is_ready))
      {
        // No longer ready
        is_ready = false;

        if (dfPlayerReady)
        {
          //player.play(buttonNumber + 1);
          is_playing = true;
        }

        // Signal the button was pressed
        has_answered[buttonNumber] = true;

        // Change button status
        for (unsigned char btn = 0; btn < 4; btn++)
          led_status[btn] = (btn == buttonNumber) ? LED_on : LED_off;

        // Turn off the ready light
        digitalWrite(PIN_STATUS_LED, LOW);
      }
    }

    setup_ACK_payload();
  }
}

