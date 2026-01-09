# A6 Findings: Metrics Module Architecture

## Executive Summary
The metrics.c module will collect system health metrics from /proc filesystem and expose them via HTTP /metrics endpoint in Prometheus text format. The module uses lazy evaluation (metrics collected only on endpoint access), GString buffer accumulation, and integrates with the existing HTTP server via the handler registration pattern identified by A1.

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
#include "http_server.h"

// Collect system metrics (uptime, memory, CPU, load)
void collect_system_metrics(GString* output);

// Collect network metrics (rx/tx bytes per interface)
void collect_network_metrics(GString* output);

// Collect disk metrics (total/free bytes)
void collect_disk_metrics(GString* output);

// Collect service metrics (process count, HTTP requests, I2C errors)
void collect_service_metrics(GString* output);

// HTTP handler for /metrics endpoint
void metrics_handler(int client_fd, HttpRequest* request, gpointer user_data);

// Global counters for application metrics (incremented by application)
extern uint64_t g_http_requests_total;
extern uint64_t g_i2c_errors_total;

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
    } else {
        syslog(LOG_WARNING, "Failed to collect uptime metric");
    }

    // Memory from /proc/meminfo
    MemoryInfo mem;
    if (get_memory_info(&mem) == 0) {
        append_metric_gauge(output, "ptz_memory_total_bytes",
                           "Total memory in bytes", NULL, (double)mem.total_bytes);
        append_metric_gauge(output, "ptz_memory_available_bytes",
                           "Available memory in bytes", NULL, (double)mem.available_bytes);
    } else {
        syslog(LOG_WARNING, "Failed to collect memory metrics");
    }

    // Load average from /proc/loadavg
    double load_1m = get_load_average_1m();
    if (load_1m >= 0) {
        append_metric_gauge(output, "ptz_load_average_1m",
                           "1-minute load average", NULL, load_1m);
    } else {
        syslog(LOG_WARNING, "Failed to collect load average metric");
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
    } else {
        syslog(LOG_WARNING, "Failed to collect CPU stats");
    }
}
```

**Notes:**
- Use helper functions from A2 (get_uptime, get_memory_info, etc.)
- Static variables for CPU calculation (requires previous sample)
- Skip metrics that fail to collect (don't abort)
- Log warnings for debugging failed collections

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
    // Auto-detect primary interface (first non-loopback)
    char primary_interface[32];
    if (get_primary_interface_name(primary_interface, sizeof(primary_interface)) != 0) {
        syslog(LOG_WARNING, "Failed to detect primary network interface");
        return;
    }

    NetworkStats stats;
    if (get_network_stats(&stats, primary_interface) == 0) {
        char labels[128];
        snprintf(labels, sizeof(labels), "interface=\"%s\"", primary_interface);

        append_metric_counter(output, "ptz_network_rx_bytes_total",
                             "Total bytes received", labels, stats.rx_bytes);
        append_metric_counter(output, "ptz_network_tx_bytes_total",
                             "Total bytes transmitted", labels, stats.tx_bytes);
    } else {
        syslog(LOG_WARNING, "Failed to collect network stats for %s", primary_interface);
    }
}
```

**Notes:**
- Use auto-detection for primary interface (eth0, eno1, etc.)
- Use label for interface name
- Counter type (monotonically increasing)
- Skip if interface not found

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
                           "Free disk space in bytes", NULL, (double)stats.available_bytes);
    } else {
        syslog(LOG_WARNING, "Failed to collect disk stats");
    }
}
```

**Notes:**
- Monitor root filesystem "/"
- Gauge type (can increase or decrease)
- Use available_bytes (free for non-root users)

---

#### Function 4: collect_service_metrics()

**Purpose:** Collect application-specific metrics (HTTP requests, I2C errors)

**Signature:**
```c
void collect_service_metrics(GString* output);
```

**Pseudo-code:**
```c
// Global counters (defined in metrics.c, incremented by lrf_controller.c)
uint64_t g_http_requests_total = 0;
uint64_t g_i2c_errors_total = 0;

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
    } else {
        syslog(LOG_WARNING, "Failed to collect process count");
    }
}
```

**Notes:**
- Use global counters for application metrics
- Counters should be thread-safe (use atomic operations if multi-threaded)
- Current single-threaded model doesn't require atomics (from A1)

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

**Notes:**
- Follows Prometheus format from A4
- HELP and TYPE are required
- Labels are optional (NULL or empty string = no labels)
- Use %.2f for gauge values (two decimal places)

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

**Notes:**
- Use %lu for uint64_t values
- Counter metric names should have _total suffix (already included in caller)
- No timestamps (optional in Prometheus format)

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
    // Validate method (pattern from A1 existing handlers)
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

    // Ensure trailing newline (required by Prometheus format from A4)
    if (metrics->len == 0 || metrics->str[metrics->len - 1] != '\n') {
        g_string_append_c(metrics, '\n');
    }

    // Build HTTP response with Prometheus MIME type (from A4)
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
             "Content-Length: %zu\r\n"
             "\r\n",
             metrics->len);

    // Send response (blocking I/O, single-threaded model from A1)
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
- Follows blocking I/O pattern from A1 (single-threaded)

---

### Integration with HTTP Server

**Based on A1 findings, registration pattern:**

```c
// In lrf_controller.c main() function
#include "metrics.h"

