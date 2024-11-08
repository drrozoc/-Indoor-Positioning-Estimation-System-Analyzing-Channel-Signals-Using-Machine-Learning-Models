#ifndef ESP32_CSI_CSI_COMPONENT_H
#define ESP32_CSI_CSI_COMPONENT_H

#include "time_component.h"
#include "math.h"
#include <sstream>
#include <iostream>
#include <mutex> // Include for std::mutex (to protect shared data)
#include <thread> // Include for std::this_thread::sleep_for (for simulating delays)
#include <vector> // Include for std::vector (to store CSI data)

std::vector<std::string> csi_data_vector; // Vector to store CSI data as strings
std::vector<int> all_csi_data; // Vector to store all collected CSI data
std::mutex mutex; // Mutex to protect access to the data

char *project_type; // Project type identifier

#define CSI_RAW 1 // Option for raw CSI data
#define CSI_AMPLITUDE 0 // Option for CSI amplitude data
#define CSI_PHASE 0 // Option for CSI phase data

#define CSI_TYPE CSI_RAW // Define the type of CSI data to be used (e.g., raw)

int x = 0; // Example value for X coordinate
int y = 0; // Example value for Y coordinate
const char *current_AP = ""; // Variable to store the current AP (Access Point) connected

bool data_collected = false; // Flag to indicate if data has been collected
bool all_aps_collected = false; // Flag to indicate if all APs' data have been collected

// Function to update the location coordinates
void get_location(int &ub_x, int &ub_y) {
    x = ub_x; // Update X coordinate
    y = ub_y; // Update Y coordinate
}

// Function to update the current Access Point (AP) connected
void get_AP(const char *ACCES_POINT){
    std::lock_guard<std::mutex> lock(mutex); // Lock the mutex to protect shared data
    if (strcmp(current_AP, ACCES_POINT) != 0) {
        current_AP = ACCES_POINT; // Update the AP if it's different
    }
    data_collected = false; // Reset the data collected flag when AP changes
}

// Callback function for handling CSI data
void _wifi_csi_cb(void *ctx, wifi_csi_info_t *data) {
    std::lock_guard<std::mutex> lock(mutex); // Lock the mutex to protect shared data

    if (!data_collected) { // If data has not been collected yet
        std::stringstream ss; // String stream to format the CSI data

        wifi_csi_info_t d = data[0]; // Get the first CSI data
        char mac[20] = {0}; // Array to store the MAC address
        sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5]); // Format MAC address

        ss  << current_AP << "," // Add the current AP information
            << d.rx_ctrl.rssi << "," // Add the RSSI value
            << data->len << ",["; // Add the length of the data

        int data_len = 128; // Set the data length (adjust if needed)

        int8_t *my_ptr; // Pointer to the CSI data buffer
#if CSI_RAW
        my_ptr = data->buf; // Get the raw CSI data
        for (int i = 0; i < data_len; i++) {
            ss << (int)my_ptr[i] << " "; // Add each raw CSI value to the string stream
        }
#endif
#if CSI_AMPLITUDE
        my_ptr = data->buf; // Get the amplitude data
        for (int i = 0; i < data_len / 2; i++) {
            ss << (int)sqrt(pow(my_ptr[i * 2], 2) + pow(my_ptr[(i * 2) + 1], 2)) << " "; // Calculate the amplitude and add it
        }
#endif
#if CSI_PHASE
        my_ptr = data->buf; // Get the phase data
        for (int i = 0; i < data_len / 2; i++) {
            ss << (int)atan2(my_ptr[i * 2], my_ptr[(i * 2) + 1]) << " "; // Calculate the phase and add it
        }
#endif
        ss << "]\n"; // Close the CSI data list

        csi_data_vector.push_back(ss.str()); // Store the formatted CSI data in the vector

        printf("%s", ss.str().c_str()); // Print the collected CSI data
        fflush(stdout); // Ensure the output is printed immediately

        data_collected = true; // Set the flag to true after data is collected
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate a small delay
}

