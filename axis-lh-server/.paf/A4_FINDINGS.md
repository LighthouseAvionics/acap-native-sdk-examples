# A4 Findings: Prometheus Text Format Specification

## Executive Summary
Prometheus text exposition format 0.0.4 is a line-oriented, UTF-8 format using HELP comments, TYPE declarations, and metric lines with optional labels. The format uses `text/plain; version=0.0.4` MIME type and supports gauge (bidirectional values), counter (monotonic increases), histogram, and summary metric types. Validation is performed using `promtool check metrics`.

## Key Findings
1. **Format Structure**: HELP comments, TYPE declarations, metric lines (newline-separated)
2. **Metric Types**: gauge (snapshots), counter (monotonic with `_total` suffix), histogram, summary
3. **Label Formatting**: {key="value"} with escaping for `\`, `"`, and `\n`
4. **MIME Type**: text/plain; version=0.0.4; charset=utf-8
5. **Validation**: Use promtool check metrics

## Detailed Analysis

### Prometheus Text Format Structure

**Basic Structure:**
```
# HELP <metric_name> <help text>
# TYPE <metric_name> <metric_type>
<metric_name>{label1="value1",label2="value2"} <value> <timestamp_ms>
```

**Components:**
1. **HELP comment**: Human-readable description of metric (optional but recommended)
   - Pattern: `# HELP metric_name docstring`
   - UTF-8 characters allowed, escape backslashes as `\\` and newlines as `\n`
   - Only one HELP declaration per metric

2. **TYPE declaration**: Metric type (gauge, counter, histogram, summary, untyped)
   - Pattern: `# TYPE metric_name [counter|gauge|histogram|summary|untyped]`
   - Must appear before any samples for that metric
   - Only one TYPE line per metric

3. **Metric line**: Metric name, optional labels, value, optional timestamp
   - Pattern: `metric_name{labels} value [timestamp]`
   - Values are floats (including NaN, +Inf, -Inf)
   - Timestamps are int64 milliseconds since epoch (optional)

**Line Format Rules:**
- Lines separated by newline characters (`\n`)
- Final line must terminate in a newline
- Empty lines are ignored
- Tokens within a line separated by at least one space or tab
- Leading and trailing whitespace is stripped
- All samples for a single metric must form one contiguous group

**Example:**
```
# HELP ptz_uptime_seconds PTZ camera system uptime in seconds
# TYPE ptz_uptime_seconds gauge
ptz_uptime_seconds 12345.67
```

---

### Metric Types

#### Type 1: Gauge
**Definition:** A metric that represents a single numerical value that can arbitrarily go up and down

**Use Cases:**
- Memory usage (can increase or decrease)
- Temperature readings (fluctuates)
- Disk space available (changes bidirectionally)
- CPU usage percentage (goes up and down)
- Active connections count (increases and decreases)

**Example:**
```
# HELP ptz_memory_available_bytes Available memory in bytes
# TYPE ptz_memory_available_bytes gauge
ptz_memory_available_bytes 45678912
```

**Rules:**
- Can increase or decrease freely
- No rate calculation applied by default
- Represents current state snapshot
- DO NOT use gauge for values that only increase (use counter instead)

---

#### Type 2: Counter
**Definition:** A cumulative metric that represents a single monotonically increasing counter whose value can only increase or be reset to zero on restart

**Use Cases:**
- Network bytes transmitted (always increases)
- HTTP requests total (cumulative count)
- I2C errors total (error count never decreases)
- Tasks completed (monotonic)

**Example:**
```
# HELP ptz_network_tx_bytes_total Total bytes transmitted
# TYPE ptz_network_tx_bytes_total counter
ptz_network_tx_bytes_total{interface="eth0"} 987654321
```

**Rules:**
- Only increases (or resets to 0 on restart)
- MUST use `_total` suffix by convention
- Use `rate()` or `increase()` in PromQL queries to get per-second rate
- NEVER use counter for values that can decrease
- Example: Don't use counter for "number of currently running processes" (use gauge)

