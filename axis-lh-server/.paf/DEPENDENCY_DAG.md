# Task Dependency Graph

**Task:** PTZ Camera Health Monitoring Implementation
**Framework:** Parallel Agent Framework v2.0

---

## Dependency Visualization

```
Wave 1 (Independent Analysis - 5 agents in parallel)
┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐
│   A1   │  │   A2   │  │   A3   │  │   A4   │  │   A5   │
│  HTTP  │  │  Proc  │  │ VAPIX  │  │ Prom   │  │ Health │
│ Server │  │ Metrics│  │  API   │  │ Format │  │ Schema │
└───┬────┘  └───┬────┘  └───┬────┘  └───┬────┘  └───┬────┘
    │           │           │           │           │
    └─────┬─────┴───────────┘           │           │
          │                 └─────┬─────┴───────────┘
          │                       │
          ▼                       ▼
    ┌─────────┐              ┌─────────┐
    │   A6    │              │   A7    │              Wave 2
    │ Metrics │              │ Health  │              (Design)
    │  Design │              │ Design  │
    └────┬────┘              └────┬────┘
         │                        │
    ┌────┴────┐              ┌───┴─────┐
    │   A8    │              │   A9    │
    │  VAPIX  │              │   Log   │
    │  Client │              │  Buffer │
    └────┬────┘              └────┬────┘
         │                        │
         └───────┬──────┬─────────┘
                 ▼      ▼
            ┌────────┐ ┌────────┐          Wave 3
            │  A10   │ │  A11   │          (Implementation Planning)
            │ Metrics│ │ Health │
            │  Impl  │ │  Impl  │
            └───┬────┘ └───┬────┘
                │          │
                └────┬─────┘
                     ▼
                ┌────────┐                  Wave 4
                │  A12   │                  (Integration)
                │ Prom/  │
                │Grafana │
                └────────┘
```

---

## Wave 1: Independent Analysis (Spawn Immediately)

| Agent | Task | Depends On | Blocks | Timeout | Critical? |
|-------|------|------------|--------|---------|-----------|
| **A1** | Analyze lrf-controller HTTP server architecture | None | A6, A7 | 10min | YES |
| **A2** | Research /proc filesystem metrics | None | A6 | 8min | YES |
| **A3** | Analyze VAPIX API integration | None | A8 | 10min | YES |
| **A4** | Review Prometheus text format specs | None | A6 | 8min | YES |
| **A5** | Analyze victor-health JSON schema | None | A7 | 10min | YES |

**Spawn Command:**
```bash
timeout 600 claude -p "$(cat .paf/prompts/AGENT_A1_PROMPT.md)" > .paf/findings/A1_FINDINGS.md 2>&1 &
timeout 480 claude -p "$(cat .paf/prompts/AGENT_A2_PROMPT.md)" > .paf/findings/A2_FINDINGS.md 2>&1 &
timeout 600 claude -p "$(cat .paf/prompts/AGENT_A3_PROMPT.md)" > .paf/findings/A3_FINDINGS.md 2>&1 &
timeout 480 claude -p "$(cat .paf/prompts/AGENT_A4_PROMPT.md)" > .paf/findings/A4_FINDINGS.md 2>&1 &
timeout 600 claude -p "$(cat .paf/prompts/AGENT_A5_PROMPT.md)" > .paf/findings/A5_FINDINGS.md 2>&1 &
wait
```

**Failure Handling:**
- **A1 fails:** CRITICAL - Retry once with extended timeout, abort if still fails (blocks A6, A7)
- **A2 fails:** CRITICAL - Retry once, abort if still fails (blocks A6)
- **A3 fails:** CRITICAL - Retry once, abort if still fails (blocks A8)
- **A4 fails:** CRITICAL - Retry once, abort if still fails (blocks A6)
- **A5 fails:** CRITICAL - Retry once, abort if still fails (blocks A7)

---

## Wave 2: Component Design (Spawn After Wave 1 Complete)

| Agent | Task | Depends On | Blocks | Timeout | Critical? |
|-------|------|------------|--------|---------|-----------|
| **A6** | Design metrics.c module architecture | A1, A2, A4 | A10 | 12min | YES |
| **A7** | Design health.c module architecture | A1, A5 | A9, A11 | 12min | YES |
| **A8** | Design VAPIX client integration | A3 | A10 | 10min | YES |
| **A9** | Design circular log buffer | A7 | A11 | 10min | YES |

