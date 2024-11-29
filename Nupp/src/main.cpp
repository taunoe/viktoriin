/**
 *  Klient - NUPP
 *  Edited: 29.11.2024
 *  Tauno Erik
 *  
 *  Board: Arduino Nano
 */
#include <Arduino.h>
#include <RF24.h>

// !!!! VALI !!!!
//#define DEVICE_ID 1 // Punane
//#define DEVICE_ID 2 // Roheline
//#define DEVICE_ID 3 // Kollane
//#define DEVICE_ID 4 // Sinine
#define DEVICE_ID 5 // Valge

#define NUM_OF_PLAYERS 5

// Arduino Nano:
//   4 -> Button to GND
//   5 -> LED to GND (Button)
//   9 -> CE  (nRF24)
//  10 -> CSN (nRF24)
//  11 -> MO  (nRF24)
//  12 -> MI  (nRF24)
//  13 -> SCK (nRF24)

#define PIN_BUTTON 4
#define PIN_LED 5
#define PIN_CE 9
#define PIN_CSN 10


RF24 radio(PIN_CE, PIN_CSN);


enum LED_Status : uint8_t
{
  LED_off = 0,
  LED_on = 1,
  LED_flashing = 2
};

bool is_connected = false;  // Is in contact with the controller?
bool btn_enabled = false;

uint8_t btn_number = DEVICE_ID;
uint8_t write_pipe[6] = "1QBTN";
uint8_t read_pipe[6] = "0QBTN";

uint32_t last_status_send = 0;
uint32_t btn_down_time = 0;

LED_Status led_status = LED_off;
 

/*******************************************************/
bool find_btn_controller();
bool send_btn_status(bool is_down);
/*******************************************************/


void setup()
{
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);

  Serial.begin(57600);
  /*while (!Serial)
  {
  };
  */

  if (!radio.begin())
  {
    Serial.write("RF24 device failed to begin\n");
  }
  radio.setPALevel(RF24_PA_LOW); // Max power
  radio.enableDynamicPayloads();
  radio.enableAckPayload();
  radio.setDataRate(RF24_250KBPS);
  radio.setRetries(2, 2);
  radio.maskIRQ(false, false, false); // not using the IRQs

  if (!radio.isChipConnected())
  {
    Serial.write("RF24 device not detected.\n");
  }
  else
  {
    Serial.write("RF24 device found\n");
  }

  radio.openWritingPipe(write_pipe);
  radio.openReadingPipe(1, read_pipe);
  radio.stopListening();
}


void loop()
{
  uint32_t now = millis();

  if (radio.isChipConnected())
  {

    // If connectin ACK timeout or not connected
    if ((now - last_status_send > 1000) || (!is_connected))
    {
      // A short blip meaning its powered up, but not working
      while (!find_btn_controller())
      {
      };
      digitalWrite(PIN_LED, LOW);
      is_connected = true;
      last_status_send = now;
    }

    // If the button was pressed down (and its been 300ms since last check)
    if ((digitalRead(PIN_BUTTON) == LOW) && (now - btn_down_time > 300) && (btn_enabled))
    {
      // This ensures we get a random number sequence unique to this player.  The random number is used to prevent packet collision
      randomSeed(now);
      // Send the DOWN state
      if (send_btn_status(true))
      {
        btn_down_time = now;
        last_status_send = now;
      }
    }

    // If its been 150ms since last TX send status
    if (now - last_status_send > 150)
    {
      if (send_btn_status(false))
      {
        last_status_send = now;
      }
      else
        delay(10);
    }

    digitalWrite(PIN_LED, (led_status == LED_on) || ((led_status == LED_flashing) && ((now & 255) > 128)));
  }
  else
  {
    // Error flash sequence
    digitalWrite(PIN_LED, (now & 1023) < 100);
  }

  // Slow the main loop down
  delay(1);
}


/*************************************************************
** Search for the button controller channel
**************************************************************/
bool find_btn_controller()
{
  Serial.write("Searching for controller...\n");

  for (int a = 125; a > 0; a -= 10)
  {
    radio.setChannel(a);
    delay(15);
    // Send a single byte for status
    if (send_btn_status(false))
    {
      Serial.write("Quiz Controller found on channel ");
      char buffer[10];
      itoa(a, buffer, 10);
      Serial.write(buffer);
      Serial.write("\n");
      return true;
    }
    digitalWrite(PIN_LED, (millis() & 2047) > 2000);
  }

  // Add a 1.5 second pause before trying again (but still flash the LED)
  uint32_t start_time = millis();

  while (millis() - start_time < 1500)
  {
    digitalWrite(PIN_LED, (millis() & 2047) > 2000);
    delay(15);
  }

  return false;
}


/*******************************************************************************
** Attempt to send the sttaus of the button and receive what we shoudl be doing
********************************************************************************/
bool send_btn_status(bool is_down)
{
  uint8_t message = btn_number;

  if (is_down)
  {
    message |= 128;
  }

  for (uint8_t retries = 0; retries < NUM_OF_PLAYERS; retries++) //4
  {
    // This delay is used incase transmit fails.  We will assume it fails because of data collision with another button.
    // This is inspired by https://www.geeksforgeeks.org/back-off-algorithm-csmacd/
    unsigned int random_delay_amount = random(1, 2 + ((retries * retries) * 2));

    if (radio.write(&message, 1))
    {
      if (radio.available())
      {
        Serial.write("radio.available()\n");
        if (radio.getDynamicPayloadSize() == NUM_OF_PLAYERS)
        {
          unsigned char tmp[NUM_OF_PLAYERS];
          radio.read(&tmp, NUM_OF_PLAYERS);

          btn_enabled = (tmp[btn_number - 1] & 128) != 0;
          led_status = (LED_Status)(tmp[btn_number - 1] & 3);
          Serial.write("Radio write OK, ACK Payload\n");

          return true;
        }
        else
        {
          // Remove redundant data
          int total = radio.getDynamicPayloadSize();
          unsigned char tmp;
          while (total-- > 0)
          {
            radio.read(&tmp, 1);
          }
          Serial.write("Radio write OK, ACK wrong size\n");
          delay(random_delay_amount);
        }
      }
      else
      {
        // This shouldn't really happen, but can sometimes if the controller is busy
        Serial.write("Radio write OK, no ACK\n");
        return true;
      }
    }
    else
    {
      delay(random_delay_amount);
    }
  }

  Serial.write("Radio write Failed\n");
  return false;
}