---

#### Type 3: Histogram
**Definition:** Bucketed distribution of observations with cumulative counters

**Use Cases:**
- HTTP request latency distribution
- Response size distribution

**Example:**
```
# HELP ptz_http_duration_seconds HTTP request duration
# TYPE ptz_http_duration_seconds histogram
ptz_http_duration_seconds_bucket{le="0.1"} 100
ptz_http_duration_seconds_bucket{le="0.5"} 150
ptz_http_duration_seconds_bucket{le="1.0"} 180
ptz_http_duration_seconds_bucket{le="+Inf"} 200
ptz_http_duration_seconds_sum 125.5
ptz_http_duration_seconds_count 200
```

**Rules:**
- Buckets use `_bucket` suffix with `le` (less than or equal) label
- Must include `+Inf` bucket (equals total count)
- Buckets must be in increasing order
- Include `_sum` metric (sum of all observed values)
- Include `_count` metric (total count of observations)
- The `+Inf` bucket count must equal `_count` value

**Note:** Histograms are complex - may not be needed for Phase 1 PTZ metrics

---

#### Type 4: Summary
**Definition:** Pre-calculated quantiles with sum and count

**Example:**
```
# HELP ptz_http_duration_seconds HTTP request duration
# TYPE ptz_http_duration_seconds summary
ptz_http_duration_seconds{quantile="0.5"} 0.12
ptz_http_duration_seconds{quantile="0.9"} 0.45
ptz_http_duration_seconds{quantile="0.99"} 0.89
ptz_http_duration_seconds_sum 125.5
ptz_http_duration_seconds_count 200
```

**Rules:**
- Quantiles calculated on client side
- Cannot be aggregated across multiple instances
- Include `_sum` and `_count` metrics
- Prometheus recommends histograms over summaries when possible

**Note:** Summaries are complex - may not be needed for Phase 1 PTZ metrics

---

### Label Formatting

#### Label Syntax
```
metric_name{label1="value1",label2="value2"} value
```

**Rules:**
1. Labels enclosed in `{}`
2. Label name: `[a-zA-Z_][a-zA-Z0-9_]*`
3. Label value: UTF-8 string in double quotes
4. Multiple labels separated by commas (NO spaces between labels)
5. At least one space/tab between labels `}` and value

#### Label Escaping

**Special characters to escape in label values:**
- `\` → `\\` (backslash)
- `"` → `\"` (double quote)
- newline → `\n` (literal backslash-n)

**Example:**
```c
// Escaping function for label values
char* escape_label_value(const char* value) {
    // Allocate worst-case size (each char could become 2)
    size_t len = strlen(value);
    char* escaped = malloc(len * 2 + 1);
    char* dst = escaped;

    for (const char* src = value; *src; src++) {
        if (*src == '\\' || *src == '"') {
            *dst++ = '\\';
        }
        *dst++ = *src;
    }
    *dst = '\0';

    return escaped;
}
```

#### Label Best Practices

1. **Keep label cardinality low**: Don't use timestamps, UUIDs, or unbounded values as label values
   - High cardinality kills Prometheus performance
   - Example: Don't use `{user_id="12345"}` if you have millions of users

2. **Use consistent naming**: Pick one convention and stick to it
   - Good: `interface` everywhere
   - Bad: Mix of `interface`, `iface`, `if`, `network_interface`

3. **Avoid redundancy**: Don't repeat metric name in labels
   - Good: `ptz_network_tx_bytes_total{interface="eth0"}`
   - Bad: `ptz_network_tx_bytes_total{network_interface_name="eth0"}`

4. **Reserved labels**: Labels starting with `__` are reserved for internal use

5. **Don't put data in labels**: Timestamps, measurements, or values don't belong in labels
   - Good: `temperature_celsius 45.0`
   - Bad: `status{temperature="45.0"} 1`

