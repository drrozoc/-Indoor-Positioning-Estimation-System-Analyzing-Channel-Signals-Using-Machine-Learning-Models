#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include <esp_http_server.h>

// Define a global array to store intermediate data
#define DATA_SIZE 128
float intermediate_buffer[DATA_SIZE]; // Buffer to hold the intermediate data
int current_index = 0; // Current index for buffer population

// Function declaration for collecting CSI data
void collect_csi_data(wifi_csi_info_t *data);

// Function to transmit data using a socket, in a loop
void socket_transmitter_sta_loop(bool (*is_wifi_connected)()) {
    int socket_fd = -1; // Socket file descriptor
    int num_packages_sent = 0; // Number of packages sent so far
    bool flag = true; // Control flag for loop execution

    // Define a variable to store CSI data
    wifi_csi_info_t csi_info;
    
    // Define the destination IP and port for sending data
    char *ip = (char *) "192.168.4.1"; // Target IP address
    struct sockaddr_in caddr; // Socket address structure
    caddr.sin_family = AF_INET; // Address family (IPv4)
    caddr.sin_port = htons(2223); // Port number (converted to network byte order)
    
    // Main loop for the socket transmitter
    while (flag) {
        // Check if the WiFi is connected
        if (!is_wifi_connected()) {
            printf("wifi not connected. waiting...\n");
            vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay and check again
            continue;
        }

        // Print message once the WiFi connection is established
        printf("initial wifi connection established.\n");

        // Convert IP address string to network format
        if (inet_aton(ip, &caddr.sin_addr) == 0) {
            printf("ERROR: inet_aton\n");
            continue;
        }

        // Create a UDP socket
        socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
        if (socket_fd == -1) {
            printf("ERROR: Socket creation error [%s]\n", strerror(errno));
            continue;
        }

        // Connect the socket to the destination address
        if (connect(socket_fd, (const struct sockaddr *) &caddr, sizeof(struct sockaddr)) == -1) {
            printf("ERROR: socket connection error [%s]\n", strerror(errno));
            close(socket_fd); // Close the socket and try again
            continue;
        }

        // Print message when the socket is ready to send data
        printf("sending frames.\n");
        bool flag2 = true; // Inner loop flag

        // Inner loop to continuously send data
        while (flag2) {
            // Check if the WiFi is still connected
            if (!is_wifi_connected()) {
                printf("ERROR: wifi is not connected\n");
                break; // Exit if WiFi is disconnected
            }

            // Call the function to collect CSI data (this should be implemented elsewhere)
            collect_csi_data(&csi_info);

            // Populate the intermediate buffer with sample data (replace with actual data)
            for (int i = 0; i < DATA_SIZE; i++) {
                intermediate_buffer[i] = i; // Example: filling with incremental values
            }

            // Send the populated data buffer through the socket
            if (sendto(socket_fd, intermediate_buffer, sizeof(intermediate_buffer), 0, (const struct sockaddr *) &caddr, sizeof(caddr)) != sizeof(intermediate_buffer)) {
                printf("ERROR: Failed to send data [%s]\n", strerror(errno));
                vTaskDelay(1); // Wait before retrying
                continue;
            }
            num_packages_sent++; // Increment the sent package counter
            
            // Handle packet rate configuration if defined
            #if defined CONFIG_PACKET_RATE && (CONFIG_PACKET_RATE > 0)
                double end_time = get_steady_clock_timestamp();
                //lag = end_time - start_time; // Calculate the lag (optional)
                double wait_duration = (1000.0 / CONFIG_PACKET_RATE) - lag; // Calculate the waiting time based on packet rate
                int w = floor(wait_duration); // Convert the wait duration to integer milliseconds
                vTaskDelay(w); // Delay according to the packet rate
            #else
                vTaskDelay(10); // Default delay if no rate configuration is provided
            #endif
            
            // Set flag2 to false to exit the inner loop after sending one package
            flag2 = false;
        }

        // Close the socket after sending the required data
        close(socket_fd);
        flag = false; // Exit the outer loop after the data has been sent
    }
}
