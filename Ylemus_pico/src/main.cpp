/*********************************************
 * Ülemus
 * main.cpp
 * Tauno Erik
 * 20.12.2024
 *
 * Raspberry Pi Pico
 * nRF24L01 Radio
 *********************************************/
#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>

#define DEBUG 1
#if DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

#define NUM_OF_PLAYERS 5 // Mängijaid
// LEDS
#define NUPP_0_LED 10 // GPIO10
#define NUPP_1_LED 11
#define NUPP_2_LED 12
#define NUPP_3_LED 13
#define NUPP_4_LED 14
#define PIN_STATUS_LED 15
// Buttons
#define PIN_RESET_BTN 8
#define PIN_READY_BTN 9
#define PRESSED 0

// Radio - SPI
// https://www.youtube.com/watch?v=y5tFUnR5hM0
// For Arduino-pico core
#define PIN_MISO 16
#define PIN_MOSI 19
#define PIN_CS 17
#define PIN_SCK 18
#define PIN_CE 22

const uint8_t WRITE_PIPE[6] = "0QBTN";
const uint8_t READ_PIPE[6] = "1QBTN";

RF24 radio(PIN_CE, PIN_CS);

// NUPP LED pins
uint8_t BTN_LEDS[NUM_OF_PLAYERS] = {
    NUPP_0_LED,
    NUPP_1_LED,
    NUPP_2_LED,
    NUPP_3_LED,
    NUPP_4_LED};

enum LED_Status : uint8_t
{
  LED_off = 0,
  LED_on = 1,
  LED_flashing = 2
};

LED_Status led_status[NUM_OF_PLAYERS] = {LED_off, LED_off, LED_off, LED_off, LED_off};
bool btn_enabled[NUM_OF_PLAYERS] = {false, false, false, false, false};
bool btn_connected[NUM_OF_PLAYERS] = {false, false, false, false, false};
bool has_answered[NUM_OF_PLAYERS] = {false, false, false, false, false};
uint32_t last_contact_time[NUM_OF_PLAYERS] = {0, 0, 0, 0, 0};

// Last loop time
uint32_t now = 0;

// System status
bool is_ready = false;

/*************************************************************
 * Function Prototypes
 *************************************************************/
void setup_radio();
void start_radio();
bool find_empty_channel();
void setup_ACK_payload();
void check_radio_message_received();

/*************************************************************/

void setup()
{
  Serial.begin(57600);
  // Pins
  pinMode(PIN_STATUS_LED, OUTPUT);
  pinMode(PIN_RESET_BTN, INPUT_PULLUP);
  pinMode(PIN_READY_BTN, INPUT_PULLUP);

  // while (!Serial)
  //{
  // };

  setup_radio();
  start_radio();
}


void loop()
{
  now = millis();

  // Reset Button
  if (digitalRead(PIN_RESET_BTN) == PRESSED)
  {
    Serial.print("Reset btn\n");

    for (uint8_t btn = 0; btn < NUM_OF_PLAYERS; btn++)
    {
      led_status[btn] = LED_off;
      btn_enabled[btn] = false;
      has_answered[btn] = false;
    }
    is_ready = false;
    digitalWrite(PIN_STATUS_LED, LOW);
  }
  // Ready Button
  else if (digitalRead(PIN_READY_BTN) == PRESSED)
  {
    Serial.print("Ready btn\n");
    // Make the buttons flash that havent answered yet
    for (uint8_t button = 0; button < NUM_OF_PLAYERS; button++)
    {
      btn_enabled[button] = !has_answered[button];
      led_status[button] = has_answered[button] ? LED_off : LED_flashing;
    }
    is_ready = true;
    digitalWrite(PIN_STATUS_LED, HIGH);
  }

  // Update our LEDs and monitor for ones that are out of contact
  for (uint8_t button = 0; button < NUM_OF_PLAYERS; button++)
  {
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
        //digitalWrite(BTN_LEDS[button], (led_status[button] == LED_on) || ((led_status[button] == LED_flashing) && (now % 256) > 128)); // (now & 255) > 128

        bool is_led_on = (led_status[button] == LED_on);
        bool is_led_flashing = (led_status[button] == LED_flashing);
        bool is_flash_active = (now % 256) > 128;

        if (is_led_on || (is_led_flashing && is_flash_active)) {
          digitalWrite(BTN_LEDS[button], HIGH);
        } else {
          digitalWrite(BTN_LEDS[button], LOW);
        }
      }
    }
    else
    {
      // For disconnected ones we just give a short 'blip' once per few second
      digitalWrite(BTN_LEDS[button], (now & 2047) > 2000);
    }
  }

  check_radio_message_received();
}