**Good Example:**
```
ptz_network_tx_bytes_total{interface="eth0"} 123456
ptz_service_up{service="ssh",state="active"} 1
```

**Bad Example:**
```
ptz_network_tx_bytes_total{network_interface_name="eth0",timestamp="1234567890"} 123456
ptz_temperature{unit="celsius",reading="45"} 1
```

---

### Metric Naming Conventions

#### Naming Rules
1. **Format**: `<namespace>_<name>_<unit>` or `<namespace>_<name>_<unit>_<suffix>`
2. **Case**: snake_case (lowercase with underscores)
3. **Regex**: `[a-zA-Z_:][a-zA-Z0-9_:]*` (best practice: avoid colons)
4. **Namespace**: Application prefix (e.g., `ptz` for PTZ camera)
5. **Units**: Use base units (seconds, bytes, celsius, not milliseconds or megabytes)
6. **Suffix conventions**:
   - `_total` for counters (REQUIRED)
   - `_count` for histogram/summary counts
   - `_sum` for histogram/summary sums
   - `_bucket` for histogram buckets

#### What to Avoid

1. **Don't use colons**: Reserved for recording rules on Prometheus server
2. **Don't include metric type**: Don't put "gauge" or "counter" in name
3. **Don't use camelCase**: Use snake_case instead
4. **Don't put label names in metric name**: Redundant and causes confusion
5. **Don't use non-base units**: Use `seconds` not `milliseconds`, `bytes` not `megabytes`

#### PTZ Metric Examples

**Correct:**
```
ptz_uptime_seconds              # Time in base unit (seconds)
ptz_memory_total_bytes          # Size in base unit (bytes)
ptz_temperature_celsius         # Temperature in celsius
ptz_http_requests_total         # Counter with _total suffix
ptz_network_rx_bytes_total      # Counter with _total suffix
ptz_load_average_1m             # Load average (dimensionless)
```

**Incorrect:**
```
ptz_uptime_minutes              # Should be seconds (base unit)
ptz_memory_total_mb             # Should be bytes (base unit)
ptz_temp                        # Missing unit
ptz_http_requests               # Counter missing _total
PTZ_Uptime_Seconds              # Wrong case (should be snake_case)
ptz_uptime_seconds_gauge        # Don't include metric type in name
ptz_temperature_celsius_reading # Redundant "_reading" suffix
```

---

### Complete PTZ Metrics Example

```
# HELP ptz_uptime_seconds PTZ camera system uptime
# TYPE ptz_uptime_seconds gauge
ptz_uptime_seconds 12345.67

# HELP ptz_memory_total_bytes Total memory in bytes
# TYPE ptz_memory_total_bytes gauge
ptz_memory_total_bytes 598528000

# HELP ptz_memory_available_bytes Available memory in bytes
# TYPE ptz_memory_available_bytes gauge
ptz_memory_available_bytes 45678000

# HELP ptz_cpu_usage_percent CPU usage percentage
# TYPE ptz_cpu_usage_percent gauge
ptz_cpu_usage_percent 15.3

# HELP ptz_load_average_1m 1-minute load average
# TYPE ptz_load_average_1m gauge
ptz_load_average_1m 0.52

# HELP ptz_disk_total_bytes Total disk space in bytes
# TYPE ptz_disk_total_bytes gauge
ptz_disk_total_bytes 104857600

# HELP ptz_disk_free_bytes Free disk space in bytes
# TYPE ptz_disk_free_bytes gauge
ptz_disk_free_bytes 78643200

# HELP ptz_temperature_celsius PTZ camera temperature in celsius
# TYPE ptz_temperature_celsius gauge
ptz_temperature_celsius 45.0

# HELP ptz_network_rx_bytes_total Total bytes received
# TYPE ptz_network_rx_bytes_total counter
ptz_network_rx_bytes_total{interface="eth0"} 98765432

# HELP ptz_network_tx_bytes_total Total bytes transmitted
# TYPE ptz_network_tx_bytes_total counter
ptz_network_tx_bytes_total{interface="eth0"} 12345678

# HELP ptz_http_requests_total Total HTTP requests handled
# TYPE ptz_http_requests_total counter
ptz_http_requests_total 1234

# HELP ptz_i2c_errors_total Total I2C communication errors
# TYPE ptz_i2c_errors_total counter
ptz_i2c_errors_total 0

# HELP ptz_service_up Service availability status (1=up, 0=down)
# TYPE ptz_service_up gauge
ptz_service_up{service="ssh"} 1
ptz_service_up{service="http"} 1

# HELP ptz_process_count Number of running processes
# TYPE ptz_process_count gauge
ptz_process_count 87
```

