#ifndef ESP32_CSI_CSI_COMPONENT_H
#define ESP32_CSI_CSI_COMPONENT_H

#include "time_component.h"
#include <cmath>
#include <sstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// External definitions and declarations for Arduino
extern size_t csi_buffer_size;  // External variable for buffer size
extern int csi_buffer_index;    // Current buffer index
extern int csi_buffer[];        // Buffer to store CSI data

std::mutex mutex;
char *project_type;

#define CSI_RAW 1
#define CSI_AMPLITUDE 0
#define CSI_PHASE 0

#define CSI_TYPE CSI_RAW

int x = 0;
int y = 0;
const char *current_AP = "";  // Currently connected AP

extern int rssi_value = 0;

std::vector<std::string> csi_data_vector;  // Vector to store CSI data
bool data_collected = false;  // Flag to indicate if data has been collected

// Structure to store CSI data
typedef struct {
    int8_t data[128];
    size_t length;
} CSI_Data;

CSI_Data csi_data;

size_t csi_data_len = 128;  // Length of CSI data

// Function to get CSI data with specified offset and length
int get_csi_data(size_t offset, size_t length, float *out_ptr) {
    if (offset + length > csi_data_len) {
        return -1;  // Return error if requested range exceeds available data
    }

    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = static_cast<float>(csi_data.data[offset + i]);
    }

    return 0;  // Return 0 if successful
}

// Function to retrieve location and update coordinates
void get_location(int &ub_x, int &ub_y) {
    x = ub_x;
    y = ub_y;
}

// Function to update AP connection status
void get_AP(const char* ACCESS_POINT) {
    std::lock_guard<std::mutex> lock(mutex);  // Lock mutex
    if (strcmp(current_AP, ACCESS_POINT) != 0) {
        current_AP = ACCESS_POINT;
        data_collected = false;  // Reset flag when AP changes
    }
}

// Callback function for WiFi CSI data
void _wifi_csi_cb(void *ctx, wifi_csi_info_t *data) {
    std::lock_guard<std::mutex> lock(mutex);  // Lock mutex

    if (!data_collected) {
        std::stringstream ss;

        wifi_csi_info_t d = data[0];
        char mac[20] = {0};
        sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5]);

        rssi_value = d.rx_ctrl.rssi;

        ss << "CSI packet: "
           << d.rx_ctrl.rssi << ", "
           << data->len << ", [";

        int data_len = 128;

        // Store CSI data in the global buffer
        if (csi_buffer_index + data_len <= csi_buffer_size) {
            for (int i = 0; i < data_len; i++) {
                csi_buffer[csi_buffer_index++] = data->buf[i];
            }
        } else {
            std::cerr << "ERROR: Buffer overflow\n";
        }

        int8_t *my_ptr;
        #if CSI_RAW
            my_ptr = data->buf;
            for (int i = 0; i < data_len; i++) {
                ss << (int) my_ptr[i] << " ";
            }
        #endif
        #if CSI_AMPLITUDE
            my_ptr = data->buf;
            for (int i = 0; i < data_len / 2; i++) {
                ss << (int) sqrt(pow(my_ptr[i * 2], 2) + pow(my_ptr[(i * 2) + 1], 2)) << " ";
            }
        #endif
        #if CSI_PHASE
            my_ptr = data->buf;
            for (int i = 0; i < data_len / 2; i++) {
                ss << (int) atan2(my_ptr[i*2], my_ptr[(i*2)+1]) << " ";
            }
        #endif
        ss << "]\n";

        csi_data_vector.push_back(ss.str());  // Store CSI data in vector

        printf(ss.str().c_str());
        fflush(stdout);

        data_collected = true;  // Set flag to true after data collection
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Simulate delay
}

// Function to print CSV header for CSI data
void _print_csi_csv_header() {
    char *header_str = (char *) "AP,rssi,real_timestamp,len,CSI_DATA\n";
    printf(header_str);
}

// Function to collect CSI data and store it in a vector
void collect_csi_data(wifi_csi_info_t *data) {
    std::lock_guard<std::mutex> lock(mutex);  // Lock mutex

    if (!data_collected) {
        std::stringstream ss;

        int data_len = 128;  // Adjust length as needed

        int8_t *my_ptr;
        #if CSI_RAW
            my_ptr = data->buf;
            for (int i = 0; i < data_len; i++) {
                ss << (int) my_ptr[i] << " ";
            }
        #endif
        #if CSI_AMPLITUDE
            my_ptr = data->buf;
            for (int i = 0; i < data_len / 2; i++) {
                ss << (int) sqrt(pow(my_ptr[i * 2], 2) + pow(my_ptr[(i * 2) + 1], 2)) << " ";
            }
        #endif
        #if CSI_PHASE
            my_ptr = data->buf;
            for (int i = 0; i < data_len / 2; i++) {
                ss << (int) atan2(my_ptr[i * 2], my_ptr[(i * 2) + 1]) << " ";
            }
        #endif
        ss << "]";

        csi_data_vector.push_back(ss.str());  // Store CSI data in vector
    }
}

// Function to print all stored CSI data
void print_stored_csi_data() {
    std::lock_guard<std::mutex> lock(mutex);  // Lock mutex
    for (const auto &data : csi_data_vector) {
        printf("%s\n", data.c_str());
    }
}

// Function to reset data collection flag
void reset_data_collected_flag() {
    std::lock_guard<std::mutex> lock(mutex);  // Lock mutex
    data_collected = false;
}

// Initialization function for CSI settings
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

    _print_csi_csv_header();
}

// Deinitialization function for CSI settings
void csi_deinit() {
    ESP_ERROR_CHECK(esp_wifi_set_csi(0));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(NULL, NULL));
}

#endif // ESP32_CSI_CSI_COMPONENT_H