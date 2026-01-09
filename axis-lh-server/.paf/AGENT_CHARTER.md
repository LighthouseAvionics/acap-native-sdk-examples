# Agent Charter: PTZ Camera Health Monitoring Implementation

**Task:** ptz-health-monitoring
**Date:** 2026-01-09
**Framework:** Parallel Agent Framework v2.0
**Pattern:** Staged Waves

---

## 🎯 Mission

Decompose the PTZ camera health monitoring plan into detailed implementation specifications for extending the lrf-controller ACAP with Prometheus metrics and health status endpoints.

**Success Criteria:**
- ✅ All system components analyzed and design specifications created
- ✅ Implementation checklists generated for all modules
- ✅ Integration requirements documented for Prometheus/Grafana
- ✅ All findings follow structured format with code examples

---

## 👥 Agent Roster

### Wave 1: Independent Analysis (Spawn Immediately)

| Agent ID | Role | Task | Timeout | Output File |
|----------|------|------|---------|-------------|
| **A1** | HTTP Server Analyst | Analyze lrf-controller HTTP server architecture and extension points | 10min | A1_FINDINGS.md |
| **A2** | System Metrics Analyst | Research /proc filesystem metrics for PTZ monitoring | 8min | A2_FINDINGS.md |
| **A3** | VAPIX Integration Analyst | Analyze VAPIX API capabilities and integration patterns | 10min | A3_FINDINGS.md |
| **A4** | Prometheus Format Expert | Review Prometheus text format specifications and best practices | 8min | A4_FINDINGS.md |
| **A5** | Health Schema Analyst | Analyze victor-health JSON schema and status calculation | 10min | A5_FINDINGS.md |

**Execution:** Spawn A1-A5 in parallel → wait for all to complete

---

### Wave 2: Component Design (Spawn After Wave 1 Complete)

| Agent ID | Role | Task | Timeout | Depends On | Output File |
|----------|------|------|---------|------------|-------------|
| **A6** | Metrics Module Designer | Design metrics.c architecture and collection functions | 12min | A1, A2, A4 | A6_FINDINGS.md |
| **A7** | Health Module Designer | Design health.c architecture and status calculation | 12min | A1, A5 | A7_FINDINGS.md |
| **A8** | VAPIX Client Designer | Design VAPIX client integration for temperature/device info | 10min | A3 | A8_FINDINGS.md |
| **A9** | Log Buffer Designer | Design circular log buffer and structured logging | 10min | A7 | A9_FINDINGS.md |

**Execution:** Wait for Wave 1 completion → Spawn A6-A9 in parallel → wait for all to complete

---

### Wave 3: Implementation Planning (Spawn After Wave 2 Complete)

| Agent ID | Role | Task | Timeout | Depends On | Output File |
|----------|------|------|---------|------------|-------------|
| **A10** | Metrics Implementation Planner | Create detailed implementation checklist for metrics endpoint | 10min | A6, A8 | A10_FINDINGS.md |
| **A11** | Health Implementation Planner | Create detailed implementation checklist for health endpoint | 10min | A7, A9 | A11_FINDINGS.md |

**Execution:** Wait for Wave 2 completion → Spawn A10-A11 in parallel → wait for all to complete

---

### Wave 4: Integration Planning (Spawn After Wave 3 Complete)

| Agent ID | Role | Task | Timeout | Depends On | Output File |
|----------|------|------|---------|------------|-------------|
| **A12** | Integration Architect | Create Prometheus/Grafana integration and deployment checklist | 12min | A10, A11 | A12_FINDINGS.md |

**Execution:** Wait for Wave 3 completion → Spawn A12 → wait for completion

---

## 📊 Task Details

### Agent A1: HTTP Server Analyst

**Context Files:**
- `app/lrf_controller.c` - Main application and HTTP server setup
- `app/http_server.c` - HTTP server implementation
- `app/http_server.h` - HTTP server interface

**Task:**
1. Analyze the current HTTP server architecture (GLib + sockets)
2. Identify how handlers are registered and invoked
3. Document the request/response flow and buffer limitations
4. List extension points for adding new endpoints
5. Note any threading or concurrency considerations