---

### HTTP Response Format

#### Required Headers

```
HTTP/1.1 200 OK
Content-Type: text/plain; version=0.0.4; charset=utf-8
Content-Length: <length>

<metrics content>
```

**Key Points:**
- MIME type: `text/plain` (NOT `application/json`)
- Version parameter: `version=0.0.4` (current Prometheus format)
- Charset: `charset=utf-8`
- Status: 200 OK for success
- Content-Length: Actual byte count of response body
- Final newline: Metrics content MUST end with `\n`

#### C Implementation Example

```c
void metrics_handler(int client_fd, HttpRequest* request, gpointer user_data) {
    if (g_strcmp0(request->method, "GET") != 0) {
        http_send_error(client_fd, 405, "Method not allowed");
        return;
    }

    GString* metrics = g_string_new("");

    // Collect all metrics
    collect_system_metrics(metrics);
    collect_network_metrics(metrics);
    collect_service_metrics(metrics);

    // Ensure trailing newline
    if (metrics->len == 0 || metrics->str[metrics->len - 1] != '\n') {
        g_string_append_c(metrics, '\n');
    }

    // Send response with correct headers
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
             "Content-Length: %zu\r\n"
             "\r\n",
             metrics->len);

    send(client_fd, header, strlen(header), 0);
    send(client_fd, metrics->str, metrics->len, 0);

    g_string_free(metrics, TRUE);
}
```

---

### Common Mistakes to Avoid

#### Mistake 1: Wrong MIME Type
**Wrong:** `Content-Type: application/json`
**Wrong:** `Content-Type: text/plain` (missing version)
**Correct:** `Content-Type: text/plain; version=0.0.4; charset=utf-8`

#### Mistake 2: Inconsistent Units
**Wrong:** `ptz_uptime_minutes`, `ptz_memory_mb`, `ptz_temperature_fahrenheit`
**Correct:** `ptz_uptime_seconds`, `ptz_memory_bytes`, `ptz_temperature_celsius`
**Why:** Always use base units (seconds, bytes). Keep units consistent.

#### Mistake 3: Missing TYPE Declaration
**Wrong:**
```
# HELP ptz_uptime_seconds Uptime
ptz_uptime_seconds 123
```
**Correct:**
```
# HELP ptz_uptime_seconds Uptime
# TYPE ptz_uptime_seconds gauge
ptz_uptime_seconds 123
```

#### Mistake 4: Spaces in Labels
**Wrong:** `metric{label1 = "value", label2 ="value2"}`
**Correct:** `metric{label1="value",label2="value2"}`
**Why:** No spaces around `=` or after commas in label sets

#### Mistake 5: Counter Without _total Suffix
**Wrong:** `ptz_http_requests{} 100`
**Wrong:** `ptz_network_bytes{} 12345`
**Correct:** `ptz_http_requests_total{} 100`
**Correct:** `ptz_network_bytes_total{} 12345`

#### Mistake 6: Mixing Gauges and Counters
**Wrong:** Using gauge for network bytes transmitted (should be counter - monotonic)
**Wrong:** Using counter for memory available (should be gauge - bidirectional)
**Correct:** Network bytes = counter (only increases), Memory = gauge (fluctuates)

