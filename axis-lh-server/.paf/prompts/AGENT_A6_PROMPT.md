# Agent A6: Metrics Module Designer

## Your Mission
Design the metrics.c module architecture for collecting and exposing Prometheus-formatted system metrics.

## Context Files (READ ONLY THESE)
- `.paf/findings/A1_FINDINGS.md` - HTTP server architecture and extension points
- `.paf/findings/A2_FINDINGS.md` - /proc filesystem metrics specification
- `.paf/findings/A4_FINDINGS.md` - Prometheus format guide

**DO NOT READ:** Code files directly (A1 already analyzed them), other agent findings

## Your Task
1. Design metrics.c module interface (public functions and structures)
2. Specify function signatures for collect_system_metrics, collect_network_metrics, etc.
3. Design metrics_handler() HTTP endpoint function
4. Create GString buffer accumulation strategy
5. Design error handling for missing /proc files
6. Specify integration points with HTTP server (from A1)
7. Document memory management approach
8. Provide complete function signatures and pseudo-code

## Output Format (STRICTLY FOLLOW)

```markdown
# A6 Findings: Metrics Module Architecture

## Executive Summary
[2-3 sentences: module purpose, collection strategy, integration with HTTP server]

## Key Findings
1. **Module Interface**: collect_* functions for each metric category, metrics_handler for HTTP
2. **Buffer Strategy**: Use GString for accumulation, append helper functions
3. **Error Handling**: Skip individual metrics on failure, don't fail entire collection
4. **Integration**: Register metrics_handler with HTTP server using pattern from A1
5. **Performance**: Target < 100ms collection time, lazy evaluation on endpoint access

## Detailed Analysis

### Module Interface (metrics.h)

**Public Functions:**
```c
#ifndef METRICS_H
#define METRICS_H

#include <glib.h>

// Collect system metrics (uptime, memory, CPU, load)
void collect_system_metrics(GString* output);

// Collect network metrics (rx/tx bytes per interface)
void collect_network_metrics(GString* output);

// Collect disk metrics (total/free bytes)
void collect_disk_metrics(GString* output);

// Collect service metrics (process count, service status)
void collect_service_metrics(GString* output);

// HTTP handler for /metrics endpoint
void metrics_handler(int client_fd, HttpRequest* request, gpointer user_data);

