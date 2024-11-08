#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_client.h"

// Custom Components
#include "../../_components/nvs_component.h"
#include "../../_components/sd_component.h"
#include "../../_components/csi_component.h"
#include "../../_components/time_component.h"
#include "../../_components/input_component.h"
#include "../../_components/sockets_component.h"

// Definitions
#define CONFIG_ESP_MAXIMUM_RETRY 5 // Max retries to connect to Wi-Fi
#define WIFI_CHANNEL 6 // Wi-Fi channel to connect to
#define WIFI_CONNECTED_BIT BIT0 // Wi-Fi connection status flag
#define WIFI_FAIL_BIT      BIT1 // Wi-Fi connection failure flag

static const char *TAG = "wifi_station"; // Tag for logging
static int s_retry_num = 0; // Retry counter for Wi-Fi connection
static bool wifi_connected = false; // Wi-Fi connection status flag

// HTTP Event Handler: Handles HTTP responses
esp_err_t _http_event_handle(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (!real_time_set) {
                    // If real-time data is not set, allocate memory and copy the response data
                    char *data = (char *) malloc(evt->data_len + 1);
                    strncpy(data, (char *) evt->data, evt->data_len);
                    data[evt->data_len] = '\0';
                    time_set(data); // Set the time using the response data
                    free(data); // Free the allocated memory
                }
            }
            break;
        default:
            break;
    }
    return ESP_OK; // Return OK after handling the event
}

// Wi-Fi Event Handler: Handles Wi-Fi connection and disconnection events
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect(); // Start Wi-Fi connection
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // If disconnected, attempt to reconnect up to the maximum retry limit
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Reconnecting to the AP");
        } else {
            wifi_connected = false;
            ESP_LOGI(TAG, "Failed to connect to the AP");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // If IP is obtained, set connection status to true
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP obtained: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0; // Reset the retry counter
        wifi_connected = true; // Set the connection flag to true
    }
}

// Function to check Wi-Fi connection status
bool is_wifi_connected() {
    return wifi_connected; // Return the Wi-Fi connection status
}

// Initializes network interface
void init_func() {
    ESP_ERROR_CHECK(esp_netif_init()); // Initialize network interface
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // Create the default event loop
    esp_netif_create_default_wifi_sta(); // Create Wi-Fi station interface
}

// Initializes Wi-Fi and connects to the provided network
void wifi_init_sta(const char *ssid_name, const char *pass_name) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); // Initialize Wi-Fi with the default configuration

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {};
    strlcpy((char *)wifi_config.sta.ssid, ssid_name, sizeof(wifi_config.sta.ssid)); // Set SSID
    strlcpy((char *)wifi_config.sta.password, pass_name, sizeof(wifi_config.sta.password)); // Set password
    wifi_config.sta.channel = WIFI_CHANNEL; // Set the Wi-Fi channel

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // Set Wi-Fi mode to station
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config)); // Apply Wi-Fi configuration
    ESP_ERROR_CHECK(esp_wifi_start()); // Start Wi-Fi

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    while (!wifi_connected) {
        vTaskDelay(50 / portTICK_PERIOD_MS); // Wait until connected
    }

    ESP_LOGI(TAG, "Connected to AP SSID:%s, password:%s", ssid_name, pass_name);

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
}

// Function to disconnect from Wi-Fi
void wifi_disconnect() {
    ESP_ERROR_CHECK(esp_wifi_disconnect()); // Disconnect from AP
    ESP_ERROR_CHECK(esp_wifi_stop()); // Stop Wi-Fi
    wifi_connected = false; // Update connection status
    s_retry_num = 0; // Reset the retry counter
}

extern "C" void app_main(void) {
    const char *ssid_list[] = { "AP4", "AP4", "AP4" }; // List of SSIDs to connect to
    const char *pass_list[] = { "12345678", "12345678", "12345678" }; // Corresponding passwords

    int n_pack = 5; // Number of connection attempts
    int vuelta = 0; // Counter for the number of connection rounds

    nvs_init(); // Initialize NVS
    init_func(); // Initialize the network interface
    for (int j = 0; j < n_pack; j++) {
        // Clear CSI data before each connection round
        all_csi_data.clear();
        csi_data_vector.clear();

        for (int i = 0; i < 3; i++) {
            get_AP(ssid_list[i]); // Get the current AP details
            ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
            ESP_LOGI(TAG, "SSID: %s", ssid_list[i]);
            ESP_LOGI(TAG, "Password: %s", pass_list[i]);

            wifi_init_sta(ssid_list[i], pass_list[i]); // Initialize and connect to the Wi-Fi network

            csi_init((char *)"STA"); // Initialize CSI (Channel State Information)

            socket_transmitter_sta_loop(&is_wifi_connected); // Start the loop to transmit data
            vTaskDelay(100 / portTICK_PERIOD_MS); // Wait for a short time before disconnecting

            wifi_disconnect(); // Disconnect from the Wi-Fi network
        }
        vuelta++;
        ESP_LOGE(TAG, "Round %d ---------------------------------------->", vuelta);

        // Mark that all APs have been collected
        mark_all_aps_collected();

        // Organize and display all collected CSI data
        collect_all_csi_data();

        // Reset the flag indicating data collection is complete
        reset_data_collected_flag();
    }
    ESP_LOGE(TAG, "<---------------------------------------- COMPLETED ---------------------------------------->");
}
