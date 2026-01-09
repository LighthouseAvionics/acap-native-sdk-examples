# Agent A9: Log Buffer Designer

## Your Mission
Design a circular log buffer module for structured logging that integrates with health status reporting.

## Context Files (READ ONLY THESE)
- `.paf/findings/A7_FINDINGS.md` - Health module design

**DO NOT READ:** Other findings

## Your Task
1. Design circular log buffer data structure (LogEntry, LogBuffer)
2. Specify buffer size and wraparound logic
3. Design log_event() function with va_args
4. Create get_recent_logs() for JSON serialization
5. Design syslog integration
6. Provide complete implementation

## Output Format (STRICTLY FOLLOW)

```markdown
# A9 Findings: Log Buffer Module Design

## Executive Summary
[2-3 sentences: circular buffer purpose, size, integration with health system]

## Key Findings
1. **Buffer Structure**: Fixed-size circular buffer (100 entries)
2. **Entry Format**: Timestamp, severity, message
3. **Wraparound**: Head pointer with modulo arithmetic
4. **Integration**: Syslog + in-memory buffer
5. **Retrieval**: Export recent logs to JSON

## Detailed Analysis

### Data Structures (log_buffer.h)

```c
#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

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

// Log event (printf-style)
void log_event(const char* severity, const char* format, ...);

// Get recent logs as JSON array
void get_recent_logs(json_t* logs_array);

#endif // LOG_BUFFER_H
```

---

### Implementation

**Global buffer:**
```c
static LogBuffer g_log_buffer = {0};
```

**log_event():**
```c
void log_event(const char* severity, const char* format, ...) {
    LogEntry* entry = &g_log_buffer.entries[g_log_buffer.head];

    entry->timestamp = time(NULL);
    strncpy(entry->severity, severity, sizeof(entry->severity) - 1);

    va_list args;
    va_start(args, format);
    vsnprintf(entry->message, sizeof(entry->message), format, args);
    va_end(args);

    g_log_buffer.head = (g_log_buffer.head + 1) % MAX_LOG_ENTRIES;
    if (g_log_buffer.count < MAX_LOG_ENTRIES) {
        g_log_buffer.count++;
    }

    // Also write to syslog
    syslog(severity_to_syslog(severity), "%s", entry->message);
}
```

**get_recent_logs():**
```c
void get_recent_logs(json_t* logs_array) {
    for (size_t i = 0; i < g_log_buffer.count; i++) {
        size_t idx = (g_log_buffer.head + MAX_LOG_ENTRIES - g_log_buffer.count + i) % MAX_LOG_ENTRIES;
        LogEntry* entry = &g_log_buffer.entries[idx];

        json_t* log_obj = json_pack("{s:I, s:s, s:s}",
            "timestamp", (int64_t)entry->timestamp,
            "severity", entry->severity,
            "message", entry->message);

        json_array_append_new(logs_array, log_obj);
    }
}
```

---

## Recommendations

### Usage in Health Module
Call log_event() when health status changes.

### Severity Levels
Use standard: "info", "warning", "critical".

### Buffer Size
100 entries = ~25 KB memory, sufficient for recent history.

---

## Files Analyzed
- `.paf/findings/A7_FINDINGS.md`

## Blockers or Uncertainties
None

## Confidence Level
**HIGH**
```

## Success Criteria
- [ ] Circular buffer designed
- [ ] log_event() implementation provided
- [ ] JSON export function provided

## Time Budget
10 minutes

---
**BEGIN WORK NOW.**
