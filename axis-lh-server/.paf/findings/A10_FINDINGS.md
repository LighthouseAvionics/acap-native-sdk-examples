# A10 Findings: Metrics Implementation Checklist

## Executive Summary
Complete implementation plan for metrics endpoint integration into lrf-controller, requiring 10 files (7 new, 3 modified) with comprehensive system/network/disk/service metrics collection via /proc filesystem, VAPIX temperature integration via libcurl with D-Bus credentials, and full testing procedures including promtool validation.

## Implementation Checklist

### Phase 1: Create Helper Module Files

#### Task 1.1: Create app/metrics_helpers.h
- [ ] Create file with include guards
- [ ] Add struct definitions:
  - `MemoryInfo` (total_bytes, available_bytes)
  - `CPUStats` (user, nice, system, idle, iowait, irq, softirq, steal)
  - `NetworkStats` (rx_bytes, tx_bytes)
  - `DiskStats` (total_bytes, available_bytes)
- [ ] Declare helper functions:
  - `double get_uptime(void);`
  - `int get_memory_info(MemoryInfo* mem);`
  - `int get_cpu_stats(CPUStats* stats);`
  - `double calculate_cpu_usage(const CPUStats* prev, const CPUStats* curr);`
  - `double get_load_average_1m(void);`
  - `int get_network_stats(NetworkStats* stats, const char* interface);`
  - `int get_primary_interface_name(char* buf, size_t size);`
  - `int get_disk_stats(const char* path, DiskStats* stats);`
  - `int get_process_count(void);`
- [ ] Add include for `<stdint.h>` and `<stddef.h>`

#### Task 1.2: Create app/metrics_helpers.c
- [ ] Implement `get_uptime()` - Parse /proc/uptime, return first double
- [ ] Implement `get_memory_info()` - Parse /proc/meminfo for MemTotal and MemAvailable
- [ ] Implement `get_cpu_stats()` - Parse /proc/stat first line (cpu line), extract 8 time values
- [ ] Implement `calculate_cpu_usage()` - Calculate percentage from delta of prev/curr stats
- [ ] Implement `get_load_average_1m()` - Parse /proc/loadavg, return first double
- [ ] Implement `get_network_stats()` - Parse /proc/net/dev for interface rx/tx bytes
- [ ] Implement `get_primary_interface_name()` - Scan /proc/net/dev for first non-loopback interface
- [ ] Implement `get_disk_stats()` - Use statfs() on path, populate total/available bytes
- [ ] Implement `get_process_count()` - Count numeric directories in /proc using opendir/readdir
- [ ] Add includes: stdio.h, stdlib.h, string.h, syslog.h, sys/statfs.h, dirent.h, ctype.h
- [ ] Add error logging to syslog on all failure paths

---

### Phase 2: Create Core Metrics Module

#### Task 2.1: Create app/metrics.h
- [ ] Create file with include guards
- [ ] Add `#include <glib.h>`
- [ ] Add `#include "http_server.h"`
- [ ] Declare collection functions:
  - `void collect_system_metrics(GString* output);`
  - `void collect_network_metrics(GString* output);`
  - `void collect_disk_metrics(GString* output);`
  - `void collect_service_metrics(GString* output);`
  - `void collect_vapix_metrics(GString* output);`
- [ ] Declare HTTP handler:
  - `void metrics_handler(int client_fd, HttpRequest* request, gpointer user_data);`
- [ ] Declare global counters as extern:
  - `extern uint64_t g_http_requests_total;`
  - `extern uint64_t g_i2c_errors_total;`

#### Task 2.2: Create app/metrics.c
- [ ] Define global counters:
  - `uint64_t g_http_requests_total = 0;`
  - `uint64_t g_i2c_errors_total = 0;`
- [ ] Implement `append_metric_gauge()`:
  - Print HELP comment: `# HELP <name> <help>`
  - Print TYPE comment: `# TYPE <name> gauge`
  - Print metric line: `<name>{<labels>} <value>` (or without labels if NULL)
  - Use `g_string_append_printf()` with format `%.2f` for value
- [ ] Implement `append_metric_counter()`:
  - Print HELP comment
  - Print TYPE comment (counter)
  - Print metric line with format `%lu` for uint64_t value
