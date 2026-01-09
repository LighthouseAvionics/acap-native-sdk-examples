# A11 Findings: Health Implementation Checklist

## Executive Summary
Complete implementation checklist for integrating the health status endpoint into lrf-controller. The implementation spans 6 files (4 new, 2 modified) and introduces threshold-based health monitoring with circular log buffering. Testing validates JSON schema compliance, threshold-based status transitions, and worst-case status aggregation logic.

## Implementation Checklist

### Phase 1: Create Core Health Files

#### Task 1.1: Create app/health.h
- [ ] Create new file `app/health.h`
- [ ] Add standard include guards (`#ifndef HEALTH_H`)
- [ ] Include dependencies:
  - `#include <jansson.h>`
  - `#include <time.h>`
  - `#include <stdbool.h>`
  - `#include <glib.h>`
- [ ] Define `HealthStatus` enum:
  - `HEALTH_STATUS_HEALTHY = 0`
  - `HEALTH_STATUS_DEGRADED = 1`
  - `HEALTH_STATUS_UNHEALTHY = 2`
- [ ] Define `ThresholdType` enum:
  - `THRESHOLD_TYPE_LOWER_BAD` (for memory/disk)
  - `THRESHOLD_TYPE_HIGHER_BAD` (for temperature/CPU)
- [ ] Define `HealthCheck` struct:
  - `char name[64]`
  - `double value`
  - `double warning_threshold`
  - `double critical_threshold`
  - `ThresholdType threshold_type`
  - `HealthStatus status`
- [ ] Define `DependencyCheck` struct:
  - `char service[64]`
  - `bool reachable`
  - `HealthStatus status`
- [ ] Define `HealthReport` struct:
  - `char service_name[64]`
  - `HealthStatus overall_status`
  - `char timestamp[32]`
  - `HealthCheck* checks`
  - `size_t check_count`
  - `DependencyCheck* dependencies`
  - `size_t dependency_count`
- [ ] Add function declarations:
  - `HealthStatus calculate_check_status(const HealthCheck* check);`
  - `HealthStatus calculate_overall_status(HealthCheck* checks, size_t check_count, DependencyCheck* deps, size_t dep_count);`
  - `const char* health_status_to_string(HealthStatus status);`
  - `const char* health_status_to_severity(HealthStatus status);`
  - `void get_iso8601_timestamp(char* buffer, size_t buffer_size);`
  - `void generate_health_report(HealthReport* report);`
  - `json_t* health_report_to_json(const HealthReport* report);`
  - `void health_handler(int client_fd, HttpRequest* request, gpointer user_data);`

---

#### Task 1.2: Create app/health.c
- [ ] Create new file `app/health.c`
- [ ] Add includes:
  - `#include "health.h"`
  - `#include "log_buffer.h"`
  - `#include "http_server.h"`
  - `#include <stdio.h>`
  - `#include <string.h>`
  - `#include <sys/statvfs.h>`
  - `#include <fcntl.h>`
  - `#include <unistd.h>`
  - `#include <syslog.h>`

**Function: calculate_check_status()**
- [ ] Implement threshold comparison logic:
  - If `THRESHOLD_TYPE_LOWER_BAD`:
    - Return `HEALTH_STATUS_UNHEALTHY` if `value < critical_threshold`
    - Return `HEALTH_STATUS_DEGRADED` if `value < warning_threshold`
    - Otherwise return `HEALTH_STATUS_HEALTHY`
  - If `THRESHOLD_TYPE_HIGHER_BAD`:
    - Return `HEALTH_STATUS_UNHEALTHY` if `value > critical_threshold`
    - Return `HEALTH_STATUS_DEGRADED` if `value > warning_threshold`
    - Otherwise return `HEALTH_STATUS_HEALTHY`

**Function: calculate_overall_status()**
- [ ] Initialize `overall` to `HEALTH_STATUS_HEALTHY`
- [ ] Loop through all checks:
  - Call `calculate_check_status()` for each check
  - Update `overall` to worst-case status using `if (checks[i].status > overall)`