**Output:** Structured findings with code snippets showing handler registration patterns

---

### Agent A2: System Metrics Analyst

**Context Files:**
- `PLAN.md` - Lines 161-181 (metrics list)
- Research system: /proc filesystem documentation

**Task:**
1. List all /proc files needed for system metrics (uptime, meminfo, loadavg, stat, net/dev)
2. Document the parsing strategy for each file
3. Identify metric types (gauge, counter) for Prometheus
4. Note any edge cases or platform-specific concerns
5. Estimate parsing performance impact

**Output:** Detailed specification for each metric with file paths and parsing code examples

---

### Agent A3: VAPIX Integration Analyst

**Context Files:**
- `PLAN.md` - Lines 576-607 (VAPIX integration section)
- Research: VAPIX API documentation for temperature and device info

**Task:**
1. Document VAPIX API endpoints needed (temperature, device info)
2. Analyze authentication requirements (localhost access)
3. Specify request/response format for each endpoint
4. Identify error handling needs
5. Design caching strategy (1-5 minute TTL)

**Output:** VAPIX integration specification with curl examples and response parsing

---

### Agent A4: Prometheus Format Expert

**Context Files:**
- `PLAN.md` - Lines 157-220 (metrics endpoint section)
- Research: Prometheus text exposition format

**Task:**
1. Review Prometheus text format specifications
2. Document HELP and TYPE comment requirements
3. Specify label formatting and escaping rules
4. Provide examples for gauge, counter, histogram types
5. List common formatting mistakes to avoid

**Output:** Prometheus format guide with correct examples for PTZ metrics

---

### Agent A5: Health Schema Analyst

**Context Files:**
- `PLAN.md` - Lines 228-324 (health status section)
- Research: victor-health library schema

**Task:**
1. Analyze the victor-health JSON schema structure
2. Document status levels (HEALTHY, DEGRADED, UNHEALTHY)
3. Specify threshold-based status calculation algorithm
4. Define dependency check format
5. Create JSON schema validation rules

**Output:** Health status specification with JSON schema and calculation examples

---

### Agent A6: Metrics Module Designer

**Context Files:**
- `.paf/findings/A1_FINDINGS.md` - HTTP server architecture
- `.paf/findings/A2_FINDINGS.md` - System metrics specification
- `.paf/findings/A4_FINDINGS.md` - Prometheus format guide

**Task:**
1. Design metrics.c module interface (collect_system_metrics, collect_service_metrics)
2. Specify function signatures and data structures
3. Design GString buffer accumulation pattern
4. Create error handling strategy for missing /proc files
5. Design metrics_handler() HTTP endpoint function

**Output:** C module architecture with function signatures, data structures, and implementation approach

---

### Agent A7: Health Module Designer

**Context Files:**
- `.paf/findings/A1_FINDINGS.md` - HTTP server architecture
- `.paf/findings/A5_FINDINGS.md` - Health schema specification

**Task:**
1. Design health.c module interface (HealthCheck, HealthReport structures)
2. Specify threshold comparison algorithm
3. Design overall status calculation (worst-case aggregation)
4. Create JSON serialization strategy using Jansson
5. Design health_handler() HTTP endpoint function

**Output:** C module architecture with structs, enums, and status calculation algorithm

---

### Agent A8: VAPIX Client Designer

**Context Files:**
- `.paf/findings/A3_FINDINGS.md` - VAPIX integration specification

**Task:**
1. Design vapix.c module interface (get_temperature_vapix, get_device_info_vapix)
2. Specify libcurl usage pattern for HTTP requests
3. Design response parsing with Jansson
4. Create caching mechanism with timestamp validation
5. Design error handling and fallback strategy

**Output:** VAPIX client module design with caching and error handling

---

### Agent A9: Log Buffer Designer

**Context Files:**
- `.paf/findings/A7_FINDINGS.md` - Health module design

**Task:**
1. Design circular log buffer data structure (LogEntry, LogBuffer)
2. Specify buffer size and wraparound logic
3. Design log_event() function with va_args
4. Create get_recent_logs() JSON serialization
5. Design syslog integration for persistent logging

**Output:** Log buffer module design with circular queue implementation

---

