/*********************************************
 * Ülemus
 * main.cpp
 * Tauno Erik
 * Edited 14.02.2025
 *
 * Raspberry Pi Pico
 * nRF24L01 Radio
 *********************************************/
#include <Arduino.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define RESET_BUTTON_PIN 8
#define READY_BUTTON_PIN 9
#define LED_PINS {10, 11, 12, 13, 14} // LED pins for players 0-4
#define CE_PIN 22
#define CSN_PIN 17

RF24 radio(CE_PIN, CSN_PIN); // Create radio object
const byte address[6] = "00001"; // Communication address

int playerLEDs[] = LED_PINS; // Array of LED pins
bool playerStates[5] = {false}; // Track which players have buzzed
int firstPlayer = -1; // Track the first player to buzz

/**************************************/
void resetGame();
void readyGame();
void notifyPlayer(int playerID);
/**************************************/

void setup() {
  Serial.begin(57600);
  // Initialize buttons
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(READY_BUTTON_PIN, INPUT_PULLUP);

  // Initialize LEDs
  for (int i = 0; i < 5; i++) {
    pinMode(playerLEDs[i], OUTPUT);
    digitalWrite(playerLEDs[i], LOW); // Ensure LEDs are off initially
  }

  // Initialize radio
  radio.begin();
  radio.openReadingPipe(0, address); // Set communication address
  radio.setPALevel(RF24_PA_LOW); // Set power level (low for short range)
  radio.startListening(); // Set as receiver

  Serial.println("Game manager setup complete. Waiting for player input...");
}

void loop() {
  // Check for reset button press
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    Serial.println("Reset button pressed. Resetting game...");
    resetGame();
    delay(100); // Debounce delay
    while (digitalRead(RESET_BUTTON_PIN) == LOW); // Wait for button release
  }

  // Check for ready button press
  if (digitalRead(READY_BUTTON_PIN) == LOW) {
    Serial.println("Ready button pressed. Starting game...");
    readyGame();
    delay(100); // Debounce delay
    while (digitalRead(READY_BUTTON_PIN) == LOW); // Wait for button release
  }

  // Check for incoming player signals
  if (radio.available()) {
    int playerID;
    radio.read(&playerID, sizeof(playerID)); // Read player ID
    Serial.print("Received player ID: ");
    Serial.println(playerID);
    if (firstPlayer == -1) { // If no player has buzzed yet
      firstPlayer = playerID; // Set the first player
      notifyPlayer(firstPlayer); // Notify the first player
      Serial.print("First player to buzz: ");
      Serial.println(firstPlayer);
      digitalWrite(playerLEDs[playerID], HIGH); // Turn on corresponding LED
    }
  }
}

void resetGame() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(playerLEDs[i], LOW); // Turn off all LEDs
  }
  firstPlayer = -1; // Reset first player
  Serial.println("Game reset.");
}

void readyGame() {
  resetGame(); // Reset the game before starting
  // Additional logic for starting the game can be added here
  Serial.println("Game ready.");
}

// Function to notify the first player
void notifyPlayer(int playerID) {
  Serial.print("Notifying player: ");
  Serial.println(playerID);
  radio.stopListening(); // Stop listening and switch to transmitter mode
  radio.openWritingPipe(address); // Set communication address for writing
  
  bool success = radio.write(&playerID, sizeof(playerID)); // Send message to the player

  if (success) {
    Serial.println("Message sent successfully.");
  } else {
    Serial.println("Failed to send message.");
  }
  delay(10); // Small delay to ensure the message is sent
  radio.startListening(); // Switch back to receiver mode
}