// Function to check if a string is numeric
bool is_numeric(const std::string& str) {
    if (str.empty()) return false; // Return false if the string is empty

    for (char c : str) {
        if (!std::isdigit(c) && c != '-') return false; // Return false if the character is not a digit or a minus sign
    }
    return true; // Return true if all characters are digits or minus sign
}

// Function to collect all CSI data and store it in the `all_csi_data` vector
void collect_all_csi_data() {
    std::lock_guard<std::mutex> lock(mutex); // Lock the mutex to protect shared data

    all_csi_data.clear(); // Clear the `all_csi_data` vector before collecting new data

    for (const auto& data_str : csi_data_vector) { // Iterate over all stored CSI data
        std::stringstream ss(data_str); // Create a string stream to parse the data
        std::string item;

        std::getline(ss, item, '['); // Skip the part before the opening bracket

        while (std::getline(ss, item, ' ')) { // Read each value between spaces
            if (!item.empty() && item.back() == ']') { // Check for closing bracket
                item.pop_back(); // Remove the closing bracket
                if (!item.empty()) {
                    int value;
                    if (std::istringstream(item) >> value) {
                        all_csi_data.push_back(value); // Convert the string to an integer and add it to the vector
                    } else {
                        printf("Error: Invalid argument for string to integer conversion.\n"); // Handle invalid conversion
                    }
                }
                break; // Exit the loop after reading the last value
            }
            if (!item.empty()) {
                int value;
                if (std::istringstream(item) >> value) {
                    all_csi_data.push_back(value); // Convert the string to an integer and add it to the vector
                } else {
                    printf("Error: Invalid argument for string to integer conversion.\n"); // Handle invalid conversion
                }
            }
        }
    }

    std::stringstream final_ss; // Create a final string stream to format the collected data
    final_ss << "CSI_DATA ";
    for (const auto& val : all_csi_data) {
        final_ss << val << " "; // Add each value to the string stream
    }
    std::string final_str = final_ss.str();
    if (!all_csi_data.empty()) {
        final_str.pop_back(); // Remove the last space
    }
    final_str += " "; // Add a space at the end

    printf("%s\n", final_str.c_str()); // Print the final formatted CSI data
    fflush(stdout); // Ensure the output is printed immediately

    // Clear the vector once all AP data has been collected
    if (all_aps_collected) {
        all_csi_data.clear();
        all_aps_collected = false; // Reset the flag for the next cycle
    }
}

// Function to mark that all APs have been collected
void mark_all_aps_collected() {
    std::lock_guard<std::mutex> lock(mutex); // Lock the mutex to protect shared data
    all_aps_collected = true; // Set the flag to true
}

// Function to reset the data collected flag
void reset_data_collected_flag() {
    std::lock_guard<std::mutex> lock(mutex); // Lock the mutex to protect shared data
    data_collected = false; // Reset the flag
}

// Function to collect only CSI data and store it in an array
void collect_csi_data(wifi_csi_info_t *data) {
    std::lock_guard<std::mutex> lock(mutex); // Lock the mutex to protect shared data

    if (!data_collected) { // If data has not been collected yet
        std::stringstream ss; // String stream to format the CSI data

        int data_len = 128; // Adjust the length as needed

        int8_t *my_ptr; // Pointer to the CSI data buffer
#if CSI_RAW
        my_ptr = data->buf; // Get the raw CSI data
        for (int i = 0; i < data_len; i++) {
            ss << (int)my_ptr[i] << " "; // Add each raw CSI value to the string stream
        }
#endif
#if CSI_AMPLITUDE
        my_ptr = data->buf; // Get the amplitude data
        for (int i = 0; i < data_len / 2; i++) {
            ss << (int)sqrt(pow(my_ptr[i * 2], 2) + pow(my_ptr[(i * 2) + 1], 2)) << " "; // Add the amplitude
        }
#endif
#if CSI_PHASE
        my_ptr = data->buf; // Get the phase data
        for (int i = 0; i < data_len / 2; i++) {
            ss << (int)atan2(my_ptr[i * 2], my_ptr[(i * 2) + 1]) << " "; // Add the phase
        }
#endif
        ss << "]\n"; // Close the CSI data list

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