- [ ] Loop through dependencies:
  - Set `status = HEALTH_STATUS_DEGRADED` if `reachable == false`
  - Set `status = HEALTH_STATUS_HEALTHY` if `reachable == true`
  - Update `overall` to worst-case status
- [ ] Return `overall`

**Function: health_status_to_string()**
- [ ] Implement switch statement:
  - `HEALTH_STATUS_HEALTHY` → `"healthy"`
  - `HEALTH_STATUS_DEGRADED` → `"degraded"`
  - `HEALTH_STATUS_UNHEALTHY` → `"unhealthy"`
  - `default` → `"unknown"`

**Function: health_status_to_severity()**
- [ ] Implement switch statement:
  - `HEALTH_STATUS_HEALTHY` → `"info"`
  - `HEALTH_STATUS_DEGRADED` → `"warning"`
  - `HEALTH_STATUS_UNHEALTHY` → `"critical"`
  - `default` → `"unknown"`

**Function: get_iso8601_timestamp()**
- [ ] Get current time with `time(NULL)`
- [ ] Convert to UTC with `gmtime(&now)`
- [ ] Format using `strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", tm_info)`

**Helper Function: get_memory_info()**
- [ ] Create function `int get_memory_info(double* available_mb)`
- [ ] Open and parse `/proc/meminfo`
- [ ] Find `MemAvailable:` line
- [ ] Parse value in kB and convert to MB
- [ ] Return 0 on success, -1 on error

**Helper Function: get_disk_stats()**
- [ ] Create function `int get_disk_stats(const char* path, double* free_mb)`
- [ ] Call `statvfs(path, &stat)`
- [ ] Calculate free bytes: `stat.f_bavail * stat.f_frsize`
- [ ] Convert to MB
- [ ] Return 0 on success, -1 on error

**Helper Function: get_temperature()**
- [ ] Create function `double get_temperature(void)`
- [ ] Open `/sys/class/thermal/thermal_zone0/temp`
- [ ] Read value (in millidegrees Celsius)
- [ ] Divide by 1000 to get degrees Celsius
- [ ] Return temperature or -1.0 on error

**Helper Function: check_i2c_bus_available()**
- [ ] Create function `bool check_i2c_bus_available(void)`
- [ ] Attempt to open `/dev/i2c-0` with `O_RDWR`
- [ ] If successful, close fd and return `true`
- [ ] If failed, return `false`

**Function: generate_health_report()**
- [ ] Set `report->service_name` to `"lrf-controller"`
- [ ] Call `get_iso8601_timestamp(report->timestamp, sizeof(report->timestamp))`
- [ ] Create static array: `static HealthCheck checks[4];`
- [ ] Set `report->checks = checks` and `report->check_count = 4`

**Check 1: Memory Available**
- [ ] Call `get_memory_info(&mem_value)`
- [ ] Set `checks[0].name = "memory_available_mb"`
- [ ] Set `checks[0].value = mem_value`
- [ ] Set `checks[0].warning_threshold = 50.0`
- [ ] Set `checks[0].critical_threshold = 20.0`
- [ ] Set `checks[0].threshold_type = THRESHOLD_TYPE_LOWER_BAD`

**Check 2: Disk Free Space**
- [ ] Call `get_disk_stats("/", &disk_value)`
- [ ] Set `checks[1].name = "disk_free_mb"`
- [ ] Set `checks[1].value = disk_value`
- [ ] Set `checks[1].warning_threshold = 100.0`
- [ ] Set `checks[1].critical_threshold = 50.0`
- [ ] Set `checks[1].threshold_type = THRESHOLD_TYPE_LOWER_BAD`