#endif // METRICS_H
```

---

### Collection Functions Design

#### Function 1: collect_system_metrics()

**Purpose:** Collect uptime, memory, CPU, load average from /proc

**Signature:**
```c
void collect_system_metrics(GString* output);
```

**Pseudo-code:**
```c
void collect_system_metrics(GString* output) {
    // Uptime from /proc/uptime
    double uptime = get_uptime();  // Returns -1 on error
    if (uptime >= 0) {
        append_metric_gauge(output, "ptz_uptime_seconds",
                           "PTZ camera system uptime", NULL, uptime);
    }

    // Memory from /proc/meminfo
    MemoryInfo mem;
    if (get_memory_info(&mem) == 0) {
        append_metric_gauge(output, "ptz_memory_total_bytes",
                           "Total memory in bytes", NULL, (double)mem.total_bytes);
        append_metric_gauge(output, "ptz_memory_available_bytes",
                           "Available memory in bytes", NULL, (double)mem.available_bytes);
    }

    // Load average from /proc/loadavg
    double load_1m = get_load_average_1m();
    if (load_1m >= 0) {
        append_metric_gauge(output, "ptz_load_average_1m",
                           "1-minute load average", NULL, load_1m);
    }

    // CPU usage (requires previous sample - may skip on first call)
    static CPUStats prev_cpu = {0};
    static int first_sample = 1;
    CPUStats curr_cpu;

    if (get_cpu_stats(&curr_cpu) == 0) {
        if (!first_sample) {
            double cpu_usage = calculate_cpu_usage(&prev_cpu, &curr_cpu);
            append_metric_gauge(output, "ptz_cpu_usage_percent",
                               "CPU usage percentage", NULL, cpu_usage);
        }
        prev_cpu = curr_cpu;
        first_sample = 0;
    }
}
```

**Notes:**
- Use helper functions from A2 (get_uptime, get_memory_info, etc.)
- Static variables for CPU calculation (requires previous sample)
- Skip metrics that fail to collect (don't abort)

---

#### Function 2: collect_network_metrics()

**Purpose:** Collect network rx/tx bytes from /proc/net/dev

**Signature:**
```c
void collect_network_metrics(GString* output);
```

**Pseudo-code:**
```c
void collect_network_metrics(GString* output) {
    // Collect for primary interface (eth0 or similar)
    NetworkStats stats;

    // Try eth0 first
    if (get_network_stats(&stats, "eth0") == 0) {
        char labels[128];
        snprintf(labels, sizeof(labels), "interface=\"eth0\"");

        append_metric_counter(output, "ptz_network_rx_bytes_total",
                             "Total bytes received", labels, stats.rx_bytes);
        append_metric_counter(output, "ptz_network_tx_bytes_total",
                             "Total bytes transmitted", labels, stats.tx_bytes);
    }
    // Fallback to other interfaces if needed
}
```

**Notes:**
- Focus on primary interface (eth0) to reduce metric cardinality
- Use label for interface name
- Counter type (monotonically increasing)

---

#### Function 3: collect_disk_metrics()

**Purpose:** Collect disk usage via statfs()

**Signature:**
```c
void collect_disk_metrics(GString* output);
```

**Pseudo-code:**
```c
void collect_disk_metrics(GString* output) {
    DiskStats stats;

    if (get_disk_stats("/", &stats) == 0) {
        append_metric_gauge(output, "ptz_disk_total_bytes",
                           "Total disk space in bytes", NULL, (double)stats.total_bytes);
        append_metric_gauge(output, "ptz_disk_free_bytes",
                           "Free disk space in bytes", NULL, (double)stats.free_bytes);
    }
}
```

**Notes:**
- Monitor root filesystem "/"
- Gauge type (can increase or decrease)

---

#### Function 4: collect_service_metrics()

**Purpose:** Collect application-specific metrics (HTTP requests, I2C errors)

**Signature:**
```c
void collect_service_metrics(GString* output);
```

**Pseudo-code:**
```c
// Global counters (incremented by application)
extern uint64_t g_http_requests_total;
extern uint64_t g_i2c_errors_total;

void collect_service_metrics(GString* output) {
    // HTTP request counter
    append_metric_counter(output, "ptz_http_requests_total",
                         "Total HTTP requests handled", NULL, g_http_requests_total);

    // I2C error counter
    append_metric_counter(output, "ptz_i2c_errors_total",
                         "Total I2C communication errors", NULL, g_i2c_errors_total);

    // Process count from /proc
    int process_count = get_process_count();  // Count entries in /proc
    if (process_count >= 0) {
        append_metric_gauge(output, "ptz_process_count",
                           "Number of running processes", NULL, (double)process_count);
    }
}
```

**Notes:**
- Use global counters for application metrics
- Counters should be thread-safe (use atomic operations if multi-threaded)

---

### Helper Functions

#### append_metric_gauge()

**Purpose:** Append gauge metric with HELP, TYPE, and value

**Signature:**
```c
void append_metric_gauge(GString* output, const char* name, const char* help,
                         const char* labels, double value);
```

**Implementation:**
```c
void append_metric_gauge(GString* output, const char* name, const char* help,
                         const char* labels, double value) {
    // HELP comment
    g_string_append_printf(output, "# HELP %s %s\n", name, help);

    // TYPE declaration
    g_string_append_printf(output, "# TYPE %s gauge\n", name);

    // Metric line
    if (labels && labels[0]) {
        g_string_append_printf(output, "%s{%s} %.2f\n", name, labels, value);
    } else {
        g_string_append_printf(output, "%s %.2f\n", name, value);
    }
}
```

---

#### append_metric_counter()

**Purpose:** Append counter metric with HELP, TYPE, and value

**Signature:**
```c
void append_metric_counter(GString* output, const char* name, const char* help,
                           const char* labels, uint64_t value);
