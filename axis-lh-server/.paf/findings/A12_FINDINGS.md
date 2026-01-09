# A12 Findings: Prometheus/Grafana Integration Checklist

## Executive Summary
Complete integration checklist for deploying PTZ camera monitoring to the VCT cluster server's Prometheus/Grafana stack. Includes Django API modifications for HTTP service discovery, Prometheus scrape configuration with HTTPS/basic auth, comprehensive 8-panel Grafana dashboard specification, 5 alerting rules for temperature/memory/disk/health, and end-to-end validation procedures.

## Integration Checklist

### Phase 1: Prometheus Configuration

#### Task 1.1: Update Django API for PTZ Service Discovery
- [ ] Open `vct-cluster-server/backend/api/views.py`
- [ ] Locate the `prometheus_targets()` function
- [ ] Add PTZ camera target generation after existing targets:
```python
def prometheus_targets(request):
    targets = []

    # Existing Orin/TCB targets
    # ... keep existing code ...

    # Add PTZ cameras
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

- [ ] Verify PTZCamera model exists in `vct-cluster-server/backend/api/models.py`
- [ ] If PTZCamera model doesn't exist, create it:
```python
class PTZCamera(models.Model):
    hostname = models.CharField(max_length=255, unique=True)
    ip_address = models.GenericIPAddressField()
    model = models.CharField(max_length=100, default="Q6225-LE")
    enabled = models.BooleanField(default=True)
    location = models.CharField(max_length=255, blank=True)
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        db_table = 'ptz_cameras'
        ordering = ['hostname']

    def __str__(self):
        return f"{self.hostname} ({self.ip_address})"
```

- [ ] Run Django migrations:
```bash
cd vct-cluster-server/backend
python manage.py makemigrations
python manage.py migrate
```

- [ ] Add PTZ camera entries via Django admin or shell:
```python
python manage.py shell
from api.models import PTZCamera
PTZCamera.objects.create(
    hostname="ptz-camera-01",
    ip_address="192.168.30.15",
    model="Q6225-LE",
    enabled=True
)
```

- [ ] Verify API endpoint returns PTZ targets:
```bash
curl http://localhost:8000/api/prometheus/targets/ | jq .
```

---

#### Task 1.2: Update Prometheus Scrape Config
- [ ] Locate `monitoring/config/prometheus.yml.template`
- [ ] Add new scrape job for PTZ cameras:
```yaml
scrape_configs:
  # ... existing configs (orin, tcb, etc.) ...

  - job_name: 'ptz_cameras'
    scheme: https
    tls_config:
      insecure_skip_verify: true  # PTZ cameras use self-signed certificates
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
    metric_relabel_configs:
      - source_labels: [__name__]
        regex: 'ptz_.*'
        action: keep
```

- [ ] Set environment variable in `.env` or docker-compose:
```bash
AXIS_ADMIN_PASSWORD=your_camera_password
```

- [ ] Verify Prometheus template syntax:
```bash
promtool check config monitoring/config/prometheus.yml.template
```

- [ ] Restart Prometheus container:
```bash
cd monitoring
docker-compose restart prometheus
```

---

#### Task 1.3: Verify Prometheus Targets
- [ ] Navigate to Prometheus targets page: `http://lcs-plg.local:9090/targets`
- [ ] Verify "ptz_cameras" job appears in targets list
- [ ] Verify PTZ camera target shows state: **UP** (green)
- [ ] Check last scrape time is recent (< 30s ago)
- [ ] Check scrape duration is reasonable (< 1s)

**If target is DOWN:**
- [ ] Check PTZ camera is reachable: `curl -k -u admin:password https://192.168.30.15/axis-cgi/param.cgi`
- [ ] Verify lrf-controller ACAP is running: `ssh root@192.168.30.15 'eap-install.sh status lrf_controller'`
- [ ] Check metrics endpoint: `curl -k -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics`
- [ ] Review Prometheus logs: `docker-compose logs prometheus | grep ptz`

