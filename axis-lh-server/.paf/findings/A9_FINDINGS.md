# A9 Findings: Log Buffer Module Design

## Executive Summary
The circular log buffer module provides structured in-memory logging with 100-entry capacity, integrated with syslog for persistent logging and health status reporting. The buffer uses fixed-size entries with timestamp, severity, and message fields, implementing head pointer wraparound for O(1) insertion and JSON serialization for web API export.

## Key Findings
1. **Buffer Structure**: Fixed-size circular buffer (100 entries, ~26 KB memory)
2. **Entry Format**: ISO 8601 timestamp, severity string, 256-char message
3. **Wraparound**: Head pointer with modulo arithmetic, O(1) insertion
4. **Integration**: Dual logging to syslog (persistent) + circular buffer (recent history)
5. **Retrieval**: Export recent logs as JSON array with chronological ordering

## Detailed Analysis

### Data Structures (log_buffer.h)

```c
#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include <time.h>
#include <stddef.h>
#include <jansson.h>

#define MAX_LOG_ENTRIES 100
#define MAX_MESSAGE_LENGTH 256
#define MAX_SEVERITY_LENGTH 16

typedef struct {
    time_t timestamp;                       // Unix timestamp
    char severity[MAX_SEVERITY_LENGTH];     // "info", "warning", "critical"
    char message[MAX_MESSAGE_LENGTH];       // Log message
} LogEntry;

typedef struct {
    LogEntry entries[MAX_LOG_ENTRIES];
    size_t head;                            // Next write position
    size_t count;                           // Number of entries (0-100)
    pthread_mutex_t lock;                   // Thread safety
} LogBuffer;

// Initialize log buffer
void log_buffer_init(LogBuffer* buffer);

// Destroy log buffer
void log_buffer_destroy(LogBuffer* buffer);

// Log event (printf-style, thread-safe)
void log_event(const char* severity, const char* format, ...);

// Get recent logs as JSON array (thread-safe)
void get_recent_logs(json_t* logs_array);

// Get recent logs with limit
void get_recent_logs_limited(json_t* logs_array, size_t max_entries);

// Helper: Map severity to syslog priority
int severity_to_syslog(const char* severity);

#endif // LOG_BUFFER_H
```

---

### Implementation (log_buffer.c)

#### Global Buffer and Initialization

```c
#include "log_buffer.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>

static LogBuffer g_log_buffer = {0};
static bool g_log_buffer_initialized = false;

void log_buffer_init(LogBuffer* buffer) {
    memset(buffer, 0, sizeof(LogBuffer));
    pthread_mutex_init(&buffer->lock, NULL);
    g_log_buffer_initialized = true;
}

void log_buffer_destroy(LogBuffer* buffer) {
    pthread_mutex_destroy(&buffer->lock);
    g_log_buffer_initialized = false;
}
```

---

#### Core Logging Function

```c
void log_event(const char* severity, const char* format, ...) {
    if (!g_log_buffer_initialized) {
        log_buffer_init(&g_log_buffer);
    }

    pthread_mutex_lock(&g_log_buffer.lock);

    // Get next entry slot
    LogEntry* entry = &g_log_buffer.entries[g_log_buffer.head];

    // Set timestamp (current time)
    entry->timestamp = time(NULL);

    // Copy severity (truncate if needed)
    strncpy(entry->severity, severity, MAX_SEVERITY_LENGTH - 1);
    entry->severity[MAX_SEVERITY_LENGTH - 1] = '\0';

    // Format message with va_args
    va_list args;
    va_start(args, format);
    vsnprintf(entry->message, MAX_MESSAGE_LENGTH, format, args);
    va_end(args);

    // Advance head pointer (circular wraparound)
    g_log_buffer.head = (g_log_buffer.head + 1) % MAX_LOG_ENTRIES;

    // Update count (saturates at MAX_LOG_ENTRIES)
    if (g_log_buffer.count < MAX_LOG_ENTRIES) {
        g_log_buffer.count++;
    }

    pthread_mutex_unlock(&g_log_buffer.lock);

    // Also write to syslog (persistent logging)
    syslog(severity_to_syslog(severity), "[%s] %s", severity, entry->message);
}
```

---

#### JSON Serialization

