/**
 *  Klient - NUPP
 *  Edited: 17.02.2025
 *  Tauno Erik
 *  
 *  Board: Arduino Nano
 *  NRF24L01
 */
#include <Arduino.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define BUTTON_PIN 4
#define LED_PIN 5
#define CE_PIN 9
#define CSN_PIN 10

enum player {
  Red,    // 0
  Green,
  Yellow,
  Blue,
  White  // 4
};

// Unique player ID (0-4)
//const int PLAYER_ID = 0; // Red
//const int PLAYER_ID = 1; // Green
//const int PLAYER_ID = 2; // Yellow
//const int PLAYER_ID = 3; // Blue
//const int PLAYER_ID = 4; // White

const int PLAYER_ID = Red;

RF24 radio(CE_PIN, CSN_PIN);
const byte ADDRESS[6] = "00001";

void setup() {
  Serial.begin(57600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  for (size_t i = 0; i < 3; i++)
  {
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    delay(500);
  }

  digitalWrite(LED_PIN, LOW);

  radio.begin();
  radio.openWritingPipe(ADDRESS);
  radio.openReadingPipe(1, ADDRESS);
  radio.setPALevel(RF24_PA_MAX); // Set power level (low for short range)
  ///
  radio.enableDynamicPayloads();
  radio.enableAckPayload();
  radio.setDataRate(RF24_250KBPS);
  radio.setRetries(2, 8);
  radio.maskIRQ(false, false, false); // not using the IRQs
  ///
  radio.stopListening(); // Set as transmitter initially

  Serial.println("Player setup complete. Waiting for button press...");
}

void loop() {
  // Check if the game master has sent a message to this player
  radio.startListening();

  if (radio.available())
  {
    int message;
    radio.read(&message, sizeof(message));
    Serial.print("message in: ");
    Serial.println(message);

    if (message == PLAYER_ID)
    {
      Serial.println("Message is for this player. Turning on LED.");
      digitalWrite(LED_PIN, HIGH);
      delay(1000); // Keep the LED on for 1 second (or as long as needed)
      digitalWrite(LED_PIN, LOW);
    }
  }

  radio.stopListening(); // Stop listening and switch back to transmitter mode

  // Send player ID when the button is pressed
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    Serial.println("Button pressed. Sending player ID...");
    radio.openWritingPipe(ADDRESS); // Set communication address for writing
    bool success = radio.write(&PLAYER_ID, sizeof(PLAYER_ID)); // Send player ID

    if (success)
    {
      Serial.println("Player ID sent successfully.");
    } else {
      Serial.println("Failed to send Player ID.");
    }

    unsigned long debounceStart = millis();
    while (digitalRead(BUTTON_PIN) == LOW && (millis() - debounceStart < 500)); // Wait for button release or 500ms
  }
}
