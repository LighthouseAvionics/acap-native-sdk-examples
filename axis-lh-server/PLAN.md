# PTZ Camera Health Monitoring Plan

## Executive Summary

This plan outlines the implementation of health monitoring for Axis Q6225-LE PTZ cameras integrated with the existing VICTOR PLG (Prometheus-Loki-Grafana) monitoring infrastructure. After comprehensive research into the current monitoring architecture and PTZ constraints, I recommend a **native C ACAP application** that exports Prometheus-style metrics and logs to the existing PLG stack.

**Timeline**: 2-3 weeks for full implementation
**Approach**: Extend existing `lrf-controller` ACAP to include health monitoring endpoints
**Integration**: Push metrics/logs to cluster server's Prometheus/Loki via HTTP

---

## Research Findings Summary

### Current VICTOR Monitoring Architecture

**PLG Stack (Cluster Server - lightmode.local:192.168.30.13)**:
- **Prometheus** (port 9090): 30-day metric retention, remote_write receiver enabled
- **Loki** (port 3100): 30-day log retention, TSDB indexing
- **Grafana** (port 3000): 8 dashboards, unified alerting, Slack+Email notifications
- **Alloy** (port 12345): Central collection agent with journald parsing

**Edge Device Pattern (Orins & TCBs)**:
- **Alloy agent** running in Docker with host networking
- **6 exporters** (Orin): node, postgres, redis, nginx, cadvisor, jetson
- **2 exporters** (TCB): node, cadvisor
- **Remote write**: Metrics → Prometheus, Logs → Loki
- **Service Discovery**: HTTP SD from Django API endpoint
- **mDNS**: Uses `lcs-plg.local` for cluster discovery

**victor-health Library Integration**:
- Python library for service health status reporting
- Reports via JSON logs to journald (parsed by Alloy)
- Includes: health checks, dependency checks, thread monitoring, systemd integration
- Status levels: HEALTHY, DEGRADED, UNHEALTHY with auto-calculated severity

### PTZ Camera Constraints (192.168.30.15 - Q6225-LE)

**Hardware Resources**:
- CPU: ARMv7 dual-core @ 1.5 GHz
- RAM: 570 MB total, **only 14 MB free** (highly constrained)
- Storage: 100% full root filesystem, 78 MB free flash
- Network: Full IPv4/IPv6, mDNS, MQTT support

**Running Services**: 177 processes including:
- PTZ control daemons (ptzvapixd, iopptzd, acbd)
- Streaming services (larod, stclient)
- VAPIX API server (200+ CGI endpoints)
- I2C buses (9 buses, 12 devices detected)

**Available APIs**:
- VAPIX REST API: Device info, thermal, uptime, storage, I/O status
- System files: /proc filesystem for metrics
- D-Bus: Systemd service status (requires credentials)
- Reverse proxy: Apache routes `/local/<app>/` to app endpoints

**Critical Limitations**:
- **Cannot run Alloy**: Requires 50-100 MB RAM (too heavy)
- **No i2c-tools**: Cannot use `i2cdetect`, `i2cget`, `i2cset`
- **No Python runtime**: Only native compiled binaries
- **Limited disk space**: Applications must be < 10 MB

### Existing lrf-controller ACAP

**Current Implementation**:
- Simple C HTTP server (GLib + Jansson + sockets)
- Port 8080, reverse proxy at `/local/lrf_controller/lrf/*`
- 3 endpoints: `/distance`, `/command`, `/status`
- I2C integration for laser rangefinder communication
- Single-threaded blocking design
- 4096-byte request/response buffer

**Strengths**:
- Already has HTTP server infrastructure
- JSON response format in place
- I2C access working (can monitor I2C health)
- Reverse proxy with admin authentication
- Auto-respawn on failure
- ~390 KB binary size (room to expand)

---

## Recommended Architecture

### Option A: Native C ACAP Health Monitor (RECOMMENDED)

**Approach**: Extend `lrf-controller` to include health monitoring endpoints that export Prometheus-style metrics and structured logs.

**Why This Approach**:
1. **Resource efficient**: 2-5 MB memory footprint (vs 50-100 MB for Alloy)
2. **Reuses existing infrastructure**: HTTP server, reverse proxy, admin auth
3. **Native integration**: C code compiles to <500 KB binary
4. **Push-based**: No need for Alloy agent on PTZ
5. **Proven pattern**: Similar to jetson_exporter on Orins

**Architecture**:

```
┌────────────────────────────────────────────────────────────┐
│  Axis PTZ Camera (192.168.30.15)                          │
├────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐  │
│  │  lrf-controller ACAP (Extended)                     │  │
│  │  ├─ Existing: /distance, /command, /status         │  │
│  │  ├─ NEW: /metrics (Prometheus text format)         │  │
│  │  └─ NEW: /health (JSON health status)              │  │
│  └─────────────────────────────────────────────────────┘  │
│           ↓                                                 │
│  Apache Reverse Proxy (/local/lrf_controller/lrf/*)       │
│           ↓                                                 │
│  HTTPS :443 (admin authentication required)                │
│                                                             │
└────────────────────────────────────────────────────────────┘
                        ↓
                 Network (mDNS)
                        ↓
┌────────────────────────────────────────────────────────────┐
│  Cluster Server (lcs-plg.local:192.168.30.13)             │
├────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐  │
│  │  Prometheus (9090)                                  │  │
│  │  ├─ HTTP SD: Discover PTZ cameras from Django API  │  │
│  │  ├─ Scrape: https://<ptz-ip>/local/lrf.../metrics  │  │
│  │  └─ Store: 30-day retention                        │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐  │
│  │  Loki (3100)                                        │  │
│  │  └─ Receives: Logs pushed from PTZ health endpoint │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐  │
│  │  Grafana (3000)                                     │  │
│  │  ├─ Dashboard: PTZ Camera Health                   │  │
│  │  ├─ Alerts: High temp, disk full, service down     │  │
│  │  └─ Notifications: Slack + Email                   │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                             │
└────────────────────────────────────────────────────────────┘
```

**Data Flow**:
1. PTZ ACAP collects metrics from /proc filesystem and VAPIX API
2. Prometheus scrapes `/metrics` endpoint (15-30s interval)
3. PTZ pushes health status logs to Loki (on status change or 30s interval)
4. Grafana visualizes metrics and generates alerts

---

## Implementation Plan

### Phase 1: Extend lrf-controller with Metrics Endpoint (Week 1)

#### 1.1 Add Prometheus Metrics Endpoint

**New file**: `metrics.c` + `metrics.h`

**Metrics to collect**:

```c
// System metrics (from /proc)
ptz_uptime_seconds           // /proc/uptime
ptz_cpu_usage_percent        // /proc/stat
ptz_memory_total_bytes       // /proc/meminfo
ptz_memory_available_bytes   // /proc/meminfo
ptz_load_average_1m          // /proc/loadavg
ptz_disk_usage_bytes         // statfs() on /
ptz_network_rx_bytes_total   // /proc/net/dev
ptz_network_tx_bytes_total   // /proc/net/dev

// Service metrics (from ps/proc)
ptz_process_count            // /proc filesystem
ptz_service_up{service=""}   // systemctl via D-Bus

// PTZ-specific (from VAPIX or I2C)
ptz_temperature_celsius      // VAPIX /axis-cgi/param.cgi
ptz_i2c_errors_total         // From lrf-controller operations
ptz_http_requests_total      // From server counters
```

**Implementation**:
```c
// metrics.c
void collect_system_metrics(GString* output) {
    // Read /proc/uptime
    FILE* fp = fopen("/proc/uptime", "r");
    double uptime;
    fscanf(fp, "%lf", &uptime);
    fclose(fp);
    g_string_append_printf(output, "ptz_uptime_seconds %.2f\n", uptime);

    // Read /proc/meminfo
    fp = fopen("/proc/meminfo", "r");
    // Parse MemTotal, MemAvailable
    // ... (implementation details)

    // Read /proc/loadavg
    // ... etc
}

void metrics_handler(int client_fd, HttpRequest* request, gpointer user_data) {
    if (g_strcmp0(request->method, "GET") != 0) {
        http_send_error(client_fd, 405, "Method not allowed");
        return;
    }

    GString* metrics = g_string_new("# HELP ptz_uptime_seconds PTZ camera uptime\n");
    g_string_append(metrics, "# TYPE ptz_uptime_seconds gauge\n");

    collect_system_metrics(metrics);
    collect_service_metrics(metrics);
    collect_ptz_metrics(metrics);

    // Send Prometheus text format
    http_send_response(client_fd, 200, "text/plain", metrics->str);
    g_string_free(metrics, TRUE);
}
```

**Registration in main()**:
```c
http_server_add_handler(http_server, "/metrics", metrics_handler, NULL);
```

#### 1.2 Add Health Status Endpoint

**New file**: `health.c` + `health.h`

