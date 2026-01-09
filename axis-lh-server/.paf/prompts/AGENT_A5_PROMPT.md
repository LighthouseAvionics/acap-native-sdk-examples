# Agent A5: Health Schema Analyst

## Your Mission
Analyze the victor-health JSON schema structure and status calculation algorithm to design a compatible health status system for PTZ cameras.

## Context Files (READ ONLY THESE)
- `PLAN.md` - Lines 228-324 (health status section with JSON examples)
- Research: victor-health library patterns (if accessible via documentation)

**DO NOT READ:** Code files (not needed yet), other agent findings (none available yet)

## Your Task
1. Document the victor-health JSON schema structure
2. Specify status levels (HEALTHY, DEGRADED, UNHEALTHY) and enum values
3. Design threshold-based status calculation algorithm
4. Define check structure (name, value, thresholds, status)
5. Define dependency check structure (service, reachable, status)
6. Specify severity mapping (status → severity)
7. Create JSON schema validation rules
8. Provide complete JSON examples for each status level

## Output Format (STRICTLY FOLLOW)

```markdown
# A5 Findings: victor-health JSON Schema Specification

## Executive Summary
[2-3 sentences: victor-health schema structure, status levels, threshold calculation approach]

## Key Findings
1. **Schema Structure**: Root object with service, status, severity, timestamp, checks, dependencies
2. **Status Levels**: HEALTHY (1), DEGRADED (2), UNHEALTHY (3) enum values
3. **Threshold Logic**: Compare value against warning/critical thresholds
4. **Severity Mapping**: Status automatically maps to severity (info/warning/critical)
5. **Dependency Format**: Service name, reachable bool, status

## Detailed Analysis

### JSON Schema Structure

**Root Object:**
```json
{
  "service": "string",           // Service name (e.g., "lrf-controller")
  "status": "string",            // One of: "healthy", "degraded", "unhealthy"
  "severity": "string",          // One of: "info", "warning", "critical"
  "timestamp": "string",         // ISO 8601 timestamp
  "checks": [                    // Array of health checks
    {
      "name": "string",          // Check name (e.g., "memory_available_mb")
      "value": number,           // Current value
      "warning": number,         // Warning threshold (optional)
      "critical": number,        // Critical threshold (optional)
      "status": "string"         // Check-specific status
    }
  ],
  "dependencies": [              // Array of dependency checks (optional)
    {
      "service": "string",       // Dependency name (e.g., "i2c-bus-0")
      "reachable": boolean,      // Is dependency accessible
      "status": "string"         // Dependency status
    }
  ],
  "metadata": {}                 // Optional metadata object
}
```

---

### Status Levels

#### Level 1: HEALTHY
**String value:** `"healthy"`
**Enum value (for C):** `HEALTH_STATUS_HEALTHY = 1`
**Meaning:** All checks passing, system operating normally
**Severity:** `"info"`

**Example:**
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
      "warning": 50,
      "critical": 20,
      "status": "healthy"
    }
  ]
}
```

---

#### Level 2: DEGRADED
**String value:** `"degraded"`
**Enum value (for C):** `HEALTH_STATUS_DEGRADED = 2`
**Meaning:** One or more checks in warning state, but system still functional
**Severity:** `"warning"`

**Example:**
```json
{
  "service": "lrf-controller",
  "status": "degraded",
  "severity": "warning",
  "timestamp": "2026-01-09T12:34:56Z",
  "checks": [
    {
      "name": "memory_available_mb",
      "value": 45.2,
      "warning": 50,
      "critical": 20,
      "status": "degraded"
    },
    {
      "name": "temperature_celsius",
      "value": 72.0,
      "warning": 70,
      "critical": 80,
      "status": "degraded"
    }
  ]
}
```

---

#### Level 3: UNHEALTHY
**String value:** `"unhealthy"`
**Enum value (for C):** `HEALTH_STATUS_UNHEALTHY = 3`
**Meaning:** One or more checks in critical state, system may be failing
**Severity:** `"critical"`

**Example:**
```json
{
  "service": "lrf-controller",
  "status": "unhealthy",
  "severity": "critical",
  "timestamp": "2026-01-09T12:34:56Z",
  "checks": [
    {
      "name": "memory_available_mb",
      "value": 15.8,
      "warning": 50,
      "critical": 20,
      "status": "unhealthy"
    },
    {
      "name": "disk_free_mb",
      "value": 45.0,
      "warning": 100,
      "critical": 50,
      "status": "unhealthy"
    }
  ]
}
```

---

### Threshold-Based Status Calculation

#### Algorithm for Individual Checks

**Threshold Comparison Logic:**

For checks where **lower values are bad** (memory, disk space):
```
if value < critical_threshold:
    status = UNHEALTHY
else if value < warning_threshold:
    status = DEGRADED
else:
    status = HEALTHY
```