**Check 3: Temperature**
- [ ] Call `get_temperature()` → `temp_value`
- [ ] Set `checks[2].name = "temperature_celsius"`
- [ ] Set `checks[2].value = temp_value`
- [ ] Set `checks[2].warning_threshold = 70.0`
- [ ] Set `checks[2].critical_threshold = 80.0`
- [ ] Set `checks[2].threshold_type = THRESHOLD_TYPE_HIGHER_BAD`

**Check 4: CPU Usage (Optional - Set to 0)**
- [ ] Set `checks[3].name = "cpu_usage_percent"`
- [ ] Set `checks[3].value = 0.0` (placeholder)
- [ ] Set `checks[3].warning_threshold = 80.0`
- [ ] Set `checks[3].critical_threshold = 95.0`
- [ ] Set `checks[3].threshold_type = THRESHOLD_TYPE_HIGHER_BAD`
- [ ] Add comment: `// TODO: Implement CPU usage monitoring`

**Dependencies**
- [ ] Create static array: `static DependencyCheck deps[1];`
- [ ] Set `report->dependencies = deps` and `report->dependency_count = 1`
- [ ] Set `deps[0].service = "i2c-bus-0"`
- [ ] Call `check_i2c_bus_available()` → `deps[0].reachable`

**Overall Status Calculation**
- [ ] Call `calculate_overall_status(report->checks, report->check_count, report->dependencies, report->dependency_count)`
- [ ] Store result in `report->overall_status`

**Function: health_report_to_json()**
- [ ] Create root object: `json_t* root = json_object();`
- [ ] Add root fields:
  - `json_object_set_new(root, "service", json_string(report->service_name));`
  - `json_object_set_new(root, "status", json_string(health_status_to_string(report->overall_status)));`
  - `json_object_set_new(root, "severity", json_string(health_status_to_severity(report->overall_status)));`
  - `json_object_set_new(root, "timestamp", json_string(report->timestamp));`
- [ ] Create checks array: `json_t* checks_array = json_array();`
- [ ] Loop through `report->checks`:
  - Create check object with fields: `name`, `value`, `warning`, `critical`, `status`
  - Append to `checks_array`
- [ ] Add checks array to root: `json_object_set_new(root, "checks", checks_array);`
- [ ] Create dependencies array: `json_t* deps_array = json_array();`
- [ ] Loop through `report->dependencies`:
  - Create dependency object with fields: `service`, `reachable`, `status`
  - Append to `deps_array`
- [ ] Add dependencies array to root: `json_object_set_new(root, "dependencies", deps_array);`
- [ ] Return `root`

**Function: health_handler()**
- [ ] Check if `request->method` is `"GET"`:
  - If not, call `http_send_error(client_fd, 405, "Method not allowed")` and return
- [ ] Create `HealthReport report;`
- [ ] Call `generate_health_report(&report);`
- [ ] Serialize to JSON: `json_t* json = health_report_to_json(&report);`
- [ ] Dump to string: `char* json_str = json_dumps(json, JSON_INDENT(2));`
- [ ] Send response: `http_send_json(client_fd, 200, json_str);`
- [ ] Cleanup: `free(json_str);` and `json_decref(json);`

---

#### Task 1.3: Create app/log_buffer.h
- [ ] Create new file `app/log_buffer.h`
- [ ] Add standard include guards (`#ifndef LOG_BUFFER_H`)
- [ ] Include dependencies:
  - `#include <time.h>`
  - `#include <stddef.h>`
  - `#include <jansson.h>`
  - `#include <pthread.h>`
  - `#include <stdbool.h>`
- [ ] Define constants:
  - `#define MAX_LOG_ENTRIES 100`
  - `#define MAX_MESSAGE_LENGTH 256`
  - `#define MAX_SEVERITY_LENGTH 16`
- [ ] Define `LogEntry` struct:
  - `time_t timestamp`
  - `char severity[MAX_SEVERITY_LENGTH]`
  - `char message[MAX_MESSAGE_LENGTH]`