**Health status structure** (inspired by victor-health):
```c
typedef enum {
    HEALTH_STATUS_HEALTHY,
    HEALTH_STATUS_DEGRADED,
    HEALTH_STATUS_UNHEALTHY
} HealthStatus;

typedef struct {
    char* name;
    double value;
    double warning_threshold;
    double critical_threshold;
    HealthStatus status;
} HealthCheck;

typedef struct {
    char* service_name;
    bool reachable;
    HealthStatus status;
} DependencyCheck;

typedef struct {
    HealthStatus overall_status;
    time_t timestamp;
    HealthCheck** checks;
    size_t check_count;
    DependencyCheck** dependencies;
    size_t dependency_count;
} HealthReport;
```

**Health checks to implement**:
```c
HealthCheck memory_check = {
    .name = "memory_available_mb",
    .value = get_available_memory_mb(),
    .warning_threshold = 50,   // <50 MB = warning
    .critical_threshold = 20   // <20 MB = critical
};

HealthCheck disk_check = {
    .name = "disk_free_mb",
    .value = get_disk_free_mb(),
    .warning_threshold = 100,  // <100 MB = warning
    .critical_threshold = 50   // <50 MB = critical
};

HealthCheck temperature_check = {
    .name = "temperature_celsius",
    .value = get_temperature_vapix(),
    .warning_threshold = 70,   // >70°C = warning
    .critical_threshold = 80   // >80°C = critical
};
```

**JSON output** (similar to victor-health):
```json
{
  "service": "lrf-controller",
  "status": "healthy",
  "severity": "info",
  "timestamp": "2025-01-09T12:34:56Z",
  "checks": [
    {
      "name": "memory_available_mb",
      "value": 45.2,
      "warning": 50,
      "critical": 20,
      "status": "degraded"
    },
    {
      "name": "disk_free_mb",
      "value": 78.5,
      "warning": 100,
      "critical": 50,
      "status": "degraded"
    }
  ],
  "dependencies": [
    {
      "service": "i2c-bus-0",
      "reachable": true,
      "status": "healthy"
    }
  ],
  "metadata": {
    "version": "1.1.0",
    "camera_model": "Q6225-LE",
    "i2c_errors": 0,
    "http_requests": 1234
  }
}
```

#### 1.3 Testing & Validation

**Build**:
```bash
cd lrf-controller
docker build --build-arg ARCH=aarch64 -t lrf-controller:v1.1.0 .
docker cp $(docker create lrf-controller:v1.1.0):/opt/app/lrf_controller_1_1_0_aarch64.eap .
```

**Deploy to PTZ**:
```bash
scp lrf_controller_1_1_0_aarch64.eap root@192.168.30.15:/tmp/
ssh root@192.168.30.15 'eap-install.sh install /tmp/lrf_controller_1_1_0_aarch64.eap'
```

**Test endpoints**:
```bash
# Metrics
curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics

# Health status
curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/health
```

**Expected output**:
- Metrics: Prometheus text format with 10+ metrics
- Health: JSON with status, checks, and dependencies
- Response time: < 500ms
- Memory usage: < 5 MB RSS

---

### Phase 2: Cluster Server Integration (Week 2)

#### 2.1 Prometheus Configuration

**Add HTTP Service Discovery target**:

Update Django API endpoint to include PTZ cameras:
```python
# vct-cluster-server/backend/api/views.py
def prometheus_targets(request):
    targets = []

    # Existing Orin/TCB targets
    # ...

    # Add PTZ targets
    ptz_cameras = PTZCamera.objects.filter(enabled=True)
    ptz_targets = [f"{cam.ip}:443" for cam in ptz_cameras]

    targets.append({
        "targets": ptz_targets,
        "labels": {
            "__metrics_path__": "/local/lrf_controller/lrf/metrics",
            "__scheme__": "https",
            "job": "ptz",
            "device_type": "ptz_camera",
            "camera_model": "Q6225-LE"
        }
    })

    return JsonResponse(targets, safe=False)
```

**Update Prometheus scrape config**:
```yaml
# monitoring/config/prometheus.yml.template
scrape_configs:
  # ... existing configs ...

  - job_name: 'ptz_cameras'
    scheme: https
    tls_config:
      insecure_skip_verify: true  # For self-signed certs
    basic_auth:
      username: 'admin'
      password: '${AXIS_ADMIN_PASSWORD}'
    http_sd_configs:
      - url: 'http://host.docker.internal:80/api/prometheus/targets/'
        refresh_interval: 30s
    relabel_configs:
      - source_labels: [job]
        regex: 'ptz'
        action: keep
```

#### 2.2 Loki Log Collection

**Option A: Push from PTZ (Recommended)**

