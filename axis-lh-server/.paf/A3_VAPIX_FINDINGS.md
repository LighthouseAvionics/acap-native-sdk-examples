# A3 Findings: VAPIX API Integration Specification

## Executive Summary
VAPIX provides two temperature APIs (temperaturecontrol.cgi returns numeric values, param.cgi for Properties), device info via basicdeviceinfo.cgi JSON API (SerialNumber, Version, ProdNbr), and ACAP applications can authenticate via localhost virtual host 127.0.0.12 using D-Bus acquired credentials. Recommend 60-second cache TTL for temperature, 300-second for device info, with fallback to /proc metrics if VAPIX unavailable.

## Key Findings
1. **Temperature Endpoint**: Available via temperaturecontrol.cgi (preferred) or param.cgi, returns numeric °C, requires 60s caching
2. **Device Info Endpoint**: Available via basicdeviceinfo.cgi JSON API, returns model/serial/firmware in structured format
3. **Authentication**: ACAP apps use D-Bus GetCredentials method to obtain admin credentials for 127.0.0.12 virtual host (no external auth needed)
4. **Caching Strategy**: 60-second TTL for temperature, 300-second for device info to minimize VAPIX load
5. **Error Handling**: Serve stale cache on VAPIX failure, fallback to /proc metrics if no cache available

## Detailed Analysis

### Endpoint 1: Temperature Monitoring (RECOMMENDED)

**API Endpoint:**
```
GET /axis-cgi/temperaturecontrol.cgi?device=sensor&id=<id>&action=query&temperatureunit=celsius
```

**Authentication:**
- Method: HTTP Basic Auth via D-Bus acquired credentials
- Virtual Host: `http://127.0.0.12` (ACAP localhost access)
- Security Level: Operator
- ACAP apps acquire credentials via D-Bus method `com.axis.HTTPConf1.VAPIXServiceAccounts1.GetCredentials`
- Credentials valid only on 127.0.0.12, not stored to disk, memory-only

**Request Example:**
```bash
# External access (testing from development machine)
curl --anyauth -u admin:password \
  'http://192.168.30.15/axis-cgi/temperaturecontrol.cgi?device=sensor&id=2&action=query&temperatureunit=celsius'

# ACAP localhost access (from within camera)
curl --anyauth -u $(DBUS_CREDENTIALS) \
  'http://127.0.0.12/axis-cgi/temperaturecontrol.cgi?device=sensor&id=2&action=query'
```

**Response Format:**
```
46.71
```

**Response Type:** `text/html`, HTTP 200 OK, numeric value only (no JSON wrapper)

**Parsing Strategy:**
```c
// Parse response to extract temperature value
double parse_temperature_response(const char* response) {
    if (!response || response[0] == '\0') {
        return -1.0;
    }

    double temperature;
    if (sscanf(response, "%lf", &temperature) != 1) {
        syslog(LOG_ERR, "Failed to parse temperature response: %s", response);
        return -1.0;
    }

    // Sanity check: camera operating range typically -40°C to +70°C
    if (temperature < -50.0 || temperature > 100.0) {
        syslog(LOG_WARNING, "Temperature out of expected range: %.2f°C", temperature);
    }

    return temperature;
}
```

**Caching Requirements:**
- Cache TTL: 60 seconds (temperature changes gradually)
- Cache on first successful read
- Refresh cache on expiry
- Serve stale cache if API call fails (prevents gaps in monitoring)

**Error Codes:**
- HTTP 200: Success
- HTTP 400: Invalid temperature unit or sensor ID
- HTTP 401: Authentication failure
- Timeout: VAPIX service overloaded

---

### Endpoint 1-ALT: Temperature Monitoring via param.cgi (LEGACY)

**Note:** This endpoint is legacy. Use temperaturecontrol.cgi instead for cleaner numeric responses.

**API Endpoint:**
```
GET /axis-cgi/param.cgi?action=list&group=Properties.System.Temperature
```

**Response Format:**
```
root.Properties.System.Temperature=45.0
```

**Parsing Strategy:**
```c
double parse_param_temperature_response(const char* response) {
    const char* temp_str = strstr(response, "Temperature=");
    if (!temp_str) {
        syslog(LOG_ERR, "Temperature parameter not found in response");
        return -1.0;
    }

    double temperature;
    if (sscanf(temp_str, "Temperature=%lf", &temperature) != 1) {
        return -1.0;
    }

    return temperature;
}
```

---

### Endpoint 2: Device Information (RECOMMENDED)