#### Mistake 7: Missing Trailing Newline
**Wrong:** Metrics string without final `\n`
**Correct:** All metrics output must end with newline character
**Why:** Prometheus text format specification requires final newline

#### Mistake 8: High Cardinality Labels
**Wrong:** `ptz_request{user_id="12345",timestamp="1234567890"}` (unbounded values)
**Correct:** `ptz_request{endpoint="/api/status",method="GET"}` (bounded values)
**Why:** High cardinality kills Prometheus performance

---

### Validation & Testing

#### Using promtool

**Install promtool:**
```bash
# On Ubuntu/Debian development machine
apt-get install prometheus

# On macOS
brew install prometheus
```

**Validate metrics:**
```bash
# Fetch metrics from endpoint
curl -u admin:password http://192.168.30.15/local/lrf_controller/lrf/metrics > metrics.txt

# Validate format
promtool check metrics metrics.txt
```

**Expected output:**
```
metrics.txt
  GOOD: 12 samples found
  GOOD: 12 metrics
```

**Common errors promtool catches:**
```
# Invalid metric name (uppercase)
Error: invalid metric name "PTZ_uptime_seconds"

# Missing TYPE declaration
Error: metric "ptz_uptime_seconds" does not have a TYPE

# Counter without _total suffix
Warning: counter "ptz_http_requests" should have _total suffix

# Duplicate metric
Error: duplicate sample for metric "ptz_uptime_seconds{}"

# Invalid label syntax
Error: expected '=' after label name, got ' '
```

#### Manual Validation Checklist