Add log push endpoint to lrf-controller:
```c
// Push logs to Loki periodically (every 30s or on status change)
void push_logs_to_loki() {
    // Collect recent logs from circular buffer
    json_t* streams = json_array();

    json_t* stream = json_object();
    json_object_set_new(stream, "stream", json_pack("{s:s, s:s}",
        "job", "ptz_camera",
        "hostname", get_hostname()));

    json_t* values = json_array();
    // Add log entries: [timestamp_ns, log_line]
    json_array_append_new(values, json_pack("[s, s]",
        get_timestamp_ns(),
        create_log_line()));

    json_object_set_new(stream, "values", values);
    json_array_append_new(streams, stream);

    // HTTP POST to Loki
    send_to_loki("http://lcs-plg.local:3100/loki/api/v1/push", streams);
    json_decref(streams);
}
```

**Option B: Alloy scrapes via HTTP (Alternative)**

Configure Alloy to scrape health endpoint:
```alloy
// monitoring/config/alloy-victor-health.config
loki.source.http "ptz_health" {
  http {
    url = "https://<ptz-ip>/local/lrf_controller/lrf/health"
    basic_auth {
      username = "admin"
      password = env("AXIS_ADMIN_PASSWORD")
    }
  }

  forward_to = [loki.write.default.receiver]
}
```

#### 2.3 Grafana Dashboard

**Create new dashboard**: `ptz-camera-health.json`

**Panels**:

1. **PTZ Status Overview**
   - Stat panel showing HEALTHY/DEGRADED/UNHEALTHY
   - Query: `ptz_health_status{job="ptz"}`
   - Color: Green/Yellow/Red

2. **Memory Usage**
   - Graph panel showing available memory over time
   - Query: `ptz_memory_available_bytes{job="ptz"}`
   - Threshold: 50 MB warning, 20 MB critical

3. **Disk Usage**
   - Gauge panel showing disk utilization
   - Query: `(ptz_disk_total_bytes - ptz_disk_free_bytes) / ptz_disk_total_bytes * 100`
   - Threshold: 90% warning, 95% critical

4. **Temperature**
   - Graph panel showing temperature trend
   - Query: `ptz_temperature_celsius{job="ptz"}`
   - Threshold: 70°C warning, 80°C critical

5. **Network Traffic**
   - Graph panel showing RX/TX bytes
   - Query: `rate(ptz_network_rx_bytes_total[5m])`

6. **I2C Errors**
   - Counter panel showing I2C communication errors
   - Query: `increase(ptz_i2c_errors_total[1h])`

7. **Service Status**
   - Table showing systemd service states
   - Query: `ptz_service_up{service=~".*"}`

8. **Recent Logs**
   - Logs panel with health status logs
   - Query: `{job="ptz_camera"} | json | line_format "{{.timestamp}} [{{.severity}}] {{.service}}: {{.status}}"`

#### 2.4 Alerting Rules

**Create alert rules**: `monitoring/config/grafana/provisioning/alerting/ptz-alerts.yaml`

```yaml
groups:
  - name: ptz_camera_health
    interval: 30s
    rules:
      - alert: PTZHighTemperature
        expr: ptz_temperature_celsius > 75
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} temperature high"
          description: "Temperature is {{ $value }}°C (threshold: 75°C)"

      - alert: PTZCriticalTemperature
        expr: ptz_temperature_celsius > 80
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} temperature critical"
          description: "Temperature is {{ $value }}°C (threshold: 80°C)"

      - alert: PTZLowMemory
        expr: ptz_memory_available_bytes < 50 * 1024 * 1024
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} low memory"
          description: "Available memory is {{ $value | humanize }}B (threshold: 50 MB)"

      - alert: PTZDiskFull
        expr: (ptz_disk_total_bytes - ptz_disk_free_bytes) / ptz_disk_total_bytes > 0.95
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} disk almost full"
          description: "Disk usage is {{ $value | humanizePercentage }} (threshold: 95%)"

      - alert: PTZServiceDown
        expr: ptz_service_up{service=~"ptzvapixd|iopptzd"} == 0
        for: 2m
        labels:
          severity: critical
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} service {{ $labels.service }} down"
          description: "Critical service is not running"

      - alert: PTZHealthUnhealthy
        expr: ptz_health_status == 3  # UNHEALTHY
        for: 2m
        labels:
          severity: critical
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} health status: UNHEALTHY"
          description: "Camera health check failed"
```

---

### Phase 3: Advanced Features (Week 3)

#### 3.1 VAPIX Integration for Enhanced Metrics

**Add VAPIX client**:

```c
// vapix.c
#include <curl/curl.h>

// Fetch temperature from VAPIX
double get_temperature_vapix() {
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    curl_easy_setopt(curl, CURLOPT_URL,
        "http://localhost/axis-cgi/param.cgi?action=list&group=Properties.System.Temperature");
    curl_easy_setopt(curl, CURLOPT_USERPWD, "admin:password");

    // Parse response and extract temperature value
    // ... implementation

    curl_easy_cleanup(curl);
    return temperature;
}

// Fetch device info
void get_device_info_vapix(char* model, char* serial, char* firmware) {
    // Similar pattern for device identification
}
```

**Additional VAPIX metrics**:
- Storage info (SD card usage)
- Network statistics (bandwidth, packet errors)
- Video stream status (active streams, bitrate)
- I/O port status (digital inputs/outputs)

#### 3.2 Log Buffering & Structured Logging

**Implement circular log buffer**:

```c
// log_buffer.c
#define MAX_LOG_ENTRIES 100

typedef struct {
    time_t timestamp;
    char severity[16];
    char message[256];
} LogEntry;

typedef struct {
    LogEntry entries[MAX_LOG_ENTRIES];
    size_t head;
    size_t count;
} LogBuffer;

static LogBuffer log_buffer = {0};

void log_event(const char* severity, const char* format, ...) {
    LogEntry* entry = &log_buffer.entries[log_buffer.head];

    entry->timestamp = time(NULL);
    strncpy(entry->severity, severity, sizeof(entry->severity) - 1);

    va_list args;
    va_start(args, format);
    vsnprintf(entry->message, sizeof(entry->message), format, args);
    va_end(args);

    log_buffer.head = (log_buffer.head + 1) % MAX_LOG_ENTRIES;
    if (log_buffer.count < MAX_LOG_ENTRIES) {
        log_buffer.count++;
    }

    // Also write to syslog
    syslog(severity_to_syslog(severity), "%s", entry->message);
}

void get_recent_logs(json_t* logs_array) {
    for (size_t i = 0; i < log_buffer.count; i++) {
        size_t idx = (log_buffer.head + MAX_LOG_ENTRIES - log_buffer.count + i) % MAX_LOG_ENTRIES;
        LogEntry* entry = &log_buffer.entries[idx];

        json_t* log_obj = json_pack("{s:I, s:s, s:s}",
            "timestamp", (int64_t)entry->timestamp,
            "severity", entry->severity,
            "message", entry->message);

        json_array_append_new(logs_array, log_obj);
    }
}
```

**Add logs endpoint**:
```c
void logs_handler(int client_fd, HttpRequest* request, gpointer user_data) {
    json_t* root = json_object();
    json_t* logs = json_array();

    get_recent_logs(logs);

    json_object_set_new(root, "service", json_string("lrf-controller"));
    json_object_set_new(root, "logs", logs);

    char* json_str = json_dumps(root, JSON_INDENT(2));
    http_send_json(client_fd, 200, json_str);

    free(json_str);
    json_decref(root);
}
```

#### 3.3 Performance Monitoring

**Add request timing**:

```c
// http_server.c
typedef struct {
    uint64_t total_requests;
    uint64_t errors;
    double total_duration_ms;
    double max_duration_ms;
} ServerStats;

static ServerStats server_stats = {0};

void handle_request(...) {
    struct timeval start, end;
    gettimeofday(&start, NULL);

    server_stats.total_requests++;

    // ... process request ...

    gettimeofday(&end, NULL);
    double duration_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                         (end.tv_usec - start.tv_usec) / 1000.0;

    server_stats.total_duration_ms += duration_ms;
    if (duration_ms > server_stats.max_duration_ms) {
        server_stats.max_duration_ms = duration_ms;
    }
}

// Export in metrics
void collect_server_metrics(GString* output) {
    g_string_append_printf(output,
        "ptz_http_requests_total %lu\n"
        "ptz_http_errors_total %lu\n"
        "ptz_http_duration_ms_max %.2f\n"
        "ptz_http_duration_ms_avg %.2f\n",
        server_stats.total_requests,
        server_stats.errors,
        server_stats.max_duration_ms,
        server_stats.total_duration_ms / server_stats.total_requests);
}
```

#### 3.4 Service Discovery & Auto-Registration

**Add mDNS announcement**:

```c
// mdns.c (using Avahi or mdnsd)
void announce_service() {
    // Announce service: _prometheus-http._tcp.local
    // PTR: lrf-controller._prometheus-http._tcp.local
    // SRV: <hostname>.local:8080
    // TXT: path=/local/lrf_controller/lrf/metrics
}
```

**Django model for PTZ cameras**:

```python
# backend/models.py
class PTZCamera(models.Model):
    hostname = models.CharField(max_length=255)
    ip_address = models.GenericIPAddressField()
    model = models.CharField(max_length=100)
    serial_number = models.CharField(max_length=100)
    firmware_version = models.CharField(max_length=50)
    enabled = models.BooleanField(default=True)
    last_seen = models.DateTimeField(auto_now=True)

    def __str__(self):
        return f"{self.hostname} ({self.ip_address})"
```

**Auto-discovery endpoint**:

```python
# backend/api/views.py
@api_view(['POST'])
def register_ptz(request):
    """PTZ cameras can POST to this endpoint to register themselves"""
    data = request.data

    PTZCamera.objects.update_or_create(
        serial_number=data['serial_number'],
        defaults={
            'hostname': data['hostname'],
            'ip_address': data['ip_address'],
            'model': data['model'],
            'firmware_version': data['firmware_version'],
        }
    )

    return Response({'status': 'registered'})
```

---

## Testing & Validation

### Unit Testing

**Test metrics collection**:
```bash
# Build with test mode
docker build --build-arg BUILD_TESTS=1 -t lrf-controller:test .

# Run unit tests
docker run lrf-controller:test /opt/app/run_tests

# Expected output:
# [PASS] test_metrics_collection
# [PASS] test_health_status_calculation
# [PASS] test_log_buffer
# [PASS] test_vapix_client
```

### Integration Testing

**Test Prometheus scraping**:
```bash
# Manual curl test
curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics

# Validate with promtool
curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics | promtool check metrics

# Expected: No errors, all metrics valid Prometheus format
```

**Test health endpoint**:
```bash
curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/health | jq .

# Validate JSON schema
curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/health | python validate_health_schema.py

# Expected: Valid JSON matching victor-health schema
```

**Test log push**:
```bash
# Check Loki for PTZ logs
curl -G http://lcs-plg.local:3100/loki/api/v1/query \
  --data-urlencode 'query={job="ptz_camera"}' | jq .

# Expected: Recent log entries from PTZ camera
```

### Load Testing

**Metrics endpoint performance**:
```bash
# Apache Bench test
ab -n 1000 -c 10 -A admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics

# Expected:
# - Requests/sec: > 50
# - Mean response time: < 100ms
# - Memory usage: < 5 MB RSS
# - No crashes or errors
```

### Monitoring Validation

**Prometheus targets**:
```bash
curl http://lcs-plg.local:9090/api/v1/targets | jq '.data.activeTargets[] | select(.labels.job=="ptz")'

# Expected: PTZ camera listed with "up" health status
```

**Grafana dashboard**:
```bash
# Navigate to http://lcs-plg.local:3000/monitoring/
# Open "PTZ Camera Health" dashboard
# Verify all panels showing data
```

**Alerting**:
```bash
# Simulate high temperature
ssh root@192.168.30.15 'echo 85 > /tmp/fake_temperature'

# Check for alert in Grafana
curl http://lcs-plg.local:3000/api/alerts | jq '.[] | select(.name=="PTZHighTemperature")'

# Expected: Alert firing, Slack notification sent
```

---

## Deployment Strategy

### Rollout Plan

**Phase 1: Single Camera Pilot (Week 1)**
1. Deploy lrf-controller v1.1.0 to test camera (192.168.30.15)
2. Configure Prometheus to scrape metrics
3. Validate data collection for 48 hours
4. Monitor resource usage (memory, CPU, disk)

**Phase 2: Cluster Integration (Week 2)**
1. Add PTZ dashboard to Grafana
2. Configure alerting rules
3. Set up Slack/Email notifications
4. Test alert firing and resolution

**Phase 3: Fleet Deployment (Week 3)**
1. Deploy to 2-3 additional cameras
2. Monitor for stability issues
3. Fine-tune scrape intervals and retention
4. Document operational procedures

**Phase 4: Full Production (Week 4+)**
1. Deploy to all PTZ cameras
2. Train operations team
3. Establish on-call procedures
4. Create runbooks for common issues

### Rollback Plan

**If issues detected**:
1. Stop lrf-controller ACAP via web UI
2. Uninstall package: `eap-install.sh uninstall lrf-controller`
3. Remove PTZ scrape config from Prometheus
4. Revert Django API changes
5. Investigate root cause before re-deployment

---

## Resource Requirements

### Development Resources

**Engineer Time**:
- Week 1: 40 hours (metrics endpoint, health status, testing)
- Week 2: 40 hours (cluster integration, dashboards, alerting)
- Week 3: 30 hours (advanced features, documentation)
- **Total**: ~110 hours (~3 weeks)