**API Endpoint:**
```
POST /axis-cgi/basicdeviceinfo.cgi
```

**Request Format:** JSON POST body
```json
{
  "apiVersion": "1.0",
  "context": "health-monitor",
  "method": "getAllProperties"
}
```

**Authentication:**
- Same as temperature endpoint (D-Bus credentials, 127.0.0.12)
- Security Level: Operator

**Request Example:**
```bash
# External access (testing)
curl -X POST \
  --anyauth -u admin:password \
  -H "Content-Type: application/json" \
  -d '{"apiVersion":"1.0","context":"test","method":"getAllProperties"}' \
  'http://192.168.30.15/axis-cgi/basicdeviceinfo.cgi'

# ACAP localhost access
curl -X POST \
  --anyauth -u $(DBUS_CREDENTIALS) \
  -H "Content-Type: application/json" \
  -d '{"apiVersion":"1.0","context":"health-monitor","method":"getAllProperties"}' \
  'http://127.0.0.12/axis-cgi/basicdeviceinfo.cgi'
```

**Response Format:**
```json
{
  "apiVersion": "1.0",
  "context": "health-monitor",
  "data": {
    "propertyList": {
      "SerialNumber": "ACCC8E78B977",
      "Version": "8.20.1",
      "ProdNbr": "Q3505 Mk II",
      "ProdFullName": "AXIS Q3505 Mk II Network Camera",
      "ProdType": "Network Camera",
      "Architecture": "armv7hf",
      "Soc": "Ambarella S5L",
      "BuildDate": "Feb 15 2024"
    }
  }
}
```

**Fields to Extract:**
- `SerialNumber`: Unique camera identifier (e.g., "ACCC8E78B977")
- `Version`: Firmware version string (e.g., "8.20.1")
- `ProdNbr`: Product model number (e.g., "Q3505 Mk II")
- `ProdFullName`: Full product name (optional, for display)
- `Architecture`: Hardware architecture (e.g., "armv7hf")
- `Soc`: System on Chip (e.g., "Ambarella S5L")

**Parsing Strategy:**
```c
#include <jansson.h> // JSON library included in ACAP SDK

typedef struct {
    char serial_number[64];
    char firmware_version[64];
    char model[64];
    char architecture[32];
    char soc[64];
} DeviceInfo;

int parse_device_info_response(const char* response, DeviceInfo* info) {
    if (!response || !info) return -1;

    json_error_t error;
    json_t* root = json_loads(response, 0, &error);
    if (!root) {
        syslog(LOG_ERR, "JSON parse error: %s", error.text);
        return -1;
    }

    json_t* data = json_object_get(root, "data");
    if (!data) {
        json_decref(root);
        return -1;
    }

    json_t* property_list = json_object_get(data, "propertyList");
    if (!property_list) {
        json_decref(root);
        return -1;
    }

    // Extract SerialNumber
    json_t* serial = json_object_get(property_list, "SerialNumber");
    if (json_is_string(serial)) {
        strncpy(info->serial_number, json_string_value(serial), sizeof(info->serial_number) - 1);
    }

    // Extract Version (firmware)
    json_t* version = json_object_get(property_list, "Version");
    if (json_is_string(version)) {
        strncpy(info->firmware_version, json_string_value(version), sizeof(info->firmware_version) - 1);
    }

    // Extract ProdNbr (model)
    json_t* model = json_object_get(property_list, "ProdNbr");
    if (json_is_string(model)) {
        strncpy(info->model, json_string_value(model), sizeof(info->model) - 1);
    }

    // Extract Architecture
    json_t* arch = json_object_get(property_list, "Architecture");
    if (json_is_string(arch)) {
        strncpy(info->architecture, json_string_value(arch), sizeof(info->architecture) - 1);
    }

    // Extract Soc
    json_t* soc = json_object_get(property_list, "Soc");
    if (json_is_string(soc)) {
        strncpy(info->soc, json_string_value(soc), sizeof(info->soc) - 1);
    }

    json_decref(root);
    return 0;
}
```

**Caching Requirements:**
- Cache TTL: 300 seconds (5 minutes) - device info rarely changes
- Read once on startup
- Refresh periodically for firmware update detection
- No need to serve stale cache (device info is static)

---

### Endpoint 3: Storage Information (Optional)

**Status:** Not documented in standard VAPIX API
**Alternative:** Check `/proc/mounts` for SD card presence or use param.cgi to query storage parameters if available

**Note:** SD card monitoring may not be applicable for all PTZ camera models. Defer implementation until device info confirms SD card presence.

