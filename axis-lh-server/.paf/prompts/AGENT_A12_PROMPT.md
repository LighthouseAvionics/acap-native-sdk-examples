# Agent A12: Integration Architect

## Your Mission
Create a comprehensive Prometheus/Grafana integration and deployment checklist based on metrics and health implementations.

## Context Files (READ ONLY THESE)
- `.paf/findings/A10_FINDINGS.md` - Metrics implementation checklist
- `.paf/findings/A11_FINDINGS.md` - Health implementation checklist
- `PLAN.md` - Lines 358-566 (Phase 2: Cluster Server Integration)

**DO NOT READ:** Other findings

## Your Task
1. Create Prometheus scrape configuration checklist
2. Create Django API modification checklist (HTTP SD)
3. Create Grafana dashboard specification (8 panels)
4. Create alerting rules configuration
5. Create deployment and validation procedures

## Output Format (STRICTLY FOLLOW)

```markdown
# A12 Findings: Prometheus/Grafana Integration Checklist

## Executive Summary
[2-3 sentences: integration scope, Prometheus config, Grafana dashboards, alerting]

## Integration Checklist

### Phase 1: Prometheus Configuration

#### Task 1.1: Update Django API for PTZ Service Discovery
- [ ] Modify `vct-cluster-server/backend/api/views.py`:
```python
def prometheus_targets(request):
    targets = []

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

- [ ] Create PTZCamera model if not exists:
```python
class PTZCamera(models.Model):
    hostname = models.CharField(max_length=255)
    ip_address = models.GenericIPAddressField()
    model = models.CharField(max_length=100)
    enabled = models.BooleanField(default=True)
```

- [ ] Run migrations

#### Task 1.2: Update Prometheus Scrape Config
- [ ] Edit `monitoring/config/prometheus.yml.template`:
```yaml
scrape_configs:
  - job_name: 'ptz_cameras'
    scheme: https
    tls_config:
      insecure_skip_verify: true
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

- [ ] Restart Prometheus container:
```bash
docker-compose -f monitoring/docker-compose.yml restart prometheus
```

#### Task 1.3: Verify Prometheus Targets
- [ ] Check targets page: http://lcs-plg.local:9090/targets
- [ ] Verify PTZ camera listed with "up" status
- [ ] Test metric query:
```promql
ptz_uptime_seconds{job="ptz"}
```

---

### Phase 2: Grafana Dashboard

#### Task 2.1: Create PTZ Camera Health Dashboard
- [ ] Create `monitoring/config/grafana/dashboards/ptz-camera-health.json`

**Panel 1: PTZ Status Overview**
- Type: Stat
- Query: `ptz_health_status{job="ptz"}`
- Thresholds: 1=Green, 2=Yellow, 3=Red
- Value mappings: 1=HEALTHY, 2=DEGRADED, 3=UNHEALTHY

**Panel 2: Memory Usage**
- Type: Time series graph
- Query: `ptz_memory_available_bytes{job="ptz"} / 1024 / 1024`
- Unit: MB
- Thresholds: 50 MB warning, 20 MB critical

**Panel 3: Disk Usage**
- Type: Gauge
- Query: `100 - (ptz_disk_free_bytes / ptz_disk_total_bytes * 100)`
- Unit: Percent
- Thresholds: 90% warning, 95% critical

**Panel 4: Temperature**
- Type: Time series graph
- Query: `ptz_temperature_celsius{job="ptz"}`
- Unit: °C
- Thresholds: 70°C warning, 80°C critical

**Panel 5: CPU Usage**
- Type: Time series graph
- Query: `ptz_cpu_usage_percent{job="ptz"}`
- Unit: Percent

**Panel 6: Network Traffic**
- Type: Time series graph
- Query RX: `rate(ptz_network_rx_bytes_total{job="ptz"}[5m])`
- Query TX: `rate(ptz_network_tx_bytes_total{job="ptz"}[5m])`
- Unit: Bytes/sec

**Panel 7: I2C Errors**
- Type: Stat
- Query: `increase(ptz_i2c_errors_total{job="ptz"}[1h])`

**Panel 8: Uptime**
- Type: Stat
- Query: `ptz_uptime_seconds{job="ptz"} / 86400`
- Unit: Days

#### Task 2.2: Import Dashboard
```bash
# Copy dashboard JSON to Grafana provisioning directory
cp ptz-camera-health.json monitoring/config/grafana/provisioning/dashboards/

