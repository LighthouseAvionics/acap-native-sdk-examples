# I2C Device Detection ACAP Application

This example demonstrates how to build an ACAP application that scans I2C buses for connected devices. The application is similar to the `i2cdetect` utility from i2c-tools and logs detected devices to the system log.

## Overview

The I2C Detect application:
- Scans all available I2C buses (typically `/dev/i2c-0` through `/dev/i2c-9`)
- Probes I2C addresses from 0x03 to 0x77 (standard address range)
- Uses safe I2C_SMBUS quick write commands to detect device presence
- Logs a formatted grid showing detected devices at each address
- Reports results to the system log for easy viewing

This is useful for:
- Discovering I2C devices connected to your Axis camera
- Debugging I2C hardware connectivity
- Verifying I2C device addresses before communication
- Understanding the I2C topology of your system

## Hardware Requirements

- An Axis camera with I2C bus access
- This example was designed for the **Axis Q6225-LE PTZ** with up-to-date firmware
- I2C devices connected to the camera's I2C bus (optional, for testing)

**Note:** Not all Axis cameras expose I2C buses to user applications. The availability of I2C depends on the specific camera model and firmware version.

## Getting Started

Below is the structure and files used in this example:

```sh
i2c-detect
├── app
│   ├── i2c_detect.c
│   ├── LICENSE
│   ├── Makefile
│   └── manifest.json
├── Dockerfile
└── README.md
```

- **app/i2c_detect.c** - Main application that scans I2C buses
- **app/LICENSE** - Apache 2.0 license file
- **app/Makefile** - Build instructions for the ACAP application
- **app/manifest.json** - Application configuration and metadata
- **Dockerfile** - Docker build configuration with ACAP SDK
- **README.md** - This file with instructions

## How to Build the Application

Standing in your working directory run the following commands:

> [!NOTE]
>
> Depending on the network your local build machine is connected to, you may need to add proxy
> settings for Docker. See
> [Proxy in build time](https://developer.axis.com/acap/develop/proxy/#proxy-in-build-time).

### Build for aarch64 (64-bit ARM)

For the **Axis Q6225-LE PTZ** and most modern Axis cameras, use aarch64:

```sh
docker build --platform=linux/amd64 --build-arg ARCH=aarch64 --tag i2c-detect:1.0 .
```

### Build for armv7hf (32-bit ARM)

For older Axis cameras:

```sh
docker build --platform=linux/amd64 --build-arg ARCH=armv7hf --tag i2c-detect:1.0 .
```

### Extract the Built Application

Copy the result from the container image to a local directory called `build`:

```sh
docker cp $(docker create --platform=linux/amd64 i2c-detect:1.0):/opt/app ./build
```

The working directory now contains a build folder with the application package:

```sh
build
├── i2c_detect*
├── i2c_detect_1_0_0_aarch64.eap
├── i2c_detect_1_0_0_LICENSE.txt
└── ... (other build artifacts)
```

The `.eap` file is the ACAP application package that you'll install on your camera.

> [!NOTE]
>
> For detailed information on how to build, install, and run ACAP applications, refer to the official ACAP documentation: [Build, install, and run](https://developer.axis.com/acap/develop/build-install-run/).

## Install and Run the Application

### Installation Steps

1. Browse to the application page of the Axis device:
   ```
   http://<AXIS_DEVICE_IP>/index.html#apps
   ```

2. Click on the tab **Apps** in the device GUI

3. Enable **Allow unsigned apps** toggle

4. Click **(+ Add app)** button to upload the application file

5. Browse to the newly built ACAP application:
   - For Axis Q6225-LE PTZ: `i2c_detect_1_0_0_aarch64.eap`
   - For older cameras: `i2c_detect_1_0_0_armv7hf.eap`

6. Click **Install**

7. Run the application by enabling the **Start** switch

### Running the Application

The application runs once and scans all available I2C buses, then exits. Each time you start it, it will perform a fresh scan.

## Viewing the Results

Application log can be found directly at:
```
http://<AXIS_DEVICE_IP>/axis-cgi/admin/systemlog.cgi?appname=i2c_detect
```

Or by clicking on the **App log** link in the device GUI.

### Expected Output

If I2C buses are available and devices are detected:

```sh
----- Contents of SYSTEM_LOG for 'i2c_detect' -----

14:23:10.123 [ INFO ] i2c_detect[1234]: Starting I2C Detect application
14:23:10.124 [ INFO ] i2c_detect[1234]: Scanning I2C bus 0...
14:23:10.125 [ INFO ] i2c_detect[1234]:      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
14:23:10.126 [ INFO ] i2c_detect[1234]: 00:          -- -- -- -- -- -- -- -- -- -- -- -- --
14:23:10.127 [ INFO ] i2c_detect[1234]: 10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
14:23:10.128 [ INFO ] i2c_detect[1234]: 20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
14:23:10.129 [ INFO ] i2c_detect[1234]: 30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
14:23:10.130 [ INFO ] i2c_detect[1234]: 40: -- -- -- -- -- -- -- -- 48 -- -- -- -- -- -- --
14:23:10.131 [ INFO ] i2c_detect[1234]: 50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
14:23:10.132 [ INFO ] i2c_detect[1234]: 60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
14:23:10.133 [ INFO ] i2c_detect[1234]: 70: -- -- -- -- -- -- -- --
14:23:10.134 [ INFO ] i2c_detect[1234]: Found 1 device(s) on I2C bus 0
14:23:10.135 [ INFO ] i2c_detect[1234]: Scan complete. Found 1 I2C bus(es)
14:23:10.136 [ INFO ] i2c_detect[1234]: I2C Detect application finished
```

In this example, a device was detected at address `0x48` on I2C bus 0.

### If No I2C Buses Are Available

If the camera doesn't expose I2C buses or if kernel modules aren't loaded:

```sh
14:23:10.123 [ INFO ] i2c_detect[1234]: Starting I2C Detect application
14:23:10.140 [ WARNING ] i2c_detect[1234]: No I2C buses found on this system
14:23:10.141 [ INFO ] i2c_detect[1234]: Note: I2C functionality may require specific hardware or kernel modules
14:23:10.142 [ INFO ] i2c_detect[1234]: I2C Detect application finished
```

## Understanding the Output

The output grid shows I2C addresses in hexadecimal:
- Each row represents 16 addresses (e.g., row `40:` shows addresses 0x40-0x4F)
- `--` indicates no device detected at that address
- `48` (for example) indicates a device was detected at address 0x48
- Addresses 0x00-0x02 and 0x78-0x7F are reserved and not scanned

## Troubleshooting

### No I2C Buses Found

This can happen if:
1. The camera model doesn't expose I2C buses to user applications
2. I2C kernel modules are not loaded
3. Permissions prevent access to `/dev/i2c-*` devices

### Permission Denied Errors

If you see permission errors in the log, the ACAP application may need additional capabilities. You can modify the `manifest.json` to request device access.

### Application Architecture

For the Axis Q6225-LE PTZ, ensure you're building with `ARCH=aarch64`. Check your camera's architecture in the device settings.

## Technical Details

### I2C Probing Method

The application uses `I2C_SMBUS_WRITE_QUICK` commands to probe addresses. This is the safest probing method as it:
- Doesn't read or write actual data
- Minimizes the risk of corrupting device state
- Is supported by most I2C devices

### Address Range

The application scans addresses 0x03 to 0x77:
- 0x00-0x02: Reserved addresses
- 0x03-0x77: Standard device address range
- 0x78-0x7F: Reserved addresses

### Multiple Bus Support

The application automatically scans up to 10 I2C buses (`/dev/i2c-0` through `/dev/i2c-9`). Most cameras will have 0-2 buses.

## Further Development

This example can be extended to:
- Read data from specific I2C devices
- Periodically monitor I2C bus status
- Communicate with specific sensors or peripherals
- Integrate with other ACAP features (events, parameters, web interface)

## License

**[Apache License 2.0](../LICENSE)**