- [ ] Test metric queries in Prometheus UI (`http://lcs-plg.local:9090/graph`):
```promql
# Test uptime metric
ptz_uptime_seconds{job="ptz"}

# Test memory metric
ptz_memory_available_bytes{job="ptz"}

# Test temperature metric
ptz_temperature_celsius{job="ptz"}

# Test health status metric (from health endpoint)
ptz_health_status{job="ptz"}
```

- [ ] Verify query returns data with correct labels (hostname, job, device_type)

---

### Phase 2: Grafana Dashboard

#### Task 2.1: Create PTZ Camera Health Dashboard JSON
- [ ] Create new file: `monitoring/config/grafana/dashboards/ptz-camera-health.json`
- [ ] Use the following dashboard specification:

```json
{
  "dashboard": {
    "title": "PTZ Camera Health",
    "uid": "ptz-health-001",
    "tags": ["ptz", "health", "monitoring"],
    "timezone": "browser",
    "schemaVersion": 38,
    "refresh": "30s",
    "panels": [
      {
        "id": 1,
        "title": "PTZ Status Overview",
        "type": "stat",
        "gridPos": {"h": 4, "w": 6, "x": 0, "y": 0},
        "targets": [
          {
            "expr": "ptz_health_status{job=\"ptz\"}",
            "legendFormat": "{{hostname}}"
          }
        ],
        "options": {
          "colorMode": "background",
          "graphMode": "none",
          "textMode": "value_and_name"
        },
        "fieldConfig": {
          "defaults": {
            "mappings": [
              {"type": "value", "value": "0", "text": "HEALTHY"},
              {"type": "value", "value": "1", "text": "DEGRADED"},
              {"type": "value", "value": "2", "text": "UNHEALTHY"}
            ],
            "thresholds": {
              "mode": "absolute",
              "steps": [
                {"color": "green", "value": 0},
                {"color": "yellow", "value": 1},
                {"color": "red", "value": 2}
              ]
            }
          }
        }
      },
      {
        "id": 2,
        "title": "Memory Usage",
        "type": "timeseries",
        "gridPos": {"h": 8, "w": 12, "x": 0, "y": 4},
        "targets": [
          {
            "expr": "ptz_memory_available_bytes{job=\"ptz\"} / 1024 / 1024",
            "legendFormat": "{{hostname}} - Available Memory"
          }
        ],
        "fieldConfig": {
          "defaults": {
            "unit": "MB",
            "thresholds": {
              "mode": "absolute",
              "steps": [
                {"color": "red", "value": 0},
                {"color": "yellow", "value": 20},
                {"color": "green", "value": 50}
              ]
            }
          }
        }
      },
      {
        "id": 3,
        "title": "Disk Usage",
        "type": "gauge",
        "gridPos": {"h": 8, "w": 6, "x": 12, "y": 4},
        "targets": [
          {
            "expr": "100 - (ptz_disk_free_bytes{job=\"ptz\"} / ptz_disk_total_bytes{job=\"ptz\"} * 100)",
            "legendFormat": "{{hostname}}"
          }
        ],
        "fieldConfig": {
          "defaults": {
            "unit": "percent",
            "min": 0,
            "max": 100,
            "thresholds": {
              "mode": "absolute",
              "steps": [
                {"color": "green", "value": 0},
                {"color": "yellow", "value": 90},
                {"color": "red", "value": 95}
              ]
            }
          }
        }
      },
      {
        "id": 4,
        "title": "Temperature",
        "type": "timeseries",
        "gridPos": {"h": 8, "w": 12, "x": 0, "y": 12},
        "targets": [
          {
            "expr": "ptz_temperature_celsius{job=\"ptz\"}",
            "legendFormat": "{{hostname}}"
          }
        ],
        "fieldConfig": {
          "defaults": {
            "unit": "celsius",
            "thresholds": {
              "mode": "absolute",
              "steps": [
                {"color": "green", "value": 0},
                {"color": "yellow", "value": 70},
                {"color": "red", "value": 80}
              ]
            }
          }
        }
      },
      {
        "id": 5,
        "title": "CPU Usage",
        "type": "timeseries",
        "gridPos": {"h": 8, "w": 12, "x": 12, "y": 12},
        "targets": [
          {
            "expr": "ptz_cpu_usage_percent{job=\"ptz\"}",
            "legendFormat": "{{hostname}}"
          }
        ],
        "fieldConfig": {
          "defaults": {
            "unit": "percent",
            "min": 0,
            "max": 100
          }
        }
      },
      {
        "id": 6,
        "title": "Network Traffic",
        "type": "timeseries",
        "gridPos": {"h": 8, "w": 12, "x": 0, "y": 20},
        "targets": [
          {
            "expr": "rate(ptz_network_rx_bytes_total{job=\"ptz\"}[5m])",
            "legendFormat": "{{hostname}} - RX"
          },
          {
            "expr": "rate(ptz_network_tx_bytes_total{job=\"ptz\"}[5m])",
            "legendFormat": "{{hostname}} - TX"
          }
        ],
        "fieldConfig": {
          "defaults": {
            "unit": "Bps"
          }
        }
      },
      {
        "id": 7,
        "title": "I2C Errors",
        "type": "stat",
        "gridPos": {"h": 4, "w": 6, "x": 12, "y": 20},
        "targets": [
          {
            "expr": "increase(ptz_i2c_errors_total{job=\"ptz\"}[1h])",
            "legendFormat": "{{hostname}}"
          }
        ],
        "options": {
          "colorMode": "background",
          "textMode": "value_and_name"
        },
        "fieldConfig": {
          "defaults": {
            "thresholds": {
              "mode": "absolute",
              "steps": [
                {"color": "green", "value": 0},
                {"color": "yellow", "value": 5},
                {"color": "red", "value": 20}
              ]
            }
          }
        }
      },
      {
        "id": 8,
        "title": "Uptime",
        "type": "stat",
        "gridPos": {"h": 4, "w": 6, "x": 18, "y": 20},
        "targets": [
          {
            "expr": "ptz_uptime_seconds{job=\"ptz\"} / 86400",
            "legendFormat": "{{hostname}}"
          }
        ],
        "fieldConfig": {
          "defaults": {
            "unit": "d",
            "decimals": 2
          }
        }
      }
    ]
  }
}
```

