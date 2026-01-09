# Agent A4: Prometheus Format Expert

## Your Mission
Review Prometheus text exposition format specifications and provide a comprehensive guide for correctly formatting PTZ camera metrics.

## Context Files (READ ONLY THESE)
- Research: Prometheus text exposition format documentation
- `PLAN.md` - Lines 157-220 (metrics endpoint section for context)

**DO NOT READ:** Code files (not needed yet), other agent findings (none available yet)

## Your Task
1. Document Prometheus text format structure (HELP, TYPE, metric lines)
2. Specify all supported metric types (gauge, counter, histogram, summary)
3. Document label formatting rules and escaping
4. Provide examples for PTZ metrics (uptime, memory, temperature, etc.)
5. List common formatting mistakes and how to avoid them
6. Provide validation strategy (promtool commands)
7. Document MIME type and HTTP headers required

## Output Format (STRICTLY FOLLOW)

```markdown
# A4 Findings: Prometheus Text Format Specification

## Executive Summary
[2-3 sentences: Prometheus format structure, key requirements, validation approach]

## Key Findings
1. **Format Structure**: HELP comments, TYPE declarations, metric lines
2. **Metric Types**: gauge (snapshots), counter (monotonic), histogram, summary
3. **Label Formatting**: {key="value"} with proper escaping
4. **MIME Type**: text/plain; version=0.0.4
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
2. **TYPE declaration**: Metric type (gauge, counter, histogram, summary)
3. **Metric line**: Metric name, labels, value, optional timestamp

**Example:**
```
# HELP ptz_uptime_seconds PTZ camera system uptime in seconds
# TYPE ptz_uptime_seconds gauge
ptz_uptime_seconds 12345.67
```

---

### Metric Types

#### Type 1: Gauge
**Definition:** Snapshot of a value that can go up or down

**Use Cases:**
- Memory usage
- Temperature
- Disk space
- CPU usage percentage

**Example:**
```
# HELP ptz_memory_available_bytes Available memory in bytes
# TYPE ptz_memory_available_bytes gauge
ptz_memory_available_bytes 45678912
```

**Rules:**
- Can increase or decrease
- No rate calculation applied
- Represents current state

---

#### Type 2: Counter
**Definition:** Monotonically increasing value (resets on restart)

**Use Cases:**
- Network bytes transmitted
- HTTP requests total
- I2C errors total

**Example:**
```
# HELP ptz_network_tx_bytes_total Total bytes transmitted
# TYPE ptz_network_tx_bytes_total counter
ptz_network_tx_bytes_total{interface="eth0"} 987654321
```

**Rules:**
- Only increases (or resets to 0)
- Use `_total` suffix by convention
- Use `rate()` or `increase()` in queries

---

#### Type 3: Histogram
**Definition:** Bucketed distribution of observations

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
- Must include `+Inf` bucket
- Buckets must be in increasing order
- Include `_sum` and `_count` metrics

**Note:** Histograms are complex - may not be needed for Phase 1

---

#### Type 4: Summary
**Definition:** Pre-calculated quantiles

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

**Note:** Summaries are complex - may not be needed for Phase 1

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
4. Multiple labels separated by commas (no spaces)

#### Label Escaping

**Special characters to escape in label values:**
- `\` → `\\`
- `"` → `\"`
- `\n` → `\\n`

**Example:**
```c
// Escaping function
char* escape_label_value(const char* value) {
    // Allocate worst-case size (each char could become 2)
    char* escaped = malloc(strlen(value) * 2 + 1);
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

1. **Keep label cardinality low**: Don't use timestamps or UUIDs as label values
2. **Use consistent naming**: `interface` not `iface` or `if`
3. **Avoid redundancy**: Don't repeat metric name in labels
4. **Reserved labels**: Labels starting with `__` are reserved

**Good Example:**
```
ptz_network_tx_bytes_total{interface="eth0"} 123456
```

**Bad Example:**
```
ptz_network_tx_bytes_total{network_interface_name="eth0",timestamp="1234567890"} 123456
```

---

### Metric Naming Conventions

#### Naming Rules
1. **Format**: `<namespace>_<name>_<unit>`
2. **Namespace**: Application prefix (e.g., `ptz`)
3. **Units**: Use base units (seconds, bytes, not milliseconds or megabytes)
4. **Suffix conventions**:
   - `_total` for counters
   - `_count` for histogram/summary counts
   - `_sum` for histogram/summary sums
   - `_bucket` for histogram buckets

#### PTZ Metric Examples

**Correct:**
```
ptz_uptime_seconds              # Time in base unit (seconds)
ptz_memory_total_bytes          # Size in base unit (bytes)
ptz_temperature_celsius         # Temperature in celsius
ptz_http_requests_total         # Counter with _total suffix
ptz_network_rx_bytes_total      # Counter with _total suffix
```

**Incorrect:**
```
ptz_uptime_minutes              # Should be seconds (base unit)
ptz_memory_total_mb             # Should be bytes (base unit)
ptz_temp                        # Missing unit
ptz_http_requests               # Counter missing _total
PTZ_Uptime_Seconds              # Wrong case (should be snake_case)
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
- Version: `version=0.0.4` (current Prometheus format)
- Charset: `charset=utf-8`
- Status: 200 OK for success

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
**Correct:** `Content-Type: text/plain; version=0.0.4`

