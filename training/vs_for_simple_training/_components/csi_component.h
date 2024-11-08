#ifndef ESP32_CSI_CSI_COMPONENT_H
#define ESP32_CSI_CSI_COMPONENT_H

#include "time_component.h"
#include "math.h"
#include <sstream>
#include <iostream>
#include <mutex> // Include for std::mutex to handle concurrent access
#include <thread> // Include for std::this_thread::sleep_for to simulate delays
#include <vector> // Include for std::vector to store CSI data

std::vector<std::string> csi_data_vector; // Vector to store the CSI data
std::vector<int> all_csi_data; // Vector to store all collected CSI data
std::mutex mutex; // Mutex to protect access to data (synchronization)

char *project_type;

#define CSI_RAW 1
#define CSI_AMPLITUDE 0
#define CSI_PHASE 0

#define CSI_TYPE CSI_RAW // Define which type of CSI data to collect

int x = 0; // Example: value of X
int y = 0; // Example: value of Y
const char *current_AP = ""; // The current access point connected

bool data_collected = false; // Flag to indicate if data has been collected
bool all_aps_collected = false; // Flag to indicate if data from all APs has been collected

// Function to get the current location (for position tracking)
void get_location(int &ub_x, int &ub_y) {
    x = ub_x; // Example: value of X
    y = ub_y; // Example: value of Y
}

// Function to update the current access point (AP)
void get_AP(const char *ACCES_POINT){
    std::lock_guard<std::mutex> lock(mutex); // Lock the mutex to prevent race conditions
    if (strcmp(current_AP, ACCES_POINT) != 0) { // If the current AP is different, update it
        current_AP = ACCES_POINT;
    }
    data_collected = false; // Reset data collected flag when switching APs
}

// Callback function to handle CSI data collection
void _wifi_csi_cb(void *ctx, wifi_csi_info_t *data) {
    std::lock_guard<std::mutex> lock(mutex); // Lock the mutex to ensure thread safety

    if (!data_collected) { // Collect data only once for each AP change
        std::stringstream ss;

        wifi_csi_info_t d = data[0];
        char mac[20] = {0};
        sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5]);

        ss << "4B" << "," 
            //<<current_AP << ","  // Uncomment if AP info is needed in the log
           << d.rx_ctrl.rssi << ","; // Include the RSSI value

        int data_len = 128; // Length of the CSI data

        int8_t *my_ptr;
#if CSI_RAW
        // If CSI_RAW is selected, append the raw data values
        my_ptr = data->buf;
        for (int i = 0; i < data_len; i++) {
            ss << (int)my_ptr[i] << " ";
        }
#endif
#if CSI_AMPLITUDE
        // If CSI_AMPLITUDE is selected, compute the amplitude and append it
        my_ptr = data->buf;
        for (int i = 0; i < data_len / 2; i++) {
            ss << (int)sqrt(pow(my_ptr[i * 2], 2) + pow(my_ptr[(i * 2) + 1], 2)) << " ";
        }
#endif
#if CSI_PHASE
        // If CSI_PHASE is selected, compute the phase and append it
        my_ptr = data->buf;
        for (int i = 0; i < data_len / 2; i++) {
            ss << (int)atan2(my_ptr[i * 2], my_ptr[(i * 2) + 1]) << " ";
        }
#endif
        ss << "\n"; // End the line for this data packet

        csi_data_vector.push_back(ss.str()); // Store the collected CSI data in the vector

        printf("%s", ss.str().c_str()); // Print the data to the console
        fflush(stdout); // Ensure data is immediately printed

        data_collected = true; // Mark data as collected for this AP
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate a delay to avoid overloading the system
}

// Function to check if a string represents a numeric value
bool is_numeric(const std::string& str) {
    if (str.empty()) return false; // Return false for empty strings

    for (char c : str) {
        if (!std::isdigit(c) && c != '-') return false; // Allow '-' for negative numbers
    }
    return true;
}

