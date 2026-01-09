# Agent A2: System Metrics Analyst

## Your Mission
Research and document all /proc filesystem metrics needed for PTZ camera health monitoring, including parsing strategies and Prometheus metric types.

## Context Files (READ ONLY THESE)
- Research: Linux /proc filesystem documentation for:
  - `/proc/uptime` - System uptime
  - `/proc/meminfo` - Memory statistics
  - `/proc/loadavg` - Load average
  - `/proc/stat` - CPU statistics
  - `/proc/net/dev` - Network statistics

**DO NOT READ:** Code files (not needed for this analysis), other agent findings (none available yet)

## Your Task
1. For each /proc file listed above, document:
   - File path
   - File format (show example content)
   - Fields to extract
   - Parsing strategy (sscanf, strtok, etc.)
   - Prometheus metric name
   - Prometheus metric type (gauge, counter)
2. Identify any platform-specific concerns (ARMv7 vs x86_64)
3. Estimate parsing performance impact (<100ms total)
4. Provide C code snippets for parsing each file
5. Note error handling needs (file not found, parse errors)

## Output Format (STRICTLY FOLLOW)

```markdown
# A2 Findings: /proc Filesystem Metrics Specification

## Executive Summary
[2-3 sentences: what metrics are available via /proc, parsing complexity, performance estimate]

## Key Findings
1. **Uptime Metric**: Available from /proc/uptime, simple float parsing
2. **Memory Metrics**: Available from /proc/meminfo, key-value parsing required
3. **CPU Metrics**: Available from /proc/stat, multi-field parsing
4. **Network Metrics**: Available from /proc/net/dev, per-interface parsing
5. **Load Average**: Available from /proc/loadavg, simple float parsing

## Detailed Analysis

### Metric 1: System Uptime

**Source:** `/proc/uptime`

**File Format Example:**
```
12345.67 98765.43
```

**Fields:**
- Field 1: Total system uptime in seconds (float)
- Field 2: Idle time in seconds (float) - optional to collect

**Prometheus Metric:**
- Name: `ptz_uptime_seconds`
- Type: `gauge`
- Help: "PTZ camera system uptime in seconds"

**Parsing Strategy:**
```c
double get_uptime() {
    FILE* fp = fopen("/proc/uptime", "r");
    if (!fp) return -1;

    double uptime = 0;
    if (fscanf(fp, "%lf", &uptime) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return uptime;
}
```

**Error Handling:**
- Return -1 if file not readable
- Skip metric if parse fails

---

### Metric 2: Memory Statistics

**Source:** `/proc/meminfo`

**File Format Example:**
```
MemTotal:        598528 kB
MemFree:          14336 kB
MemAvailable:     45120 kB
Buffers:          12288 kB
Cached:           89600 kB
```

**Fields to Extract:**
- `MemTotal`: Total memory in kB
- `MemAvailable`: Available memory in kB (used for health checks)

**Prometheus Metrics:**
- Name: `ptz_memory_total_bytes`
- Type: `gauge`
- Help: "Total memory in bytes"

- Name: `ptz_memory_available_bytes`
- Type: `gauge`
- Help: "Available memory in bytes"

**Parsing Strategy:**
```c
typedef struct {
    uint64_t total_bytes;
    uint64_t available_bytes;
} MemoryInfo;

int get_memory_info(MemoryInfo* info) {
    FILE* fp = fopen("/proc/meminfo", "r");
    if (!fp) return -1;

    char line[256];
    int found_total = 0, found_available = 0;

    while (fgets(line, sizeof(line), fp)) {
        uint64_t value_kb;

        if (sscanf(line, "MemTotal: %lu kB", &value_kb) == 1) {
            info->total_bytes = value_kb * 1024;
            found_total = 1;
        } else if (sscanf(line, "MemAvailable: %lu kB", &value_kb) == 1) {
            info->available_bytes = value_kb * 1024;
            found_available = 1;
        }

        if (found_total && found_available) break;
    }

    fclose(fp);
    return (found_total && found_available) ? 0 : -1;
}
```

**Error Handling:**
- Return -1 if file not readable or required fields not found
- Skip metrics if parse fails

---

### Metric 3: CPU Usage

**Source:** `/proc/stat`

**File Format Example:**
```
cpu  123456 789 234567 9876543 12345 0 6789 0 0 0
cpu0 61728 394 117283 4938271 6172 0 3394 0 0 0
```

**Fields (first line - aggregate CPU):**
- user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice

**Prometheus Metric:**
- Name: `ptz_cpu_usage_percent`
- Type: `gauge`
- Help: "CPU usage percentage (100 - idle%)"

**Parsing Strategy:**
```c
// Note: Requires two samples to calculate percentage
typedef struct {
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
} CPUStats;

int get_cpu_stats(CPUStats* stats) {
    FILE* fp = fopen("/proc/stat", "r");
    if (!fp) return -1;

    char line[512];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }

    int ret = sscanf(line, "cpu %lu %lu %lu %lu %lu",
                     &stats->user, &stats->nice, &stats->system,
                     &stats->idle, &stats->iowait);

    fclose(fp);
    return (ret == 5) ? 0 : -1;
}