**Equipment**:
- 1x Axis Q6225-LE PTZ for testing (existing: 192.168.30.15)
- Access to cluster server (existing: 192.168.30.13)
- Docker build environment (existing)

### Operational Resources

**Per PTZ Camera**:
- Memory: 2-5 MB (current free: 14 MB - sufficient)
- Storage: ~500 KB binary + <1 MB logs (current free: 78 MB - sufficient)
- CPU: <1% average (should not impact camera operations)
- Network: ~5 KB/s metrics + periodic log push (<1 KB/s) = ~6 KB/s total

**Cluster Server**:
- Prometheus storage: ~10 KB/camera/day × 30 days = 300 KB/camera
- Loki storage: ~50 KB/camera/day × 30 days = 1.5 MB/camera
- For 10 cameras: ~3 MB Prometheus + ~15 MB Loki = ~18 MB total (negligible)

---

## Risk Assessment & Mitigation

### Technical Risks

**Risk 1: Memory Constraints**
- **Likelihood**: Medium
- **Impact**: High (could cause camera instability)
- **Mitigation**:
  - Implement strict memory limits (< 5 MB)
  - Use fixed-size buffers (no dynamic allocation in hot paths)
  - Test under load for 72+ hours
  - Monitor RSS with `/proc/[pid]/status`

**Risk 2: CPU Impact on Video Processing**
- **Likelihood**: Low
- **Impact**: Medium (degraded video quality)
- **Mitigation**:
  - Keep metrics collection < 100ms
  - Use lazy evaluation (collect only on scrape)
  - Test during peak video streaming load
  - Monitor CPU with `top` and `/proc/stat`

**Risk 3: ACAP Crashes**
- **Likelihood**: Low
- **Impact**: High (monitoring blind spot)
- **Mitigation**:
  - Use `runMode: "respawn"` in manifest
  - Implement signal handlers (SIGTERM, SIGSEGV)
  - Add watchdog timer
  - Alert on ACAP restart events

**Risk 4: Network Bandwidth**
- **Likelihood**: Low
- **Impact**: Low (metrics are small)
- **Mitigation**:
  - Compress responses with gzip (if HTTP server supports)
  - Limit scrape frequency (15-30s minimum)
  - Monitor network usage with `/proc/net/dev`

**Risk 5: VAPIX API Rate Limits**
- **Likelihood**: Medium
- **Impact**: Low (some metrics unavailable)
- **Mitigation**:
  - Cache VAPIX responses (1-5 minute TTL)
  - Use VAPIX sparingly (only for advanced metrics)
  - Fallback to /proc filesystem if VAPIX fails

### Operational Risks

**Risk 6: Authentication Management**
- **Likelihood**: Medium
- **Impact**: Medium (scraping failures)
- **Mitigation**:
  - Store credentials in environment variables
  - Use Kubernetes secrets or Vault for production
  - Rotate credentials periodically
  - Monitor authentication failures

**Risk 7: Certificate Expiration**
- **Likelihood**: Low
- **Impact**: Medium (scraping failures)
- **Mitigation**:
  - Use `insecure_skip_verify` for self-signed certs
  - Monitor certificate expiry dates
  - Alert 30 days before expiration

**Risk 8: Firmware Upgrades**
- **Likelihood**: Medium
- **Impact**: High (ACAP compatibility)
- **Mitigation**:
  - Test ACAP on new firmware before deployment
  - Version ACAP for different firmware versions
  - Document firmware compatibility matrix

---

## Alternative Approaches Considered

### Option B: VAPIX Polling from Cluster Server

**Approach**: Python script on cluster server polls VAPIX API endpoints.

**Pros**:
- No code on PTZ camera
- Centralized implementation
- Easy to update

**Cons**:
- Limited metrics (only what VAPIX exposes)
- No /proc filesystem access
- No custom health checks
- Higher network bandwidth
- Authentication complexity

**Verdict**: NOT RECOMMENDED - lacks comprehensive metrics

### Option C: Syslog Export

**Approach**: Configure PTZ to export syslog to cluster server, parse with Alloy.

**Pros**:
- Standard protocol
- No custom code needed
- Works with existing syslog infrastructure

**Cons**:
- Logs only (no metrics)
- Limited structured data
- No health status calculation
- Requires syslog server setup

**Verdict**: NOT RECOMMENDED - insufficient for full monitoring

### Option D: SNMP Monitoring

**Approach**: Use SNMP agent on PTZ camera.

**Pros**:
- Industry standard
- Many existing tools

**Cons**:
- Limited Axis SNMP support
- No custom metrics
- Complex MIB management
- Not integrated with PLG stack

**Verdict**: NOT RECOMMENDED - poor Axis integration