### Agent A10: Metrics Implementation Planner

**Context Files:**
- `.paf/findings/A6_FINDINGS.md` - Metrics module design
- `.paf/findings/A8_FINDINGS.md` - VAPIX client design

**Task:**
1. Create step-by-step implementation checklist for metrics.c
2. List all files to create/modify
3. Specify build system changes (Makefile additions)
4. Create testing checklist (unit tests, promtool validation)
5. Document memory and performance testing procedures

**Output:** Implementation checklist with file-by-file tasks and testing procedures

---

### Agent A11: Health Implementation Planner

**Context Files:**
- `.paf/findings/A7_FINDINGS.md` - Health module design
- `.paf/findings/A9_FINDINGS.md` - Log buffer design

**Task:**
1. Create step-by-step implementation checklist for health.c
2. List all files to create/modify
3. Specify threshold configuration (memory, disk, temperature)
4. Create testing checklist (JSON schema validation, status transitions)
5. Document load testing procedures

**Output:** Implementation checklist with threshold configuration and testing procedures

---

### Agent A12: Integration Architect

**Context Files:**
- `.paf/findings/A10_FINDINGS.md` - Metrics implementation checklist
- `.paf/findings/A11_FINDINGS.md` - Health implementation checklist
- `PLAN.md` - Lines 358-566 (Phase 2: Cluster Server Integration)

**Task:**
1. Create Prometheus configuration checklist (scrape config, HTTP SD)
2. Create Django API modification checklist (PTZ target discovery)
3. Create Grafana dashboard specification (8 panels, alerting rules)
4. Create deployment checklist (build, deploy, test, monitor)
5. Document validation procedures (targets, scraping, alerts)

**Output:** Complete integration checklist with configuration examples

---

## ⏱️ Timeline Estimate

| Wave | Agents | Duration | Cumulative |
|------|--------|----------|------------|
| Wave 1 | A1-A5 (parallel) | 10 min | 10 min |
| Wave 2 | A6-A9 (parallel) | 12 min | 22 min |
| Wave 3 | A10-A11 (parallel) | 10 min | 32 min |
| Wave 4 | A12 | 12 min | **44 min total** |

**Critical Path:** A1 → A6 → A10 → A12 = 44 minutes

---

## 🔧 Execution Script

