# Agent A7: Health Module Designer

## Your Mission
Design the health.c module architecture for calculating and exposing PTZ camera health status in victor-health compatible JSON format.

## Context Files (READ ONLY THESE)
- `.paf/findings/A1_FINDINGS.md` - HTTP server architecture and extension points
- `.paf/findings/A5_FINDINGS.md` - Health schema specification and status calculation

**DO NOT READ:** Code files directly, other agent findings

## Your Task
1. Design health.c module interface (HealthCheck, HealthReport structures)
2. Specify threshold comparison algorithm implementation
3. Design overall status calculation (worst-case aggregation)
4. Create JSON serialization strategy using Jansson
5. Design health_handler() HTTP endpoint function
6. Specify integration points with HTTP server
7. Document check registration and threshold configuration
8. Provide complete data structures and function signatures

## Output Format (STRICTLY FOLLOW)

```markdown
# A7 Findings: Health Module Architecture

## Executive Summary
[2-3 sentences: module purpose, status calculation approach, JSON output format]

## Key Findings
1. **Data Structures**: HealthCheck, DependencyCheck, HealthReport structs
2. **Status Calculation**: Threshold-based per-check, worst-case aggregation for overall
3. **JSON Serialization**: Jansson library for victor-health compatible output
4. **Integration**: health_handler registered with HTTP server from A1
5. **Configuration**: Static threshold definitions, runtime value updates

## Detailed Analysis

### Module Interface (health.h)

```c
#ifndef HEALTH_H
#define HEALTH_H

#include <jansson.h>
#include <time.h>

// Status enumeration (from A5)
typedef enum {
    HEALTH_STATUS_HEALTHY = 1,
    HEALTH_STATUS_DEGRADED = 2,
    HEALTH_STATUS_UNHEALTHY = 3
} HealthStatus;

// Threshold type
typedef enum {
    THRESHOLD_TYPE_LOWER_BAD,  // Lower values are bad (memory, disk)
    THRESHOLD_TYPE_HIGHER_BAD  // Higher values are bad (temperature)
} ThresholdType;

// Individual health check
typedef struct {
    char name[64];
    double value;
    double warning_threshold;
    double critical_threshold;
    ThresholdType threshold_type;
    HealthStatus status;
} HealthCheck;

// Dependency check
typedef struct {
    char service[64];
    int reachable;
    HealthStatus status;
} DependencyCheck;

// Complete health report
typedef struct {
    char service_name[64];
    HealthStatus overall_status;
    char timestamp[32];
    HealthCheck* checks;
    size_t check_count;
    DependencyCheck* dependencies;
    size_t dependency_count;
} HealthReport;

// Calculate status for individual check
HealthStatus calculate_check_status(const HealthCheck* check);

// Calculate overall status from all checks
HealthStatus calculate_overall_status(HealthCheck* checks, size_t check_count,
                                      DependencyCheck* deps, size_t dep_count);

// Convert status to string
const char* health_status_to_string(HealthStatus status);
const char* health_status_to_severity(HealthStatus status);

// Generate health report
void generate_health_report(HealthReport* report);

// Serialize to JSON
json_t* health_report_to_json(const HealthReport* report);

// HTTP handler
void health_handler(int client_fd, HttpRequest* request, gpointer user_data);