---

## VAPIX Client Design

### D-Bus Credential Acquisition (ACAP Apps)

**Manifest Declaration:**
Add to `manifest.json`:
```json
{
  "resources": {
    "dbus": {
      "requiredMethods": [
        "com.axis.HTTPConf1.VAPIXServiceAccounts1.GetCredentials"
      ]
    }
  }
}
```

**D-Bus Call Implementation:**
```c
#include <glib.h>
#include <gio/gio.h>

typedef struct {
    char username[128];
    char password[256];
} VAPIXCredentials;

int acquire_vapix_credentials(VAPIXCredentials* creds) {
    GError* error = NULL;
    GDBusConnection* conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!conn) {
        syslog(LOG_ERR, "Failed to connect to D-Bus: %s", error->message);
        g_error_free(error);
        return -1;
    }

    GVariant* result = g_dbus_connection_call_sync(
        conn,
        "com.axis.HTTPConf1",
        "/com/axis/HTTPConf1/VAPIXServiceAccounts1",
        "com.axis.HTTPConf1.VAPIXServiceAccounts1",
        "GetCredentials",
        g_variant_new("(s)", "health-monitor"), // Username parameter
        G_VARIANT_TYPE("(ss)"), // Return type: (username, password)
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (!result) {
        syslog(LOG_ERR, "Failed to get VAPIX credentials: %s", error->message);
        g_error_free(error);
        g_object_unref(conn);
        return -1;
    }

    const char* username;
    const char* password;
    g_variant_get(result, "(&s&s)", &username, &password);

    strncpy(creds->username, username, sizeof(creds->username) - 1);
    strncpy(creds->password, password, sizeof(creds->password) - 1);

    g_variant_unref(result);
    g_object_unref(conn);

    syslog(LOG_INFO, "Acquired VAPIX credentials for user: %s", creds->username);
    return 0;
}
```

**Security Notes:**
- Credentials are valid ONLY on 127.0.0.12 virtual host
- Never write credentials to disk
- Re-acquire on each application restart
- Maximum 200 service accounts per device
- Credentials invalidated on device reboot

---

### HTTP Client Implementation

**Library Choice:** libcurl (lightweight, included in ACAP SDK)

**Function Signatures:**
```c
#include <curl/curl.h>

typedef struct {
    char* data;
    size_t size;
} MemoryBuffer;

// Initialize VAPIX client (acquire credentials)
int vapix_init(void);

// Cleanup VAPIX client
void vapix_cleanup(void);

// Fetch temperature
int vapix_get_temperature(double* temperature);

// Fetch device info
int vapix_get_device_info(DeviceInfo* info);
```

**Implementation Pattern:**
```c
static VAPIXCredentials g_vapix_creds = {0};
static int g_vapix_initialized = 0;

int vapix_init(void) {
    if (g_vapix_initialized) return 0;

    if (acquire_vapix_credentials(&g_vapix_creds) != 0) {
        syslog(LOG_ERR, "Failed to acquire VAPIX credentials");
        return -1;
    }

    g_vapix_initialized = 1;
    syslog(LOG_INFO, "VAPIX client initialized");
    return 0;
}

void vapix_cleanup(void) {
    // Zero out credentials in memory
    memset(&g_vapix_creds, 0, sizeof(g_vapix_creds));
    g_vapix_initialized = 0;
}

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    MemoryBuffer* mem = (MemoryBuffer*)userp;

    char* ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        syslog(LOG_ERR, "Out of memory in curl callback");
        return 0; // Out of memory
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

int vapix_get_temperature(double* temperature) {
    if (!g_vapix_initialized) {
        syslog(LOG_ERR, "VAPIX client not initialized");
        return -1;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        syslog(LOG_ERR, "Failed to initialize curl");
        return -1;
    }

    MemoryBuffer buffer = {0};

    // Build authentication string
    char auth[512];
    snprintf(auth, sizeof(auth), "%s:%s", g_vapix_creds.username, g_vapix_creds.password);

    curl_easy_setopt(curl, CURLOPT_URL,
        "http://127.0.0.12/axis-cgi/temperaturecontrol.cgi?device=sensor&id=2&action=query&temperatureunit=celsius");
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC | CURLAUTH_DIGEST);
    curl_easy_setopt(curl, CURLOPT_USERPWD, auth);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); // 5 second timeout
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&buffer);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        syslog(LOG_ERR, "VAPIX temperature request failed: %s", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(buffer.data);
        return -1;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        syslog(LOG_ERR, "VAPIX temperature request returned HTTP %ld", http_code);
        curl_easy_cleanup(curl);
        free(buffer.data);
        return -1;
    }

    *temperature = parse_temperature_response(buffer.data);

    curl_easy_cleanup(curl);
    free(buffer.data);

    return (*temperature >= 0) ? 0 : -1;
}

int vapix_get_device_info(DeviceInfo* info) {
    if (!g_vapix_initialized) {
        syslog(LOG_ERR, "VAPIX client not initialized");
        return -1;
    }

    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    MemoryBuffer buffer = {0};
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    char auth[512];
    snprintf(auth, sizeof(auth), "%s:%s", g_vapix_creds.username, g_vapix_creds.password);

    const char* json_payload = "{\"apiVersion\":\"1.0\",\"context\":\"health-monitor\",\"method\":\"getAllProperties\"}";

    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.12/axis-cgi/basicdeviceinfo.cgi");
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC | CURLAUTH_DIGEST);
    curl_easy_setopt(curl, CURLOPT_USERPWD, auth);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&buffer);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        syslog(LOG_ERR, "VAPIX device info request failed: %s", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(buffer.data);
        return -1;
    }

    int result = parse_device_info_response(buffer.data, info);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(buffer.data);

    return result;
}
```