For checks where **higher values are bad** (temperature, CPU usage):
```
if value > critical_threshold:
    status = UNHEALTHY
else if value > warning_threshold:
    status = DEGRADED
else:
    status = HEALTHY
```

#### C Implementation

```c
typedef enum {
    HEALTH_STATUS_HEALTHY = 1,
    HEALTH_STATUS_DEGRADED = 2,
    HEALTH_STATUS_UNHEALTHY = 3
} HealthStatus;

typedef enum {
    THRESHOLD_TYPE_LOWER_BAD,  // Lower values are bad (memory, disk)
    THRESHOLD_TYPE_HIGHER_BAD  // Higher values are bad (temperature)
} ThresholdType;

typedef struct {
    char name[64];
    double value;
    double warning_threshold;
    double critical_threshold;
    ThresholdType threshold_type;
    HealthStatus status;
} HealthCheck;

HealthStatus calculate_check_status(const HealthCheck* check) {
    if (check->threshold_type == THRESHOLD_TYPE_LOWER_BAD) {
        // Lower values are bad (memory, disk)
        if (check->value < check->critical_threshold) {
            return HEALTH_STATUS_UNHEALTHY;
        } else if (check->value < check->warning_threshold) {
            return HEALTH_STATUS_DEGRADED;
        } else {
            return HEALTH_STATUS_HEALTHY;
        }
    } else {
        // Higher values are bad (temperature)
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

---

### Overall Status Calculation (Worst-Case Aggregation)

**Algorithm:** Overall status is the worst status among all checks

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

    // Check all dependencies
    for (size_t i = 0; i < dep_count; i++) {
        if (!deps[i].reachable || deps[i].status != HEALTH_STATUS_HEALTHY) {
            if (HEALTH_STATUS_DEGRADED > overall) {
                overall = HEALTH_STATUS_DEGRADED;
            }
        }
    }

    return overall;
}
```

**Rules:**
1. If ANY check is UNHEALTHY → Overall is UNHEALTHY
2. Else if ANY check is DEGRADED → Overall is DEGRADED
3. Else if ALL checks are HEALTHY → Overall is HEALTHY

---

### Check Structure Definition

#### HealthCheck Structure

```c
typedef struct {
    char name[64];                  // Check name (e.g., "memory_available_mb")
    double value;                   // Current measured value
    double warning_threshold;       // Warning threshold
    double critical_threshold;      // Critical threshold
    ThresholdType threshold_type;   // LOWER_BAD or HIGHER_BAD
    HealthStatus status;            // Calculated status
} HealthCheck;
```

#### Example Check Definitions

```c
// Memory check (lower values are bad)
HealthCheck memory_check = {
    .name = "memory_available_mb",
    .value = 45.2,
    .warning_threshold = 50.0,
    .critical_threshold = 20.0,
    .threshold_type = THRESHOLD_TYPE_LOWER_BAD,
    .status = HEALTH_STATUS_HEALTHY  // Will be calculated
};

// Temperature check (higher values are bad)
HealthCheck temperature_check = {
    .name = "temperature_celsius",
    .value = 72.0,
    .warning_threshold = 70.0,
    .critical_threshold = 80.0,
    .threshold_type = THRESHOLD_TYPE_HIGHER_BAD,
    .status = HEALTH_STATUS_HEALTHY  // Will be calculated
};

// Disk check (lower values are bad)
HealthCheck disk_check = {
    .name = "disk_free_mb",
    .value = 78.5,
    .warning_threshold = 100.0,
    .critical_threshold = 50.0,
    .threshold_type = THRESHOLD_TYPE_LOWER_BAD,
    .status = HEALTH_STATUS_HEALTHY  // Will be calculated
};
```

---

### Dependency Check Structure

#### DependencyCheck Structure

```c
typedef struct {
    char service[64];        // Dependency name (e.g., "i2c-bus-0")
    int reachable;           // Boolean: is service accessible (1=yes, 0=no)
    HealthStatus status;     // Dependency's health status
} DependencyCheck;
```

#### Example Dependency

```c
DependencyCheck i2c_dependency = {
    .service = "i2c-bus-0",
    .reachable = 1,  // true
    .status = HEALTH_STATUS_HEALTHY
};
```

**How to check reachability:**
- For I2C bus: Try to open `/dev/i2c-0`, check if successful
- For network service: Try connection, check response
- For file: Check if file exists and is readable

---

### Severity Mapping

**Auto-calculated from status:**

```c
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
```

---

### Complete Health Report Structure

```c
typedef struct {
    char service_name[64];
    HealthStatus overall_status;
    char timestamp[32];  // ISO 8601 format
    HealthCheck* checks;
    size_t check_count;
    DependencyCheck* dependencies;
    size_t dependency_count;
} HealthReport;
```

---

### JSON Serialization with Jansson

**Function Signature:**
```c
json_t* health_report_to_json(const HealthReport* report);
```