int main(int argc, char* argv[]) {
    // ... existing initialization ...

    // Create HTTP server
    HttpServer* http_server = http_server_new(PORT);

    // Register existing handlers (from A1:lrf_controller.c:163-173)
    http_server_add_handler(http_server, "/distance", distance_handler, NULL);
    http_server_add_handler(http_server, "/command", command_handler, NULL);
    http_server_add_handler(http_server, "/status", status_handler, NULL);

    // Register NEW metrics handler
    http_server_add_handler(http_server, "/metrics", metrics_handler, NULL);

    // Start server
    if (!http_server_start(http_server)) {
        http_server_free(http_server);
        if (lrf_device) {
            lrf_close(lrf_device);
        }
        panic("Failed to start HTTP server");
    }

    // ... rest of main ...
}
```

**Registration location:** After existing handler registration in main()

**Integration points:**
1. Include metrics.h in lrf_controller.c
2. Add handler registration after line 173 (after status_handler)
3. Increment g_http_requests_total in each existing handler (optional - for observability)
4. Increment g_i2c_errors_total when lrf_read_distance fails

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
- Follows graceful degradation pattern

**Error codes:**
- -1 for functions returning integers/doubles (sentinel value)
- Non-zero return code for functions taking output pointers
- NULL checks for file operations

---

### Memory Management

**Strategy:**
1. **GString usage**: Let GLib manage buffer growth automatically
2. **No dynamic allocation in hot path**: Use stack variables where possible
3. **Static buffers for labels**: Fixed-size char arrays for label strings
4. **Cleanup**: Always free GString after response sent

**Memory footprint estimate:**
- GString buffer: ~2-4 KB for all metrics (12 metrics × ~200 bytes average)
- Stack variables: ~1 KB
- Static CPU stats: ~64 bytes
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

**GString advantages:**
- Automatic reallocation on growth
- Efficient appending (amortized O(1))
- GLib memory management (consistent with codebase from A1)
- Easy to get final size (metrics->len) for Content-Length

---

### Performance Considerations

**Target:** < 100ms total collection time

**Optimization strategies:**
1. **Lazy evaluation**: Only collect when /metrics accessed
2. **Cache expensive operations**: Store previous CPU stats (already designed)
3. **Minimize file I/O**: Read each /proc file once
4. **Efficient parsing**: Use fscanf/sscanf instead of string manipulation
5. **Skip unnecessary metrics**: Don't collect metrics not needed
6. **Early exit optimization**: Stop parsing /proc/meminfo after finding both fields (from A2)

**Estimated collection times (from A2):**
- Uptime: <1ms
- Memory: <5ms
- CPU: <2ms
- Load: <1ms
- Network: <10ms
- Disk: <1ms
- Process count: <5ms (count /proc entries)
- **Total: <25ms** (well under 100ms target)

**Measurement:**
```c
#include <sys/time.h>

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
- `app/metrics.c` - Implementation (metrics_handler, collect_* functions, append_* helpers)
- `app/metrics_helpers.c` - Helper functions from A2 (get_uptime, get_memory_info, get_cpu_stats, etc.)
- `app/metrics_helpers.h` - Helper function declarations and structs (MemoryInfo, CPUStats, NetworkStats, DiskStats)

**Modify existing files:**
- `app/lrf_controller.c` - Add handler registration (line ~174), increment g_http_requests_total, g_i2c_errors_total
- `app/Makefile` - Add metrics.c and metrics_helpers.c to build

**Rationale:**
- Separation of concerns: metrics collection logic separate from HTTP handler
- Testability: Helper functions can be unit tested independently
- Maintainability: Clear module boundaries

---

### Build System Changes

**Makefile additions:**
```makefile
# Add to SRCS
SRCS += metrics.c metrics_helpers.c

# Add to OBJS
OBJS += metrics.o metrics_helpers.o

# Dependencies remain the same (GLib already included)
```

**Dependencies:**
- GLib (already used in http_server.c from A1)
- syslog.h (already used in lrf_controller.c from A1)
- sys/statfs.h (standard POSIX, available on ARM)
- No additional libraries needed

---

### Testing Strategy

**Unit tests:**
1. Test helper functions with mock /proc files
   ```c
   // Test get_uptime with known input
   // Test get_memory_info with known /proc/meminfo
   // Test append_metric_* functions with known inputs
   ```

2. Test append_metric_* functions
   ```c
   GString* output = g_string_new("");
   append_metric_gauge(output, "test_metric", "Test help", NULL, 42.5);
   assert(strstr(output->str, "# HELP test_metric Test help\n"));
   assert(strstr(output->str, "# TYPE test_metric gauge\n"));
   assert(strstr(output->str, "test_metric 42.50\n"));
   g_string_free(output, TRUE);
   ```

