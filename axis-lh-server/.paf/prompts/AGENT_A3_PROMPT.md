# Agent A3: VAPIX Integration Analyst

## Your Mission
Analyze VAPIX API capabilities for retrieving PTZ camera temperature and device information, and design the integration approach.

## Context Files (READ ONLY THESE)
- `PLAN.md` - Lines 576-607 (VAPIX integration section for context)
- Research: VAPIX API documentation for:
  - `/axis-cgi/param.cgi?action=list&group=Properties.System.Temperature`
  - `/axis-cgi/param.cgi?action=list&group=Properties.System`
  - Authentication requirements

**DO NOT READ:** Code files (not needed yet), other agent findings (none available yet)

## Your Task
1. Document VAPIX API endpoints needed for health monitoring:
   - Temperature monitoring endpoint
   - Device info endpoint (model, serial, firmware)
   - Storage info endpoint (if applicable for SD card)
2. Analyze authentication requirements (localhost vs external access)
3. Specify request/response format with actual examples
4. Design caching strategy (1-5 minute TTL to avoid API rate limits)
5. Identify error scenarios and fallback behavior
6. Provide curl command examples for testing
7. Note any API rate limits or restrictions

## Output Format (STRICTLY FOLLOW)

```markdown
# A3 Findings: VAPIX API Integration Specification

## Executive Summary
[2-3 sentences: what VAPIX endpoints are available, authentication needs, caching approach]

## Key Findings
1. **Temperature Endpoint**: Available via param.cgi, returns °C, requires caching
2. **Device Info Endpoint**: Available via param.cgi, returns model/serial/firmware
3. **Authentication**: Admin credentials required (or localhost exemption analysis)
4. **Caching Strategy**: 1-5 minute TTL to avoid rate limits
5. **Error Handling**: Fallback to /proc metrics if VAPIX unavailable

## Detailed Analysis

### Endpoint 1: Temperature Monitoring

**API Endpoint:**
```
GET /axis-cgi/param.cgi?action=list&group=Properties.System.Temperature
```

**Authentication:**
- Method: HTTP Basic Auth
- Username: `admin` (or camera default)
- Password: [From camera configuration]
- Localhost exemption: [YES/NO - document if requests from localhost bypass auth]

**Request Example:**
```bash
curl -u admin:password \
  'http://localhost/axis-cgi/param.cgi?action=list&group=Properties.System.Temperature'
```

**Response Format:**
```
root.Properties.System.Temperature=45.0
```

**Parsing Strategy:**
```c
// Parse response to extract temperature value
double parse_temperature_response(const char* response) {
    // Look for pattern: "Temperature=XX.X"
    const char* temp_str = strstr(response, "Temperature=");
    if (!temp_str) return -1.0;

    double temperature;
    if (sscanf(temp_str, "Temperature=%lf", &temperature) != 1) {
        return -1.0;
    }

    return temperature;
}
```

**Caching Requirements:**
- Cache TTL: 60 seconds (temperature doesn't change rapidly)
- Cache on first successful read
- Refresh cache on expiry
- Serve stale cache if API call fails

---

### Endpoint 2: Device Information

**API Endpoint:**
```
GET /axis-cgi/param.cgi?action=list&group=Properties.System
```

**Request Example:**
```bash
curl -u admin:password \
  'http://localhost/axis-cgi/param.cgi?action=list&group=Properties.System'
```

**Response Format:**
```
root.Properties.System.SerialNumber=ACCC1234567
root.Properties.System.Architecture=armv7hf
root.Properties.System.Soc=Ambarella S5L
root.Properties.Firmware.Version=10.12.123
```

**Fields to Extract:**
- `SerialNumber`: Unique camera identifier
- `Firmware.Version`: Firmware version string
- Model: [May need different endpoint - document]

**Parsing Strategy:**
```c
typedef struct {
    char serial_number[64];
    char firmware_version[64];
    char model[64];
} DeviceInfo;

int parse_device_info_response(const char* response, DeviceInfo* info) {
    // Parse key-value pairs
    const char* line = response;

    // Look for SerialNumber
    const char* serial = strstr(line, "SerialNumber=");
    if (serial) {
        sscanf(serial, "SerialNumber=%63s", info->serial_number);
    }

    // Look for Firmware.Version
    const char* firmware = strstr(line, "Firmware.Version=");
    if (firmware) {
        sscanf(firmware, "Firmware.Version=%63s", info->firmware_version);
    }

    return 0;
}
```

**Caching Requirements:**
- Cache TTL: 300 seconds (5 minutes) - device info rarely changes
- Read once on startup
- Refresh periodically for firmware update detection

---

### Endpoint 3: Storage Information (Optional)

**API Endpoint:**
```
GET /axis-cgi/disks/list.cgi
```

**Purpose:** Detect SD card presence and usage

**Request Example:**
```bash
curl -u admin:password 'http://localhost/axis-cgi/disks/list.cgi'
```

**Response Format:** [Document if SD card monitoring is needed]

**Note:** May not be applicable if camera doesn't have SD card slot

---

## VAPIX Client Design

### HTTP Client Implementation

**Library Choice:** libcurl (lightweight, included in ACAP SDK)

**Function Signature:**
```c
#include <curl/curl.h>

typedef struct {
    char* data;
    size_t size;
} MemoryBuffer;

int vapix_get_temperature(double* temperature);
int vapix_get_device_info(DeviceInfo* info);
```

**Implementation Pattern:**
```c
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    MemoryBuffer* mem = (MemoryBuffer*)userp;

    char* ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        return 0; // Out of memory
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