```

**Implementation:**
```c
void append_metric_counter(GString* output, const char* name, const char* help,
                           const char* labels, uint64_t value) {
    // HELP comment
    g_string_append_printf(output, "# HELP %s %s\n", name, help);

    // TYPE declaration
    g_string_append_printf(output, "# TYPE %s counter\n", name);

    // Metric line
    if (labels && labels[0]) {
        g_string_append_printf(output, "%s{%s} %lu\n", name, labels, value);
    } else {
        g_string_append_printf(output, "%s %lu\n", name, value);
    }
}
```

---

### HTTP Endpoint Handler

#### metrics_handler()

**Purpose:** Handle GET /metrics requests and return Prometheus format

**Signature:**
```c
void metrics_handler(int client_fd, HttpRequest* request, gpointer user_data);
```

**Implementation:**
```c
void metrics_handler(int client_fd, HttpRequest* request, gpointer user_data) {
    // Validate method
    if (g_strcmp0(request->method, "GET") != 0) {
        http_send_error(client_fd, 405, "Method not allowed");
        return;
    }

    // Allocate buffer
    GString* metrics = g_string_new("");

    // Collect all metrics
    collect_system_metrics(metrics);
    collect_network_metrics(metrics);
    collect_disk_metrics(metrics);
    collect_service_metrics(metrics);

    // Build HTTP response
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
             "Content-Length: %zu\r\n"
             "\r\n",
             metrics->len);

    // Send response
    send(client_fd, header, strlen(header), 0);
    send(client_fd, metrics->str, metrics->len, 0);

    // Cleanup
    g_string_free(metrics, TRUE);
}
```

**Notes:**
- Lazy evaluation: metrics collected only when endpoint accessed
- GString automatically manages buffer growth
- Use pattern from A4 for Content-Type header
- Clean up GString after sending response

---

### Integration with HTTP Server

**Based on A1 findings, registration pattern:**

```c
// In lrf_controller.c main() function
#include "metrics.h"

