/*********************************************
 * Ülemus
 * main.cpp
 * Tauno Erik
 * Edited 25.02.2025
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
//#define LED_PINS {10, 11, 12, 13, 14} // LED pins for players 0-4
#define CE_PIN 22
#define CSN_PIN 17

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "00001";

int player_LEDs[5] =  {10, 11, 12, 13, 14}; // Array of LED pins
bool playerStates[5] = {false}; // Track which players have buzzed
int first_player = -1; // Track the first player to buzz
bool second_attempt = false;

/**************************************/
void reset_game();
void ready_game();
void notify_player(int playerID);
/**************************************/

void setup() {
  Serial.begin(57600);
  // Initialize buttons
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(READY_BUTTON_PIN, INPUT_PULLUP);

  // Initialize LEDs
  for (int i = 0; i < 5; i++)
  {
    pinMode(player_LEDs[i], OUTPUT);
    digitalWrite(player_LEDs[i], LOW); // Ensure LEDs are off initially
  }

  // Initialize radio
  radio.begin();
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_MAX);
  ///
  radio.enableDynamicPayloads();
  radio.enableAckPayload();
  radio.setDataRate(RF24_250KBPS);
  radio.setRetries(2, 10);
  radio.maskIRQ(false, false, false); // not using the IRQs
  radio.setCRCLength(RF24_CRC_16);    // Use 16-bit CRC (default)
  ///
  radio.startListening();

  Serial.println("Game manager setup complete. Waiting for player input...");
}

void loop()
{
  // Check for reset button press
  if (digitalRead(RESET_BUTTON_PIN) == LOW)
  {
    Serial.println("Reset button pressed. Resetting game...");
    reset_game();
    delay(100); // Debounce delay
    while (digitalRead(RESET_BUTTON_PIN) == LOW); // Wait for button release
  }

  // Check for ready button press
  if (digitalRead(READY_BUTTON_PIN) == LOW)
  {
    Serial.println("Ready button pressed. Starting game...");
    ready_game();
    delay(100); // Debounce delay
    while (digitalRead(READY_BUTTON_PIN) == LOW); // Wait for button release
  }

  // Check for incoming player signals
  if (radio.available())
  {
    int playerID;
    radio.read(&playerID, sizeof(playerID)); // Read player ID
    Serial.print("Received player ID: ");
    Serial.println(playerID);

    if (first_player == -1)
    { // If no player has buzzed yet
      first_player = playerID; // Set the first player
      notify_player(first_player); // Notify the first player
      Serial.print("First player to buzz: ");
      Serial.println(first_player);
      digitalWrite(player_LEDs[playerID], HIGH); // Turn on corresponding LED
    }

    if (second_attempt)
    {
      if (playerID != first_player)
      {
        notify_player(playerID); // Notify the second player
        Serial.print("Second player to buzz: ");
        Serial.println(playerID);
        digitalWrite(player_LEDs[playerID], HIGH); // Turn on corresponding LED
        second_attempt = false; // Reset second attempt flag
      }

    }
  }
}


void reset_game() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(player_LEDs[i], LOW); // Turn off all LEDs
  }
  first_player = -1; // Reset first player
  second_attempt = false; // Reset second attempt flag
  Serial.println("Game reset.");
}

void ready_game() {
  //reset_game(); // Reset the game before starting
  for (int i = 0; i < 5; i++) {
    digitalWrite(player_LEDs[i], LOW); // Turn off all LEDs
  }

  second_attempt = true;

  // Additional logic for starting the game can be added here
  Serial.println("Game ready.");
}

// Function to notify the first player
void notify_player(int playerID) {
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
  delay(100); // Small delay to ensure the message is sent
  radio.startListening(); // Switch back to receiver mode
}