- [ ] Copy dashboard JSON to Grafana provisioning directory:
```bash
cp monitoring/config/grafana/dashboards/ptz-camera-health.json \
   monitoring/config/grafana/provisioning/dashboards/
```

---

#### Task 2.2: Configure Grafana Dashboard Provisioning
- [ ] Verify provisioning config exists: `monitoring/config/grafana/provisioning/dashboards/dashboards.yml`
- [ ] If not exists, create:
```yaml
apiVersion: 1

providers:
  - name: 'Default'
    orgId: 1
    folder: ''
    type: file
    disableDeletion: false
    updateIntervalSeconds: 10
    allowUiUpdates: true
    options:
      path: /etc/grafana/provisioning/dashboards
      foldersFromFilesStructure: true
```

- [ ] Restart Grafana container:
```bash
docker-compose restart grafana
```

---

#### Task 2.3: Verify Dashboard in Grafana
- [ ] Navigate to Grafana: `http://lcs-plg.local:3000`
- [ ] Login with admin credentials
- [ ] Search for "PTZ Camera Health" dashboard
- [ ] Verify all 8 panels are visible:
  1. PTZ Status Overview (stat panel)
  2. Memory Usage (time series)
  3. Disk Usage (gauge)
  4. Temperature (time series)
  5. CPU Usage (time series)
  6. Network Traffic (time series)
  7. I2C Errors (stat panel)
  8. Uptime (stat panel)