- [ ] Define `LogBuffer` struct:
  - `LogEntry entries[MAX_LOG_ENTRIES]`
  - `size_t head`
  - `size_t count`
  - `pthread_mutex_t lock`
- [ ] Add function declarations:
  - `void log_buffer_init(LogBuffer* buffer);`
  - `void log_buffer_destroy(LogBuffer* buffer);`
  - `void log_event(const char* severity, const char* format, ...);`
  - `void get_recent_logs(json_t* logs_array);`
  - `void get_recent_logs_limited(json_t* logs_array, size_t max_entries);`
  - `int severity_to_syslog(const char* severity);`

---

#### Task 1.4: Create app/log_buffer.c
- [ ] Create new file `app/log_buffer.c`
- [ ] Add includes:
  - `#include "log_buffer.h"`
  - `#include <stdio.h>`
  - `#include <stdarg.h>`
  - `#include <string.h>`
  - `#include <syslog.h>`
- [ ] Create global buffer: `static LogBuffer g_log_buffer = {0};`
- [ ] Create initialization flag: `static bool g_log_buffer_initialized = false;`

**Function: log_buffer_init()**
- [ ] Zero buffer: `memset(buffer, 0, sizeof(LogBuffer));`
- [ ] Initialize mutex: `pthread_mutex_init(&buffer->lock, NULL);`
- [ ] Set `g_log_buffer_initialized = true;`

**Function: log_buffer_destroy()**
- [ ] Destroy mutex: `pthread_mutex_destroy(&buffer->lock);`
- [ ] Set `g_log_buffer_initialized = false;`

**Function: log_event()**
- [ ] Check if buffer is initialized; if not, call `log_buffer_init(&g_log_buffer);`
- [ ] Lock mutex: `pthread_mutex_lock(&g_log_buffer.lock);`
- [ ] Get next entry slot: `LogEntry* entry = &g_log_buffer.entries[g_log_buffer.head];`
- [ ] Set timestamp: `entry->timestamp = time(NULL);`
- [ ] Copy severity: `strncpy(entry->severity, severity, MAX_SEVERITY_LENGTH - 1);`
- [ ] Null-terminate: `entry->severity[MAX_SEVERITY_LENGTH - 1] = '\0';`
- [ ] Format message with va_args:
  - `va_list args;`
  - `va_start(args, format);`
  - `vsnprintf(entry->message, MAX_MESSAGE_LENGTH, format, args);`
  - `va_end(args);`
- [ ] Advance head pointer: `g_log_buffer.head = (g_log_buffer.head + 1) % MAX_LOG_ENTRIES;`
- [ ] Update count (saturate at MAX): `if (g_log_buffer.count < MAX_LOG_ENTRIES) g_log_buffer.count++;`
- [ ] Unlock mutex: `pthread_mutex_unlock(&g_log_buffer.lock);`
- [ ] Log to syslog: `syslog(severity_to_syslog(severity), "[%s] %s", severity, entry->message);`

**Function: get_recent_logs()**
- [ ] Call `get_recent_logs_limited(logs_array, MAX_LOG_ENTRIES);`

**Function: get_recent_logs_limited()**
- [ ] Check if buffer is initialized; if not, return early
- [ ] Lock mutex: `pthread_mutex_lock(&g_log_buffer.lock);`
- [ ] Calculate `entries_to_export = min(max_entries, g_log_buffer.count)`
- [ ] Calculate start index: `start_idx = (g_log_buffer.head + MAX_LOG_ENTRIES - entries_to_export) % MAX_LOG_ENTRIES;`
- [ ] Loop through entries (chronological order):
  - Calculate index: `idx = (start_idx + i) % MAX_LOG_ENTRIES`
  - Get entry: `LogEntry* entry = &g_log_buffer.entries[idx];`
  - Format timestamp as ISO 8601:
    - `struct tm* tm_info = gmtime(&entry->timestamp);`
    - `strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", tm_info);`
  - Create JSON object: `json_pack("{s:s, s:s, s:s}", "timestamp", timestamp_str, "severity", entry->severity, "message", entry->message)`
  - Append to array: `json_array_append_new(logs_array, log_obj);`
