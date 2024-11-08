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

// Define a global array to store intermediate CSI data
#define DATA_SIZE 128
float intermediate_buffer[DATA_SIZE];  // Buffer for holding CSI data
int current_index = 0;  // Current index to keep track of data storage

// Declaration of the function to collect CSI data
void collect_csi_data(wifi_csi_info_t *data);

void socket_transmitter_sta_loop(bool (*is_wifi_connected)()) {
    int socket_fd = -1;  // Socket file descriptor
    int num_packages_sent = 0;  // Number of packages sent
    bool flag = true;  // Control flag for main loop

    // Define a variable to store CSI data
    wifi_csi_info_t csi_info;
    
    // Define the target IP address and port for communication
    char *ip = (char *) "192.168.4.1";  // IP address of the receiver
    struct sockaddr_in caddr;
    caddr.sin_family = AF_INET;  // Address family is IPv4
    caddr.sin_port = htons(2223);  // Port number in network byte order
    
    // Main loop that keeps sending CSI data
    while (flag) {
        // Check if the device is connected to Wi-Fi
        if (!is_wifi_connected()) {
            printf("wifi not connected. waiting...\n");  // Wait if Wi-Fi is not connected
            vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay for 1 second
            continue;
        }
        printf("initial wifi connection established.\n");
        
        // Convert the IP address string to in_addr format
        if (inet_aton(ip, &caddr.sin_addr) == 0) {
            printf("ERROR: inet_aton\n");
            continue;
        }

        // Create a UDP socket
        socket_fd = socket(PF_INET, SOCK_DGRAM, 0);  // UDP socket creation
        if (socket_fd == -1) {
            printf("ERROR: Socket creation error [%s]\n", strerror(errno));  // Handle socket creation error
            continue;
        }
        
        // Connect the socket to the specified address and port
        if (connect(socket_fd, (const struct sockaddr *) &caddr, sizeof(struct sockaddr)) == -1) {
            printf("ERROR: socket connection error [%s]\n", strerror(errno));  // Handle connection error
            close(socket_fd);  // Close the socket
            continue;
        }

        printf("sending frames.\n");
        bool flag2 = true;  // Control flag for inner loop
        
        // Loop to keep sending the data
        while (flag2) {
            // Check Wi-Fi connection again
            if (!is_wifi_connected()) {
                printf("ERROR: wifi is not connected\n");
                break;  // Exit the loop if Wi-Fi is not connected
            }

            // Call the function to collect CSI data
            collect_csi_data(&csi_info);  // Function to collect CSI data
            
            // Fill the intermediate buffer with simulated data (replace with actual CSI data)
            for (int i = 0; i < DATA_SIZE; i++) {
                intermediate_buffer[i] = i;  // Example: populate buffer with incremental values
            }

            // Send the data in the intermediate buffer to the target address
            if (sendto(socket_fd, intermediate_buffer, sizeof(intermediate_buffer), 0, (const struct sockaddr *) &caddr, sizeof(caddr)) != sizeof(intermediate_buffer)) {
                printf("ERROR: Failed to send data [%s]\n", strerror(errno));  // Handle send error
                vTaskDelay(1);  // Small delay before retrying
                continue;
            }
            num_packages_sent++;  // Increment the number of packages sent

            // Packet sending rate control (if configured)
            #if defined CONFIG_PACKET_RATE && (CONFIG_PACKET_RATE > 0)
                double end_time = get_steady_clock_timestamp();  // Get timestamp after sending packet
                double wait_duration = (1000.0 / CONFIG_PACKET_RATE) - lag;  // Calculate wait time
                int w = floor(wait_duration);  // Wait for the calculated duration
                vTaskDelay(w);  // Delay before sending the next packet
            #else
                vTaskDelay(10);  // Default delay if no rate control is configured
            #endif
            
            flag2 = false;  // Exit the loop after sending one package (this can be modified as per the logic)
        }

        close(socket_fd);  // Close the socket after sending the data
        flag = false;  // Exit the main loop after sending the required number of packages
    }
}
