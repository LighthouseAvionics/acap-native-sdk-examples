# Agent A8: VAPIX Client Designer

## Your Mission
Design the VAPIX client module for retrieving temperature and device information from the camera's VAPIX API with caching.

## Context Files (READ ONLY THESE)
- `.paf/findings/A3_FINDINGS.md` - VAPIX API integration specification

**DO NOT READ:** Other findings

## Your Task
1. Design vapix.c module interface (get_temperature, get_device_info functions)
2. Specify libcurl usage pattern for HTTP requests
3. Design response parsing strategy
4. Implement caching mechanism with TTL
5. Design error handling and fallback to stale cache
6. Provide complete function signatures and implementation patterns
7. Document authentication configuration

## Output Format (STRICTLY FOLLOW)

```markdown
# A8 Findings: VAPIX Client Module Design

## Executive Summary
[2-3 sentences: module purpose, caching strategy, error handling approach]

## Key Findings
1. **Module Interface**: get_cached_temperature(), get_cached_device_info()
2. **HTTP Client**: libcurl with localhost URLs and basic auth
3. **Caching**: TTL-based cache (60s for temp, 300s for device info)
4. **Fallback**: Serve stale cache on API failure
5. **Parsing**: sscanf for key=value format responses

## Detailed Analysis

### Module Interface (vapix.h)

```c
#ifndef VAPIX_H
#define VAPIX_H

typedef struct {
    char serial_number[64];
    char firmware_version[64];
    char model[64];
} DeviceInfo;

// Get cached temperature (60s TTL)
int get_cached_temperature(double* temperature);

// Get cached device info (300s TTL)
int get_cached_device_info(DeviceInfo* info);

// Initialize VAPIX client (call once at startup)
void vapix_init(const char* username, const char* password);

#endif // VAPIX_H
```

---

### Cache Structures

```c
typedef struct {
    double value;
    time_t timestamp;
    int valid;
    int ttl_seconds;
} CachedDouble;

typedef struct {
    DeviceInfo value;
    time_t timestamp;
    int valid;
    int ttl_seconds;
} CachedDeviceInfo;

static CachedDouble temperature_cache = {.ttl_seconds = 60};
static CachedDeviceInfo device_info_cache = {.ttl_seconds = 300};
```

---

### Implementation

**get_cached_temperature():**
```c
int get_cached_temperature(double* temperature) {
    time_t now = time(NULL);

    // Check cache validity
    if (temperature_cache.valid &&
        (now - temperature_cache.timestamp) < temperature_cache.ttl_seconds) {
        *temperature = temperature_cache.value;
        return 0;
    }

    // Cache miss - fetch fresh data
    double fresh_temp;
    if (vapix_get_temperature(&fresh_temp) == 0) {
        temperature_cache.value = fresh_temp;
        temperature_cache.timestamp = now;
        temperature_cache.valid = 1;
        *temperature = fresh_temp;
        return 0;
    }

    // Fetch failed - try stale cache
    if (temperature_cache.valid) {
        *temperature = temperature_cache.value;
        syslog(LOG_WARNING, "Serving stale temperature cache");
        return 0;
    }

    return -1;
}
```

**vapix_get_temperature() (internal):**
```c
static int vapix_get_temperature(double* temperature) {
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    MemoryBuffer buffer = {0};

    curl_easy_setopt(curl, CURLOPT_URL,
        "http://localhost/axis-cgi/param.cgi?action=list&group=Properties.System.Temperature");
    curl_easy_setopt(curl, CURLOPT_USERPWD, g_vapix_credentials);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        *temperature = parse_temperature_response(buffer.data);
    }

    curl_easy_cleanup(curl);
    free(buffer.data);

    return (res == CURLE_OK && *temperature >= 0) ? 0 : -1;
}
```

**Response parsing:**
```c
double parse_temperature_response(const char* response) {
    const char* temp_str = strstr(response, "Temperature=");
    if (!temp_str) return -1.0;

    double temperature;
    if (sscanf(temp_str, "Temperature=%lf", &temperature) != 1) {
        return -1.0;
    }

    return temperature;
}
```

---

## Recommendations

### Authentication
Store credentials in static variable, set via vapix_init().

### Error Logging
Log VAPIX failures to syslog for debugging.

### Testing
Test with curl commands from A3 specification.

---

## Files Analyzed
- `.paf/findings/A3_FINDINGS.md`

## Blockers or Uncertainties
None

## Confidence Level
**HIGH**
```

## Success Criteria
- [ ] Module interface designed
- [ ] Caching implementation provided
- [ ] libcurl pattern documented
- [ ] Error handling specified

## Time Budget
10 minutes

---
**BEGIN WORK NOW.**