- [ ] Unlock mutex: `pthread_mutex_unlock(&g_log_buffer.lock);`

**Function: severity_to_syslog()**
- [ ] Implement mapping:
  - `"critical"` → `LOG_CRIT`
  - `"warning"` → `LOG_WARNING`
  - `"info"` → `LOG_INFO`
  - `"debug"` → `LOG_DEBUG`
  - `default` → `LOG_NOTICE`

---

### Phase 2: Configure Health Thresholds

#### Task 2.1: Verify Threshold Definitions in generate_health_report()

**Memory Available:**
- Warning threshold: `< 50 MB`
- Critical threshold: `< 20 MB`
- Threshold type: `THRESHOLD_TYPE_LOWER_BAD`

**Disk Free Space:**
- Warning threshold: `< 100 MB`
- Critical threshold: `< 50 MB`
- Threshold type: `THRESHOLD_TYPE_LOWER_BAD`

**Temperature:**
- Warning threshold: `> 70°C`
- Critical threshold: `> 80°C`
- Threshold type: `THRESHOLD_TYPE_HIGHER_BAD`

**CPU Usage (Placeholder):**
- Warning threshold: `> 80%`
- Critical threshold: `> 95%`
- Threshold type: `THRESHOLD_TYPE_HIGHER_BAD`

---

### Phase 3: Modify Existing Files

#### Task 3.1: Modify app/lrf_controller.c
- [ ] Open `app/lrf_controller.c`
- [ ] Add `#include "health.h"` at the top with other includes
- [ ] Add `#include "log_buffer.h"` at the top with other includes
- [ ] Locate the `main()` function
- [ ] Find the section where HTTP handlers are registered (around line 173-180)
- [ ] After existing handler registrations (`/distance`, `/command`, `/status`), add:
  ```c
  http_server_add_handler(http_server, "/health", health_handler, NULL);
  ```
- [ ] Optionally add initialization: `log_buffer_init(&g_log_buffer);` early in `main()` (before HTTP server start)
- [ ] Optionally add cleanup: `log_buffer_destroy(&g_log_buffer);` before program exit

---

#### Task 3.2: Modify app/Makefile
- [ ] Open `app/Makefile`
- [ ] Locate the `SRCS` variable definition
- [ ] Add `health.c` to the source list
- [ ] Add `log_buffer.c` to the source list
- [ ] Verify jansson library is already linked (should be present from existing code)
- [ ] Verify pthread library is linked (add `-lpthread` if not present)

Example modification:
```makefile
SRCS = lrf_controller.c http_server.c health.c log_buffer.c
```

---

### Phase 4: Build & Deploy

#### Task 4.1: Build ACAP
- [ ] Clean previous builds:
  ```bash
  docker build --no-cache --build-arg ARCH=aarch64 -t lrf-controller:v1.1.0 .
  ```
  Or with existing cache:
  ```bash
  docker build --build-arg ARCH=aarch64 -t lrf-controller:v1.1.0 .
  ```
- [ ] Verify build succeeds without errors
- [ ] Locate output `.eap` file (e.g., `lrf_controller_1_1_0_aarch64.eap`)

---

#### Task 4.2: Deploy to Camera
- [ ] Copy package to camera:
  ```bash
  scp lrf_controller_1_1_0_aarch64.eap root@192.168.30.15:/tmp/
  ```
- [ ] Install package:
  ```bash
  ssh root@192.168.30.15 'eap-install.sh install /tmp/lrf_controller_1_1_0_aarch64.eap'
  ```
- [ ] Start application:
  ```bash
  ssh root@192.168.30.15 'eap-install.sh start lrf_controller'
  ```
- [ ] Verify application is running:
  ```bash
  ssh root@192.168.30.15 'eap-install.sh status lrf_controller'
  ```