- [ ] Verify all panels show data (not "No data")
- [ ] Verify auto-refresh is enabled (30s interval)
- [ ] Verify time range picker works correctly
- [ ] Test panel drill-down functionality

**If panels show "No data":**
- [ ] Check Prometheus data source is configured: `http://prometheus:9090`
- [ ] Verify metrics exist in Prometheus UI first
- [ ] Check Grafana logs: `docker-compose logs grafana | grep -i error`
- [ ] Verify dashboard queries are correct (no syntax errors)

---

### Phase 3: Alerting Rules

#### Task 3.1: Create Prometheus Alert Rules
- [ ] Create new file: `monitoring/config/prometheus/alerts/ptz-alerts.yml`
- [ ] Add alert rule definitions:

```yaml
groups:
  - name: ptz_camera_health
    interval: 30s
    rules:
      - alert: PTZHighTemperature
        expr: ptz_temperature_celsius{job="ptz"} > 75
        for: 5m
        labels:
          severity: warning
          component: ptz_camera
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} high temperature"
          description: "Temperature is {{ $value }}°C (warning threshold: 75°C)"
          dashboard_url: "http://lcs-plg.local:3000/d/ptz-health-001"

      - alert: PTZCriticalTemperature
        expr: ptz_temperature_celsius{job="ptz"} > 80
        for: 1m
        labels:
          severity: critical
          component: ptz_camera
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} CRITICAL temperature"
          description: "Temperature is {{ $value }}°C (critical threshold: 80°C). Immediate action required."
          dashboard_url: "http://lcs-plg.local:3000/d/ptz-health-001"

      - alert: PTZLowMemory
        expr: ptz_memory_available_bytes{job="ptz"} < 52428800  # 50 MB
        for: 5m
        labels:
          severity: warning
          component: ptz_camera
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} low memory"
          description: "Available memory: {{ $value | humanize }}B (warning threshold: 50 MB)"
          dashboard_url: "http://lcs-plg.local:3000/d/ptz-health-001"

      - alert: PTZCriticalMemory
        expr: ptz_memory_available_bytes{job="ptz"} < 20971520  # 20 MB
        for: 2m
        labels:
          severity: critical
          component: ptz_camera
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} CRITICAL low memory"
          description: "Available memory: {{ $value | humanize }}B (critical threshold: 20 MB). System may become unstable."
          dashboard_url: "http://lcs-plg.local:3000/d/ptz-health-001"

      - alert: PTZDiskFull
        expr: (ptz_disk_total_bytes{job="ptz"} - ptz_disk_free_bytes{job="ptz"}) / ptz_disk_total_bytes{job="ptz"} > 0.95
        for: 5m
        labels:
          severity: critical
          component: ptz_camera
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} disk almost full"
          description: "Disk usage: {{ $value | humanizePercentage }} (critical threshold: 95%)"
          dashboard_url: "http://lcs-plg.local:3000/d/ptz-health-001"

      - alert: PTZHealthUnhealthy
        expr: ptz_health_status{job="ptz"} == 2  # UNHEALTHY status
        for: 2m
        labels:
          severity: critical
          component: ptz_camera
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} health check FAILED"
          description: "Overall health status is UNHEALTHY. Check individual metrics and dependencies."
          dashboard_url: "http://lcs-plg.local:3000/d/ptz-health-001"

      - alert: PTZHealthDegraded
        expr: ptz_health_status{job="ptz"} == 1  # DEGRADED status
        for: 5m
        labels:
          severity: warning
          component: ptz_camera
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} health degraded"
          description: "Health status is DEGRADED. One or more checks are outside normal thresholds."
          dashboard_url: "http://lcs-plg.local:3000/d/ptz-health-001"

      - alert: PTZHighI2CErrors
        expr: increase(ptz_i2c_errors_total{job="ptz"}[1h]) > 10
        for: 5m
        labels:
          severity: warning
          component: ptz_camera
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} high I2C error rate"
          description: "{{ $value }} I2C errors in the last hour. Check LRF sensor connection."
          dashboard_url: "http://lcs-plg.local:3000/d/ptz-health-001"
```