// Function to collect all CSI data and format it for transmission
void collect_all_csi_data() {
    std::lock_guard<std::mutex> lock(mutex); // Lock the mutex to avoid concurrent access issues

    all_csi_data.clear(); // Clear the previous collected data before filling it with new data

    // Process each entry in csi_data_vector
    for (const auto& data_str : csi_data_vector) {
        std::stringstream ss(data_str);
        std::string item;

        std::getline(ss, item, '['); // Skip the opening bracket '['

        // Read the data values between spaces and store them in all_csi_data
        while (std::getline(ss, item, ' ')) {
            if (!item.empty() && item.back() == ']') { // Remove the closing bracket ']'
                item.pop_back();
                if (!item.empty()) {
                    int value;
                    if (std::istringstream(item) >> value) { // Convert string to integer
                        all_csi_data.push_back(value);
                    } else {
                        printf("Error: Invalid argument for string to integer conversion.\n");
                    }
                }
                break;
            }
            if (!item.empty()) {
                int value;
                if (std::istringstream(item) >> value) { // Convert string to integer
                    all_csi_data.push_back(value);
                } else {
                    printf("Error: Invalid argument for string to integer conversion.\n");
                }
            }
        }
    }

    // Format the data for final output
    std::stringstream final_ss;
    final_ss << "CSI_DATA,["; 
    for (const auto& val : all_csi_data) {
        final_ss << val << " "; // Append each value
    }
    std::string final_str = final_ss.str();
    if (!all_csi_data.empty()) {
        final_str.pop_back(); // Remove the last space
    }
    final_str += "]"; // Add closing bracket

    // Print the final formatted data
    printf("%s\n", final_str.c_str());
    fflush(stdout); // Ensure immediate printing

    // Clear the data vector after collecting all APs
    if (all_aps_collected) {
        all_csi_data.clear(); // Clear all data after collection is complete
        all_aps_collected = false; // Reset the flag for the next cycle
    }
}

// Function to mark that all APs have been collected
void mark_all_aps_collected() {
    std::lock_guard<std::mutex> lock(mutex);
    all_aps_collected = true; // Set the flag to true
}

// Function to reset the data collected flag
void reset_data_collected_flag() {
    std::lock_guard<std::mutex> lock(mutex);
    data_collected = false; // Reset the flag
}

// Function to collect CSI data and store it in an array
void collect_csi_data(wifi_csi_info_t *data) {
    std::lock_guard<std::mutex> lock(mutex); // Lock the mutex to ensure thread safety

    if (!data_collected) { // Collect data only once for each AP change
        std::stringstream ss;

        int data_len = 128; // Length of the CSI data

        int8_t *my_ptr;
#if CSI_RAW
        my_ptr = data->buf; // Get the raw data buffer
        for (int i = 0; i < data_len; i++) {
            ss << (int)my_ptr[i] << " "; // Add each raw data byte to the stringstream
        }
#endif
#if CSI_AMPLITUDE
        my_ptr = data->buf;
        for (int i = 0; i < data_len / 2; i++) {
            ss << (int)sqrt(pow(my_ptr[i * 2], 2) + pow(my_ptr[(i * 2) + 1], 2)) << " "; // Add amplitude data
        }
#endif
#if CSI_PHASE
        my_ptr = data->buf;
        for (int i = 0; i < data_len / 2; i++) {
            ss << (int)atan2(my_ptr[i * 2], my_ptr[(i * 2) + 1]) << " "; // Add phase data
        }
#endif
        ss << "]"; // Close the data section

        csi_data_vector.push_back(ss.str()); // Store the CSI data in the vector
    }
}

// Function to print all stored CSI data
void print_stored_csi_data() {
    std::lock_guard<std::mutex> lock(mutex); // Lock the mutex
    for (const auto &data : csi_data_vector) {
        printf("%s\n", data.c_str());
    }
}

void csi_init(char *type) {
    project_type = type;

    ESP_ERROR_CHECK(esp_wifi_set_csi(1));

    wifi_csi_config_t configuration_csi;
    configuration_csi.lltf_en = 1;
    configuration_csi.htltf_en = 1;
    configuration_csi.stbc_htltf2_en = 1;
    configuration_csi.ltf_merge_en = 1;
    configuration_csi.channel_filter_en = 0;
    configuration_csi.manu_scale = 0;

    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&configuration_csi));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(&_wifi_csi_cb, NULL));
}

#endif // ESP32_CSI_CSI_COMPONENT_H
