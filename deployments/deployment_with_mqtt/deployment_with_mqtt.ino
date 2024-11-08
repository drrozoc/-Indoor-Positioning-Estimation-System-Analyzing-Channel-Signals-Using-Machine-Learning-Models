void run_ei() {
    signal_t signal;
    signal.total_length = SIZE_SUB_ARRAY;  // Set the total length of the signal to the size of the sub-array
    // signal.get_data = &csi_complete;  // Set the function to get CSI data (commented out)
    signal.get_data = &simulacion_csi;  // Set the function to get CSI data (using the simulation function)
    
    ei_impulse_result_t result;  // Variable to store the classification result
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, true);  // Run the classifier on the signal
    
    if (err != EI_IMPULSE_OK) {  // Check if there was an error during classification
        Serial.println("Classification error.");
        return;
    }

    Serial.println("Predictions:");  // Print the predictions
    float max_value = -1.0;  // Initialize the maximum value to a very low number
    const char* max_label = nullptr;  // Initialize the label for the highest value

    // Iterate through all classification labels and values
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        Serial.print(result.classification[ix].label);  // Print the label
        Serial.print(": ");
        Serial.print(result.classification[ix].value * 100, 2);  // Print the confidence value as percentage
        Serial.println("%");

        // Update the maximum label and value if a higher value is found
        if (result.classification[ix].value > max_value) {
            max_value = result.classification[ix].value;
            max_label = result.classification[ix].label;
        }
    }

    // Print the highest classification label and confidence
    Serial.print("Classification: ");
    Serial.print(max_label);
    Serial.print(" with a confidence of ");
    Serial.print(max_value * 100, 2);  // Print the maximum confidence percentage
    Serial.println("%");

    setup_wifi();  // Setup WiFi connection
    send_inference(max_label, max_value);  // Send the inference result

    reset_csi_buffer();  // Reset the CSI data buffer
    csi_data_vector.clear();  // Clear the stored CSI data
}

void reset_csi_buffer() {
    csi_buffer_index = 0;  // Reset the index for the CSI buffer
}

void setup() {
  Serial.begin(115200);  // Start serial communication at 115200 baud rate

  WiFi.mode(WIFI_STA);  // Set WiFi mode to station (STA)
  Serial.println("------------------------------------------------------------------------------");
  Serial.println("STARTING TEST");

  Serial.println("Insert location in the following format x.y");
  while (!location_ready) {  // Wait until location is set
      if (Serial.available() > 0) {  // Check if there's data available in serial
          String input = Serial.readStringUntil('\n');  // Read the location input
          sscanf(input.c_str(), "%d.%d", &x_location, &y_location);  // Parse the input
          location_ready = true;  // Mark location as ready
          Serial.print("Location received -> X: ");
          Serial.print(x_location);  // Print X coordinate
          Serial.print(" . Y: ");
          Serial.println(y_location);  // Print Y coordinate
      }
      delay(100);  // Delay before checking for input again
  }

  csi_buffer_index = 0;  // Initialize CSI buffer index
}

void loop() {
  /* The following section is commented out. It used to handle WiFi connection and CSI data collection. */
  /* 
  for (int i = 0; i < NUM_SSIDS; i++) {
      Serial.print("Connecting to ");
      Serial.print(ssid_list[i]);
      Serial.println("...");

      WiFi.begin(ssid_list[i], pass_list[i]);

      unsigned long start = millis();
     /* while (WiFi.status() != WL_CONNECTED) {
          if (millis() - start > 50000) {
              Serial.println("Unable to connect");
              ESP.restart();
              break;
          }
          delay(300);
          Serial.print(".");
      }

      //if (WiFi.status() == WL_CONNECTED) {
        if(1){
          Serial.println("Connected");
          get_AP(ssid_list[i]);

          csi_init("STA");

          //send_csi = true;
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
  */

  // Run the Edge Impulse classifier
  run_ei();

  Serial.println("ENDING TEST");
  Serial.println("------------------------------------------------------------------------------");
  Serial.println("------------------------------------------------------------------------------");
  Serial.println("|                                                                            |");
  Serial.println("|                                 reset?                                     |");
  Serial.println("|                                                                            |");
  Serial.println("------------------------------------------------------------------------------");
  delay(5000);  // Wait for 5 seconds before ending the test

  /*
  The following commented-out section was designed to allow for a reset after the test.
  While reset is not triggered, the ESP will restart after receiving an input through the serial.
  */
  /*
  while (!reset) {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.c_str();
        ESP.restart();
        reset = true;
        }
    }*/
}
