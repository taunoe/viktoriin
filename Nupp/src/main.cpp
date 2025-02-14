/**
 *  Klient - NUPP
 *  Edited: 14.02.2025
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

const int PLAYER_ID = White;

RF24 radio(CE_PIN, CSN_PIN); // Create radio object
const byte address[6] = "00001"; // Communication address

void setup() {
  Serial.begin(57600);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Button input
  pinMode(LED_PIN, OUTPUT); // LED output
  digitalWrite(LED_PIN, LOW); // Ensure LED is off initially

  radio.begin(); // Start radio
  radio.openWritingPipe(address); // Set communication address for writing
  radio.openReadingPipe(1, address); // Set communication address for reading
  radio.setPALevel(RF24_PA_LOW); // Set power level (low for short range)
  radio.stopListening(); // Set as transmitter initially

  Serial.println("Player setup complete. Waiting for button press...");
}

void loop() {
  // Check if the game master has sent a message to this player
  radio.startListening(); // Start listening for messages
  if (radio.available()) {
    int message;
    radio.read(&message, sizeof(message)); // Read the message
    Serial.print("message in: ");
    Serial.println(message);

    if (message == PLAYER_ID) { // If the message is for this player
      Serial.println("Message is for this player. Turning on LED.");
      digitalWrite(LED_PIN, HIGH); // Turn on the LED
      delay(1000); // Keep the LED on for 1 second (or as long as needed)
      digitalWrite(LED_PIN, LOW); // Turn off the LED
    }
  }

  radio.stopListening(); // Stop listening and switch back to transmitter mode

  // Send player ID when the button is pressed
  if (digitalRead(BUTTON_PIN) == LOW) { // If button is pressed
    Serial.println("Button pressed. Sending player ID...");
    radio.openWritingPipe(address); // Set communication address for writing
    bool success = radio.write(&PLAYER_ID, sizeof(PLAYER_ID)); // Send player ID
    if (success) {
      Serial.println("Player ID sent successfully.");
    } else {
      Serial.println("Failed to send Player ID.");
    }
    delay(500); // Debounce delay
    while (digitalRead(BUTTON_PIN) == LOW); // Wait for button release
  }
}