---

## Caching Strategy

### Cache Structure

```c
typedef struct {
    double value;
    time_t timestamp;
    int valid;
} CachedDouble;

typedef struct {
    DeviceInfo value;
    time_t timestamp;
    int valid;
} CachedDeviceInfo;

static CachedDouble temperature_cache = {0};
static CachedDeviceInfo device_info_cache = {0};
static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;
```

### Cache Access Pattern

```c
int get_cached_temperature(double* temperature) {
    pthread_mutex_lock(&cache_mutex);

    time_t now = time(NULL);

    // Check if cache is valid (within TTL)
    if (temperature_cache.valid &&
        (now - temperature_cache.timestamp) < 60) {
        *temperature = temperature_cache.value;
        pthread_mutex_unlock(&cache_mutex);
        syslog(LOG_DEBUG, "Serving cached temperature: %.2f°C (age: %lds)",
            *temperature, now - temperature_cache.timestamp);
        return 0;
    }

    pthread_mutex_unlock(&cache_mutex);

    // Cache miss or expired - fetch fresh data
    double fresh_temp;
    if (vapix_get_temperature(&fresh_temp) == 0) {
        pthread_mutex_lock(&cache_mutex);
        temperature_cache.value = fresh_temp;
        temperature_cache.timestamp = now;
        temperature_cache.valid = 1;
        pthread_mutex_unlock(&cache_mutex);

        *temperature = fresh_temp;
        syslog(LOG_INFO, "Fetched fresh temperature: %.2f°C", fresh_temp);
        return 0;
    }

    // Fetch failed - return stale cache if available
    pthread_mutex_lock(&cache_mutex);
    if (temperature_cache.valid) {
        *temperature = temperature_cache.value;
        pthread_mutex_unlock(&cache_mutex);
        syslog(LOG_WARNING, "VAPIX fetch failed, serving stale cache: %.2f°C (age: %lds)",
            *temperature, now - temperature_cache.timestamp);
        return 0; // Serving stale data
    }
    pthread_mutex_unlock(&cache_mutex);

    syslog(LOG_ERR, "No temperature data available (VAPIX failed, no cache)");
    return -1; // No data available
}

int get_cached_device_info(DeviceInfo* info) {
    pthread_mutex_lock(&cache_mutex);

    time_t now = time(NULL);

    // Check if cache is valid (within TTL)
    if (device_info_cache.valid &&
        (now - device_info_cache.timestamp) < 300) {
        memcpy(info, &device_info_cache.value, sizeof(DeviceInfo));
        pthread_mutex_unlock(&cache_mutex);
        return 0;
    }

    pthread_mutex_unlock(&cache_mutex);

    // Cache miss or expired - fetch fresh data
    DeviceInfo fresh_info;
    if (vapix_get_device_info(&fresh_info) == 0) {
        pthread_mutex_lock(&cache_mutex);
        memcpy(&device_info_cache.value, &fresh_info, sizeof(DeviceInfo));
        device_info_cache.timestamp = now;
        device_info_cache.valid = 1;
        pthread_mutex_unlock(&cache_mutex);

        memcpy(info, &fresh_info, sizeof(DeviceInfo));
        return 0;
    }

    // Fetch failed - device info is static, no need for stale cache
    syslog(LOG_ERR, "Failed to fetch device info from VAPIX");
    return -1;
}
```