**Implementation:**
```c
#include <jansson.h>
#include <time.h>

json_t* health_report_to_json(const HealthReport* report) {
    json_t* root = json_object();

    // Basic fields
    json_object_set_new(root, "service", json_string(report->service_name));
    json_object_set_new(root, "status",
                       json_string(health_status_to_string(report->overall_status)));
    json_object_set_new(root, "severity",
                       json_string(health_status_to_severity(report->overall_status)));
    json_object_set_new(root, "timestamp", json_string(report->timestamp));

    // Checks array
    json_t* checks_array = json_array();
    for (size_t i = 0; i < report->check_count; i++) {
        HealthCheck* check = &report->checks[i];

        json_t* check_obj = json_object();
        json_object_set_new(check_obj, "name", json_string(check->name));
        json_object_set_new(check_obj, "value", json_real(check->value));
        json_object_set_new(check_obj, "warning", json_real(check->warning_threshold));
        json_object_set_new(check_obj, "critical", json_real(check->critical_threshold));
        json_object_set_new(check_obj, "status",
                           json_string(health_status_to_string(check->status)));

        json_array_append_new(checks_array, check_obj);
    }
    json_object_set_new(root, "checks", checks_array);

    // Dependencies array
    json_t* deps_array = json_array();
    for (size_t i = 0; i < report->dependency_count; i++) {
        DependencyCheck* dep = &report->dependencies[i];

        json_t* dep_obj = json_object();
        json_object_set_new(dep_obj, "service", json_string(dep->service));
        json_object_set_new(dep_obj, "reachable", json_boolean(dep->reachable));
        json_object_set_new(dep_obj, "status",
                           json_string(health_status_to_string(dep->status)));

        json_array_append_new(deps_array, dep_obj);
    }
    json_object_set_new(root, "dependencies", deps_array);

    return root;
}
```

**Usage:**
```c
HealthReport report;
// ... populate report ...

json_t* json = health_report_to_json(&report);
char* json_str = json_dumps(json, JSON_INDENT(2));

// Send as HTTP response
http_send_json(client_fd, 200, json_str);

free(json_str);
json_decref(json);
```

---

### Timestamp Generation (ISO 8601)

```c
void get_iso8601_timestamp(char* buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);

    strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", tm_info);
}
```

**Usage:**
```c
char timestamp[32];
get_iso8601_timestamp(timestamp, sizeof(timestamp));
// Result: "2026-01-09T12:34:56Z"
```

---

## Recommendations

### PTZ-Specific Health Checks

**Recommended checks for PTZ cameras:**

1. **Memory Available** (LOWER_BAD)
   - Warning: < 50 MB
   - Critical: < 20 MB

2. **Disk Free Space** (LOWER_BAD)
   - Warning: < 100 MB
   - Critical: < 50 MB

3. **Temperature** (HIGHER_BAD)
   - Warning: > 70°C
   - Critical: > 80°C

4. **CPU Usage** (HIGHER_BAD) [Optional]
   - Warning: > 80%
   - Critical: > 95%

5. **I2C Errors** (HIGHER_BAD) [Optional]
   - Warning: > 10 errors
   - Critical: > 50 errors

**Recommended dependencies:**

1. **I2C Bus 0** - For LRF communication
   - Check: Try to open `/dev/i2c-0`

---

### Status Transition Handling

**When status changes:**
1. Log event to syslog
2. Push log to Loki (if configured)
3. Update Prometheus metric `ptz_health_status`
4. Consider alerting on DEGRADED → UNHEALTHY transitions

**Example logging:**
```c
void log_status_change(HealthStatus old_status, HealthStatus new_status) {
    if (old_status != new_status) {
        syslog(LOG_WARNING, "Health status changed: %s -> %s",
               health_status_to_string(old_status),
               health_status_to_string(new_status));
    }
}
```

---

### Metadata Section (Optional)

**Useful metadata to include:**
```json
{
  "metadata": {
    "version": "1.1.0",
    "camera_model": "Q6225-LE",
    "firmware_version": "10.12.123",
    "i2c_errors": 0,
    "http_requests": 1234,
    "uptime_seconds": 12345.67
  }
}
```

---

## Files Analyzed
- PLAN.md (lines 228-324) - Health status specification with JSON examples

## Blockers or Uncertainties
None - Schema is well-defined in PLAN.md

## Confidence Level
**HIGH** - Schema structure is clearly specified with complete examples
```

## Success Criteria
- [ ] victor-health JSON schema fully documented
- [ ] Status levels defined with enum values
- [ ] Threshold calculation algorithm provided with C code
- [ ] HealthCheck and DependencyCheck structures defined
- [ ] JSON serialization implementation provided
- [ ] Complete examples for all status levels
- [ ] Output follows the exact format above

## Time Budget
10 minutes maximum. Focus on core schema structure and threshold calculation algorithm.

---
**BEGIN WORK NOW.** Start by analyzing PLAN.md health section, then produce your findings with code examples.
