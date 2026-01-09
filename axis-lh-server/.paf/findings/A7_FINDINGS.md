# A7 Findings: Health Module Architecture

## Executive Summary
The health.c module provides a threshold-based health monitoring system for lrf-controller that calculates per-check status (HEALTHY/DEGRADED/UNHEALTHY) using warning/critical thresholds, aggregates overall status using worst-case logic, and serializes results to victor-health compatible JSON using Jansson. The module integrates with the existing HTTP server via health_handler() registered at /health endpoint.

## Key Findings
1. **Data Structures**: HealthCheck, DependencyCheck, HealthReport structs with threshold metadata
2. **Status Calculation**: Threshold-based per-check comparison (< for lower-bad, > for higher-bad), worst-case aggregation for overall status
3. **JSON Serialization**: Jansson library for victor-health schema output with service, status, severity, timestamp, checks, dependencies
4. **Integration**: health_handler() function registered with http_server_add_handler() from A1 findings
5. **Configuration**: Static threshold definitions with THRESHOLD_TYPE_LOWER_BAD/HIGHER_BAD for direction control

## Detailed Analysis

### Module Interface (health.h)

```c
#ifndef HEALTH_H
#define HEALTH_H

#include <jansson.h>
#include <time.h>
#include <stdbool.h>
#include <glib.h>

// Status enumeration (from A5 schema)
typedef enum {
    HEALTH_STATUS_HEALTHY = 0,
    HEALTH_STATUS_DEGRADED = 1,
    HEALTH_STATUS_UNHEALTHY = 2
} HealthStatus;

// Threshold type determines comparison direction
typedef enum {
    THRESHOLD_TYPE_LOWER_BAD,  // Lower values are bad (memory, disk)
    THRESHOLD_TYPE_HIGHER_BAD  // Higher values are bad (temperature, CPU)
} ThresholdType;

// Individual health check with threshold configuration
typedef struct {
    char name[64];
    double value;
    double warning_threshold;
    double critical_threshold;
    ThresholdType threshold_type;
    HealthStatus status;
} HealthCheck;

// Dependency check for external services
typedef struct {
    char service[64];
    bool reachable;
    HealthStatus status;
} DependencyCheck;

// Complete health report aggregating all checks
typedef struct {
    char service_name[64];
    HealthStatus overall_status;
    char timestamp[32];
    HealthCheck* checks;
    size_t check_count;
    DependencyCheck* dependencies;
    size_t dependency_count;
} HealthReport;

// Status calculation functions
HealthStatus calculate_check_status(const HealthCheck* check);
HealthStatus calculate_overall_status(HealthCheck* checks, size_t check_count,
                                      DependencyCheck* deps, size_t dep_count);

// Status conversion utilities
const char* health_status_to_string(HealthStatus status);
const char* health_status_to_severity(HealthStatus status);

// Timestamp generation (ISO 8601)
void get_iso8601_timestamp(char* buffer, size_t buffer_size);

// Health report generation (populates checks, calculates status)
void generate_health_report(HealthReport* report);

// JSON serialization for victor-health schema
json_t* health_report_to_json(const HealthReport* report);

// HTTP endpoint handler (integrates with A1 HTTP server)
void health_handler(int client_fd, HttpRequest* request, gpointer user_data);

#endif // HEALTH_H
```

---

### Data Structures (Detailed)

**HealthCheck Structure:**
- `name[64]`: Check identifier (e.g., "memory_available_mb", "temperature_celsius")
- `value`: Current measured value (double for precision)
- `warning_threshold`: Threshold for DEGRADED status
- `critical_threshold`: Threshold for UNHEALTHY status
- `threshold_type`: LOWER_BAD (memory/disk) or HIGHER_BAD (temp/CPU)
- `status`: Calculated status (populated by calculate_check_status())

**DependencyCheck Structure:**
- `service[64]`: Dependency name (e.g., "i2c-bus-0")
- `reachable`: Boolean indicating if dependency is accessible
- `status`: Dependency's health status (HEALTHY if reachable, DEGRADED otherwise)

**HealthReport Structure:**
- `service_name[64]`: Service identifier (constant "lrf-controller")
- `overall_status`: Worst-case aggregated status across all checks
- `timestamp[32]`: ISO 8601 timestamp string
- `checks`: Pointer to static array of HealthCheck structs
- `check_count`: Number of health checks (currently 4)
- `dependencies`: Pointer to static array of DependencyCheck structs
- `dependency_count`: Number of dependencies (currently 1)