3. Verify Prometheus format correctness
   ```bash
   # Generate metrics output to file
   ./test_metrics_output > metrics.txt
   # Validate with promtool
   promtool check metrics metrics.txt
   ```

**Integration tests:**
1. Build and deploy ACAP to camera
   ```bash
   make
   scp lrf_controller_1_0_0_armv7hf.eap admin@192.168.30.15:/tmp/
   ssh admin@192.168.30.15 "cd /tmp && eap-install.sh install lrf_controller_1_0_0_armv7hf.eap"
   ```

2. Test /metrics endpoint
   ```bash
   curl -u admin:password http://192.168.30.15/local/lrf_controller/lrf/metrics
   ```

3. Validate format
   ```bash
   curl -u admin:password http://192.168.30.15/local/lrf_controller/lrf/metrics | promtool check metrics
   ```

4. Check response time
   ```bash
   time curl -u admin:password http://192.168.30.15/local/lrf_controller/lrf/metrics
   # Expect < 500ms (network + collection time)
   ```

5. Verify memory usage
   ```bash
   ssh admin@192.168.30.15 "ps aux | grep lrf_controller"
   # Memory should be < 5 MB total application
   ```

**Performance benchmarks:**
```bash
# Stress test with ApacheBench
ab -n 100 -c 10 -A admin:password http://192.168.30.15/local/lrf_controller/lrf/metrics

# Check for memory leaks (run multiple times)
for i in {1..100}; do
  curl -u admin:password http://192.168.30.15/local/lrf_controller/lrf/metrics > /dev/null
done
ssh admin@192.168.30.15 "ps aux | grep lrf_controller"
# Memory should remain stable
```

---

### Integration with VAPIX (from A8)

**Note:** VAPIX integration (temperature) will be handled by A8 agent.
Metrics module should provide hook for adding VAPIX metrics:

```c
// In metrics.h - add declaration
void collect_vapix_metrics(GString* output);

// In metrics.c - add placeholder implementation
void collect_vapix_metrics(GString* output) {
    // Will be implemented by VAPIX client module (A8)
    // Placeholder for now - A8 will provide implementation
    (void)output;  // Unused in Phase 1
}
```

Call from metrics_handler():
```c
void metrics_handler(...) {
    // ...
    collect_system_metrics(metrics);
    collect_network_metrics(metrics);
    collect_disk_metrics(metrics);
    collect_service_metrics(metrics);
    collect_vapix_metrics(metrics);  // Add VAPIX metrics (A8)
    // ...
}
```

**Integration approach:**
- A8 will provide implementation for collect_vapix_metrics()
- Temperature metric will be cached by VAPIX client (background polling)
- collect_vapix_metrics() will read cached value (fast, no blocking HTTP call)
- If temperature unavailable, skip metric (consistent with error handling strategy)

---

### Application Metric Tracking

**Increment global counters in lrf_controller.c:**

```c
// At top of file, after includes
#include "metrics.h"

// In each HTTP handler (distance_handler, command_handler, status_handler)
static void distance_handler(...) {
    g_http_requests_total++;  // Increment at start of handler

    // ... existing logic ...
}

// When I2C errors occur (in lrf_read_distance or similar)
if (!lrf_read_distance(lrf_device, &distance_m)) {
    g_i2c_errors_total++;  // Increment on I2C failure
    http_send_error(client_fd, 500, "Failed to read distance from LRF");
    return;
}
```

**Alternative: Increment in http_server.c:**
```c
// In http_server_run() accept loop, after successful handler dispatch
// This automatically tracks all requests including /metrics
g_http_requests_total++;
```

**Trade-offs:**
- Per-handler increment: More granular (can add per-endpoint labels later)
- Central increment: Simpler, automatic tracking for all endpoints
- **Recommendation:** Central increment in http_server.c for Phase 1

---

### Process Count Implementation

**Helper function (in metrics_helpers.c):**
```c
#include <dirent.h>
#include <ctype.h>

int get_process_count() {
    DIR* dir = opendir("/proc");
    if (!dir) {
        return -1;
    }

    int count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        // Count only numeric directories (PIDs)
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            count++;
        }
    }

    closedir(dir);
    return count;
}
```

**Performance:** <5ms (typically 50-200 processes on embedded system)

---

## Files Analyzed
- `.paf/findings/A1_FINDINGS.md` - HTTP server architecture, handler registration pattern, blocking I/O model
- `.paf/findings/A2_FINDINGS.md` - /proc metrics specifications, parsing code, performance estimates
- `.paf/findings/A4_FINDINGS.md` - Prometheus format specification, MIME type, validation

## Blockers or Uncertainties
None - All dependencies from Wave 1 provide necessary design information

## Confidence Level
**HIGH** - Clear integration pattern from A1, complete metric specifications from A2/A4, all design decisions grounded in existing codebase patterns
