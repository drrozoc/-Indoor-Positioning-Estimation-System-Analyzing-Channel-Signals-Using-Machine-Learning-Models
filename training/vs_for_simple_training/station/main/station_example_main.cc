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
#define CONFIG_ESP_MAXIMUM_RETRY 5     // Maximum number of retry attempts for WiFi connection
#define WIFI_CHANNEL 6                 // WiFi channel to connect to
#define WIFI_CONNECTED_BIT BIT0        // Bit flag for successful WiFi connection
#define WIFI_FAIL_BIT      BIT1        // Bit flag for WiFi connection failure

static const char *TAG = "wifi_station";  // Tag used for WiFi logs
static int s_retry_num = 0;               // WiFi retry attempt counter
static bool wifi_connected = false;       // WiFi connection status

// HTTP Event Handler
esp_err_t _http_event_handle(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:  // Event triggered when HTTP data is received
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (!real_time_set) {   // If time hasn't been set yet
                    char *data = (char *) malloc(evt->data_len + 1);
                    strncpy(data, (char *) evt->data, evt->data_len);
                    data[evt->data_len] = '\0';
                    time_set(data);  // Set time using the received data
                    free(data);
                }
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

// WiFi Event Handler
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();  // Start WiFi connection
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying connection to AP");
        } else {
            wifi_connected = false;
            ESP_LOGI(TAG, "Failed to connect to AP");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        wifi_connected = true;  // Update connection status
    }
}

// Checks if WiFi is connected
bool is_wifi_connected() {
    return wifi_connected;
}

// Initializes network interface
void init_func() {
    ESP_ERROR_CHECK(esp_netif_init());  // Initialize network
    ESP_ERROR_CHECK(esp_event_loop_create_default());  // Create default event loop
    esp_netif_create_default_wifi_sta();  // Create WiFi interface in station mode
}

// Initializes and connects to WiFi
void wifi_init_sta(const char *ssid_name, const char *pass_name) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers for WiFi connection
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    // Configure SSID and password
    wifi_config_t wifi_config = {};
    strlcpy((char *)wifi_config.sta.ssid, ssid_name, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, pass_name, sizeof(wifi_config.sta.password));
    wifi_config.sta.channel = WIFI_CHANNEL;

    // Set WiFi mode to station and start the connection
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    // Wait until WiFi is connected
    while (!wifi_connected) {
        vTaskDelay(500 / portTICK_PERIOD_MS);  // Wait for 500 ms before retrying
    }

    ESP_LOGI(TAG, "Connected to AP SSID:%s, Password:%s", ssid_name, pass_name);

    // Unregister events after successful connection
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
}

// Disconnects from WiFi
void wifi_disconnect() {
    ESP_ERROR_CHECK(esp_wifi_disconnect()); // Disconnect from AP
    ESP_ERROR_CHECK(esp_wifi_stop());       // Stop WiFi module
    wifi_connected = false; // Update connection status
    s_retry_num = 0; // Reset retry counter
}

extern "C" void app_main(void) {
    const char *ssid_list[] = { "AP4"};
    const char *pass_list[] = { "12345678"};

    int n_pack = 2500;  // Number of packets to send

    nvs_init();  // Initialize the NVS system (non-volatile storage)
    init_func(); // Initialize network
    wifi_init_sta(ssid_list[0], pass_list[0]);

    for (int j = 0; j < n_pack; j++) {
        // Clear the collected CSI data at the beginning of each loop
        all_csi_data.clear();
        csi_data_vector.clear();

        // Collect CSI data in 3 iterations
        for (int i = 0; i < 3; i++) {
            get_AP(ssid_list[0]);  // Get the specified access point
            ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
            ESP_LOGI(TAG, "SSID: %s", ssid_list[0]);
            ESP_LOGI(TAG, "Password: %s", pass_list[0]);

            wifi_init_sta(ssid_list[0], pass_list[0]);

            csi_init((char *)"STA");  // Initialize CSI in station mode

            socket_transmitter_sta_loop(&is_wifi_connected);  // Transmit CSI data
            vTaskDelay(100 / portTICK_PERIOD_MS);  // Wait between each collection

            
        }
    
    }
    ESP_LOGE(TAG, "<---------------------------------------- FINISHED ---------------------------------------->");
}