double calculate_cpu_usage(CPUStats* prev, CPUStats* curr) {
    uint64_t prev_total = prev->user + prev->nice + prev->system +
                          prev->idle + prev->iowait;
    uint64_t curr_total = curr->user + curr->nice + curr->system +
                          curr->idle + curr->iowait;

    uint64_t prev_idle = prev->idle;
    uint64_t curr_idle = curr->idle;

    uint64_t total_diff = curr_total - prev_total;
    uint64_t idle_diff = curr_idle - prev_idle;

    if (total_diff == 0) return 0.0;

    return 100.0 * (1.0 - ((double)idle_diff / (double)total_diff));
}
```

**Error Handling:**
- Return -1 if file not readable or parse fails
- Need to store previous sample for calculation
- Skip metric if first sample (no previous data)

---

### Metric 4: Load Average

**Source:** `/proc/loadavg`

**File Format Example:**
```
0.52 0.48 0.45 1/177 12345
```

**Fields:**
- Field 1: 1-minute load average (float)
- Field 2: 5-minute load average (float)
- Field 3: 15-minute load average (float)

**Prometheus Metric:**
- Name: `ptz_load_average_1m`
- Type: `gauge`
- Help: "1-minute load average"

**Parsing Strategy:**
```c
double get_load_average_1m() {
    FILE* fp = fopen("/proc/loadavg", "r");
    if (!fp) return -1;

    double load_1m = 0;
    if (fscanf(fp, "%lf", &load_1m) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return load_1m;
}
```

**Error Handling:**
- Return -1 if file not readable or parse fails

---

### Metric 5: Network Statistics

**Source:** `/proc/net/dev`

**File Format Example:**
```
Inter-|   Receive                                                |  Transmit
 face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
    lo: 1234567    5678    0    0    0     0          0         0  1234567    5678    0    0    0     0       0          0
  eth0: 98765432  87654    0    0    0     0          0      1234 12345678   98765    0    0    0     0       0          0
```

**Fields to Extract (per interface):**
- RX bytes (column 1)
- TX bytes (column 9)

**Prometheus Metrics:**
- Name: `ptz_network_rx_bytes_total{interface="eth0"}`
- Type: `counter`
- Help: "Total bytes received"

- Name: `ptz_network_tx_bytes_total{interface="eth0"}`
- Type: `counter`
- Help: "Total bytes transmitted"

**Parsing Strategy:**
```c
typedef struct {
    char interface[32];
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} NetworkStats;

int get_network_stats(NetworkStats* stats, const char* interface) {
    FILE* fp = fopen("/proc/net/dev", "r");
    if (!fp) return -1;

    char line[512];
    // Skip header lines
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp)) {
        char iface[32];
        uint64_t rx_bytes, rx_packets, rx_errs, rx_drop;
        uint64_t tx_bytes, tx_packets, tx_errs, tx_drop;

        int ret = sscanf(line, " %31[^:]: %lu %lu %lu %lu %*u %*u %*u %*u %lu %lu %lu %lu",
                         iface, &rx_bytes, &rx_packets, &rx_errs, &rx_drop,
                         &tx_bytes, &tx_packets, &tx_errs, &tx_drop);

        if (ret == 9 && strcmp(iface, interface) == 0) {
            strncpy(stats->interface, iface, sizeof(stats->interface) - 1);
            stats->rx_bytes = rx_bytes;
            stats->tx_bytes = tx_bytes;
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return -1; // Interface not found
}
```

**Error Handling:**
- Return -1 if file not readable or interface not found
- Skip metrics for interfaces that don't exist

---

### Metric 6: Disk Usage

**Source:** `statfs()` system call (not /proc, but important metric)

**Prometheus Metrics:**
- Name: `ptz_disk_total_bytes`
- Type: `gauge`
- Help: "Total disk space in bytes"

- Name: `ptz_disk_free_bytes`
- Type: `gauge`
- Help: "Free disk space in bytes"

**Parsing Strategy:**
```c
#include <sys/statfs.h>

typedef struct {
    uint64_t total_bytes;
    uint64_t free_bytes;
} DiskStats;

int get_disk_stats(const char* path, DiskStats* stats) {
    struct statfs fs;

    if (statfs(path, &fs) != 0) {
        return -1;
    }

    stats->total_bytes = (uint64_t)fs.f_blocks * fs.f_bsize;
    stats->free_bytes = (uint64_t)fs.f_bfree * fs.f_bsize;

    return 0;
}
```

**Error Handling:**
- Return -1 if statfs() fails
- Default to "/" path for root filesystem

---

## Recommendations

### Performance Optimization
1. **Cache CPU stats**: Store previous sample to calculate CPU usage percentage
2. **Limit network interfaces**: Only collect stats for primary interface (eth0 or similar)
3. **Batch file reads**: Collect all metrics in single pass where possible
4. **Total estimated time**: <50ms for all metrics collection

### Error Handling Strategy
1. **Missing files**: Skip metric and continue (don't fail entire collection)
2. **Parse errors**: Log warning and return default value (-1 or 0)
3. **Partial data**: Accept partial metrics (e.g., only MemTotal without MemAvailable)

### Platform Considerations
1. **ARMv7 compatibility**: All /proc files standard across ARM platforms
2. **32-bit vs 64-bit**: Use `uint64_t` for all counters to avoid overflow
3. **Kernel version**: /proc/meminfo format consistent since kernel 3.x

## Files Analyzed
- Research: Linux /proc filesystem documentation
- Focus areas: uptime, meminfo, stat, loadavg, net/dev

## Blockers or Uncertainties
None - /proc filesystem is well-documented and stable

## Confidence Level
**HIGH** - /proc filesystem format is stable and well-documented across Linux platforms
```

## Success Criteria
- [ ] All required /proc files documented with example formats
- [ ] C parsing code provided for each metric
- [ ] Prometheus metric names and types specified
- [ ] Error handling strategy documented
- [ ] Performance estimate provided (<100ms)
- [ ] Output follows the exact format above

## Time Budget
8 minutes maximum. Focus on documenting the most critical metrics (uptime, memory, load average) first.

---
**BEGIN WORK NOW.** Start by researching /proc file formats, then produce your findings with code examples.