---

### Phase 5: Testing

#### Task 5.1: Functional Testing

**Test 5.1.1: Basic Health Endpoint Access**
- [ ] Test health endpoint (HTTP):
  ```bash
  curl http://192.168.30.15:8080/health | jq .
  ```
- [ ] Verify HTTP 200 response
- [ ] Verify response is valid JSON
- [ ] Verify output is human-readable (pretty-printed)

**Test 5.1.2: Verify JSON Structure**
- [ ] Verify root fields present:
  - `service` (string)
  - `status` (string: "healthy", "degraded", or "unhealthy")
  - `severity` (string: "info", "warning", or "critical")
  - `timestamp` (string: ISO 8601 format)
  - `checks` (array)
  - `dependencies` (array)

**Test 5.1.3: Verify Checks Array**
- [ ] Verify `checks` array has 4 elements
- [ ] For each check, verify fields:
  - `name` (string)
  - `value` (number)
  - `warning` (number)
  - `critical` (number)
  - `status` (string)
- [ ] Verify check names:
  - `"memory_available_mb"`
  - `"disk_free_mb"`
  - `"temperature_celsius"`
  - `"cpu_usage_percent"`

**Test 5.1.4: Verify Dependencies Array**
- [ ] Verify `dependencies` array has 1 element
- [ ] Verify dependency fields:
  - `service` = `"i2c-bus-0"`
  - `reachable` (boolean)
  - `status` (string)

---

#### Task 5.2: Status Calculation Testing

**Test 5.2.1: HEALTHY Status**
- [ ] Ensure all checks pass:
  - Memory available > 50 MB
  - Disk free > 100 MB
  - Temperature < 70°C
  - I2C bus reachable
- [ ] Verify overall `status` = `"healthy"`
- [ ] Verify overall `severity` = `"info"`

**Test 5.2.2: DEGRADED Status**
- [ ] Simulate degraded condition (one warning threshold crossed):
  - Option A: Fill memory to leave 45 MB available
  - Option B: Fill disk to leave 90 MB free
  - Option C: Heat system to 72°C (difficult to simulate)
- [ ] Verify overall `status` = `"degraded"`
- [ ] Verify overall `severity` = `"warning"`
- [ ] Verify the specific check shows `status: "degraded"`

**Test 5.2.3: UNHEALTHY Status**
- [ ] Simulate unhealthy condition (one critical threshold crossed):
  - Option A: Fill memory to leave 15 MB available
  - Option B: Fill disk to leave 45 MB free
- [ ] Verify overall `status` = `"unhealthy"`
- [ ] Verify overall `severity` = `"critical"`
- [ ] Verify the specific check shows `status: "unhealthy"`

**Test 5.2.4: Worst-Case Aggregation**
- [ ] Create mixed conditions:
  - One check HEALTHY
  - One check DEGRADED
  - One check UNHEALTHY
- [ ] Verify overall status is `"unhealthy"` (worst-case wins)

---

#### Task 5.3: Threshold Testing

**Test 5.3.1: Memory Thresholds**
- [ ] Verify memory values are read from `/proc/meminfo`
- [ ] Simulate warning condition (~45 MB available):
  - Verify check status transitions to `"degraded"`
- [ ] Simulate critical condition (~15 MB available):
  - Verify check status transitions to `"unhealthy"`

**Test 5.3.2: Disk Thresholds**
- [ ] Verify disk values are read via `statvfs("/")`
- [ ] Simulate warning condition (~90 MB free):
  - Verify check status transitions to `"degraded"`
- [ ] Simulate critical condition (~45 MB free):
  - Verify check status transitions to `"unhealthy"`

**Test 5.3.3: Temperature Thresholds**
- [ ] Verify temperature is read from `/sys/class/thermal/thermal_zone0/temp`
- [ ] If possible, simulate warning condition (72°C):
  - Verify check status transitions to `"degraded"`