- [ ] Implement `collect_system_metrics()`:
  - Call `get_uptime()`, append `ptz_uptime_seconds` gauge if >= 0
  - Call `get_memory_info()`, append `ptz_memory_total_bytes` and `ptz_memory_available_bytes` gauges if success
  - Call `get_load_average_1m()`, append `ptz_load_average_1m` gauge if >= 0
  - Static variables for CPU: `static CPUStats prev_cpu = {0}; static int first_sample = 1;`
  - Call `get_cpu_stats()`, calculate CPU usage if not first sample, append `ptz_cpu_usage_percent` gauge
  - Update prev_cpu and set first_sample = 0
  - Log warnings to syslog on any collection failures
- [ ] Implement `collect_network_metrics()`:
  - Call `get_primary_interface_name()` to detect interface
  - Call `get_network_stats()` for detected interface
  - Build labels string: `interface="<name>"`
  - Append `ptz_network_rx_bytes_total` counter with labels
  - Append `ptz_network_tx_bytes_total` counter with labels
  - Log warning if interface detection or stats collection fails
- [ ] Implement `collect_disk_metrics()`:
  - Call `get_disk_stats("/", &stats)`
  - Append `ptz_disk_total_bytes` gauge
  - Append `ptz_disk_free_bytes` gauge (use available_bytes)
  - Log warning on failure
- [ ] Implement `collect_service_metrics()`:
  - Append `ptz_http_requests_total` counter using g_http_requests_total
  - Append `ptz_i2c_errors_total` counter using g_i2c_errors_total
  - Call `get_process_count()`, append `ptz_process_count` gauge if >= 0
  - Log warning on process count failure
- [ ] Implement `collect_vapix_metrics()`:
  - Call `get_cached_temperature()` from vapix module
  - If success, append `ptz_temperature_celsius` gauge
  - If failure, skip metric (no error - VAPIX may be unavailable)
- [ ] Implement `metrics_handler()`:
  - Validate method is GET using `g_strcmp0(request->method, "GET")`
  - If not GET, call `http_send_error(client_fd, 405, "Method not allowed")` and return
  - Create GString: `GString* metrics = g_string_new("");`
  - Call all collect functions: system, network, disk, service, vapix
  - Ensure trailing newline: check if last char is '\n', append if not
  - Build HTTP header with Content-Type: `text/plain; version=0.0.4; charset=utf-8`
  - Calculate Content-Length using `metrics->len`
  - Send header using `send(client_fd, header, strlen(header), 0)`
  - Send body using `send(client_fd, metrics->str, metrics->len, 0)`
  - Free GString: `g_string_free(metrics, TRUE)`
- [ ] Add includes: stdio.h, string.h, syslog.h, sys/socket.h, unistd.h
- [ ] Add include for metrics_helpers.h and vapix.h

---

### Phase 3: Create VAPIX Client Module

#### Task 3.1: Create app/vapix.h
- [ ] Create file with include guards
- [ ] Add `#include <time.h>`
- [ ] Define `DeviceInfo` struct:
  - `char serial_number[64];`
  - `char firmware_version[64];`
  - `char model[64];`
  - `char architecture[32];`
  - `char soc[64];`
- [ ] Declare functions:
  - `int vapix_init(void);`
  - `void vapix_cleanup(void);`
  - `int get_cached_temperature(double* temperature);`
  - `int get_cached_device_info(DeviceInfo* info);`

#### Task 3.2: Create app/vapix.c
- [ ] Add includes: stdio.h, stdlib.h, string.h, syslog.h, pthread.h, curl/curl.h, jansson.h, glib.h, gio/gio.h
- [ ] Define internal structures:
  - `CachedDouble` (value, timestamp, valid, ttl_seconds)
  - `CachedDeviceInfo` (value, timestamp, valid, ttl_seconds)
  - `MemoryBuffer` (data, size) for curl callback
  - `VAPIXCredentials` (username[128], password[256])
- [ ] Define static globals:
  - `static CachedDouble temperature_cache = {.ttl_seconds = 60};`
  - `static CachedDeviceInfo device_info_cache = {.ttl_seconds = 300};`
  - `static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;`
  - `static VAPIXCredentials g_vapix_creds = {0};`
  - `static int g_vapix_initialized = 0;`
- [ ] Implement `write_callback()`:
  - Realloc buffer->data to fit new content
  - Memcpy contents to buffer
  - Update size
  - Null-terminate
  - Return realsize on success, 0 on OOM