/**************************************************
 *
 **************************************************/
void setup_radio()
{
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.enableDynamicPayloads();
  radio.enableAckPayload();
  radio.setDataRate(RF24_250KBPS);
  radio.setRetries(4, 8);
  radio.maskIRQ(false, false, false); // not using the IRQs
  radio.setCRCLength(RF24_CRC_16);    // Use 16-bit CRC (default)
  // radio.setCRCLength(RF24_CRC_8); // Use 8-bit CRC
}

void start_radio()
{
  if (!radio.isChipConnected())
  {
    Serial.print("ERROR - RF24 not detected\n");
    while (1)
    {
      ; // Infinite loop to stop further execution
    }
  }
  else
  {
    Serial.print("RF24 detected\n");

    digitalWrite(PIN_STATUS_LED, HIGH);

    radio.openWritingPipe(WRITE_PIPE);
    radio.openReadingPipe(1, READ_PIPE);

    for (uint8_t channel = 0; channel < NUM_OF_PLAYERS; channel++)
    {
      pinMode(BTN_LEDS[channel], OUTPUT);
      digitalWrite(BTN_LEDS[channel], LOW);
    }

    radio.startListening();

    Serial.print("Find empty channel ");
    while (!(find_empty_channel()))
    {
      Serial.print("."); // oota kuni leiab vaba kanali
    };
    // Ready
    radio.startListening();
    digitalWrite(PIN_STATUS_LED, LOW);
    setup_ACK_payload();
  }
}

/**************************************************
** Searches the radio spectrum for a quiet channel
***************************************************/
bool find_empty_channel()
{
  Serial.print("Scanning for empty channel...\n");
  char buffer[10];

  // Scan all channels looking for a quiet one.  We skip every 10
  for (int channel = 125; channel > 0; channel -= 10)
  {
    radio.setChannel(channel);
    delay(20);

    unsigned int in_use = 0;
    uint32_t test_start = millis();

    // Check for 400 ms per channel
    while (millis() - test_start < 400)
    {
      digitalWrite(PIN_STATUS_LED, millis() % 500 > 400);
      if ((radio.testCarrier()) || (radio.testRPD()))
      {
        in_use++;
      }
      delay(1);
    }

    // Low usage?
    if (in_use < 10)
    {
      itoa(channel, buffer, 10);
      Serial.print("Channel ");
      Serial.print(buffer);
      Serial.print(" selected\n");
      return true;
    }
  }
  return false;
}

/***********************************************************
** Sends a new ACK payload to the transmitter
************************************************************/
void setup_ACK_payload()
{
  // Update the ACK for the next payload
  uint8_t payload[NUM_OF_PLAYERS];

  for (uint8_t button = 0; button < NUM_OF_PLAYERS; button++)
  {
    payload[button] = (btn_enabled[button] ? 128 : 0) | led_status[button];
  }

  radio.writeAckPayload(1, &payload, NUM_OF_PLAYERS);
}

/**********************************************************
** Check for messages from the buttons
***********************************************************/
void check_radio_message_received()
{
  if (radio.available())
  {
    uint8_t buffer;
    radio.read(&buffer, 1);

    // Grab the button number from the data
    uint8_t button_number = buffer & 0x7F; // Get the button number

    Serial.print("Btn: ");
    Serial.print(button_number);
    Serial.print("\n");

    // if ((button_number >= 1) && (button_number <= NUM_OF_PLAYERS))
    if (button_number <= NUM_OF_PLAYERS)
    {
      button_number--;

      // Update the last contact time for this button
      last_contact_time[button_number] = now;

      // And that it's connected
      btn_connected[button_number] = true;

      // If the button was pressed, was enabled, hasn't answered and the system is ready for button presses
      if ((buffer & 128) && (btn_enabled[button_number]) && (!has_answered[button_number]) && (is_ready))
      {
        // No longer ready
        is_ready = false;

        // Signal the button was pressed
        has_answered[button_number] = true;

        // Change button led status
        for (uint8_t btn = 0; btn < NUM_OF_PLAYERS; btn++)
        {
          led_status[btn] = (btn == button_number) ? LED_on : LED_off;
        }

        digitalWrite(PIN_STATUS_LED, LOW);
      }
    }

    setup_ACK_payload();
  }
}