---

## Success Criteria

### Technical Success Criteria

1. **Metrics Collection**
   - ✓ 10+ system metrics collected and exposed
   - ✓ Prometheus successfully scrapes every 30 seconds
   - ✓ Data retention: 30 days in Prometheus
   - ✓ Query latency: < 100ms for standard queries

2. **Health Monitoring**
   - ✓ Health status calculation working (HEALTHY/DEGRADED/UNHEALTHY)
   - ✓ Automatic status transitions based on thresholds
   - ✓ Logs pushed to Loki or scraped by Alloy
   - ✓ Health history queryable for 30 days

3. **Resource Usage**
   - ✓ Memory: < 5 MB RSS
   - ✓ CPU: < 1% average, < 5% peak
   - ✓ Storage: < 1 MB total
   - ✓ Network: < 10 KB/s average

4. **Reliability**
   - ✓ 99.9% uptime over 30 days
   - ✓ Auto-restart on crash (< 5 second downtime)
   - ✓ No memory leaks (RSS stable over 7 days)
   - ✓ No impact on camera video processing

### Business Success Criteria

1. **Operational Visibility**
   - ✓ Grafana dashboard shows all PTZ cameras
   - ✓ Real-time status updates (< 30 second delay)
   - ✓ Historical trending for capacity planning

2. **Proactive Alerting**
   - ✓ Alerts fire before camera failures
   - ✓ False positive rate < 5%
   - ✓ Alert resolution time < 15 minutes
   - ✓ Notifications reach on-call team

3. **Team Adoption**
   - ✓ Operations team trained on new dashboard
   - ✓ Runbooks created for common issues
   - ✓ Positive feedback from operations team
   - ✓ Reduced MTTR for PTZ issues

---

## Next Steps & Action Items

### Immediate Actions (This Week)

1. **Review and Approve Plan**
   - Stakeholder review of proposed architecture
   - Approve timeline and resource allocation
   - Confirm success criteria

2. **Environment Setup**
   - Ensure access to test PTZ (192.168.30.15)
   - Verify cluster server access (192.168.30.13)
   - Set up development Docker environment

3. **Code Skeleton**
   - Create `metrics.c` and `health.c` files
   - Set up build configuration
   - Create test harness

### Week 1 Tasks

- [ ] Implement `/metrics` endpoint with basic system metrics
- [ ] Implement `/health` endpoint with health status calculation
- [ ] Add VAPIX client for temperature monitoring
- [ ] Build and deploy to test camera
- [ ] Validate metrics format with promtool
- [ ] Test memory and CPU usage

### Week 2 Tasks

- [ ] Update Django API for PTZ service discovery
- [ ] Configure Prometheus scraping
- [ ] Create Grafana dashboard
- [ ] Set up alerting rules
- [ ] Test alert notifications (Slack + Email)
- [ ] Document deployment process

### Week 3 Tasks

- [ ] Add log buffering and structured logging
- [ ] Implement performance monitoring
- [ ] Add mDNS service announcement
- [ ] Create operational runbooks
- [ ] Train operations team
- [ ] Prepare for fleet deployment

---

## Conclusion

This plan provides a **comprehensive, resource-efficient approach** to monitoring Axis PTZ cameras using the existing VICTOR PLG infrastructure. By extending the lrf-controller ACAP with native C health monitoring, we avoid the overhead of running Alloy on resource-constrained cameras while maintaining full integration with the centralized monitoring stack.

The phased rollout ensures we validate the approach on a single camera before fleet deployment, minimizing risk. The total implementation time of 3 weeks is realistic given the modular architecture and reuse of existing components.

**Recommendation**: Proceed with Option A (Native C ACAP Health Monitor) as the primary implementation strategy.

---

## Appendix: Research Documentation

All research documents are located in `/home/nick/Workspace/acap-native-sdk-examples/`:

1. **victor-health analysis**: Comprehensive 7-section report on the custom health logging system
2. **PLG stack architecture**: 11-section analysis of Prometheus/Loki/Grafana infrastructure
3. **Edge device Alloy configs**: Comparison of Orin vs TCB monitoring approaches
4. **lrf-controller analysis**: Current HTTP server implementation and extensibility
5. **PTZ capabilities**: System resources, VAPIX API, constraints (5 documents, 70 KB)
6. **ACAP research**: Resource constraints, best practices (5 documents, 85 KB)

**Total research**: 10+ documents, 150+ KB, comprehensive technical analysis

---

**Plan Document Version**: 1.0
**Created**: 2025-01-09
**Author**: Claude (Sonnet 4.5)
**Status**: Ready for Review & Approval