---

### Status Calculation Algorithm

**Implementation of calculate_check_status():**
```c
HealthStatus calculate_check_status(const HealthCheck* check) {
    if (check->threshold_type == THRESHOLD_TYPE_LOWER_BAD) {
        // For memory/disk: lower values are worse
        if (check->value < check->critical_threshold) {
            return HEALTH_STATUS_UNHEALTHY;
        } else if (check->value < check->warning_threshold) {
            return HEALTH_STATUS_DEGRADED;
        } else {
            return HEALTH_STATUS_HEALTHY;
        }
    } else {
        // For temperature/CPU: higher values are worse
        if (check->value > check->critical_threshold) {
            return HEALTH_STATUS_UNHEALTHY;
        } else if (check->value > check->warning_threshold) {
            return HEALTH_STATUS_DEGRADED;
        } else {
            return HEALTH_STATUS_HEALTHY;
        }
    }
}
```

**Implementation of calculate_overall_status():**
```c
HealthStatus calculate_overall_status(HealthCheck* checks, size_t check_count,
                                      DependencyCheck* deps, size_t dep_count) {
    HealthStatus overall = HEALTH_STATUS_HEALTHY;

    // Calculate and aggregate health check statuses
    for (size_t i = 0; i < check_count; i++) {
        checks[i].status = calculate_check_status(&checks[i]);
        if (checks[i].status > overall) {
            overall = checks[i].status;
        }
    }

    // Check dependencies (unreachable = DEGRADED minimum)
    for (size_t i = 0; i < dep_count; i++) {
        if (!deps[i].reachable) {
            deps[i].status = HEALTH_STATUS_DEGRADED;
            if (HEALTH_STATUS_DEGRADED > overall) {
                overall = HEALTH_STATUS_DEGRADED;
            }
        } else {
            deps[i].status = HEALTH_STATUS_HEALTHY;
        }
    }

    return overall;
}
```

**Aggregation Logic:**
1. Any UNHEALTHY check → Overall UNHEALTHY
2. Any DEGRADED check → Overall DEGRADED (if no UNHEALTHY)
3. All checks HEALTHY → Overall HEALTHY
4. Enum ordering enables simple > comparison (UNHEALTHY=2 > DEGRADED=1 > HEALTHY=0)

---

### Status Conversion Utilities

**Implementation:**
```c
const char* health_status_to_string(HealthStatus status) {
    switch (status) {
        case HEALTH_STATUS_HEALTHY:
            return "healthy";
        case HEALTH_STATUS_DEGRADED:
            return "degraded";
        case HEALTH_STATUS_UNHEALTHY:
            return "unhealthy";
        default:
            return "unknown";
    }
}

const char* health_status_to_severity(HealthStatus status) {
    switch (status) {
        case HEALTH_STATUS_HEALTHY:
            return "info";
        case HEALTH_STATUS_DEGRADED:
            return "warning";
        case HEALTH_STATUS_UNHEALTHY:
            return "critical";
        default:
            return "unknown";
    }
}
```

**Mapping:**
- HEALTHY → "healthy" / "info"
- DEGRADED → "degraded" / "warning"
- UNHEALTHY → "unhealthy" / "critical"

---

### Timestamp Generation (ISO 8601)

**Implementation:**
```c
void get_iso8601_timestamp(char* buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", tm_info);
}
```

**Output Format:** `2026-01-09T12:34:56Z` (UTC timezone)

---

### JSON Serialization

