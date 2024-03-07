#include <pigpio.h>
#include <iostream>
#include <cstring> // For memset
#include <map>     // For response mapping
#include <chrono>  // For delay
#include <thread>  // For delay
#include <csignal> // For signal handling
#include<vector>

using namespace std;

// Global variable to indicate if the program should continue running
bool shouldRun = true;

// Map to store responses based on the command sent by the master
map<uint8_t, vector<uint8_t>> responseMap = {
    {0x75, {0xF8}},        // If master writes 0x75, respond with 0xF8
    {0x3A, {0x05}},        // If master writes 0x3A, respond with 0x05
    {0x41, {0x00, 0x00}},  // If master writes 0x41, respond with 0x09 0xF0
    {0x19, {0x13}},        // If master writes 0x19, respond with 0x13
    {0x1A, {0x05}},        // If master writes 0x1A, respond with 0x05
    {0x1B, {0x00}},        // If master writes 0x1B, respond with 0x00
    {0x1C, {0x10}},        // If master writes 0x1C, respond with 0x10
    {0x1D, {0x05}},        // If master writes 0x1D, respond with 0x05
    {0x1D, {0x00}},        // If master writes 0x1D, respond with 0x05
    {0x37, {0x20}},        // If master writes 0x37, respond with 0x20
    {0x38, {0x00}},        // If master writes 0x38, respond with 0x00
    {0x1F, {0x64}},        // If master writes 0x1F, respond with 0x0B
};

// Signal handler function to handle Ctrl+C
void signalHandler(int signum) {
    cout << "Interrupt signal (" << signum << ") received. Exiting..." << endl;
    shouldRun = false;
}

int main() {
    // Set up signal handling for Ctrl+C
    signal(SIGINT, signalHandler);

    // Initialize GPIO
    if (gpioInitialise() < 0) {
        cerr << "Error: GPIO initialization failed." << endl;
        return -1;
    }

    // Set I2C slave Address to 0x68 and increase clock speed to 400 kHz
    bsc_xfer_t xfer;
    xfer.control = (0x68 << 16) | 0x305 | (1 << 15); // Enable clock stretching

    int status = 0;

    // Continuously read data from the I2C slave
    while (shouldRun) {
        // Perform I2C transfer
        status = bscXfer(&xfer);
        if (status >= 0) {
            // Check if there is data available to read
            if (xfer.rxCnt > 0) {
                // Print received data
                cout << "Received data (hex): ";
                for (int i = 0; i < xfer.rxCnt; ++i) {
                    cout << hex << uppercase << static_cast<int>(xfer.rxBuf[i]) << " ";
                }
                cout << endl;

                // Check for different values written by the master
                if (xfer.rxCnt > 0) {
                    cout << "Master wrote: " << hex << static_cast<int>(xfer.rxBuf[0]) << endl;

                    // Get the response from the map based on the command
                    auto it = responseMap.find(xfer.rxBuf[0]);
                    if (it != responseMap.end()) {
                        vector<uint8_t>& response = it->second;
                        // Set the response in xfer.txBuf
                        xfer.txCnt = response.size();
                        for (size_t i = 0; i < response.size(); ++i) {
                            xfer.txBuf[i] = response[i];
                            cout << "sent response 0x" << hex << static_cast<int>(response[i]) << endl;
                        }

                        // Perform I2C transfer to send response
                        status = bscXfer(&xfer);
                        if (status < 0) {
                            cerr << "Error: Failed to send response. Error code: " << status << endl;
                            break;
                        }
                        cout << "Response sent successfully." << endl;
                    } else {
                        cout << "No response for value: " << hex << static_cast<int>(xfer.rxBuf[0]) << endl;
                    }
                }

                // Clear the receive buffer
                memset(xfer.rxBuf, 0, sizeof(xfer.rxBuf));
                xfer.rxCnt = 0;
            }
        } else {
            // Print error if I2C transfer fails
            cerr << "Error: I2C transfer failed. Error code: " << status << endl;
            break;
        }
    }

    gpioTerminate();
    return 1;
}