- [ ] If possible, simulate critical condition (82°C):
  - Verify check status transitions to `"unhealthy"`

---

#### Task 5.4: JSON Schema Validation

**Test 5.4.1: Create Python Validation Script**
- [ ] Create `scripts/validate_health_schema.py`:
  ```python
  #!/usr/bin/env python3
  import json
  import sys
  import requests
  from jsonschema import validate, ValidationError

  schema = {
      "type": "object",
      "required": ["service", "status", "severity", "timestamp", "checks", "dependencies"],
      "properties": {
          "service": {"type": "string"},
          "status": {"enum": ["healthy", "degraded", "unhealthy"]},
          "severity": {"enum": ["info", "warning", "critical"]},
          "timestamp": {"type": "string", "format": "date-time"},
          "checks": {
              "type": "array",
              "items": {
                  "type": "object",
                  "required": ["name", "value", "warning", "critical", "status"],
                  "properties": {
                      "name": {"type": "string"},
                      "value": {"type": "number"},
                      "warning": {"type": "number"},
                      "critical": {"type": "number"},
                      "status": {"enum": ["healthy", "degraded", "unhealthy"]}
                  }
              }
          },
          "dependencies": {
              "type": "array",
              "items": {
                  "type": "object",
                  "required": ["service", "reachable", "status"],
                  "properties": {
                      "service": {"type": "string"},
                      "reachable": {"type": "boolean"},
                      "status": {"enum": ["healthy", "degraded", "unhealthy"]}
                  }
              }
          }
      }
  }

  # Fetch health data
  url = "http://192.168.30.15:8080/health"
  response = requests.get(url, timeout=5)

  if response.status_code != 200:
      print(f"❌ HTTP error: {response.status_code}")
      sys.exit(1)

  data = response.json()

  try:
      validate(data, schema)
      print("✓ Schema validation passed")
      print(f"✓ Service: {data['service']}")
      print(f"✓ Status: {data['status']}")
      print(f"✓ Severity: {data['severity']}")
      print(f"✓ Checks: {len(data['checks'])} checks")
      print(f"✓ Dependencies: {len(data['dependencies'])} dependencies")
  except ValidationError as e:
      print(f"❌ Schema validation failed: {e.message}")
      sys.exit(1)
  ```

**Test 5.4.2: Run Validation Script**
- [ ] Make script executable: `chmod +x scripts/validate_health_schema.py`
- [ ] Install dependencies: `pip3 install requests jsonschema`
- [ ] Run script: `python3 scripts/validate_health_schema.py`
- [ ] Verify output shows `✓ Schema validation passed`

---

#### Task 5.5: Log Buffer Testing

**Test 5.5.1: Trigger Log Events**
- [ ] Add test log calls in `health_handler()`:
  ```c
  log_event("info", "Health check requested");
  ```
- [ ] Rebuild and redeploy application
- [ ] Trigger health endpoint multiple times
- [ ] Verify logs appear in camera syslog:
  ```bash
  ssh root@192.168.30.15 'tail -f /var/log/syslog | grep lrf_controller'
  ```

**Test 5.5.2: Circular Buffer Wraparound**
- [ ] Trigger 150+ log events (exceeds MAX_LOG_ENTRIES=100)
- [ ] Verify oldest logs are overwritten
- [ ] Verify buffer maintains exactly 100 entries

**Test 5.5.3: Log Retrieval (Future)**
- [ ] If `/logs` endpoint is implemented, test:
  ```bash
  curl http://192.168.30.15:8080/logs | jq .
  ```
- [ ] Verify JSON array with recent log entries
- [ ] Verify chronological order (oldest to newest)

---

#### Task 5.6: Performance Testing

**Test 5.6.1: Response Time**
- [ ] Measure health endpoint response time:
  ```bash
  curl -o /dev/null -s -w "Time: %{time_total}s\n" http://192.168.30.15:8080/health
  ```