- [ ] Implement `acquire_vapix_credentials()`:
  - Connect to system D-Bus: `g_bus_get_sync(G_BUS_TYPE_SYSTEM, ...)`
  - Call D-Bus method: `g_dbus_connection_call_sync()` with:
    - Service: "com.axis.HTTPConf1"
    - Object: "/com/axis/HTTPConf1/VAPIXServiceAccounts1"
    - Interface: "com.axis.HTTPConf1.VAPIXServiceAccounts1"
    - Method: "GetCredentials"
    - Parameter: service account name "lrf-controller" (or appropriate name)
    - Return type: "(ss)" for (username, password)
  - Extract username/password from GVariant: `g_variant_get(result, "(&s&s)", ...)`
  - Copy to credentials struct with bounds checking
  - Cleanup GVariant and GDBusConnection
  - Log success/failure to syslog
  - Return 0 on success, -1 on failure
- [ ] Implement `vapix_init()`:
  - Check if already initialized, return 0 if so
  - Call `acquire_vapix_credentials()`
  - Return -1 if credential acquisition fails
  - Call `curl_global_init(CURL_GLOBAL_DEFAULT)`
  - Set g_vapix_initialized = 1
  - Log success to syslog
  - Return 0
- [ ] Implement `vapix_cleanup()`:
  - Check if initialized, return if not
  - Zero out credentials: `memset(&g_vapix_creds, 0, sizeof(g_vapix_creds))`
  - Call `curl_global_cleanup()`
  - Set g_vapix_initialized = 0
  - Log cleanup to syslog
- [ ] Implement `parse_temperature_response()`:
  - Check for NULL or empty response
  - Parse with sscanf: `sscanf(response, "%lf", &temperature)`
  - Sanity check: warn if < -50.0 or > 100.0
  - Return temperature on success, -1.0 on failure
- [ ] Implement `vapix_get_temperature()`:
  - Check g_vapix_initialized
  - Create curl handle: `curl_easy_init()`
  - Setup MemoryBuffer for response
  - Build auth string: `username:password`
  - Set curl options:
    - URL: "http://127.0.0.12/axis-cgi/temperaturecontrol.cgi?device=sensor&id=2&action=query&temperatureunit=celsius"
    - HTTPAUTH: CURLAUTH_BASIC | CURLAUTH_DIGEST
    - USERPWD: auth string
    - TIMEOUT: 5L
    - WRITEFUNCTION: write_callback
    - WRITEDATA: &buffer
  - Perform request: `curl_easy_perform()`
  - Check CURLcode, log error if failed
  - Get HTTP response code, check for 200
  - Parse response with `parse_temperature_response()`
  - Cleanup curl and buffer
  - Return 0 on success, -1 on failure
- [ ] Implement `parse_device_info_response()`:
  - Parse JSON with jansson: `json_loads()`
  - Extract "data.propertyList" object
  - Extract fields: SerialNumber, Version, ProdNbr, Architecture, Soc
  - Copy to DeviceInfo struct with bounds checking
  - Cleanup JSON: `json_decref()`
  - Log parsed info to syslog
  - Return 0 on success, -1 on failure
- [ ] Implement `vapix_get_device_info()`:
  - Check g_vapix_initialized
  - Create curl handle
  - Setup MemoryBuffer
  - Build auth string
  - Create JSON payload: `{"apiVersion":"1.0","context":"lrf-controller","method":"getAllProperties"}`
  - Add Content-Type header: "application/json"
  - Set curl options:
    - URL: "http://127.0.0.12/axis-cgi/basicdeviceinfo.cgi"
    - HTTPAUTH: CURLAUTH_BASIC | CURLAUTH_DIGEST
    - USERPWD: auth
    - POST: 1L
    - POSTFIELDS: json_payload
    - HTTPHEADER: headers
    - TIMEOUT: 5L
    - WRITEFUNCTION/WRITEDATA: buffer
  - Perform request
  - Check result and HTTP code
  - Parse response with `parse_device_info_response()`
  - Cleanup curl, headers, buffer
  - Return 0 on success, -1 on failure
- [ ] Implement `get_cached_temperature()`:
  - Check for NULL parameter
  - Lock mutex
  - Get current time
  - Check if cache valid and within TTL (60s)
  - If valid, copy value, unlock, log debug, return 0
  - Unlock mutex
  - Call `vapix_get_temperature()` for fresh data
  - If success: lock, update cache, unlock, return 0
  - If failure: lock, check if cache.valid, serve stale if available
  - Log warning if serving stale
  - Return -1 if no data available
