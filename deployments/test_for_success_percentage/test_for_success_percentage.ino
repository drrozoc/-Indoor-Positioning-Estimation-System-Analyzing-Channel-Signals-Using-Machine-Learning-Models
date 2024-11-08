#include <Arduino.h>
#include <WiFi.h>
#include "sockets_component.h"
#include "csi_component.h"
#include <regresion_lineal_pasillo_habtprinc_inferencing.h>

#define SIZE_SUB_ARRAY 387 // Adjust as needed
#define NUM_SSIDS 3        

// List of WiFi networks (SSIDs) and their passwords
const char *ssid_list[NUM_SSIDS] = { "AP3", "AP4", "AP5"};
const char *pass_list[NUM_SSIDS] = { "12345678", "12345678", "12345678"};

static bool location_ready = false; // Flag indicating if the location is set
static bool reset = false; // Flag to control reset action
static int x_location = 0; // X coordinate for location
static int y_location = 0; // Y coordinate for location

int csi_buffer[SIZE_SUB_ARRAY]; // Buffer for storing CSI data
size_t csi_buffer_size = SIZE_SUB_ARRAY; // Size of the CSI buffer
int csi_buffer_index = 0; // Index for tracking CSI data in the buffer

bool send_csi = true; // Flag to control CSI data sending

float csi_temp_buffer[SIZE_SUB_ARRAY]; // Temporary buffer for CSI data processing

// Check if WiFi is connected
bool isWiFiConnected() {
    return WiFi.isConnected();
}

int rssi_chain[NUM_SSIDS]; // Array for storing RSSI values of each network

// Function to fill an output buffer with CSI data and add RSSI values at specific intervals
int csi_complete_a(unsigned int start_index, unsigned int max_size, float* out_ptr) {
    int rssi_time = 0; // Interval for adding RSSI data
    int j = 0; // Counter for RSSI data
    size_t flag_rssi = 1; // Not used in the code, but declared

    // Calculate the maximum data to copy based on buffer limits
    size_t size_to_copy = (start_index + max_size <= csi_buffer_size) ? max_size : (csi_buffer_size - start_index);

    for (size_t i = 0; i < size_to_copy; i++) {
      x++;
        if (i == rssi_time && j < 3) {  // Condition to insert RSSI values without exceeding bounds
            out_ptr[i] = rssi_chain[j];
            rssi_time += 129;
            j++;
        }
        out_ptr[i] = csi_buffer[i + start_index];
    }

    return size_to_copy; // Return the amount of data copied
}

// Similar function to fill output buffer, adding RSSI values at specific intervals and avoiding index overflows
int csi_complete(unsigned int start_index, unsigned int max_size, float* out_ptr) {
    int rssi_time = 0; // Interval for adding RSSI data
    int j = 0; // Counter for RSSI data
    size_t flag_rssi = 1;
    size_t avanza = 0; // Variable to track buffer index

    // Calculate the maximum data to copy based on buffer limits
    size_t size_to_copy = (start_index + max_size <= csi_buffer_size) ? max_size : (csi_buffer_size - start_index);

    for (size_t i = 0; i < size_to_copy; i++) {
        if (i == rssi_time && j < 3) {  // Condition to insert RSSI values
            out_ptr[i] = rssi_chain[j];
            rssi_time += 129;
            j++;
            i++; // Adjust index after adding RSSI
        }
        out_ptr[i] = csi_buffer[avanza + start_index];
        avanza++;
    }

    return size_to_copy;
}

// Run the Edge Impulse model for inference on CSI data
void run_ei() {
    signal_t signal;
    signal.total_length = SIZE_SUB_ARRAY;
    signal.get_data = &csi_complete;

    ei_impulse_result_t result;
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, true);
    if (err != EI_IMPULSE_OK) {
        Serial.println("Classification error.");
        return;
    }

    Serial.println("Predictions:");
    float max_value = -1.0;
    const char* max_label = nullptr;

    // Print each classification result and find the label with highest confidence
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        Serial.print(result.classification[ix].label);
        Serial.print(": ");
        Serial.print(result.classification[ix].value * 100, 2);
        Serial.println("%");

        if (result.classification[ix].value > max_value) {
            max_value = result.classification[ix].value;
            max_label = result.classification[ix].label;
        }
    }

    Serial.print("Classification: ");
    Serial.print(max_label);
    Serial.print(" with confidence ");
    Serial.print(max_value * 100, 2);
    Serial.println("%");

    reset_csi_buffer(); // Reset CSI data buffer
    csi_data_vector.clear();  // Clear stored CSI data
}

// Reset the CSI data buffer index
void reset_csi_buffer() {
    csi_buffer_index = 0;
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  Serial.println("------------------------------------------------------------------------------");
  Serial.println("STARTING TEST");

  Serial.println("Enter location in x.y format");
  while (!location_ready) {
      if (Serial.available() > 0) {
          String input = Serial.readStringUntil('\n');
          sscanf(input.c_str(), "%d.%d", &x_location, &y_location);
          location_ready = true;
          Serial.print("Location received -> X: ");
          Serial.print(x_location);
          Serial.print(" . Y: ");
          Serial.println(y_location);
      }
      delay(100);
  }

  csi_buffer_index = 0;
}

void loop() {
  for (int i = 0; i < NUM_SSIDS; i++) {
      Serial.print("Connecting to ");
      Serial.print(ssid_list[i]);
      Serial.println("...");

      WiFi.begin(ssid_list[i], pass_list[i]);

      unsigned long start = millis();
      while (WiFi.status() != WL_CONNECTED) {
          if (millis() - start > 50000) {
              Serial.println("Failed to connect");
              ESP.restart();
              break;
          }
          delay(300);
          Serial.print(".");
      }

      if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Connected");
          get_AP(ssid_list[i]);

          csi_init("STA");

          delay(200);

          socket_transmitter_sta_loop(&isWiFiConnected);
          delay(200);

          csi_deinit();

          rssi_chain[i] = static_cast<float>(rssi_value);
      } else {
          Serial.println("Failed to connect to WiFi");
          send_csi = false;
      }

      Serial.println("NEXT AP ------------------------------------------------------------------------------");
      delay(200);
  }

  // Run the Edge Impulse classifier
  run_ei();

  Serial.println("TEST COMPLETED");
  Serial.println("------------------------------------------------------------------------------");

  // Ask for a reset confirmation
  while (!reset) {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.c_str();
        ESP.restart();
        reset = true;
        }
    }
}