**Wait Condition:**
```bash
if [ "$(cat .paf/status/A1_STATUS.md)" == "COMPLETE" ] && \
   [ "$(cat .paf/status/A2_STATUS.md)" == "COMPLETE" ] && \
   [ "$(cat .paf/status/A3_STATUS.md)" == "COMPLETE" ] && \
   [ "$(cat .paf/status/A4_STATUS.md)" == "COMPLETE" ] && \
   [ "$(cat .paf/status/A5_STATUS.md)" == "COMPLETE" ]; then
    echo "Wave 1 complete, starting Wave 2"
else
    echo "ERROR: Critical Wave 1 agents failed"
    exit 1
fi
```

**Spawn Command:**
```bash
timeout 720 claude -p "$(cat .paf/prompts/AGENT_A6_PROMPT.md)" > .paf/findings/A6_FINDINGS.md 2>&1 &
timeout 720 claude -p "$(cat .paf/prompts/AGENT_A7_PROMPT.md)" > .paf/findings/A7_FINDINGS.md 2>&1 &
timeout 600 claude -p "$(cat .paf/prompts/AGENT_A8_PROMPT.md)" > .paf/findings/A8_FINDINGS.md 2>&1 &
timeout 600 claude -p "$(cat .paf/prompts/AGENT_A9_PROMPT.md)" > .paf/findings/A9_FINDINGS.md 2>&1 &
wait
```

**Failure Handling:**
- **A6 fails:** CRITICAL - Retry once, abort if still fails (blocks A10)
- **A7 fails:** CRITICAL - Retry once, abort if still fails (blocks A9, A11)
- **A8 fails:** CRITICAL - Retry once, abort if still fails (blocks A10)
- **A9 fails:** CRITICAL - Retry once, abort if still fails (blocks A11)

---

## Wave 3: Implementation Planning (Spawn After Wave 2 Complete)

| Agent | Task | Depends On | Blocks | Timeout | Critical? |
|-------|------|------------|--------|---------|-----------|
| **A10** | Create metrics implementation checklist | A6, A8 | A12 | 10min | YES |
| **A11** | Create health implementation checklist | A7, A9 | A12 | 10min | YES |

**Wait Condition:**
```bash
if [ "$(cat .paf/status/A6_STATUS.md)" == "COMPLETE" ] && \
   [ "$(cat .paf/status/A7_STATUS.md)" == "COMPLETE" ] && \
   [ "$(cat .paf/status/A8_STATUS.md)" == "COMPLETE" ] && \
   [ "$(cat .paf/status/A9_STATUS.md)" == "COMPLETE" ]; then
    echo "Wave 2 complete, starting Wave 3"
else
    echo "ERROR: Critical Wave 2 agents failed"
    exit 1
fi
```

**Spawn Command:**
```bash
timeout 600 claude -p "$(cat .paf/prompts/AGENT_A10_PROMPT.md)" > .paf/findings/A10_FINDINGS.md 2>&1 &
timeout 600 claude -p "$(cat .paf/prompts/AGENT_A11_PROMPT.md)" > .paf/findings/A11_FINDINGS.md 2>&1 &
wait
```

**Failure Handling:**
- **A10 fails:** CRITICAL - Retry once, abort if still fails (blocks A12)
- **A11 fails:** CRITICAL - Retry once, abort if still fails (blocks A12)

---

## Wave 4: Integration Planning (Spawn After Wave 3 Complete)

| Agent | Task | Depends On | Blocks | Timeout | Critical? |
|-------|------|------------|--------|---------|-----------|
| **A12** | Create Prometheus/Grafana integration checklist | A10, A11 | None | 12min | YES |

**Wait Condition:**
```bash
if [ "$(cat .paf/status/A10_STATUS.md)" == "COMPLETE" ] && \
   [ "$(cat .paf/status/A11_STATUS.md)" == "COMPLETE" ]; then
    echo "Wave 3 complete, starting Wave 4"
else
    echo "ERROR: Critical Wave 3 agents failed"
    exit 1
fi
```

**Spawn Command:**
```bash
timeout 720 claude -p "$(cat .paf/prompts/AGENT_A12_PROMPT.md)" > .paf/findings/A12_FINDINGS.md 2>&1
```

**Failure Handling:**
- **A12 fails:** CRITICAL - Retry once, abort if still fails (no downstream agents, but critical for final deliverable)

---

## Execution Order Summary

```
Time 0:     Spawn A1, A2, A3, A4, A5 (parallel)
Time +10m:  Wave 1 complete ✓
            Spawn A6, A7, A8, A9 (parallel)
Time +22m:  Wave 2 complete ✓
            Spawn A10, A11 (parallel)
Time +32m:  Wave 3 complete ✓
            Spawn A12
Time +44m:  Wave 4 complete ✓
            ALL FINDINGS READY
```