- [ ] Implement `get_cached_device_info()`:
  - Same pattern as temperature
  - TTL is 300 seconds
  - Memcpy DeviceInfo struct
  - Log appropriately
  - Return 0 on success, -1 on failure

---

### Phase 4: Modify Existing Files

#### Task 4.1: Modify app/lrf_controller.c
- [ ] Add includes at top:
  - `#include "metrics.h"`
  - `#include "vapix.h"`
- [ ] In `main()`, after HTTP server creation but before starting:
  - Call `vapix_init()` (check return code, log warning if fails)
  - Register metrics handler: `http_server_add_handler(http_server, "/metrics", metrics_handler, NULL);`
  - Place after existing handler registrations (after status_handler around line 174)
- [ ] In cleanup section before program exit:
  - Call `vapix_cleanup()`
- [ ] In each HTTP handler (distance_handler, command_handler, status_handler):
  - Add `g_http_requests_total++;` at start of handler function
- [ ] In I2C error paths (where lrf_read_distance fails):
  - Add `g_i2c_errors_total++;` after detecting I2C failure
  - Look for existing error handling in distance_handler

#### Task 4.2: Modify app/Makefile
- [ ] Add to SRCS line (or create multi-line):
  - `metrics.c metrics_helpers.c vapix.c`
- [ ] Update LDFLAGS to include:
  - `-lcurl -ljansson -lgio-2.0 -lpthread`
- [ ] Verify CFLAGS includes GLib/GIO pkg-config:
  - `$(shell pkg-config --cflags glib-2.0 gio-2.0)`
- [ ] Verify LDFLAGS includes GLib/GIO pkg-config:
  - `$(shell pkg-config --libs glib-2.0 gio-2.0)`

---

### Phase 5: Update Manifest for D-Bus

#### Task 5.1: Modify manifest.json
- [ ] Add or update "resources" section:
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
- [ ] Verify schemaVersion is compatible (1.0 or higher)
- [ ] Ensure proper JSON formatting

---

### Phase 6: Build System

#### Task 6.1: Verify Dockerfile Dependencies
- [ ] Check build stage includes:
  - libcurl4-openssl-dev (or libcurl-dev)
  - libjansson-dev
  - libglib2.0-dev (should already be present)
- [ ] If missing, add to apt-get install line in Dockerfile

#### Task 6.2: Build ACAP Package
```bash
docker build --build-arg ARCH=aarch64 -t lrf-controller:v1.1.0 .
```
- [ ] Build completes without errors
- [ ] Check build output for missing library warnings

#### Task 6.3: Extract EAP Package
```bash
docker cp $(docker create lrf-controller:v1.1.0):/opt/app/lrf_controller_1_1_0_aarch64.eap .
```
- [ ] Verify EAP file created
- [ ] Check file size is reasonable (should be similar to previous version)

---

### Phase 7: Testing

#### Task 7.1: Unit Testing (Development Environment)
- [ ] Test `append_metric_gauge()`:
  - Create GString, call function, verify output format matches Prometheus spec
  - Check HELP line, TYPE line, metric line with/without labels
- [ ] Test `append_metric_counter()`:
  - Same format verification, check uint64_t formatting
- [ ] Test `parse_temperature_response()`:
  - Test with valid input: "46.71"
  - Test with invalid input: "", "abc", NULL
  - Verify return values
- [ ] Test helper functions with mock /proc files (if feasible):
  - Create temporary test files
  - Verify parsing logic
  - Check error handling

#### Task 7.2: Integration Testing - Deployment
```bash
scp lrf_controller_1_1_0_aarch64.eap root@192.168.30.15:/tmp/
ssh root@192.168.30.15 'eap-install.sh install /tmp/lrf_controller_1_1_0_aarch64.eap'
```
- [ ] ACAP installs without errors
- [ ] Check syslog for initialization messages:
```bash
ssh root@192.168.30.15 'tail -f /var/log/syslog | grep lrf_controller'
```
- [ ] Verify "VAPIX client initialized successfully" message
- [ ] Verify "HTTP server started on port..." message

