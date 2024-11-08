#ifndef SOCKET_COMPONENT_H
#define SOCKET_COMPONENT_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include <esp_http_server.h>
#include "time_component.h"
#include "csi_component.h"
#include <WiFi.h>

extern bool send_csi; // Declare the external variable send_csi for use in this file

char *data = (char *) "1\n"; // Data to be sent over the socket (a simple string "1\n")

// Function to manage the socket transmission in station mode
void socket_transmitter_sta_loop(bool (*is_wifi_connected)()) {
    int socket_fd = -1; // File descriptor for the socket
    int num_packages_sent = 0; // Counter to track the number of data packages sent
    int num_packages_to_send = 1; // Desired number of data packages to send
    bool flag = 1; // Flag to control the transmission loop

    while (flag) {
        close(socket_fd); // Close any existing socket before reinitializing
        char *ip = (char *) "192.168.4.1"; // IP address of the target device
        struct sockaddr_in caddr; // Structure to store the socket address
        caddr.sin_family = AF_INET; // Address family is IPv4
        caddr.sin_port = htons(2223); // Port to communicate with the target device

        // Wait until WiFi is connected
        while (!is_wifi_connected()) {
            printf("wifi not connected. waiting...\n");
            vTaskDelay(1000 / portTICK_PERIOD_MS); // Wait for 1 second
        }
        printf("initial wifi connection established.\n"); // WiFi connection established

        // Convert the target IP address to a socket address
        if (inet_aton(ip, &caddr.sin_addr) == 0) {
            printf("ERROR: inet_aton\n");
            continue; // If there is an error converting the IP, retry
        }

        // Create the socket for communication
        socket_fd = socket(PF_INET, SOCK_DGRAM, 0); // UDP socket creation
        if (socket_fd == -1) {
            printf("ERROR: Socket creation error [%s]\n", strerror(errno));
            continue; // Retry if socket creation fails
        }

        // Connect the socket to the target IP and port
        if (connect(socket_fd, (const struct sockaddr *) &caddr, sizeof(struct sockaddr)) == -1) {
            printf("ERROR: socket connection error [%s]\n", strerror(errno));
            continue; // Retry if socket connection fails
        }

        printf("sending frames.\n"); // Indicate that the transmission process has started
        double lag = 0.0; // Variable to track the transmission lag

        // Loop to send the data packages while the conditions are met
        while (send_csi && num_packages_sent < num_packages_to_send) { // Check if data should be sent
            double start_time = get_steady_clock_timestamp(); // Record the start time

            if (!is_wifi_connected()) {
                printf("ERROR: wifi is not connected\n");
                break; // Exit the loop if WiFi is disconnected
            }

            // Send the data over the socket
            if (sendto(socket_fd, &data, strlen(data), 0, (const struct sockaddr *) &caddr, sizeof(caddr)) !=
                strlen(data)) {
                vTaskDelay(1); // Retry after a short delay if the send fails
                continue;
            }

            num_packages_sent++; // Increment the counter for each successful transmission

            // If a custom packet rate is defined, adjust the transmission rate
#if defined CONFIG_PACKET_RATE && (CONFIG_PACKET_RATE > 0)
            double wait_duration = (1000.0 / CONFIG_PACKET_RATE) - lag; // Calculate the wait time
            int w = floor(wait_duration); // Round down to the nearest whole number
            vTaskDelay(w); // Delay before sending the next package
#else
            vTaskDelay(10); // Default delay to limit the TX rate (approximately 100 per second)
#endif

            double end_time = get_steady_clock_timestamp(); // Record the end time
            lag = end_time - start_time; // Update the lag based on the elapsed time
        }

        close(socket_fd); // Close the socket after sending the data
        if (num_packages_sent == num_packages_to_send) { 
            flag = 0; // Stop the loop if the desired number of packages has been sent
        }

        // Deinitialize CSI after the data package has been sent
        csi_deinit();
    }
}

#endif // SOCKET_COMPONENT_H