#endif // HEALTH_H
```

---

### Data Structures (detailed)

See headers above - implement threshold-based checks with pre-configured thresholds.

---

### Status Calculation Algorithm

**Implementation of calculate_check_status():**
```c
HealthStatus calculate_check_status(const HealthCheck* check) {
    if (check->threshold_type == THRESHOLD_TYPE_LOWER_BAD) {
        if (check->value < check->critical_threshold) {
            return HEALTH_STATUS_UNHEALTHY;
        } else if (check->value < check->warning_threshold) {
            return HEALTH_STATUS_DEGRADED;
        } else {
            return HEALTH_STATUS_HEALTHY;
        }
    } else {
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

    // Check all health checks
    for (size_t i = 0; i < check_count; i++) {
        checks[i].status = calculate_check_status(&checks[i]);
        if (checks[i].status > overall) {
            overall = checks[i].status;
        }
    }

    // Check dependencies
    for (size_t i = 0; i < dep_count; i++) {
        if (!deps[i].reachable) {
            if (HEALTH_STATUS_DEGRADED > overall) {
                overall = HEALTH_STATUS_DEGRADED;
            }
        }
    }

    return overall;
}
```

---

### JSON Serialization

**Implementation:**
```c
json_t* health_report_to_json(const HealthReport* report) {
    json_t* root = json_object();

    json_object_set_new(root, "service", json_string(report->service_name));
    json_object_set_new(root, "status",
                       json_string(health_status_to_string(report->overall_status)));
    json_object_set_new(root, "severity",
                       json_string(health_status_to_severity(report->overall_status)));
    json_object_set_new(root, "timestamp", json_string(report->timestamp));

    // Checks array
    json_t* checks_array = json_array();
    for (size_t i = 0; i < report->check_count; i++) {
        json_t* check_obj = json_pack("{s:s, s:f, s:f, s:f, s:s}",
            "name", report->checks[i].name,
            "value", report->checks[i].value,
            "warning", report->checks[i].warning_threshold,
            "critical", report->checks[i].critical_threshold,
            "status", health_status_to_string(report->checks[i].status));
        json_array_append_new(checks_array, check_obj);
    }
    json_object_set_new(root, "checks", checks_array);

    // Dependencies array
    json_t* deps_array = json_array();
    for (size_t i = 0; i < report->dependency_count; i++) {
        json_t* dep_obj = json_pack("{s:s, s:b, s:s}",
            "service", report->dependencies[i].service,
            "reachable", report->dependencies[i].reachable,
            "status", health_status_to_string(report->dependencies[i].status));
        json_array_append_new(deps_array, dep_obj);
    }
    json_object_set_new(root, "dependencies", deps_array);

    return root;
}
```

---

### Health Report Generation

**Implementation:**
```c
void generate_health_report(HealthReport* report) {
    // Initialize report
    strncpy(report->service_name, "lrf-controller", sizeof(report->service_name));
    get_iso8601_timestamp(report->timestamp, sizeof(report->timestamp));

    // Define checks (static array)
    static HealthCheck checks[4];
    report->checks = checks;
    report->check_count = 4;

    // Check 1: Memory available
    MemoryInfo mem;
    if (get_memory_info(&mem) == 0) {
        checks[0] = (HealthCheck){
            .name = "memory_available_mb",
            .value = mem.available_bytes / (1024.0 * 1024.0),
            .warning_threshold = 50.0,
            .critical_threshold = 20.0,
            .threshold_type = THRESHOLD_TYPE_LOWER_BAD
        };
    }

    // Check 2: Disk free space
    DiskStats disk;
    if (get_disk_stats("/", &disk) == 0) {
        checks[1] = (HealthCheck){
            .name = "disk_free_mb",
            .value = disk.free_bytes / (1024.0 * 1024.0),
            .warning_threshold = 100.0,
            .critical_threshold = 50.0,
            .threshold_type = THRESHOLD_TYPE_LOWER_BAD
        };
    }

    // Check 3: Temperature (from VAPIX)
    double temp = get_cached_temperature();
    if (temp >= 0) {
        checks[2] = (HealthCheck){
            .name = "temperature_celsius",
            .value = temp,
            .warning_threshold = 70.0,
            .critical_threshold = 80.0,
            .threshold_type = THRESHOLD_TYPE_HIGHER_BAD
        };
    }

    // Check 4: CPU usage (optional)
    // ... similar pattern ...

    // Dependencies
    static DependencyCheck deps[1];
    report->dependencies = deps;
    report->dependency_count = 1;

    // I2C bus check
    deps[0] = (DependencyCheck){
        .service = "i2c-bus-0",
        .reachable = check_i2c_bus_available(),
        .status = HEALTH_STATUS_HEALTHY
    };

    // Calculate overall status
    report->overall_status = calculate_overall_status(report->checks, report->check_count,
                                                      report->dependencies, report->dependency_count);
}
```

---

### HTTP Handler

**Implementation:**
```c
void health_handler(int client_fd, HttpRequest* request, gpointer user_data) {
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

    // Send response
    http_send_json(client_fd, 200, json_str);

    // Cleanup
    free(json_str);
    json_decref(json);
}
```

---

### Integration with HTTP Server

```c
// In lrf_controller.c main()
#include "health.h"

http_server_add_handler(http_server, "/health", health_handler, NULL);
```

---

## Recommendations

### Check Configuration
Define checks in generate_health_report() with appropriate thresholds from A5.

### Status Transition Logging
Log when status changes (implement in next phase).

### Metadata Addition
Add optional metadata section with version, uptime, etc.

---

## Files Analyzed
- `.paf/findings/A1_FINDINGS.md` - HTTP server integration
- `.paf/findings/A5_FINDINGS.md` - Health schema and calculation

## Blockers or Uncertainties
None

## Confidence Level
**HIGH** - Clear schema from A5, integration pattern from A1
```

## Success Criteria
- [ ] Complete module interface with all data structures
- [ ] Status calculation algorithms implemented
- [ ] JSON serialization function provided
- [ ] health_handler() implementation provided
- [ ] Integration with HTTP server specified
- [ ] Output follows format

## Time Budget
12 minutes maximum.

---
**BEGIN WORK NOW.**