#### Task 7.3: Integration Testing - Endpoint Validation
- [ ] Test metrics endpoint:
```bash
curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics
```
- [ ] Verify HTTP 200 response
- [ ] Verify Content-Type header: `text/plain; version=0.0.4; charset=utf-8`
- [ ] Check response contains all expected metrics:
  - ptz_uptime_seconds
  - ptz_memory_total_bytes
  - ptz_memory_available_bytes
  - ptz_cpu_usage_percent (may be absent on first call)
  - ptz_load_average_1m
  - ptz_disk_total_bytes
  - ptz_disk_free_bytes
  - ptz_temperature_celsius
  - ptz_network_rx_bytes_total (with interface label)
  - ptz_network_tx_bytes_total (with interface label)
  - ptz_http_requests_total
  - ptz_i2c_errors_total
  - ptz_process_count
- [ ] Verify each metric has HELP and TYPE comments
- [ ] Verify metric values are reasonable:
  - Memory values > 0
  - Temperature in range 30-70°C
  - Uptime > 0

#### Task 7.4: Prometheus Format Validation
```bash
curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics > /tmp/metrics.txt
promtool check metrics /tmp/metrics.txt
```
- [ ] promtool reports no errors
- [ ] No warnings about format issues

#### Task 7.5: Performance Testing - Response Time
```bash
time curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics > /dev/null
```
- [ ] Response time < 500ms (including network latency)
- [ ] Repeat 10 times, verify consistent performance
- [ ] Check if second request is faster (cache hit for VAPIX)

#### Task 7.6: Performance Testing - Memory Usage
```bash
ssh root@192.168.30.15 'ps aux | grep lrf_controller'
```
- [ ] RSS memory < 10 MB (should be similar to pre-metrics version + ~1-2MB)
- [ ] VSZ memory < 50 MB

#### Task 7.7: Performance Testing - Load Test
```bash
ab -n 100 -c 5 -A admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics
```
- [ ] All 100 requests succeed (no failed requests)
- [ ] Requests per second > 10 rps
- [ ] No memory leaks (check RSS before/after with ps)
- [ ] Check syslog for VAPIX cache hit messages (should only fetch temperature 1-2 times during test)

#### Task 7.8: Cache Validation
- [ ] Make request, note temperature value
- [ ] Wait 10 seconds, make another request
- [ ] Temperature should be same (cache hit)
- [ ] Check syslog for "Serving cached temperature" debug message
- [ ] Wait 65 seconds, make request
- [ ] Temperature may differ (cache refresh)
- [ ] Check syslog for "Fetched fresh temperature" message

#### Task 7.9: VAPIX Fallback Testing
- [ ] Disable VAPIX temporarily (if possible) or simulate failure
- [ ] Make metrics request
- [ ] Verify temperature metric is absent (not present if no cache)
- [ ] Verify other metrics still present
- [ ] Check syslog for VAPIX error messages
- [ ] Re-enable VAPIX
- [ ] Make request, verify temperature returns

#### Task 7.10: Counter Increment Testing
- [ ] Check initial value of ptz_http_requests_total
- [ ] Make 5 requests to /metrics
- [ ] Verify counter increments by 5
- [ ] Make requests to /distance, /command, /status
- [ ] Verify counter increments for each
- [ ] Trigger I2C error (disconnect sensor or simulate failure)
- [ ] Verify ptz_i2c_errors_total increments

#### Task 7.11: Error Handling Testing
- [ ] Verify graceful degradation if individual metrics fail:
  - Test with read-protected /proc files (if testable)
  - Verify partial metrics still returned
  - Check syslog for specific failure warnings
- [ ] Test with invalid HTTP method:
```bash
curl -X POST -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics
```
- [ ] Verify HTTP 405 Method Not Allowed response

---

### Phase 8: Documentation

#### Task 8.1: Update README or Documentation
- [ ] Document new /metrics endpoint
- [ ] List all exposed metrics with descriptions
- [ ] Explain Prometheus integration
- [ ] Provide example curl commands
- [ ] Document VAPIX dependency and D-Bus requirements
- [ ] Note cache TTLs for temperature (60s) and device info (300s)

#### Task 8.2: Document Manifest Changes
- [ ] Explain D-Bus permission requirement
- [ ] Document VAPIX service account usage
- [ ] Note fallback behavior if VAPIX unavailable

---

## Files to Create/Modify Summary

**New Files (7):**
1. app/metrics_helpers.h - Struct definitions and helper function declarations
2. app/metrics_helpers.c - /proc parsing implementations
3. app/metrics.h - Public metrics module interface
4. app/metrics.c - Metrics collection and HTTP handler
5. app/vapix.h - VAPIX client interface
6. app/vapix.c - VAPIX HTTP client with caching