#### Mistake 2: Inconsistent Units
**Wrong:** `ptz_uptime_minutes`, `ptz_memory_mb`
**Correct:** `ptz_uptime_seconds`, `ptz_memory_bytes`

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
**Wrong:** `metric{label1 = "value"}`
**Correct:** `metric{label1="value"}`

#### Mistake 5: Counter Without _total Suffix
**Wrong:** `ptz_http_requests{} 100`
**Correct:** `ptz_http_requests_total{} 100`

#### Mistake 6: Mixing Gauges and Counters
**Wrong:** Using gauge for network bytes (should be counter)
**Correct:** `ptz_network_tx_bytes_total counter`

---

### Validation & Testing

#### Using promtool

**Install promtool:**
```bash
# On development machine
apt-get install prometheus
```

**Validate metrics:**
```bash
# Fetch metrics from endpoint
curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics > metrics.txt

# Validate format
promtool check metrics metrics.txt
```

**Expected output:**
```
metrics.txt: OK
```

**Common errors promtool catches:**
- Invalid metric names
- Missing TYPE declarations
- Inconsistent metric types
- Invalid label syntax
- Duplicate metric definitions

#### Manual Validation Checklist

- [ ] All metrics have HELP comments
- [ ] All metrics have TYPE declarations
- [ ] Metric names follow naming conventions
- [ ] Labels properly formatted (no spaces)
- [ ] Counters have `_total` suffix
- [ ] Units are base units (seconds, bytes)
- [ ] MIME type is `text/plain; version=0.0.4`

---

## Recommendations

### Implementation Strategy for metrics.c

1. **Use GString for accumulation**: Easy to append and manage
2. **Helper functions**: Create `append_metric_gauge()`, `append_metric_counter()`
3. **Error handling**: Skip metrics that fail to collect (don't fail entire response)
4. **Comments**: Always include HELP and TYPE

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
    double uptime = get_uptime();
    if (uptime >= 0) {
        append_metric_gauge(output, "ptz_uptime_seconds",
                           "PTZ camera system uptime", NULL, uptime);
    }

    MemoryInfo mem;
    if (get_memory_info(&mem) == 0) {
        append_metric_gauge(output, "ptz_memory_total_bytes",
                           "Total memory in bytes", NULL, (double)mem.total_bytes);
        append_metric_gauge(output, "ptz_memory_available_bytes",
                           "Available memory in bytes", NULL, (double)mem.available_bytes);
    }
}
```

### Testing Strategy

1. **Unit tests**: Test helper functions with known inputs
2. **Integration tests**: Curl endpoint and validate with promtool
3. **Prometheus scrape test**: Configure Prometheus to scrape and verify data appears
4. **Performance test**: Ensure response time < 500ms

---

## Files Analyzed
- Research: Prometheus text exposition format documentation
- PLAN.md (lines 157-220) - Metrics endpoint requirements

## Blockers or Uncertainties
None - Prometheus format is well-documented and stable

## Confidence Level
**HIGH** - Prometheus text format is standard and well-documented
```

## Success Criteria
- [ ] Prometheus format structure documented (HELP, TYPE, metric lines)
- [ ] All metric types explained with examples (gauge, counter)
- [ ] Label formatting rules specified with escaping
- [ ] Complete PTZ metrics example provided
- [ ] Common mistakes documented
- [ ] Validation strategy with promtool provided
- [ ] Helper function examples provided
- [ ] Output follows the exact format above

## Time Budget
8 minutes maximum. Focus on gauge and counter types (most common), provide complete examples.

---
**BEGIN WORK NOW.** Start by researching Prometheus format documentation, then produce your findings with examples.