- [ ] Update Prometheus config to include alert rules:
```yaml
# monitoring/config/prometheus.yml.template
rule_files:
  - '/etc/prometheus/alerts/*.yml'
```

- [ ] Validate alert rules syntax:
```bash
promtool check rules monitoring/config/prometheus/alerts/ptz-alerts.yml
```

- [ ] Restart Prometheus:
```bash
docker-compose restart prometheus
```

---

#### Task 3.2: Configure Notification Channels

**Option A: Slack Notifications**
- [ ] In Grafana UI, navigate to "Alerting" → "Contact points"
- [ ] Click "New contact point"
- [ ] Name: `slack-ptz-alerts`
- [ ] Integration: Select "Slack"
- [ ] Webhook URL: Enter Slack webhook URL (create in Slack workspace settings)
- [ ] Template:
```
{{ range .Alerts }}
*Alert:* {{ .Labels.alertname }}
*Camera:* {{ .Labels.hostname }}
*Severity:* {{ .Labels.severity }}
*Status:* {{ .Status }}
*Description:* {{ .Annotations.description }}
*Dashboard:* {{ .Annotations.dashboard_url }}
{{ end }}
```
- [ ] Click "Test" to verify
- [ ] Save contact point

**Option B: Email Notifications**
- [ ] In Grafana UI, navigate to "Alerting" → "Contact points"
- [ ] Click "New contact point"
- [ ] Name: `email-ops-team`
- [ ] Integration: Select "Email"
- [ ] Addresses: Enter email addresses (comma-separated)
- [ ] Subject: `[PTZ Alert] {{ .GroupLabels.alertname }} on {{ .GroupLabels.hostname }}`
- [ ] Click "Test" to verify
- [ ] Save contact point

**Configure Notification Policy:**
- [ ] Navigate to "Alerting" → "Notification policies"
- [ ] Edit default policy or create new
- [ ] Match labels: `component=ptz_camera`
- [ ] Contact point: Select configured contact point
- [ ] Group by: `hostname, alertname`
- [ ] Group wait: `30s`
- [ ] Group interval: `5m`
- [ ] Repeat interval: `4h`
- [ ] Save policy

---

#### Task 3.3: Test Alerting

**Test 3.3.1: Simulate High Temperature Alert**
- [ ] If possible, stress-test PTZ camera to raise temperature:
```bash
ssh root@192.168.30.15
stress-ng --cpu 4 --timeout 60s
```
- [ ] Monitor temperature: `curl -s http://192.168.30.15:8080/health | jq '.checks[] | select(.name=="temperature_celsius")'`
- [ ] Wait 5 minutes for alert to fire
- [ ] Verify alert appears in Prometheus UI: `http://lcs-plg.local:9090/alerts`
- [ ] Verify notification received in Slack/Email