int main(int argc, char* argv[]) {
    // ... existing initialization ...

    // Create HTTP server
    HttpServer* http_server = http_server_new(8080);

    // Register existing handlers
    http_server_add_handler(http_server, "/distance", distance_handler, lrf_device);
    http_server_add_handler(http_server, "/command", command_handler, lrf_device);
    http_server_add_handler(http_server, "/status", status_handler, lrf_device);

    // Register NEW metrics handler
    http_server_add_handler(http_server, "/metrics", metrics_handler, NULL);

    // Start server
    http_server_start(http_server);

    // ... rest of main ...
}
```

**Registration location:** After existing handler registration in main()

---

### Error Handling Strategy

**Principle:** Individual metric failures should not fail entire collection

**Implementation:**
1. Each helper function (get_uptime, get_memory_info, etc.) returns error code
2. Check return code before appending metric
3. Skip metric if collection fails (don't append to output)
4. Log warning to syslog for debugging
5. Continue with remaining metrics

**Example:**
```c
void collect_system_metrics(GString* output) {
    double uptime = get_uptime();
    if (uptime >= 0) {
        append_metric_gauge(output, "ptz_uptime_seconds", "...", NULL, uptime);
    } else {
        syslog(LOG_WARNING, "Failed to collect uptime metric");
        // Continue with other metrics
    }

    // ... other metrics ...
}
```

**Benefits:**
- Partial metrics better than no metrics
- Prometheus can still scrape available data
- Easier to debug which specific metric is failing

---

### Memory Management

**Strategy:**
1. **GString usage**: Let GLib manage buffer growth automatically
2. **No dynamic allocation in hot path**: Use stack variables where possible
3. **Static buffers for labels**: Fixed-size char arrays for label strings
4. **Cleanup**: Always free GString after response sent

**Memory footprint estimate:**
- GString buffer: ~2-4 KB for all metrics
- Stack variables: ~1 KB
- Total: < 5 KB per request

**Example:**
```c
void metrics_handler(...) {
    GString* metrics = g_string_new("");  // Allocate
    // ... collect metrics ...
    // ... send response ...
    g_string_free(metrics, TRUE);  // Free
}
```

---

### Performance Considerations

**Target:** < 100ms total collection time

**Optimization strategies:**
1. **Lazy evaluation**: Only collect when /metrics accessed
2. **Cache expensive operations**: Store previous CPU stats (already designed)
3. **Minimize file I/O**: Read each /proc file once
4. **Efficient parsing**: Use sscanf instead of string manipulation
5. **Skip unnecessary metrics**: Don't collect metrics not needed

**Measurement:**
```c
void metrics_handler(...) {
    struct timeval start, end;
    gettimeofday(&start, NULL);

    // ... collect metrics ...

    gettimeofday(&end, NULL);
    double duration_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                         (end.tv_usec - start.tv_usec) / 1000.0;

    syslog(LOG_DEBUG, "Metrics collection took %.2f ms", duration_ms);
}
```

---

## Recommendations

### File Structure

**Create new files:**
- `app/metrics.h` - Public interface
- `app/metrics.c` - Implementation
- `app/metrics_helpers.c` - Helper functions from A2 (get_uptime, get_memory_info, etc.)

**Modify existing files:**
- `app/lrf_controller.c` - Add handler registration
- `app/Makefile` - Add metrics.c and metrics_helpers.c to build

---

### Build System Changes

**Makefile additions:**
```makefile
SRCS += metrics.c metrics_helpers.c
```

**Dependencies:**
- GLib (already used)
- No additional libraries needed

---

### Testing Strategy

**Unit tests:**
1. Test helper functions with mock /proc files
2. Test append_metric_* functions with known inputs
3. Verify Prometheus format correctness

**Integration tests:**
1. Build and deploy ACAP
2. Curl /metrics endpoint
3. Validate with promtool
4. Check response time < 500ms
5. Verify memory usage < 5 MB

**Commands:**
```bash
# Test endpoint
curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics

# Validate format
curl ... | promtool check metrics

# Test performance
time curl ...
```

---

### Integration with VAPIX (from A8)

**Note:** VAPIX integration (temperature) will be handled by A8 agent.
Metrics module should provide hook for adding VAPIX metrics:

```c
// In metrics.c
void collect_vapix_metrics(GString* output) {
    // Will be implemented by VAPIX client module
    double temperature = get_cached_temperature();  // From A8 design
    if (temperature >= 0) {
        append_metric_gauge(output, "ptz_temperature_celsius",
                           "PTZ camera temperature", NULL, temperature);
    }
}
```

Call from metrics_handler():
```c
void metrics_handler(...) {
    // ...
    collect_vapix_metrics(metrics);  // Add VAPIX metrics
    // ...
}
```

---

## Files Analyzed
- `.paf/findings/A1_FINDINGS.md` - HTTP server architecture
- `.paf/findings/A2_FINDINGS.md` - /proc metrics specification
- `.paf/findings/A4_FINDINGS.md` - Prometheus format guide

## Blockers or Uncertainties
None - All dependencies from Wave 1 provide necessary design information

## Confidence Level
**HIGH** - Clear integration pattern from A1, complete metric specifications from A2/A4
```

## Success Criteria
- [ ] Complete module interface designed (metrics.h)
- [ ] All collection functions specified with signatures
- [ ] metrics_handler() implementation provided
- [ ] Helper functions designed (append_metric_gauge/counter)
- [ ] Error handling strategy documented
- [ ] Integration with HTTP server specified
- [ ] Memory management approach documented
- [ ] Output follows the exact format above

## Time Budget
12 minutes maximum. Focus on core module interface and metrics_handler implementation.

---
**BEGIN WORK NOW.** Start by reading A1, A2, A4 findings, then produce your design.