**Thread Safety:** All cache access protected by mutex to prevent race conditions in multi-threaded HTTP server environment.

---

## Error Handling & Fallback Strategy

### Error Scenarios

1. **VAPIX API Unavailable**
   - Symptom: HTTP connection fails (CURLE_COULDNT_CONNECT)
   - Fallback: Serve stale cache (temperature) or skip metric (device info)
   - Action: Log warning, continue with degraded functionality

2. **Authentication Failure**
   - Symptom: HTTP 401 response
   - Fallback: Attempt to re-acquire credentials via D-Bus, then retry
   - Action: Log error, if retry fails, skip VAPIX metrics

3. **Parse Failure**
   - Symptom: Response format unexpected (sscanf/JSON parse fails)
   - Fallback: Serve stale cache or return -1
   - Action: Log error with response body for debugging

4. **Timeout**
   - Symptom: curl timeout (5 seconds)
   - Fallback: Serve stale cache or skip metric
   - Action: Log warning, may indicate camera CPU overload

5. **D-Bus Credential Acquisition Failure**
   - Symptom: GetCredentials D-Bus call fails on startup
   - Fallback: Disable VAPIX integration entirely, rely on /proc metrics only
   - Action: Log critical error, health monitor operates in degraded mode

### Fallback Decision Matrix

| Error Type | Stale Cache Available | Action |
|------------|----------------------|--------|
| Connection Failed | Yes | Serve stale cache |
| Connection Failed | No | Skip metric, use /proc fallback |
| Auth Failed (401) | Yes | Re-acquire credentials, retry once, serve stale on failure |
| Auth Failed (401) | No | Re-acquire credentials, retry once, skip on failure |
| Parse Error | Yes | Serve stale cache |
| Parse Error | No | Skip metric |
| Timeout | Yes | Serve stale cache |
| Timeout | No | Skip metric |
| D-Bus Failure | N/A | Disable VAPIX, use /proc-only mode |

### Retry Strategy

```c
int vapix_get_temperature_with_retry(double* temperature) {
    int result = vapix_get_temperature(temperature);

    if (result != 0) {
        // Check if auth failure - attempt credential refresh
        // (Implementation: check HTTP response code stored in thread-local)
        syslog(LOG_WARNING, "VAPIX temperature fetch failed, attempting credential refresh");

        if (acquire_vapix_credentials(&g_vapix_creds) == 0) {
            // Retry once with new credentials
            result = vapix_get_temperature(temperature);
        }
    }

    return result;
}
```

---

## Testing & Validation

### Manual Testing Commands

**From Development Machine (External):**
```bash
# Test temperature endpoint
curl --anyauth -u admin:password \
  'http://192.168.30.15/axis-cgi/temperaturecontrol.cgi?device=sensor&id=2&action=query&temperatureunit=celsius'

# Test device info endpoint
curl -X POST \
  --anyauth -u admin:password \
  -H "Content-Type: application/json" \
  -d '{"apiVersion":"1.0","context":"test","method":"getAllProperties"}' \
  'http://192.168.30.15/axis-cgi/basicdeviceinfo.cgi'

# Test legacy param.cgi (for comparison)
curl --anyauth -u admin:password \
  'http://192.168.30.15/axis-cgi/param.cgi?action=list&group=Properties.System'
```

**From Camera Localhost (SSH Access):**
```bash
ssh root@192.168.30.15

# Test if localhost bypasses auth (may work without credentials)
curl 'http://localhost/axis-cgi/temperaturecontrol.cgi?device=sensor&id=2&action=query'

# Test with ACAP virtual host (requires ACAP app to get credentials)
# This will fail from SSH - only works from ACAP context
curl 'http://127.0.0.12/axis-cgi/temperaturecontrol.cgi?device=sensor&id=2&action=query'
```

### Expected Responses

**Temperature:**
```
46.71
```

**Device Info:**
```json
{
  "apiVersion": "1.0",
  "context": "test",
  "data": {
    "propertyList": {
      "SerialNumber": "ACCC8E78B977",
      "Version": "8.20.1",
      "ProdNbr": "Q3505 Mk II",
      "Architecture": "armv7hf",
      "Soc": "Ambarella S5L"
    }
  }
}
```

### Error Testing

