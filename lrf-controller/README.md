# LRF Controller

This example demonstrates how to control a Laser Rangefinder (LRF) over I2C through HTTP requests using an ACAP application. The application acts as a reverse proxy that allows HTTP clients to communicate with an LRF device connected to an Axis camera's I2C bus.

## Use Case

Replace a stock Axis IR board with a custom PCB containing a Laser Rangefinder. External clients send HTTP requests to the Axis camera, which routes them through Apache reverse proxy to a CivetWeb server running in this ACAP application. The ACAP then communicates with the LRF over I2C and returns responses as JSON.

## APIs Used

- **I2C Linux Kernel Interface** - I2C SMBUS operations for hardware communication
- **CivetWeb** - Embedded HTTP server for handling requests
- **Jansson** - JSON encoding and decoding
- **GLib** - Data types and utilities

## File Structure

```
lrf-controller/
├── app/
│   ├── lrf_controller.c    # Main application with CivetWeb server and request handlers
│   ├── i2c_lrf.h           # I2C LRF abstraction header
│   ├── i2c_lrf.c           # I2C LRF implementation
│   ├── LICENSE             # Apache 2.0 License
│   ├── Makefile            # Build configuration
│   └── manifest.json       # ACAP manifest with reverse proxy configuration
├── Dockerfile              # Build container
└── README.md               # This file
```

## How It Works

1. The ACAP application starts a CivetWeb server on port 8080
2. The manifest.json configures Apache reverse proxy to route `/local/lrf_controller/lrf/*` to `http://localhost:8080`
3. The application opens an I2C connection to the LRF device at startup
4. HTTP requests are handled by endpoint-specific handlers that:
   - Parse request data (JSON for POST requests)
   - Communicate with the LRF via I2C
   - Return JSON responses

## Build Instructions

The application can be built for different architectures using Docker:

### For ARM32 (armv7hf)

```bash
docker build --build-arg ARCH=armv7hf -t lrf-controller:armv7hf .
docker cp $(docker create lrf-controller:armv7hf):/opt/app/lrf_controller_1_0_0_armv7hf.eap .
```

### For ARM64 (aarch64)

```bash
docker build --build-arg ARCH=aarch64 -t lrf-controller:aarch64 .
docker cp $(docker create lrf-controller:aarch64):/opt/app/lrf_controller_1_0_0_aarch64.eap .
```

## Installation

1. Upload the `.eap` file to your Axis camera through the web interface
2. Navigate to Apps in the camera's web interface
3. Install the uploaded ACAP application
4. Start the application

## Configuration

By default, the application uses:
- **I2C Bus**: 0
- **LRF Address**: 0x48
- **Server Port**: 8080

To modify these defaults, edit the `#define` statements in `lrf_controller.c`:

```c
#define I2C_BUS 0
#define LRF_ADDR 0x48
#define PORT "8080"
```

## API Documentation

All endpoints require admin authentication and are accessed through the reverse proxy path.

### GET /distance

Reads the current distance measurement from the LRF.

**Request:**
```bash
curl -u admin:password http://<CAMERA_IP>/local/lrf_controller/lrf/distance
```

**Response (Success):**
```json
{
  "distance_m": 12.345,
  "status": "ok"
}
```

**Response (Error):**
```json
{
  "error": "Failed to read distance from LRF"
}
```

### POST /command

Sends a raw command byte to the LRF and reads the response.

**Request:**
```bash
curl -u admin:password -X POST \
  -H "Content-Type: application/json" \
  -d '{"cmd": 16}' \
  http://<CAMERA_IP>/local/lrf_controller/lrf/command
```

**Request Body:**
- `cmd` (integer): Command byte to send (0-255)

**Response (Success):**
```json
{
  "status": "ok",
  "response": [0, 1, 2, 3, ...]
}
```

The `response` array contains 32 raw bytes read from the LRF after sending the command.

**Response (Error):**
```json
{
  "error": "Failed to send command to LRF"
}
```

### GET /status

Returns the connection status of the LRF device.

**Request:**
```bash
curl -u admin:password http://<CAMERA_IP>/local/lrf_controller/lrf/status
```

**Response:**
```json
{
  "connected": true,
  "bus": 0,
  "addr": "0x48"
}
```

## Troubleshooting

### Application starts but requests fail

Check the system log for connection errors:
```bash
tail -f /var/log/syslog | grep lrf_controller
```

Common causes:
- I2C bus does not exist (`/dev/i2c-0` not found)
- Wrong I2C address configured
- LRF device not physically connected
- I2C permissions issue

### I2C bus not found

Verify available I2C buses on your camera:
```bash
ls -l /dev/i2c-*
```

If no I2C devices are found, your camera may not support I2C or requires kernel module loading.

### Permission denied when accessing I2C

The manifest.json includes `"groups": ["gpio"]` which should provide I2C access. If issues persist:
1. Verify the ACAP has proper permissions in manifest.json
2. Check that the gpio group has access to `/dev/i2c-*` devices

### Wrong distance readings

The default implementation assumes:
- Distance data is 4 bytes (32-bit big-endian integer)
- Values are in millimeters
- Data is read from register 0x00

**You must adapt the I2C protocol in `i2c_lrf.c` to match your specific LRF device.**

## Customization

This example is a template and must be adapted to your specific LRF hardware:

1. **I2C Protocol**: Modify `lrf_read_distance()` and `lrf_send_command()` in `i2c_lrf.c` to match your LRF's I2C protocol
2. **Response Format**: Update the data parsing logic to match your device's response format
3. **Additional Endpoints**: Add more handlers in `lrf_controller.c` for device-specific features
4. **Error Handling**: Enhance error detection for your specific use case

## License

Apache License 2.0