**Implementation of health_report_to_json():**
```c
json_t* health_report_to_json(const HealthReport* report) {
    json_t* root = json_object();

    // Root fields
    json_object_set_new(root, "service", json_string(report->service_name));
    json_object_set_new(root, "status",
                       json_string(health_status_to_string(report->overall_status)));
    json_object_set_new(root, "severity",
                       json_string(health_status_to_severity(report->overall_status)));
    json_object_set_new(root, "timestamp", json_string(report->timestamp));

    // Checks array
    json_t* checks_array = json_array();
    for (size_t i = 0; i < report->check_count; i++) {
        json_t* check_obj = json_object();
        json_object_set_new(check_obj, "name", json_string(report->checks[i].name));
        json_object_set_new(check_obj, "value", json_real(report->checks[i].value));
        json_object_set_new(check_obj, "warning", json_real(report->checks[i].warning_threshold));
        json_object_set_new(check_obj, "critical", json_real(report->checks[i].critical_threshold));
        json_object_set_new(check_obj, "status",
                           json_string(health_status_to_string(report->checks[i].status)));
        json_array_append_new(checks_array, check_obj);
    }
    json_object_set_new(root, "checks", checks_array);

    // Dependencies array
    json_t* deps_array = json_array();
    for (size_t i = 0; i < report->dependency_count; i++) {
        json_t* dep_obj = json_object();
        json_object_set_new(dep_obj, "service", json_string(report->dependencies[i].service));
        json_object_set_new(dep_obj, "reachable", json_boolean(report->dependencies[i].reachable));
        json_object_set_new(dep_obj, "status",
                           json_string(health_status_to_string(report->dependencies[i].status)));
        json_array_append_new(deps_array, dep_obj);
    }
    json_object_set_new(root, "dependencies", deps_array);

    return root;
}
```

**Output Schema (victor-health compatible):**
```json
{
  "service": "lrf-controller",
  "status": "healthy",
  "severity": "info",
  "timestamp": "2026-01-09T12:34:56Z",
  "checks": [
    {
      "name": "memory_available_mb",
      "value": 120.5,
      "warning": 50.0,
      "critical": 20.0,
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

### Health Report Generation

**Implementation of generate_health_report():**
```c
void generate_health_report(HealthReport* report) {
    // Initialize report metadata
    strncpy(report->service_name, "lrf-controller", sizeof(report->service_name));
    get_iso8601_timestamp(report->timestamp, sizeof(report->timestamp));

    // Define checks (static array with lifetime beyond function)
    static HealthCheck checks[4];
    report->checks = checks;
    report->check_count = 4;

    // Check 1: Memory available (LOWER_BAD)
    MemoryInfo mem;
    if (get_memory_info(&mem) == 0) {
        snprintf(checks[0].name, sizeof(checks[0].name), "memory_available_mb");
        checks[0].value = mem.available_bytes / (1024.0 * 1024.0);
        checks[0].warning_threshold = 50.0;
        checks[0].critical_threshold = 20.0;
        checks[0].threshold_type = THRESHOLD_TYPE_LOWER_BAD;
    }

    // Check 2: Disk free space (LOWER_BAD)
    DiskStats disk;
    if (get_disk_stats("/", &disk) == 0) {
        snprintf(checks[1].name, sizeof(checks[1].name), "disk_free_mb");
        checks[1].value = disk.free_bytes / (1024.0 * 1024.0);
        checks[1].warning_threshold = 100.0;
        checks[1].critical_threshold = 50.0;
        checks[1].threshold_type = THRESHOLD_TYPE_LOWER_BAD;
    }

    // Check 3: Temperature (HIGHER_BAD)
    double temp = get_cached_temperature();
    if (temp >= 0) {
        snprintf(checks[2].name, sizeof(checks[2].name), "temperature_celsius");
        checks[2].value = temp;
        checks[2].warning_threshold = 70.0;
        checks[2].critical_threshold = 80.0;
        checks[2].threshold_type = THRESHOLD_TYPE_HIGHER_BAD;
    }

    // Check 4: CPU usage (HIGHER_BAD) - placeholder
    snprintf(checks[3].name, sizeof(checks[3].name), "cpu_usage_percent");
    checks[3].value = 0.0;  // TODO: Implement CPU usage monitoring
    checks[3].warning_threshold = 80.0;
    checks[3].critical_threshold = 95.0;
    checks[3].threshold_type = THRESHOLD_TYPE_HIGHER_BAD;

    // Dependencies
    static DependencyCheck deps[1];
    report->dependencies = deps;
    report->dependency_count = 1;

    // I2C bus check
    snprintf(deps[0].service, sizeof(deps[0].service), "i2c-bus-0");
    deps[0].reachable = check_i2c_bus_available();

    // Calculate overall status
    report->overall_status = calculate_overall_status(report->checks, report->check_count,
                                                      report->dependencies, report->dependency_count);
}
```

**Required Helper Functions (to be implemented in separate modules):**
- `get_memory_info(MemoryInfo* mem)` - Read /proc/meminfo
- `get_disk_stats(const char* path, DiskStats* disk)` - Use statvfs()
- `get_cached_temperature()` - Query VAPIX API or cache
- `check_i2c_bus_available()` - Test open("/dev/i2c-0", O_RDWR)

**Threshold Values (from A5):**
- Memory: Warning < 50 MB, Critical < 20 MB
- Disk: Warning < 100 MB, Critical < 50 MB
- Temperature: Warning > 70°C, Critical > 80°C
- CPU: Warning > 80%, Critical > 95%

---

### HTTP Handler Implementation

**Implementation of health_handler():**
```c
void health_handler(int client_fd, HttpRequest* request, gpointer user_data) {
    // Only accept GET requests
    if (g_strcmp0(request->method, "GET") != 0) {
        http_send_error(client_fd, 405, "Method not allowed");
        return;
    }

    // Generate health report
    HealthReport report;
    generate_health_report(&report);

    // Serialize to JSON
    json_t* json = health_report_to_json(&report);
    char* json_str = json_dumps(json, JSON_INDENT(2));

    // Send HTTP response
    http_send_json(client_fd, 200, json_str);

    // Cleanup
    free(json_str);
    json_decref(json);
}
```

**Handler Characteristics:**
- Synchronous (blocking) - acceptable per A1 single-threaded model
- Stateless - generates fresh report on each request
- Error handling - returns 405 for non-GET methods
- Memory management - properly frees json_str and decrefs json object

---

### Integration with HTTP Server

**Registration in lrf_controller.c main():**
```c
// In app/lrf_controller.c main() function (around line 173)
#include "health.h"