**Modified Files (3):**
1. app/lrf_controller.c - Integration, handler registration, counter increments
2. app/Makefile - Add source files and linker flags
3. manifest.json - Add D-Bus permission requirement

**Total Changes:** 10 files (7 new, 3 modified)

---

## Expected Metrics Output

### Complete List of Metrics
1. **ptz_uptime_seconds** (gauge) - System uptime from /proc/uptime
2. **ptz_memory_total_bytes** (gauge) - Total memory from /proc/meminfo
3. **ptz_memory_available_bytes** (gauge) - Available memory from /proc/meminfo
4. **ptz_cpu_usage_percent** (gauge) - CPU utilization from /proc/stat
5. **ptz_load_average_1m** (gauge) - 1-minute load average from /proc/loadavg
6. **ptz_disk_total_bytes** (gauge) - Total disk space via statfs("/")
7. **ptz_disk_free_bytes** (gauge) - Free disk space via statfs("/")
8. **ptz_temperature_celsius** (gauge) - Camera temperature from VAPIX API
9. **ptz_network_rx_bytes_total{interface="X"}** (counter) - Received bytes from /proc/net/dev
10. **ptz_network_tx_bytes_total{interface="X"}** (counter) - Transmitted bytes from /proc/net/dev
11. **ptz_http_requests_total** (counter) - Application HTTP request count
12. **ptz_i2c_errors_total** (counter) - Application I2C error count
13. **ptz_process_count** (gauge) - Running process count from /proc

### Sample Output Format
```
# HELP ptz_uptime_seconds PTZ camera system uptime
# TYPE ptz_uptime_seconds gauge
ptz_uptime_seconds 86400.45

# HELP ptz_memory_total_bytes Total memory in bytes
# TYPE ptz_memory_total_bytes gauge
ptz_memory_total_bytes 536870912.00

# HELP ptz_memory_available_bytes Available memory in bytes
# TYPE ptz_memory_available_bytes gauge
ptz_memory_available_bytes 201326592.00

# HELP ptz_temperature_celsius Camera temperature in Celsius
# TYPE ptz_temperature_celsius gauge
ptz_temperature_celsius 46.71

# HELP ptz_network_rx_bytes_total Total bytes received
# TYPE ptz_network_rx_bytes_total counter
ptz_network_rx_bytes_total{interface="eth0"} 12345678

# HELP ptz_http_requests_total Total HTTP requests handled
# TYPE ptz_http_requests_total counter
ptz_http_requests_total 142
```

---

## Validation Criteria

### Success Metrics
- [ ] All 13 metrics exposed (or 12 if VAPIX unavailable)
- [ ] promtool validation passes with no errors
- [ ] Response time < 500ms (average over 10 requests)
- [ ] Memory usage < 10 MB RSS
- [ ] No crashes or segfaults after 100 requests
- [ ] VAPIX temperature metric present and reasonable (30-70°C)
- [ ] Counters increment correctly
- [ ] Cache reduces VAPIX calls (verify in syslog)
- [ ] Graceful degradation on individual metric failures
- [ ] HTTP 405 on non-GET requests
- [ ] All HELP and TYPE comments present

### Performance Benchmarks
- [ ] Metric collection time: < 100ms (target from A6)
- [ ] HTTP response time: < 500ms (including network)
- [ ] Load test: 100 requests succeed with < 1% failure rate
- [ ] Memory stable across 100 requests (no leaks)
- [ ] VAPIX cache hit rate > 90% in burst scenarios

### Security Validation
- [ ] VAPIX credentials not logged or exposed
- [ ] Credentials zeroed on cleanup
- [ ] D-Bus permission properly declared in manifest
- [ ] No sensitive information in metric output

---

## Dependency Matrix

| Module | Dependencies | Purpose |
|--------|-------------|---------|
| metrics_helpers.c | stdio.h, stdlib.h, string.h, sys/statfs.h, dirent.h | /proc parsing, disk stats |
| metrics.c | glib.h, metrics_helpers.h, vapix.h, http_server.h | Metric collection orchestration |
| vapix.c | curl/curl.h, jansson.h, glib.h, gio/gio.h, pthread.h | VAPIX HTTP client with caching |
| lrf_controller.c | metrics.h, vapix.h | Integration and counter management |