---

## Critical Path Analysis

**Longest Path:**
```
A1 (10min) → A6 (12min) → A10 (10min) → A12 (12min) = 44 minutes
Alternative: A1 (10min) → A7 (12min) → A11 (10min) → A12 (12min) = 44 minutes
```

**Parallel Efficiency:**
- Sequential: 10+8+10+8+10+12+12+10+10+10+10+12 = 122 minutes
- Parallel: 10+12+10+12 = 44 minutes
- **Time saved: 78 minutes (64% reduction)**

---

## Dependency Rationale

### Why A6 depends on A1
**Reason:** A6 needs to understand the HTTP server architecture to design how metrics_handler() integrates with the existing handler registration system.
**Risk if violated:** Metrics module design incompatible with HTTP server infrastructure.

### Why A6 depends on A2
**Reason:** A6 needs the /proc filesystem parsing specifications to design collect_system_metrics() functions.
**Risk if violated:** Missing or incorrectly parsed system metrics.

### Why A6 depends on A4
**Reason:** A6 needs Prometheus format specifications to design correct metric output formatting.
**Risk if violated:** Invalid Prometheus format, scraping failures.

### Why A7 depends on A1
**Reason:** A7 needs HTTP server architecture to design health_handler() endpoint integration.
**Risk if violated:** Health module incompatible with HTTP server.

### Why A7 depends on A5
**Reason:** A7 needs victor-health schema to design compatible JSON structure and status calculation.
**Risk if violated:** Inconsistent health status format across system.

### Why A8 depends on A3
**Reason:** A8 needs VAPIX API specifications to design correct HTTP client and response parsing.
**Risk if violated:** Incorrect VAPIX integration, failed temperature/device info retrieval.

### Why A9 depends on A7
**Reason:** A9 needs health module design to understand what log events need buffering.
**Risk if violated:** Log buffer incompatible with health status reporting needs.

### Why A10 depends on A6
**Reason:** A10 creates implementation checklist based on metrics module design.
**Risk if violated:** Implementation checklist missing critical design elements.

### Why A10 depends on A8
**Reason:** A10 needs VAPIX client design to include VAPIX integration steps.
**Risk if violated:** Incomplete metrics implementation (missing temperature metrics).

### Why A11 depends on A7
**Reason:** A11 creates implementation checklist based on health module design.
**Risk if violated:** Implementation checklist missing critical design elements.

### Why A11 depends on A9
**Reason:** A11 needs log buffer design to include logging implementation steps.
**Risk if violated:** Incomplete health implementation (missing structured logging).

### Why A12 depends on A10
**Reason:** A12 needs metrics implementation details to configure Prometheus scraping correctly.
**Risk if violated:** Prometheus misconfigured, metrics not collected.

### Why A12 depends on A11
**Reason:** A12 needs health implementation details to configure Grafana alerting thresholds.
**Risk if violated:** Alerting rules don't match actual health check thresholds.

---

## Failure Impact Matrix

| Failed Agent | Impact | Mitigation |
|--------------|--------|------------|
| A1 | CRITICAL - Blocks A6, A7 (both design tracks) | Retry once with 15min timeout, abort if fails |
| A2 | CRITICAL - Blocks A6 (metrics design) | Retry once, use manual /proc research if fails |
| A3 | CRITICAL - Blocks A8 (VAPIX integration) | Retry once, proceed without VAPIX if fails |
| A4 | CRITICAL - Blocks A6 (metrics format) | Retry once, use public Prometheus docs if fails |
| A5 | CRITICAL - Blocks A7 (health design) | Retry once, design custom schema if fails |
| A6 | CRITICAL - Blocks A10 (metrics implementation) | Retry once with extended timeout, abort if fails |
| A7 | CRITICAL - Blocks A9, A11 (health track) | Retry once with extended timeout, abort if fails |
| A8 | CRITICAL - Blocks A10 (VAPIX integration) | Retry once, proceed without VAPIX metrics if fails |
| A9 | CRITICAL - Blocks A11 (logging) | Retry once, simplify logging if fails |
| A10 | CRITICAL - Blocks A12 (integration) | Retry once, manual checklist creation if fails |
| A11 | CRITICAL - Blocks A12 (integration) | Retry once, manual checklist creation if fails |
| A12 | CRITICAL - No downstream agents, but final deliverable | Retry once, manual integration planning if fails |

---

**DAG Status:** Validated
**Parallelization:** 4 waves with 12 total agents
**Critical Path:** 44 minutes (64% faster than sequential)
