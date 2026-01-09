# Agent A10: Metrics Implementation Planner

## Your Mission
Create a detailed implementation checklist for the metrics endpoint based on the module design from A6 and VAPIX client from A8.

## Context Files (READ ONLY THESE)
- `.paf/findings/A6_FINDINGS.md` - Metrics module design
- `.paf/findings/A8_FINDINGS.md` - VAPIX client design

**DO NOT READ:** Code files, other findings

## Your Task
1. Create step-by-step file-by-file implementation checklist
2. List all files to create/modify with specific changes
3. Specify build system changes (Makefile)
4. Create testing checklist (unit, integration, promtool)
5. Document validation procedures

## Output Format (STRICTLY FOLLOW)

```markdown
# A10 Findings: Metrics Implementation Checklist

## Executive Summary
[2-3 sentences: implementation scope, file count, testing approach]

## Implementation Checklist

### Phase 1: Create Core Metrics Files

#### Task 1.1: Create app/metrics.h
- [ ] Create file with interface from A6
- [ ] Include all function signatures
- [ ] Add include guards

#### Task 1.2: Create app/metrics.c
- [ ] Implement metrics_handler()
- [ ] Implement collect_system_metrics()
- [ ] Implement collect_network_metrics()
- [ ] Implement collect_disk_metrics()
- [ ] Implement collect_service_metrics()
- [ ] Implement append_metric_gauge()
- [ ] Implement append_metric_counter()

#### Task 1.3: Create app/metrics_helpers.c
- [ ] Implement get_uptime() from A2
- [ ] Implement get_memory_info() from A2
- [ ] Implement get_cpu_stats() from A2
- [ ] Implement calculate_cpu_usage() from A2
- [ ] Implement get_load_average_1m() from A2
- [ ] Implement get_network_stats() from A2
- [ ] Implement get_disk_stats() from A2
- [ ] Implement get_process_count()

#### Task 1.4: Create app/vapix.h
- [ ] Create interface from A8
- [ ] Define DeviceInfo struct

#### Task 1.5: Create app/vapix.c
- [ ] Implement vapix_init()
- [ ] Implement get_cached_temperature() from A8
- [ ] Implement get_cached_device_info() from A8
- [ ] Implement vapix_get_temperature() (internal)
- [ ] Implement parse_temperature_response()
- [ ] Implement write_callback() for curl

---

### Phase 2: Modify Existing Files

#### Task 2.1: Modify app/lrf_controller.c
- [ ] Add #include "metrics.h"
- [ ] Add #include "vapix.h"
- [ ] In main(), after existing handlers, add:
```c
// Initialize VAPIX client
vapix_init("admin", getenv("AXIS_ADMIN_PASSWORD"));

// Register metrics handler
http_server_add_handler(http_server, "/metrics", metrics_handler, NULL);
```
- [ ] Declare global counters: g_http_requests_total, g_i2c_errors_total
- [ ] Increment counters in appropriate locations

#### Task 2.2: Modify app/Makefile
- [ ] Add to SRCS: metrics.c metrics_helpers.c vapix.c
- [ ] Add LDFLAGS: -lcurl (for VAPIX client)
- [ ] Verify GLib already linked

---

### Phase 3: Build System

#### Task 3.1: Update Dockerfile (if needed)
- [ ] Ensure libcurl-dev installed in build stage

#### Task 3.2: Build ACAP
```bash
docker build --build-arg ARCH=aarch64 -t lrf-controller:v1.1.0 .
docker cp $(docker create lrf-controller:v1.1.0):/opt/app/lrf_controller_1_1_0_aarch64.eap .
```

---

### Phase 4: Testing

#### Task 4.1: Unit Testing (Development Machine)
- [ ] Test append_metric_gauge() with known inputs
- [ ] Test append_metric_counter() with known inputs
- [ ] Test parse_temperature_response() with sample data
- [ ] Verify Prometheus format correctness

#### Task 4.2: Integration Testing (PTZ Camera)
- [ ] Deploy ACAP to camera:
```bash
scp lrf_controller_1_1_0_aarch64.eap root@192.168.30.15:/tmp/
ssh root@192.168.30.15 'eap-install.sh install /tmp/lrf_controller_1_1_0_aarch64.eap'
```

- [ ] Test metrics endpoint:
```bash
curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics
```

- [ ] Validate with promtool:
```bash
curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics | promtool check metrics
```

- [ ] Verify all expected metrics present:
  - ptz_uptime_seconds
  - ptz_memory_total_bytes
  - ptz_memory_available_bytes
  - ptz_cpu_usage_percent
  - ptz_load_average_1m
  - ptz_disk_total_bytes
  - ptz_disk_free_bytes
  - ptz_temperature_celsius
  - ptz_network_rx_bytes_total
  - ptz_network_tx_bytes_total
  - ptz_http_requests_total
  - ptz_i2c_errors_total

#### Task 4.3: Performance Testing
- [ ] Measure response time:
```bash
time curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics
```
- [ ] Target: < 500ms

- [ ] Measure memory usage:
```bash
ssh root@192.168.30.15 'ps aux | grep lrf_controller'
```
- [ ] Target: < 5 MB RSS

- [ ] Load test with Apache Bench:
```bash
ab -n 100 -c 5 -A admin:password https://192.168.30.15/local/lrf_controller/lrf/metrics
```

#### Task 4.4: Validate VAPIX Integration
- [ ] Check temperature metric present
- [ ] Verify temperature value reasonable (30-60°C range)
- [ ] Test cache TTL (should not call VAPIX every request)

---

## Files to Create/Modify Summary

**New Files (5):**
1. app/metrics.h
2. app/metrics.c
3. app/metrics_helpers.c
4. app/vapix.h
5. app/vapix.c

**Modified Files (2):**
1. app/lrf_controller.c
2. app/Makefile

**Total Changes:** 7 files

---

## Validation Criteria

### Success Metrics
- [ ] All 12+ metrics exposed in correct Prometheus format
- [ ] promtool validation passes
- [ ] Response time < 500ms
- [ ] Memory usage < 5 MB RSS
- [ ] VAPIX temperature metric present
- [ ] No crashes or errors in syslog

---

## Files Analyzed
- `.paf/findings/A6_FINDINGS.md`
- `.paf/findings/A8_FINDINGS.md`

## Blockers or Uncertainties
None

## Confidence Level
**HIGH**
```

## Success Criteria
- [ ] Complete file-by-file checklist
- [ ] Build instructions provided
- [ ] Testing procedures documented
- [ ] Validation criteria specified

## Time Budget
10 minutes

---
**BEGIN WORK NOW.**