---

## Build Dependency Installation

### Dockerfile Requirements
Ensure these packages in build stage:
```dockerfile
RUN apt-get update && apt-get install -y \
    libglib2.0-dev \
    libcurl4-openssl-dev \
    libjansson-dev \
    pkg-config \
    build-essential
```

### Runtime Dependencies (already in AXIS OS)
- libglib-2.0.so
- libgio-2.0.so
- libcurl.so
- libjansson.so
- libpthread.so

---

## Error Scenarios and Expected Behavior

| Scenario | Expected Behavior | Validation |
|----------|------------------|------------|
| VAPIX init fails | Continue without temperature metric | Check syslog for warning, verify other metrics work |
| /proc/uptime unreadable | Skip uptime metric, continue | Check syslog warning, verify other metrics present |
| Network interface not found | Skip network metrics | Check syslog, verify system metrics still work |
| VAPIX timeout (5s) | Serve stale cache or skip metric | Check syslog, verify response < 6s total |
| Non-GET request | HTTP 405 response | Test with POST, verify error message |
| Concurrent requests | Thread-safe cache access | Load test with -c 10, verify no crashes |
| I2C sensor disconnected | ptz_i2c_errors_total increments | Disconnect sensor, make distance request, check counter |

---

## Rollback Plan

If metrics implementation causes issues:

1. **Quick Disable**: Remove metrics handler registration from lrf_controller.c:
   - Comment out `http_server_add_handler(http_server, "/metrics", metrics_handler, NULL);`
   - Rebuild and redeploy

2. **Full Rollback**: Revert to previous EAP version:
   ```bash
   ssh root@192.168.30.15 'eap-install.sh remove lrf_controller'
   scp lrf_controller_1_0_0_aarch64.eap root@192.168.30.15:/tmp/
   ssh root@192.168.30.15 'eap-install.sh install /tmp/lrf_controller_1_0_0_aarch64.eap'
   ```

3. **Partial Disable**: Remove only VAPIX integration:
   - Comment out `vapix_init()` and `collect_vapix_metrics()` calls
   - Rebuild - other metrics still functional

---

## Post-Deployment Monitoring

### Initial 24-Hour Checklist
- [ ] Monitor syslog for repeated error messages
- [ ] Check memory usage hourly (ps aux)
- [ ] Verify temperature metric updates every 60s
- [ ] Verify counters increment as expected
- [ ] Test metrics endpoint periodically
- [ ] Monitor camera system stability (no crashes)

### Long-Term Health Indicators
- [ ] Response time remains consistent
- [ ] Memory usage stable (no slow leaks)
- [ ] Counter values increase monotonically
- [ ] Temperature values remain in expected range
- [ ] No VAPIX authentication failures
- [ ] Cache hit rate remains high

---

## Implementation Timeline

**Estimated effort (assuming single developer):**
- Phase 1 (Helpers): 2-3 hours (file parsing logic)
- Phase 2 (Metrics): 2-3 hours (collection and formatting)
- Phase 3 (VAPIX): 3-4 hours (HTTP client, caching, D-Bus)
- Phase 4 (Integration): 1 hour (modify existing files)
- Phase 5 (Manifest): 15 minutes
- Phase 6 (Build): 30 minutes
- Phase 7 (Testing): 3-4 hours (comprehensive validation)
- Phase 8 (Documentation): 1 hour

**Total: 13-17 hours of development + testing**

---

## Files Analyzed
- `.paf/findings/A6_FINDINGS.md` - Complete metrics module architecture
- `.paf/findings/A8_FINDINGS.md` - Complete VAPIX client design

## Blockers or Uncertainties
**None** - All specifications complete from A6 and A8:
- A6 provides complete metrics module interface, collection logic, helper functions, HTTP handler, error handling strategy, memory management, and integration pattern
- A8 provides complete VAPIX client with D-Bus credential acquisition, libcurl HTTP requests, caching with TTLs, thread safety, JSON parsing, and fallback strategies
- Makefile modifications clear from A6 recommendations
- Manifest D-Bus requirements specified in A8
- Testing procedures comprehensive from A6 recommendations

## Confidence Level
**HIGH** - Checklist is comprehensive, file-by-file, with specific implementation details for each function, complete testing procedures, validation criteria, and rollback plans. All dependencies resolved by A6 and A8 findings.