// After existing handler registrations:
http_server_add_handler(http_server, "/distance", distance_handler, NULL);
http_server_add_handler(http_server, "/command", command_handler, NULL);
http_server_add_handler(http_server, "/status", status_handler, NULL);
http_server_add_handler(http_server, "/health", health_handler, NULL);  // NEW
```

**Integration Pattern (from A1):**
- Uses http_server_add_handler() function at app/http_server.c:69-75
- Handler stored in GLib linked list with path "/health"
- HTTP server dispatches GET requests to health_handler()
- Response sent via http_send_json() helper at app/http_server.c:212-239

**Endpoint URL:** `http://<camera-ip>:8080/health`

---

### File Organization

**Recommended file structure:**
```
app/
├── health.h              # Header with data structures and function declarations
├── health.c              # Implementation of health module
├── health_checks.h       # Helper functions for system metrics
├── health_checks.c       # Implementation of memory/disk/temp checks
├── lrf_controller.c      # Main file (add health_handler registration)
└── http_server.c/h       # Existing HTTP server (no changes needed)
```

**Compilation:**
- Add health.c and health_checks.c to Makefile
- Link against jansson (already present per A1)
- Link against GLib (already present per A1)

---

## Recommendations

### Check Configuration
Define checks in generate_health_report() with thresholds from A5 specification. Use static arrays to avoid dynamic memory allocation in embedded environment.

### Status Transition Logging
Implement status change detection and logging:
```c
static HealthStatus last_status = HEALTH_STATUS_HEALTHY;

void log_status_change(HealthStatus new_status) {
    if (last_status != new_status) {
        syslog(LOG_WARNING, "Health status changed: %s -> %s",
               health_status_to_string(last_status),
               health_status_to_string(new_status));
        last_status = new_status;
    }
}
```

### Metadata Addition
Consider adding optional metadata section for debugging:
```c
json_t* metadata = json_object();
json_object_set_new(metadata, "version", json_string("1.1.0"));
json_object_set_new(metadata, "uptime_seconds", json_real(get_uptime()));
json_object_set_new(root, "metadata", metadata);
```

### Helper Function Stubs
Provide stub implementations initially:
```c
int get_memory_info(MemoryInfo* mem) {
    mem->available_bytes = 120 * 1024 * 1024;  // 120 MB
    return 0;
}

bool check_i2c_bus_available(void) {
    int fd = open("/dev/i2c-0", O_RDWR);
    if (fd < 0) return false;
    close(fd);
    return true;
}
```

---

## Files Analyzed
- `.paf/findings/A1_FINDINGS.md` - HTTP server architecture and handler registration pattern
- `.paf/findings/A5_FINDINGS.md` - victor-health JSON schema and status calculation algorithms

## Blockers or Uncertainties
None - Complete architecture specified with clear integration points.

## Confidence Level
**HIGH** - Clear schema from A5, well-defined integration pattern from A1, straightforward C implementation with standard libraries (Jansson, GLib).