int vapix_get_temperature(double* temperature) {
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    MemoryBuffer buffer = {0};

    curl_easy_setopt(curl, CURLOPT_URL,
        "http://localhost/axis-cgi/param.cgi?action=list&group=Properties.System.Temperature");
    curl_easy_setopt(curl, CURLOPT_USERPWD, "admin:password"); // TODO: Get from config
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); // 5 second timeout
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&buffer);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        free(buffer.data);
        return -1;
    }

    *temperature = parse_temperature_response(buffer.data);

    curl_easy_cleanup(curl);
    free(buffer.data);

    return (*temperature >= 0) ? 0 : -1;
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
```

### Cache Access Pattern

```c
int get_cached_temperature(double* temperature) {
    time_t now = time(NULL);

    // Check if cache is valid (within TTL)
    if (temperature_cache.valid &&
        (now - temperature_cache.timestamp) < 60) {
        *temperature = temperature_cache.value;
        return 0;
    }

    // Cache miss or expired - fetch fresh data
    double fresh_temp;
    if (vapix_get_temperature(&fresh_temp) == 0) {
        temperature_cache.value = fresh_temp;
        temperature_cache.timestamp = now;
        temperature_cache.valid = 1;
        *temperature = fresh_temp;
        return 0;
    }

    // Fetch failed - return stale cache if available
    if (temperature_cache.valid) {
        *temperature = temperature_cache.value;
        return 0; // Serving stale data
    }

    return -1; // No data available
}
```

---

## Error Handling & Fallback Strategy

### Error Scenarios

1. **VAPIX API Unavailable**
   - Symptom: HTTP connection fails
   - Fallback: Skip VAPIX metrics, use only /proc metrics
   - Action: Log warning, continue with degraded functionality

2. **Authentication Failure**
   - Symptom: HTTP 401 response
   - Fallback: Serve stale cache or skip metric
   - Action: Log error, alert operator to check credentials

3. **Parse Failure**
   - Symptom: Response format unexpected
   - Fallback: Return -1, skip metric
   - Action: Log error with response body for debugging

4. **Timeout**
   - Symptom: curl timeout (5 seconds)
   - Fallback: Serve stale cache or skip metric
   - Action: Log warning, may indicate camera overload

### Fallback Decision Matrix

| Error Type | Stale Cache Available | Action |
|------------|----------------------|--------|
| Connection Failed | Yes | Serve stale cache |
| Connection Failed | No | Skip metric |
| Auth Failed | Yes | Serve stale cache, alert |
| Auth Failed | No | Skip metric, alert |
| Parse Error | Yes | Serve stale cache |
| Parse Error | No | Skip metric |
| Timeout | Yes | Serve stale cache |
| Timeout | No | Skip metric |

---

## Testing & Validation

### Manual Testing Commands

```bash
# Test temperature endpoint
curl -u admin:password \
  'http://192.168.30.15/axis-cgi/param.cgi?action=list&group=Properties.System.Temperature'

# Test device info endpoint
curl -u admin:password \
  'http://192.168.30.15/axis-cgi/param.cgi?action=list&group=Properties.System'

# Test from localhost (inside camera)
ssh root@192.168.30.15
curl 'http://localhost/axis-cgi/param.cgi?action=list&group=Properties.System.Temperature'
```

### Expected Responses

Temperature: `root.Properties.System.Temperature=45.0`
Device Info: Multi-line key-value pairs with SerialNumber and Firmware.Version

### Error Testing

```bash
# Test with wrong credentials
curl -u admin:wrongpassword '...'  # Should return 401

# Test timeout
curl --max-time 1 '...'  # Should timeout if camera slow
```

---

## Recommendations

### Authentication Management
1. **Credentials Source**: Read from environment variable or config file
2. **Security**: Never hardcode passwords in source code
3. **Localhost Access**: Test if localhost requests bypass authentication
4. **Credentials Storage**: Use ACAP parameter API if available

### Performance Optimization
1. **Lazy Loading**: Don't fetch VAPIX data unless metrics endpoint called
2. **Async Fetching**: Consider background thread for VAPIX calls (if threading supported)
3. **Connection Reuse**: Reuse curl handle if making multiple calls
4. **Timeout**: Keep timeout short (5 seconds) to avoid blocking

### API Rate Limiting
1. **Cache TTL**: 60 seconds for temperature, 300 seconds for device info
2. **Max Frequency**: Don't call more than once per minute
3. **Burst Protection**: Limit to 1 request per cache TTL period

## Files Analyzed
- PLAN.md (lines 576-607) - VAPIX integration requirements
- Research: VAPIX API documentation

## Blockers or Uncertainties
- **Authentication on localhost**: Need to verify if localhost requests bypass auth (test on actual camera)
- **Model detection**: May need to check additional param.cgi groups for camera model

## Confidence Level
**HIGH** - VAPIX API is well-documented and stable across Axis firmware versions
```

## Success Criteria
- [ ] Temperature endpoint documented with request/response examples
- [ ] Device info endpoint documented with parsing strategy
- [ ] Caching strategy designed with TTL values
- [ ] libcurl implementation pattern provided
- [ ] Error handling and fallback strategy documented
- [ ] Testing commands provided
- [ ] Output follows the exact format above

## Time Budget
10 minutes maximum. Focus on temperature endpoint first (most critical), then device info.

---
**BEGIN WORK NOW.** Start by researching VAPIX API documentation, then produce your findings with curl examples.
