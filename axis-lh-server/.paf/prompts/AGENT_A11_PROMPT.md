# Agent A11: Health Implementation Planner

## Your Mission
Create a detailed implementation checklist for the health status endpoint based on the module design from A7 and log buffer from A9.

## Context Files (READ ONLY THESE)
- `.paf/findings/A7_FINDINGS.md` - Health module design
- `.paf/findings/A9_FINDINGS.md` - Log buffer design

**DO NOT READ:** Code files, other findings

## Your Task
1. Create step-by-step file-by-file implementation checklist
2. Specify threshold configuration values
3. Create testing checklist (JSON validation, status transitions)
4. Document integration with HTTP server

## Output Format (STRICTLY FOLLOW)

```markdown
# A11 Findings: Health Implementation Checklist

## Executive Summary
[2-3 sentences: implementation scope, file count, testing approach]

## Implementation Checklist

### Phase 1: Create Core Health Files

#### Task 1.1: Create app/health.h
- [ ] Create interface from A7
- [ ] Define HealthStatus enum
- [ ] Define ThresholdType enum
- [ ] Define HealthCheck struct
- [ ] Define DependencyCheck struct
- [ ] Define HealthReport struct
- [ ] Add function declarations

#### Task 1.2: Create app/health.c
- [ ] Implement calculate_check_status()
- [ ] Implement calculate_overall_status()
- [ ] Implement health_status_to_string()
- [ ] Implement health_status_to_severity()
- [ ] Implement generate_health_report()
- [ ] Implement health_report_to_json()
- [ ] Implement health_handler()
- [ ] Implement get_iso8601_timestamp()
- [ ] Implement check_i2c_bus_available()

#### Task 1.3: Create app/log_buffer.h
- [ ] Create interface from A9
- [ ] Define LogEntry struct
- [ ] Define LogBuffer struct
- [ ] Define MAX_LOG_ENTRIES constant

#### Task 1.4: Create app/log_buffer.c
- [ ] Implement log_event() with va_args
- [ ] Implement get_recent_logs()
- [ ] Implement severity_to_syslog()
- [ ] Initialize global log buffer

---

### Phase 2: Configure Health Thresholds

#### Task 2.1: Define Check Thresholds in generate_health_report()

**Memory Available:**
- Warning: < 50 MB
- Critical: < 20 MB
- Type: LOWER_BAD

**Disk Free:**
- Warning: < 100 MB
- Critical: < 50 MB
- Type: LOWER_BAD

**Temperature:**
- Warning: > 70°C
- Critical: > 80°C
- Type: HIGHER_BAD

**CPU Usage (Optional):**
- Warning: > 80%
- Critical: > 95%
- Type: HIGHER_BAD

---

### Phase 3: Modify Existing Files

#### Task 3.1: Modify app/lrf_controller.c
- [ ] Add #include "health.h"
- [ ] Add #include "log_buffer.h"
- [ ] In main(), register health handler:
```c
http_server_add_handler(http_server, "/health", health_handler, NULL);
```

#### Task 3.2: Modify app/Makefile
- [ ] Add to SRCS: health.c log_buffer.c

---

### Phase 4: Build & Deploy

#### Task 4.1: Build ACAP
```bash
docker build --build-arg ARCH=aarch64 -t lrf-controller:v1.1.0 .
```

#### Task 4.2: Deploy to Camera
```bash
scp lrf_controller_1_1_0_aarch64.eap root@192.168.30.15:/tmp/
ssh root@192.168.30.15 'eap-install.sh install /tmp/lrf_controller_1_1_0_aarch64.eap'
```

---

### Phase 5: Testing

#### Task 5.1: Functional Testing
- [ ] Test health endpoint:
```bash
curl -u admin:password https://192.168.30.15/local/lrf_controller/lrf/health | jq .
```

- [ ] Verify JSON structure matches victor-health schema
- [ ] Check all required fields present:
  - service
  - status
  - severity
  - timestamp
  - checks (array)
  - dependencies (array)

#### Task 5.2: Status Calculation Testing
- [ ] Verify HEALTHY status when all checks pass
- [ ] Simulate DEGRADED status (reduce available memory)
- [ ] Simulate UNHEALTHY status (fill disk space)
- [ ] Verify overall status is worst-case of all checks

#### Task 5.3: Threshold Testing
- [ ] Memory: Verify warning at 45 MB, critical at 15 MB
- [ ] Disk: Verify warning at 90 MB, critical at 45 MB
- [ ] Temperature: Verify warning at 72°C, critical at 82°C

#### Task 5.4: JSON Schema Validation
- [ ] Create validation script:
```python
import json
import jsonschema

schema = {
    "type": "object",
    "required": ["service", "status", "severity", "timestamp", "checks"],
    "properties": {
        "service": {"type": "string"},
        "status": {"enum": ["healthy", "degraded", "unhealthy"]},
        "severity": {"enum": ["info", "warning", "critical"]},
        "timestamp": {"type": "string", "format": "date-time"},
        "checks": {"type": "array"}
    }
}

# Fetch and validate
response = requests.get("https://192.168.30.15/local/lrf_controller/lrf/health", auth=("admin", "password"))
data = response.json()
jsonschema.validate(data, schema)
print("✓ Schema valid")
```

#### Task 5.5: Log Buffer Testing
- [ ] Trigger log_event() calls
- [ ] Verify logs appear in syslog
- [ ] Verify circular buffer wraps correctly after 100 entries

---

## Files to Create/Modify Summary

**New Files (4):**
1. app/health.h
2. app/health.c
3. app/log_buffer.h
4. app/log_buffer.c

**Modified Files (2):**
1. app/lrf_controller.c
2. app/Makefile

**Total Changes:** 6 files

---

## Validation Criteria

### Success Metrics
- [ ] Health endpoint returns valid victor-health compatible JSON
- [ ] Status calculation correct (worst-case aggregation)
- [ ] Thresholds trigger status changes correctly
- [ ] JSON schema validation passes
- [ ] Response time < 500ms
- [ ] No crashes or memory leaks

---

## Files Analyzed
- `.paf/findings/A7_FINDINGS.md`
- `.paf/findings/A9_FINDINGS.md`

## Blockers or Uncertainties
None

## Confidence Level
**HIGH**
```

## Success Criteria
- [ ] Complete file-by-file checklist
- [ ] Threshold configuration documented
- [ ] Testing procedures provided
- [ ] JSON validation included

## Time Budget
10 minutes

---
**BEGIN WORK NOW.**