```bash
#!/bin/bash
set -e

PAF_DIR=".paf"
PROMPTS="$PAF_DIR/prompts"
FINDINGS="$PAF_DIR/findings"
STATUS="$PAF_DIR/status"

# Wave 1: Independent Analysis
echo "=== Starting Wave 1: Independent Analysis ==="
timeout 600 claude -p "$(cat $PROMPTS/AGENT_A1_PROMPT.md)" > "$FINDINGS/A1_FINDINGS.md" 2>&1 &
PID_A1=$!
timeout 480 claude -p "$(cat $PROMPTS/AGENT_A2_PROMPT.md)" > "$FINDINGS/A2_FINDINGS.md" 2>&1 &
PID_A2=$!
timeout 600 claude -p "$(cat $PROMPTS/AGENT_A3_PROMPT.md)" > "$FINDINGS/A3_FINDINGS.md" 2>&1 &
PID_A3=$!
timeout 480 claude -p "$(cat $PROMPTS/AGENT_A4_PROMPT.md)" > "$FINDINGS/A4_FINDINGS.md" 2>&1 &
PID_A4=$!
timeout 600 claude -p "$(cat $PROMPTS/AGENT_A5_PROMPT.md)" > "$FINDINGS/A5_FINDINGS.md" 2>&1 &
PID_A5=$!

wait $PID_A1 && echo "COMPLETE" > "$STATUS/A1_STATUS.md" || echo "FAILED:$?" > "$STATUS/A1_STATUS.md"
wait $PID_A2 && echo "COMPLETE" > "$STATUS/A2_STATUS.md" || echo "FAILED:$?" > "$STATUS/A2_STATUS.md"
wait $PID_A3 && echo "COMPLETE" > "$STATUS/A3_STATUS.md" || echo "FAILED:$?" > "$STATUS/A3_STATUS.md"
wait $PID_A4 && echo "COMPLETE" > "$STATUS/A4_STATUS.md" || echo "FAILED:$?" > "$STATUS/A4_STATUS.md"
wait $PID_A5 && echo "COMPLETE" > "$STATUS/A5_STATUS.md" || echo "FAILED:$?" > "$STATUS/A5_STATUS.md"

echo "=== Wave 1 Complete ==="

# Wave 2: Component Design
echo "=== Starting Wave 2: Component Design ==="
timeout 720 claude -p "$(cat $PROMPTS/AGENT_A6_PROMPT.md)" > "$FINDINGS/A6_FINDINGS.md" 2>&1 &
PID_A6=$!
timeout 720 claude -p "$(cat $PROMPTS/AGENT_A7_PROMPT.md)" > "$FINDINGS/A7_FINDINGS.md" 2>&1 &
PID_A7=$!
timeout 600 claude -p "$(cat $PROMPTS/AGENT_A8_PROMPT.md)" > "$FINDINGS/A8_FINDINGS.md" 2>&1 &
PID_A8=$!
timeout 600 claude -p "$(cat $PROMPTS/AGENT_A9_PROMPT.md)" > "$FINDINGS/A9_FINDINGS.md" 2>&1 &
PID_A9=$!

wait $PID_A6 && echo "COMPLETE" > "$STATUS/A6_STATUS.md" || echo "FAILED:$?" > "$STATUS/A6_STATUS.md"
wait $PID_A7 && echo "COMPLETE" > "$STATUS/A7_STATUS.md" || echo "FAILED:$?" > "$STATUS/A7_STATUS.md"
wait $PID_A8 && echo "COMPLETE" > "$STATUS/A8_STATUS.md" || echo "FAILED:$?" > "$STATUS/A8_STATUS.md"
wait $PID_A9 && echo "COMPLETE" > "$STATUS/A9_STATUS.md" || echo "FAILED:$?" > "$STATUS/A9_STATUS.md"

echo "=== Wave 2 Complete ==="

# Wave 3: Implementation Planning
echo "=== Starting Wave 3: Implementation Planning ==="
timeout 600 claude -p "$(cat $PROMPTS/AGENT_A10_PROMPT.md)" > "$FINDINGS/A10_FINDINGS.md" 2>&1 &
PID_A10=$!
timeout 600 claude -p "$(cat $PROMPTS/AGENT_A11_PROMPT.md)" > "$FINDINGS/A11_FINDINGS.md" 2>&1 &
PID_A11=$!

wait $PID_A10 && echo "COMPLETE" > "$STATUS/A10_STATUS.md" || echo "FAILED:$?" > "$STATUS/A10_STATUS.md"
wait $PID_A11 && echo "COMPLETE" > "$STATUS/A11_STATUS.md" || echo "FAILED:$?" > "$STATUS/A11_STATUS.md"

echo "=== Wave 3 Complete ==="

# Wave 4: Integration Planning
echo "=== Starting Wave 4: Integration Planning ==="
timeout 720 claude -p "$(cat $PROMPTS/AGENT_A12_PROMPT.md)" > "$FINDINGS/A12_FINDINGS.md" 2>&1
wait $! && echo "COMPLETE" > "$STATUS/A12_STATUS.md" || echo "FAILED:$?" > "$STATUS/A12_STATUS.md"

echo "=== All Waves Complete ==="
```

---

## ✅ Success Criteria

**Overall Mission Success:**
1. ✅ All 12 agents report "COMPLETE" status
2. ✅ All findings files pass format validation
3. ✅ Design specifications include function signatures and data structures
4. ✅ Implementation checklists are actionable with specific file modifications
5. ✅ Integration checklist covers Prometheus, Grafana, and deployment

---

## 🚫 Critical Constraints

**Agents MUST:**
- ❌ NOT write actual code implementations (design and specification only)
- ❌ NOT make assumptions about file contents without reading them
- ✅ ALWAYS provide specific code examples in design specifications
- ✅ ALWAYS reference line numbers when citing existing code
- ✅ ALWAYS follow the structured findings format from template
- ✅ ALWAYS include error handling considerations

---

**Charter Status:** Ready
**Next Step:** Execute Wave 1 agents in parallel