```bash
# Test with wrong credentials
curl --anyauth -u admin:wrongpassword \
  'http://192.168.30.15/axis-cgi/temperaturecontrol.cgi?device=sensor&id=2&action=query'
# Expected: HTTP 401 Unauthorized

# Test timeout
curl --max-time 1 \
  'http://192.168.30.15/axis-cgi/temperaturecontrol.cgi?device=sensor&id=2&action=query'
# Expected: curl timeout after 1 second

# Test invalid sensor ID
curl --anyauth -u admin:password \
  'http://192.168.30.15/axis-cgi/temperaturecontrol.cgi?device=sensor&id=99&action=query'
# Expected: HTTP 400 or error message

# Test invalid temperature unit
curl --anyauth -u admin:password \
  'http://192.168.30.15/axis-cgi/temperaturecontrol.cgi?device=sensor&id=2&action=query&temperatureunit=kelvin'
# Expected: HTTP 400 "Invalid temperature unit"
```

---

## Recommendations

### Authentication Management
1. **Credentials Source**: Use D-Bus GetCredentials method (recommended for ACAP apps)
2. **Security**: Never hardcode passwords, never write credentials to disk
3. **Localhost Access**: Use 127.0.0.12 virtual host (ACAP apps only)
4. **Credentials Storage**: Memory-only, re-acquire on app restart
5. **Manifest Requirement**: Add D-Bus method to `manifest.json` resources

### Performance Optimization
1. **Lazy Loading**: Don't fetch VAPIX data until first metrics request
2. **Async Fetching**: Consider background thread for periodic VAPIX polling (separate from HTTP request handling)
3. **Connection Reuse**: Create single curl handle per thread (thread-local storage)
4. **Timeout**: Keep timeout short (5 seconds) to avoid blocking HTTP responses

### API Rate Limiting
1. **Cache TTL**: 60 seconds for temperature, 300 seconds for device info
2. **Max Frequency**: Cache enforces minimum 60-second interval for temperature
3. **Burst Protection**: HTTP server may receive 100s of requests, but VAPIX called max once per TTL period
4. **Background Polling**: Optional: poll VAPIX every 60s in background thread, serve from cache

### Sensor ID Detection
1. **Temperature Sensor ID**: Hardcoded to `id=2` based on documentation example
2. **Discovery**: May need to query `action=statusall` to detect available sensor IDs
3. **Fallback**: If `id=2` returns 400, try `id=0` or `id=1`

### JSON Library
1. **Library**: Use `jansson` (included in ACAP SDK)
2. **Linking**: Add `-ljansson` to Makefile LDFLAGS
3. **Alternative**: `cJSON` (lightweight, single-file, but may need to include manually)

---

## Files Analyzed
- PLAN.md (lines 576-607) - VAPIX integration requirements
- Research: VAPIX API documentation from developer.axis.com
  - [Temperature Control API](https://developer.axis.com/vapix/network-video/temperature-control/)
  - [Basic Device Information API](https://developer.axis.com/vapix/network-video/basic-device-information/)
  - [VAPIX Access for ACAP Applications](https://developer.axis.com/acap/develop/VAPIX-access-for-ACAP-applications/)
  - [Param API](https://developer.axis.com/vapix/device-configuration/param-api/)
  - [Authentication](https://developer.axis.com/vapix/authentication/)

## Blockers or Uncertainties
- **Sensor ID**: Documentation uses `id=2` but actual camera may use different sensor ID (test required)
- **Localhost Auth Bypass**: Need to verify if plain `localhost` (not 127.0.0.12) bypasses auth for testing
- **JSON Library**: Confirm jansson availability in ACAP SDK (fallback: cJSON single-file inclusion)
- **Temperature Endpoint Availability**: Some older cameras may not support temperaturecontrol.cgi (fallback: param.cgi)

## Confidence Level
**HIGH** - VAPIX API is well-documented, stable across firmware versions, and D-Bus credential mechanism is standard for ACAP 4.x applications. Implementation pattern is proven and widely used in ACAP ecosystem.

---

## Sources
- [Temperature Control API | Axis developer documentation](https://developer.axis.com/vapix/network-video/temperature-control/)
- [Basic Device Information API | Axis developer documentation](https://developer.axis.com/vapix/network-video/basic-device-information/)
- [VAPIX Access for ACAP Applications | Axis developer documentation](https://developer.axis.com/acap/develop/VAPIX-access-for-ACAP-applications/)
- [Param API | Axis developer documentation](https://developer.axis.com/vapix/device-configuration/param-api/)
- [Authentication | Axis developer documentation](https://developer.axis.com/vapix/authentication/)