**Format Requirements:**
- [ ] All lines end with `\n` (including last line)
- [ ] No spaces around `=` in labels
- [ ] No spaces after commas in label sets
- [ ] Label values properly escaped (`\`, `"`, `\n`)

**Metric Requirements:**
- [ ] All metrics have HELP comments
- [ ] All metrics have TYPE declarations
- [ ] TYPE comes before first sample
- [ ] Metric names follow snake_case convention
- [ ] Metric names use base units (seconds, bytes)
- [ ] Counters have `_total` suffix
- [ ] No duplicate metric-label combinations

**HTTP Requirements:**
- [ ] MIME type is `text/plain; version=0.0.4; charset=utf-8`
- [ ] Content-Length header matches actual body length
- [ ] Status code is 200 OK

---

## Recommendations

### Implementation Strategy for metrics.c

1. **Use GString for accumulation**: Easy to append and manage
2. **Helper functions**: Create `append_metric_gauge()`, `append_metric_counter()`
3. **Error handling**: Skip metrics that fail to collect (don't fail entire response)
4. **Always include HELP and TYPE**: Required by specification
5. **Ensure trailing newline**: Critical for format compliance

**Helper Function Example:**
```c
void append_metric_gauge(GString* output, const char* name, const char* help,
                         const char* labels, double value) {
    g_string_append_printf(output, "# HELP %s %s\n", name, help);
    g_string_append_printf(output, "# TYPE %s gauge\n", name);
    if (labels && labels[0]) {
        g_string_append_printf(output, "%s{%s} %.2f\n", name, labels, value);
    } else {
        g_string_append_printf(output, "%s %.2f\n", name, value);
    }
}

void append_metric_counter(GString* output, const char* name, const char* help,
                           const char* labels, uint64_t value) {
    g_string_append_printf(output, "# HELP %s %s\n", name, help);
    g_string_append_printf(output, "# TYPE %s counter\n", name);
    if (labels && labels[0]) {
        g_string_append_printf(output, "%s{%s} %lu\n", name, labels, value);
    } else {
        g_string_append_printf(output, "%s %lu\n", name, value);
    }
}
```

**Usage:**
```c
void collect_system_metrics(GString* output) {
    // Uptime
    FILE* fp = fopen("/proc/uptime", "r");
    if (fp) {
        double uptime;
        if (fscanf(fp, "%lf", &uptime) == 1) {
            append_metric_gauge(output, "ptz_uptime_seconds",
                               "PTZ camera system uptime", NULL, uptime);
        }
        fclose(fp);
    }

    // Memory
    fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        uint64_t mem_total = 0, mem_available = 0;

        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "MemTotal: %lu kB", &mem_total) == 1) {
                mem_total *= 1024; // Convert to bytes
            } else if (sscanf(line, "MemAvailable: %lu kB", &mem_available) == 1) {
                mem_available *= 1024; // Convert to bytes
            }
        }
        fclose(fp);

        if (mem_total > 0) {
            append_metric_gauge(output, "ptz_memory_total_bytes",
                               "Total memory in bytes", NULL, (double)mem_total);
        }
        if (mem_available > 0) {
            append_metric_gauge(output, "ptz_memory_available_bytes",
                               "Available memory in bytes", NULL, (double)mem_available);
        }
    }

    // Load average
    fp = fopen("/proc/loadavg", "r");
    if (fp) {
        double load1;
        if (fscanf(fp, "%lf", &load1) == 1) {
            append_metric_gauge(output, "ptz_load_average_1m",
                               "1-minute load average", NULL, load1);
        }
        fclose(fp);
    }
}
```

### Testing Strategy

1. **Unit tests**: Test helper functions with known inputs
   ```c
   // Test gauge helper
   GString* output = g_string_new("");
   append_metric_gauge(output, "test_metric", "Test help", NULL, 42.5);
   assert(strstr(output->str, "# HELP test_metric Test help\n"));
   assert(strstr(output->str, "# TYPE test_metric gauge\n"));
   assert(strstr(output->str, "test_metric 42.50\n"));
   ```

2. **Integration tests**: Curl endpoint and validate with promtool
   ```bash
   #!/bin/bash
   curl -u admin:password http://192.168.30.15/local/lrf_controller/lrf/metrics > metrics.txt
   promtool check metrics metrics.txt
   if [ $? -eq 0 ]; then
       echo "✓ Metrics format valid"
   else
       echo "✗ Metrics format invalid"
       exit 1
   fi
   ```

3. **Prometheus scrape test**: Configure Prometheus to scrape and verify data appears
   ```yaml
   # prometheus.yml
   scrape_configs:
     - job_name: 'ptz_camera'
       static_configs:
         - targets: ['192.168.30.15:80']
       metrics_path: '/local/lrf_controller/lrf/metrics'
       basic_auth:
         username: 'admin'
         password: 'password'
   ```

4. **Performance test**: Ensure response time < 500ms
   ```bash
   # Benchmark metrics endpoint
   ab -n 100 -c 10 -A admin:password http://192.168.30.15/local/lrf_controller/lrf/metrics
   ```

---

## Files Analyzed
- [Prometheus Exposition Formats Documentation](https://prometheus.io/docs/instrumenting/exposition_formats/)
- [Prometheus Metric Types Documentation](https://prometheus.io/docs/concepts/metric_types/)
- [Prometheus Naming Conventions](https://prometheus.io/docs/practices/naming/)
- PLAN.md (lines 157-220) - Metrics endpoint requirements

## Blockers or Uncertainties
None - Prometheus text format 0.0.4 is stable, well-documented, and widely adopted since 2014.

## Confidence Level
**HIGH** - Prometheus text format is a stable standard with comprehensive official documentation and widespread production usage.

---

## Sources
- [Prometheus Exposition Formats](https://prometheus.io/docs/instrumenting/exposition_formats/)
- [Prometheus Metric Types](https://prometheus.io/docs/concepts/metric_types/)
- [Prometheus Metric and Label Naming](https://prometheus.io/docs/practices/naming/)
- [Prometheus Metric Types Explained](https://victoriametrics.com/blog/prometheus-monitoring-metrics-counters-gauges-histogram-summaries/)
- [Prometheus Best Practices](https://betterstack.com/community/guides/monitoring/prometheus-best-practices/)