**Test 3.3.2: Verify Alert Lifecycle**
- [ ] Trigger alert condition
- [ ] Verify alert state: Pending → Firing → Resolved
- [ ] Verify notifications sent at each state transition
- [ ] Verify repeat interval works (alert doesn't spam)

---

### Phase 4: Deployment & Validation

#### Task 4.1: End-to-End Integration Validation

**Checklist:**
- [ ] PTZ camera ACAP deployed and running
- [ ] Metrics endpoint accessible: `https://192.168.30.15/local/lrf_controller/lrf/metrics`
- [ ] Health endpoint accessible: `http://192.168.30.15:8080/health`
- [ ] Django API returns PTZ targets: `curl http://lcs-plg.local/api/prometheus/targets/ | jq '.[] | select(.labels.job=="ptz")'`
- [ ] Prometheus scraping PTZ successfully (check targets page)
- [ ] All PTZ metrics queryable in Prometheus:
  - `ptz_uptime_seconds`
  - `ptz_memory_available_bytes`
  - `ptz_temperature_celsius`
  - `ptz_disk_free_bytes`
  - `ptz_cpu_usage_percent`
  - `ptz_network_rx_bytes_total`
  - `ptz_network_tx_bytes_total`
  - `ptz_i2c_errors_total`
  - `ptz_http_requests_total`
  - `ptz_health_status`
- [ ] Grafana dashboard loads without errors
- [ ] All 8 dashboard panels show data
- [ ] Alert rules active in Prometheus/Grafana
- [ ] Notification channels configured and tested

---

#### Task 4.2: Performance Validation

**Metrics Collection Performance:**
- [ ] Test Prometheus scrape latency:
```bash
# Query Prometheus for scrape duration
curl -s 'http://lcs-plg.local:9090/api/v1/query?query=scrape_duration_seconds{job="ptz"}' | jq '.data.result[0].value[1]'
```
- [ ] Verify scrape duration < 1 second
- [ ] Verify scrape success rate = 100%:
```bash
curl -s 'http://lcs-plg.local:9090/api/v1/query?query=up{job="ptz"}' | jq .
```

**Dashboard Performance:**
- [ ] Time Grafana dashboard load:
  - Initial load < 3 seconds
  - Refresh < 1 second
- [ ] Verify no query timeouts
- [ ] Check Grafana logs for slow queries:
```bash
docker-compose logs grafana | grep -i "slow query"
```

**Alert Evaluation Performance:**
- [ ] Verify alert evaluation interval is respected (30s)
- [ ] Check Prometheus alert evaluation time:
```bash
curl -s 'http://lcs-plg.local:9090/api/v1/rules' | jq '.data.groups[] | select(.name=="ptz_camera_health") | .evaluationTime'
```
- [ ] Verify evaluation time < 5 seconds

---

#### Task 4.3: Data Retention Validation

- [ ] Verify Prometheus retention configuration:
```yaml
# docker-compose.yml or prometheus.yml
command:
  - '--storage.tsdb.retention.time=30d'
  - '--storage.tsdb.retention.size=10GB'
```

- [ ] Query historical data from 7 days ago:
```promql
ptz_temperature_celsius{job="ptz"}[7d]
```
- [ ] Verify data exists with no gaps
- [ ] Check Prometheus data directory size:
```bash
docker exec prometheus du -sh /prometheus
```
- [ ] Verify size is within expected limits

---

### Phase 5: Documentation

#### Task 5.1: Create Operational Runbook

- [ ] Create `docs/runbooks/ptz-monitoring-runbook.md` with sections:

**Section 1: Common Issues and Resolutions**

**Issue: PTZ Target Shows "DOWN" in Prometheus**
- Symptoms: Target state is DOWN, scrape errors in Prometheus logs
- Diagnosis:
  ```bash
  # Check PTZ reachability
  ping 192.168.30.15

  # Test metrics endpoint manually
  curl -k -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics

  # Check ACAP status
  ssh root@192.168.30.15 'eap-install.sh status lrf_controller'
  ```
- Resolution:
  1. Restart ACAP: `ssh root@192.168.30.15 'eap-install.sh restart lrf_controller'`
  2. Check camera network connectivity
  3. Verify credentials in Prometheus config
  4. Review camera syslog: `ssh root@192.168.30.15 'tail -100 /var/log/syslog | grep lrf_controller'`

**Issue: Metrics Not Appearing in Grafana**
- Symptoms: Dashboard panels show "No data"
- Diagnosis:
  ```bash
  # Verify metrics in Prometheus first
  curl -s 'http://lcs-plg.local:9090/api/v1/query?query=ptz_uptime_seconds{job="ptz"}' | jq .

  # Check Grafana data source
  curl -s http://admin:admin@lcs-plg.local:3000/api/datasources
  ```
- Resolution:
  1. Verify Prometheus data source configured: Settings → Data Sources
  2. Test connection in Grafana UI
  3. Check dashboard query syntax
  4. Verify time range is appropriate

**Issue: False Positive Temperature Alerts**
- Symptoms: Temperature alerts firing during normal operation
- Diagnosis: Check actual temperature trend over 24 hours
- Resolution:
  1. Review temperature thresholds (warning: 75°C, critical: 80°C)
  2. Consider environmental factors (ambient temperature, sun exposure)
  3. Adjust thresholds if necessary in alert rules
  4. Verify camera cooling is working properly

**Issue: High I2C Error Rate**
- Symptoms: `ptz_i2c_errors_total` increasing rapidly
- Diagnosis:
  ```bash
  # Check current error count
  curl -s 'http://lcs-plg.local:9090/api/v1/query?query=rate(ptz_i2c_errors_total[5m])' | jq .

  # Check LRF sensor status
  ssh root@192.168.30.15
  i2cdetect -y 0  # Check if sensor is detected
  ```
- Resolution:
  1. Check physical LRF sensor connection
  2. Verify sensor power supply
  3. Test sensor manually: `ssh root@192.168.30.15 'cd /usr/local/packages/lrf_controller && ./test_i2c.sh'`
  4. Review camera syslog for I2C-related errors

---

**Section 2: Escalation Procedures**

**Severity: WARNING**
- Response time: 1 hour
- Action: Acknowledge alert, investigate during business hours
- Notify: On-call engineer

**Severity: CRITICAL**
- Response time: 15 minutes
- Action: Immediate investigation and mitigation
- Notify: On-call engineer + team lead

**Contact Information:**
- On-call rotation: [Link to PagerDuty/Opsgenie]
- Team Slack channel: #ptz-monitoring
- Escalation email: ops-team@company.com

---

#### Task 5.2: Update System Documentation

- [ ] Create architecture diagram: `docs/architecture/ptz-monitoring-integration.png`
- [ ] Document components:
  - PTZ Camera (AXIS Q6225-LE)
  - lrf-controller ACAP
  - Metrics endpoint: `/local/lrf_controller/lrf/metrics`
  - Health endpoint: `/health`
  - Prometheus scraper
  - Grafana dashboard
  - Alert manager
  - Notification channels

- [ ] Create data flow diagram:
  ```
  PTZ Camera (ACAP)
    |
    +--> Metrics Endpoint (HTTPS) --> Prometheus (HTTP SD)
    |
    +--> Health Endpoint (HTTP) --> [Future: Loki via Alloy]

  Prometheus
    |
    +--> Grafana (Dashboard Queries)
    |
    +--> Alert Manager (Alert Rules)

  Alert Manager
    |
    +--> Slack Notifications
    +--> Email Notifications
  ```

- [ ] Document configuration files:
  ```
  vct-cluster-server/
    backend/api/
      views.py           # HTTP SD endpoint
      models.py          # PTZCamera model

  monitoring/
    config/
      prometheus.yml.template    # Scrape config
      prometheus/alerts/
        ptz-alerts.yml          # Alert rules
      grafana/
        dashboards/
          ptz-camera-health.json  # Dashboard definition
  ```

- [ ] Document Grafana dashboard usage:
  - Dashboard URL: `http://lcs-plg.local:3000/d/ptz-health-001`
  - Time range: Default 1 hour, adjustable
  - Refresh: Auto-refresh every 30 seconds
  - Filters: Use hostname variable to filter specific cameras
  - Drill-down: Click panels to explore detailed metrics

- [ ] Document alert response procedures:
  - Acknowledge alerts in Slack using emoji reaction
  - Create incident ticket for critical alerts
  - Document resolution in runbook
  - Post-mortem for repeated incidents

---

## Integration Summary

### Components Modified/Created

**Django Backend (vct-cluster-server):**
1. `backend/api/views.py` - Added PTZ targets to HTTP SD endpoint
2. `backend/api/models.py` - Created PTZCamera model (if not exists)
3. Database migration - PTZCamera table

**Prometheus Configuration:**
1. `monitoring/config/prometheus.yml.template` - New ptz_cameras scrape job
2. `monitoring/config/prometheus/alerts/ptz-alerts.yml` - 8 alert rules
3. Environment variables - AXIS_ADMIN_PASSWORD

**Grafana Configuration:**
1. `monitoring/config/grafana/dashboards/ptz-camera-health.json` - Dashboard definition
2. `monitoring/config/grafana/provisioning/dashboards/dashboards.yml` - Provisioning config
3. Notification channels - Slack and/or Email
4. Notification policies - Routing rules

**Documentation:**
1. `docs/runbooks/ptz-monitoring-runbook.md` - Operational procedures
2. `docs/architecture/ptz-monitoring-integration.png` - Architecture diagram
3. Updated README or main documentation

---

### New Infrastructure Summary

**Metrics Collected:** 13 metrics
- System: uptime, memory, CPU, load average, disk, process count
- Network: RX/TX bytes
- Service: HTTP requests, I2C errors
- Hardware: temperature
- Health: overall status (from health endpoint)

**Dashboard Panels:** 8 panels
1. PTZ Status Overview (stat)
2. Memory Usage (time series)
3. Disk Usage (gauge)
4. Temperature (time series)
5. CPU Usage (time series)
6. Network Traffic (time series)
7. I2C Errors (stat)
8. Uptime (stat)

**Alert Rules:** 8 rules
1. PTZHighTemperature (warning, >75°C, 5m)
2. PTZCriticalTemperature (critical, >80°C, 1m)
3. PTZLowMemory (warning, <50MB, 5m)
4. PTZCriticalMemory (critical, <20MB, 2m)
5. PTZDiskFull (critical, >95%, 5m)
6. PTZHealthUnhealthy (critical, status=2, 2m)
7. PTZHealthDegraded (warning, status=1, 5m)
8. PTZHighI2CErrors (warning, >10/hr, 5m)

**Notification Channels:** 2 channels
- Slack (real-time alerts)
- Email (backup/digest)

---

## Validation Checklist

### Pre-Deployment Validation
- [ ] ACAP built and tested (v1.1.0+)
- [ ] Metrics endpoint returns valid Prometheus format
- [ ] Health endpoint returns valid victor-health JSON
- [ ] promtool validates metrics and alert rules
- [ ] Dashboard JSON is valid

### Post-Deployment Validation
- [ ] Prometheus targets show PTZ as "UP"
- [ ] All 13 metrics queryable in Prometheus
- [ ] Grafana dashboard loads in < 3 seconds
- [ ] All 8 panels show data
- [ ] Alert rules active and evaluating
- [ ] Notifications reach configured channels
- [ ] Test alerts fire and resolve correctly

### Performance Validation
- [ ] Prometheus scrape latency < 1s
- [ ] Metrics endpoint response time < 500ms
- [ ] Dashboard query time < 2s per panel
- [ ] Alert evaluation time < 5s
- [ ] No gaps in metric data over 24 hours
- [ ] Memory usage stable (no leaks)

### Data Retention Validation
- [ ] 30-day Prometheus retention configured
- [ ] Historical queries work (7+ days)
- [ ] Storage usage within limits
- [ ] Backup procedures documented

---

## Files Analyzed
- `.paf/findings/A10_FINDINGS.md` - Metrics implementation checklist (13 metrics, VAPIX integration)
- `.paf/findings/A11_FINDINGS.md` - Health implementation checklist (victor-health format, thresholds)
- `PLAN.md` (lines 358-566) - Phase 2: Cluster Server Integration (Prometheus config, Grafana dashboard spec, alerting rules)

## Blockers or Uncertainties
None - All specifications complete from metrics implementation (A10), health implementation (A11), and cluster integration planning (PLAN.md Phase 2).

## Confidence Level
**HIGH** - Complete integration checklist with specific implementation steps, exact configuration snippets, comprehensive validation procedures, and troubleshooting guides. All dependencies resolved by prior findings.