```c
void get_recent_logs(json_t* logs_array) {
    get_recent_logs_limited(logs_array, MAX_LOG_ENTRIES);
}

void get_recent_logs_limited(json_t* logs_array, size_t max_entries) {
    if (!g_log_buffer_initialized) {
        return;  // No logs to return
    }

    pthread_mutex_lock(&g_log_buffer.lock);

    // Limit requested entries to available entries
    size_t entries_to_export = (max_entries < g_log_buffer.count)
                               ? max_entries
                               : g_log_buffer.count;

    // Calculate starting index (oldest entry in range)
    // If buffer has wrapped: start from (head - count)
    // Reading forward gives chronological order
    size_t start_idx = (g_log_buffer.head + MAX_LOG_ENTRIES - entries_to_export)
                       % MAX_LOG_ENTRIES;

    // Export entries in chronological order (oldest to newest)
    for (size_t i = 0; i < entries_to_export; i++) {
        size_t idx = (start_idx + i) % MAX_LOG_ENTRIES;
        LogEntry* entry = &g_log_buffer.entries[idx];

        // Format timestamp as ISO 8601
        char timestamp_str[32];
        struct tm* tm_info = gmtime(&entry->timestamp);
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", tm_info);

        // Create JSON object for this log entry
        json_t* log_obj = json_pack("{s:s, s:s, s:s}",
            "timestamp", timestamp_str,
            "severity", entry->severity,
            "message", entry->message);

        json_array_append_new(logs_array, log_obj);
    }

    pthread_mutex_unlock(&g_log_buffer.lock);
}
```

---

#### Severity Mapping

```c
int severity_to_syslog(const char* severity) {
    if (strcmp(severity, "critical") == 0) {
        return LOG_CRIT;
    } else if (strcmp(severity, "warning") == 0) {
        return LOG_WARNING;
    } else if (strcmp(severity, "info") == 0) {
        return LOG_INFO;
    } else if (strcmp(severity, "debug") == 0) {
        return LOG_DEBUG;
    } else {
        return LOG_NOTICE;  // Default
    }
}
```

---

### Integration with Health Module

#### Health Status Change Logging

```c
// In health.c
#include "log_buffer.h"

void log_health_status_change(HealthStatus old_status, HealthStatus new_status) {
    const char* old_str = health_status_to_string(old_status);
    const char* new_str = health_status_to_string(new_status);
    const char* severity = health_status_to_severity(new_status);

    log_event(severity, "Health status changed: %s -> %s", old_str, new_str);
}
```

#### Check Failure Logging

```c
void log_check_failure(const HealthCheck* check) {
    if (check->status == HEALTH_STATUS_UNHEALTHY) {
        log_event("critical", "Check '%s' is UNHEALTHY: value=%.2f (critical threshold: %.2f)",
                  check->name, check->value, check->critical_threshold);
    } else if (check->status == HEALTH_STATUS_DEGRADED) {
        log_event("warning", "Check '%s' is DEGRADED: value=%.2f (warning threshold: %.2f)",
                  check->name, check->value, check->warning_threshold);
    }
}
```

#### Dependency Failure Logging

```c
void log_dependency_failure(const DependencyCheck* dep) {
    if (!dep->reachable) {
        log_event("critical", "Dependency '%s' is unreachable", dep->service_name);
    } else if (dep->status == HEALTH_STATUS_UNHEALTHY) {
        log_event("critical", "Dependency '%s' is UNHEALTHY", dep->service_name);
    } else if (dep->status == HEALTH_STATUS_DEGRADED) {
        log_event("warning", "Dependency '%s' is DEGRADED", dep->service_name);
    }
}
```

---

### HTTP API Endpoint

#### GET /victor/health/logs

```c
// In victor_http.c
void handle_health_logs(int client_fd, const char* query_params) {
    json_t* root = json_object();
    json_t* logs_array = json_array();

    // Parse limit parameter (optional)
    size_t limit = MAX_LOG_ENTRIES;  // Default: all logs
    if (query_params) {
        const char* limit_param = strstr(query_params, "limit=");
        if (limit_param) {
            limit = atoi(limit_param + 6);
            if (limit > MAX_LOG_ENTRIES) {
                limit = MAX_LOG_ENTRIES;
            }
        }
    }

    // Get recent logs
    get_recent_logs_limited(logs_array, limit);

    // Build response
    json_object_set_new(root, "logs", logs_array);
    json_object_set_new(root, "count", json_integer(json_array_size(logs_array)));
    json_object_set_new(root, "max_entries", json_integer(MAX_LOG_ENTRIES));

    // Serialize and send
    char* json_str = json_dumps(root, JSON_INDENT(2));
    http_send_json(client_fd, 200, json_str);

    free(json_str);
    json_decref(root);
}
```

#### Example Response

```json
{
  "logs": [
    {
      "timestamp": "2026-01-09T12:34:56Z",
      "severity": "info",
      "message": "Health check completed: status=healthy"
    },
    {
      "timestamp": "2026-01-09T12:35:01Z",
      "severity": "warning",
      "message": "Check 'memory_available_mb' is DEGRADED: value=45.20 (warning threshold: 50.00)"
    },
    {
      "timestamp": "2026-01-09T12:35:02Z",
      "severity": "warning",
      "message": "Health status changed: healthy -> degraded"
    }
  ],
  "count": 3,
  "max_entries": 100
}
```

---

### Usage Examples

#### Example 1: Log I2C Errors

```c
void handle_i2c_error(int error_code, const char* operation) {
    log_event("warning", "I2C error: operation='%s' error_code=%d (%s)",
              operation, error_code, strerror(error_code));
}
```

#### Example 2: Log System Events