- [ ] Verify response time < 500ms (preferably < 200ms)

**Test 5.6.2: Concurrent Requests**
- [ ] Test concurrent access:
  ```bash
  for i in {1..10}; do
    curl http://192.168.30.15:8080/health > /dev/null 2>&1 &
  done
  wait
  ```
- [ ] Verify no crashes or errors
- [ ] Verify all responses are valid JSON

---

#### Task 5.7: Error Handling Testing

**Test 5.7.1: Method Not Allowed**
- [ ] Test POST request:
  ```bash
  curl -X POST http://192.168.30.15:8080/health
  ```
- [ ] Verify HTTP 405 Method Not Allowed response

**Test 5.7.2: Missing Dependencies**
- [ ] Disconnect I2C bus (if possible)
- [ ] Verify dependency shows `reachable: false`
- [ ] Verify overall status becomes at least `"degraded"`

---

## Files to Create/Modify Summary

**New Files (4):**
1. `app/health.h` - Health module interface
2. `app/health.c` - Health module implementation
3. `app/log_buffer.h` - Log buffer interface
4. `app/log_buffer.c` - Log buffer implementation

**Modified Files (2):**
1. `app/lrf_controller.c` - Add health handler registration and includes
2. `app/Makefile` - Add health.c and log_buffer.c to build

**Optional Test Files (1):**
1. `scripts/validate_health_schema.py` - JSON schema validation script

**Total Changes:** 6-7 files

---

## Validation Criteria

### Success Metrics
- [ ] Health endpoint returns valid victor-health compatible JSON
- [ ] Status calculation correct (worst-case aggregation verified)
- [ ] Thresholds trigger status changes correctly (warning → degraded, critical → unhealthy)
- [ ] JSON schema validation passes (all required fields present)
- [ ] Response time < 500ms under normal conditions
- [ ] No crashes or memory leaks during testing
- [ ] Log buffer successfully captures events and integrates with syslog
- [ ] Circular buffer wraparound works correctly (oldest entries overwritten)

---

## Threshold Configuration Reference

| Check                  | Warning Threshold | Critical Threshold | Threshold Type    |
|------------------------|-------------------|--------------------|-------------------|
| Memory Available (MB)  | < 50              | < 20               | LOWER_BAD         |
| Disk Free Space (MB)   | < 100             | < 50               | LOWER_BAD         |
| Temperature (°C)       | > 70              | > 80               | HIGHER_BAD        |
| CPU Usage (%)          | > 80              | > 95               | HIGHER_BAD        |

---

## Sample Expected Output

```json
{
  "service": "lrf-controller",
  "status": "healthy",
  "severity": "info",
  "timestamp": "2026-01-09T14:23:45Z",
  "checks": [
    {
      "name": "memory_available_mb",
      "value": 120.5,
      "warning": 50.0,
      "critical": 20.0,
      "status": "healthy"
    },
    {
      "name": "disk_free_mb",
      "value": 450.2,
      "warning": 100.0,
      "critical": 50.0,
      "status": "healthy"
    },
    {
      "name": "temperature_celsius",
      "value": 62.3,
      "warning": 70.0,
      "critical": 80.0,
      "status": "healthy"
    },
    {
      "name": "cpu_usage_percent",
      "value": 0.0,
      "warning": 80.0,
      "critical": 95.0,
      "status": "healthy"
    }
  ],
  "dependencies": [
    {
      "service": "i2c-bus-0",
      "reachable": true,
      "status": "healthy"
    }
  ]
}
```

---

## Files Analyzed
- `.paf/findings/A7_FINDINGS.md` - Health module architecture and implementation details
- `.paf/findings/A9_FINDINGS.md` - Log buffer design and integration patterns

## Blockers or Uncertainties
None - All design specifications complete from A7 and A9 findings.

## Confidence Level
**HIGH** - Complete architecture with clear implementation steps, threshold definitions, and comprehensive testing procedures.