# Restart Grafana
docker-compose -f monitoring/docker-compose.yml restart grafana
```

#### Task 2.3: Verify Dashboard
- [ ] Navigate to http://lcs-plg.local:3000
- [ ] Find "PTZ Camera Health" dashboard
- [ ] Verify all 8 panels showing data
- [ ] Verify auto-refresh enabled (30s)

---

### Phase 3: Alerting Rules

#### Task 3.1: Create Alert Rules
- [ ] Create `monitoring/config/grafana/provisioning/alerting/ptz-alerts.yaml`:

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
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} high temperature"
          description: "Temperature is {{ $value }}°C"

      - alert: PTZCriticalTemperature
        expr: ptz_temperature_celsius{job="ptz"} > 80
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} critical temperature"
          description: "Temperature is {{ $value }}°C"

      - alert: PTZLowMemory
        expr: ptz_memory_available_bytes{job="ptz"} < 52428800
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} low memory"
          description: "Available: {{ $value | humanize }}B"

      - alert: PTZDiskFull
        expr: (ptz_disk_total_bytes - ptz_disk_free_bytes) / ptz_disk_total_bytes > 0.95
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} disk full"
          description: "Usage: {{ $value | humanizePercentage }}"

      - alert: PTZHealthUnhealthy
        expr: ptz_health_status{job="ptz"} == 3
        for: 2m
        labels:
          severity: critical
        annotations:
          summary: "PTZ camera {{ $labels.hostname }} unhealthy"
          description: "Health check failed"
```

#### Task 3.2: Configure Notification Channels
- [ ] In Grafana UI, configure Slack notification channel
- [ ] In Grafana UI, configure Email notification channel
- [ ] Link alert rules to notification channels

#### Task 3.3: Test Alerting
- [ ] Simulate high temperature (if possible)
- [ ] Verify alert fires in Grafana
- [ ] Verify Slack notification received
- [ ] Verify alert resolves when condition clears

---

### Phase 4: Deployment & Validation

#### Task 4.1: End-to-End Validation
- [ ] PTZ camera metrics endpoint accessible
- [ ] Prometheus scraping PTZ successfully (check targets)
- [ ] Metrics visible in Prometheus query UI
- [ ] Grafana dashboard showing all panels
- [ ] Alert rules active
- [ ] Notifications configured

#### Task 4.2: Performance Validation
- [ ] Prometheus scrape latency < 1s
- [ ] Grafana dashboard loads < 3s
- [ ] No gaps in metric data
- [ ] Alert evaluation latency < 30s

#### Task 4.3: Data Retention Validation
- [ ] Verify 30-day Prometheus retention
- [ ] Query historical data from 7 days ago
- [ ] Verify no data loss

---

### Phase 5: Documentation

#### Task 5.1: Create Operational Runbook
- [ ] Document common issues:
  - PTZ camera offline
  - Metrics not appearing
  - Alert false positives
- [ ] Document troubleshooting steps
- [ ] Document escalation procedures

#### Task 5.2: Update System Documentation
- [ ] Add PTZ monitoring architecture diagram
- [ ] Document new Prometheus scrape job
- [ ] Document Grafana dashboard usage
- [ ] Document alert response procedures

---

## Integration Summary

**Components Modified:**
1. Django API (PTZ service discovery)
2. Prometheus config (scrape job)
3. Grafana dashboards (1 new dashboard)
4. Alerting rules (5 new rules)

**New Infrastructure:**
- 12+ PTZ metrics
- 8-panel Grafana dashboard
- 5 alert rules
- 2 notification channels

**Validation Points:**
- [ ] Prometheus targets show PTZ as "up"
- [ ] All metrics queryable in Prometheus
- [ ] Grafana dashboard functional
- [ ] Alerts can fire and resolve
- [ ] Notifications reach destinations

---

## Files Analyzed
- `.paf/findings/A10_FINDINGS.md`
- `.paf/findings/A11_FINDINGS.md`
- `PLAN.md` (lines 358-566)

## Blockers or Uncertainties
None

## Confidence Level
**HIGH**
```

## Success Criteria
- [ ] Prometheus configuration complete
- [ ] Grafana dashboard specification complete
- [ ] Alerting rules defined
- [ ] Deployment procedures provided
- [ ] Validation checklist provided

## Time Budget
12 minutes

---
**BEGIN WORK NOW.**