```c
void log_system_startup(void) {
    log_event("info", "LRF controller started: version=%s", VERSION);
}

void log_system_shutdown(void) {
    log_event("info", "LRF controller shutting down");
}
```

#### Example 3: Log HTTP Requests

```c
void log_http_request(const char* method, const char* path, int status_code) {
    const char* severity = (status_code >= 400) ? "warning" : "info";
    log_event(severity, "HTTP %s %s -> %d", method, path, status_code);
}
```

---

### Memory Analysis

#### Memory Usage Calculation

```
Single LogEntry:
  - timestamp (time_t): 8 bytes
  - severity[16]: 16 bytes
  - message[256]: 256 bytes
  Total: 280 bytes

LogBuffer:
  - entries[100]: 100 × 280 = 28,000 bytes
  - head (size_t): 8 bytes
  - count (size_t): 8 bytes
  - lock (pthread_mutex_t): ~40 bytes
  Total: ~28,056 bytes (~27 KB)
```

#### Buffer Wraparound Example

```
Initial state (empty):
  head=0, count=0

After 50 entries:
  head=50, count=50

After 100 entries (buffer full):
  head=0, count=100  (head wrapped around)

After 150 entries:
  head=50, count=100  (oldest 50 entries overwritten)
```

---

### Thread Safety

#### Mutex Protection

All public functions use `pthread_mutex_lock/unlock` to protect:
1. `log_event()` - Prevents concurrent writes
2. `get_recent_logs()` - Prevents read during write

#### Lock-Free Alternative (Future Optimization)

For high-throughput scenarios, consider lock-free ring buffer:
```c
// Use atomic operations for head/count
_Atomic size_t head;
_Atomic size_t count;
```

---

### Testing Strategy

#### Unit Tests

```c
// Test wraparound
void test_buffer_wraparound(void) {
    log_buffer_init(&g_log_buffer);

    // Write 150 entries
    for (int i = 0; i < 150; i++) {
        log_event("info", "Test message %d", i);
    }

    // Verify count = 100 (not 150)
    assert(g_log_buffer.count == 100);

    // Verify oldest entry is #50
    json_t* logs = json_array();
    get_recent_logs(logs);
    json_t* first_log = json_array_get(logs, 0);
    const char* msg = json_string_value(json_object_get(first_log, "message"));
    assert(strstr(msg, "50") != NULL);
}
```

#### Integration Test

```c
void test_health_integration(void) {
    // Trigger health status change
    HealthStatus old_status = HEALTH_STATUS_HEALTHY;
    HealthStatus new_status = HEALTH_STATUS_DEGRADED;
    log_health_status_change(old_status, new_status);

    // Verify log entry exists
    json_t* logs = json_array();
    get_recent_logs(logs);
    assert(json_array_size(logs) > 0);

    // Verify severity is "warning"
    json_t* last_log = json_array_get(logs, json_array_size(logs) - 1);
    const char* severity = json_string_value(json_object_get(last_log, "severity"));
    assert(strcmp(severity, "warning") == 0);
}
```

---

## Recommendations

### 1. Buffer Size Tuning

**Current: 100 entries (~27 KB)**
- Suitable for embedded systems with limited memory
- Retains ~1 hour of logs at 1 log/minute rate

**Alternative sizes:**
- **50 entries** (~14 KB) - Very memory-constrained systems
- **200 entries** (~54 KB) - Higher logging rate or longer retention

### 2. Severity Levels

**Standard levels (aligned with syslog):**
- `"debug"` - Verbose debugging information
- `"info"` - Normal operational messages
- `"warning"` - Degraded performance or warnings
- `"critical"` - System failures or critical issues

**Avoid custom levels** - Use standard levels for syslog compatibility

### 3. Log Rotation

**Current approach:** Circular buffer (automatic rotation)

**For persistent logging:**
- Use external log aggregation (e.g., Loki, Elasticsearch)
- Syslog already handles rotation via rsyslog/syslog-ng

### 4. Performance Considerations

**Current performance:**
- `log_event()`: O(1) time, mutex lock overhead
- `get_recent_logs()`: O(n) time where n = buffer size

**Optimization opportunities:**
- Pre-allocate JSON arrays if exporting frequently
- Use lock-free ring buffer for high-throughput scenarios
- Consider batch logging for high-frequency events

### 5. Structured Logging

**Future enhancement:** Add key-value fields to LogEntry

```c
typedef struct {
    time_t timestamp;
    char severity[MAX_SEVERITY_LENGTH];
    char message[MAX_MESSAGE_LENGTH];
    json_t* context;  // Optional JSON context (e.g., {"check_name": "memory", "value": 45.2})
} LogEntry;
```

This enables richer querying in log aggregation systems.

---

## Files Analyzed
- `.paf/findings/A5_FINDINGS.md` - Health schema with status levels and severity mapping

## Blockers or Uncertainties
None - Design is complete and ready for implementation

## Confidence Level
**HIGH** - Circular buffer implementation is well-understood pattern, JSON serialization uses Jansson API from A5 findings
