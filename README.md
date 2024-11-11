# Arduino Deployment Tutorial

This tutorial guides you through deploying an Arduino project for the XIAO-ESP32S3-Sense. You will need two specific libraries:

1. The library specified in the Seeed tutorial for XIAO-ESP32S3-Sense.
2. The standard library provided by Arduino.

### Required Libraries
- **XIAO-ESP32S3-Sense-main**: This is the fixed library specified in the Seeed tutorial.

## Step-by-Step Instructions

### Step 1: Replace the ESP-NN Folder
Locate the Edge library at the following path:

EXAMPLE OF ADDRESS: 
> `Habitacion_principal_11_hasta44_inferencing\src\edge-impulse-sdk\porting\espressif`

Replace the existing `ESP-NN` folder with the `ESP-NN` folder from the `XIAO-ESP32S3-Sense-main` library package. Also, for ease of use, it can be found `ESP-NN.zip` in the deployment folder of this repository.

Since the folder is compressed, you can extract it directly into the deployment directory to overwrite the original `ESP-NN` folder.

### Step 2: Move the Deployed Edge Library
Move the Edge deployment folder to the Arduino libraries directory:

EXAMPLE OF ADDRESS: 
> `C:\Users\pc\AppData\Local\Arduino15\libraries`

### Step 3: Compile in Arduino
After setting up the libraries, try compiling the code in the Arduino IDE. 

If you encounter errors, open the relevant files in a code editor, such as Visual Studio Code, to easily edit the necessary lines. This is generally more manageable than using a plain text editor because of the line numbering and syntax organization.

#### Editing Required in Source Files
The required modifications are in the following files:
- `depthwise_conv.cpp`
- `conv.cpp`

EXAMPLE OF ADDRESS:
> `C:\Users\pc\AppData\Local\Arduino15\libraries\CSI_DEPLOY1_inferencing\src\edge-impulse-sdk\tensorflow\lite\micro\kernels`

**In `depthwise_conv.cpp`, edit the following lines by removing the specified final numbers:**
- Line 1727
- Line 1731
- Line 1733
- Line 1836
- Line 1840
- Line 1842

**In `conv.cpp`, edit the following lines similarly:**
- Line 1789
- Line 1793
- Line 1795
- Line 1883
- Line 1887
- Line 1889

### Step 4: Compile and Restart Arduino
Finally, compile the code again in Arduino. If additional compilation errors appear, close the Arduino IDE, reopen it, and try compiling once more.
