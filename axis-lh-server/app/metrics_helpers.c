/**
 * Copyright (C) 2025, Axis Communications AB, Lund, Sweden
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "metrics_helpers.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statfs.h>
#include <syslog.h>

double get_uptime(void) {
    FILE* fp = fopen("/proc/uptime", "r");
    if (!fp) {
        syslog(LOG_WARNING, "Failed to open /proc/uptime");
        return -1.0;
    }

    double uptime;
    if (fscanf(fp, "%lf", &uptime) != 1) {
        syslog(LOG_WARNING, "Failed to parse /proc/uptime");
        fclose(fp);
        return -1.0;
    }

    fclose(fp);
    return uptime;
}

int get_memory_info(MemoryInfo* mem) {
    if (!mem) {
        return -1;
    }

    FILE* fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        syslog(LOG_WARNING, "Failed to open /proc/meminfo");
        return -1;
    }

    mem->total_bytes     = 0;
    mem->available_bytes = 0;

    char line[256];
    int found_total = 0, found_avail = 0;

    while (fgets(line, sizeof(line), fp)) {
        uint64_t value_kb;

        if (sscanf(line, "MemTotal: %llu kB", &value_kb) == 1) {
            mem->total_bytes = value_kb * 1024;
            found_total      = 1;
        } else if (sscanf(line, "MemAvailable: %llu kB", &value_kb) == 1) {
            mem->available_bytes = value_kb * 1024;
            found_avail          = 1;
        }

        if (found_total && found_avail) {
            break;
        }
    }

    fclose(fp);

    if (!found_total || !found_avail) {
        syslog(LOG_WARNING, "Failed to parse MemTotal or MemAvailable from /proc/meminfo");
        return -1;
    }

    return 0;
}

int get_cpu_stats(CPUStats* stats) {
    if (!stats) {
        return -1;
    }

    FILE* fp = fopen("/proc/stat", "r");
    if (!fp) {
        syslog(LOG_WARNING, "Failed to open /proc/stat");
        return -1;
    }

    char line[256];
    if (!fgets(line, sizeof(line), fp)) {
        syslog(LOG_WARNING, "Failed to read first line of /proc/stat");
        fclose(fp);
        return -1;
    }

    fclose(fp);

    if (sscanf(line,
               "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &stats->user,
               &stats->nice,
               &stats->system,
               &stats->idle,
               &stats->iowait,
               &stats->irq,
               &stats->softirq,
               &stats->steal) != 8) {
        syslog(LOG_WARNING, "Failed to parse CPU stats from /proc/stat");
        return -1;
    }

    return 0;
}

double calculate_cpu_usage(const CPUStats* prev, const CPUStats* curr) {
    if (!prev || !curr) {
        return -1.0;
    }

    uint64_t prev_idle = prev->idle + prev->iowait;
    uint64_t curr_idle = curr->idle + curr->iowait;

    uint64_t prev_total = prev->user + prev->nice + prev->system + prev->idle +
                          prev->iowait + prev->irq + prev->softirq + prev->steal;
    uint64_t curr_total = curr->user + curr->nice + curr->system + curr->idle +
                          curr->iowait + curr->irq + curr->softirq + curr->steal;

    uint64_t total_diff = curr_total - prev_total;
    uint64_t idle_diff  = curr_idle - prev_idle;

    if (total_diff == 0) {
        return 0.0;
    }

    double usage = 100.0 * (double)(total_diff - idle_diff) / (double)total_diff;
    return usage;
}

double get_load_average_1m(void) {
    FILE* fp = fopen("/proc/loadavg", "r");
    if (!fp) {
        syslog(LOG_WARNING, "Failed to open /proc/loadavg");
        return -1.0;
    }

    double load_avg;
    if (fscanf(fp, "%lf", &load_avg) != 1) {
        syslog(LOG_WARNING, "Failed to parse /proc/loadavg");
        fclose(fp);
        return -1.0;
    }

    fclose(fp);
    return load_avg;
}

int get_network_stats(NetworkStats* stats, const char* interface) {
    if (!stats || !interface) {
        return -1;
    }

    FILE* fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        syslog(LOG_WARNING, "Failed to open /proc/net/dev");
        return -1;
    }

    char line[512];

    // Skip first two header lines
    if (!fgets(line, sizeof(line), fp) || !fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }

    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        uint64_t rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame, rx_compressed, rx_multicast;
        uint64_t tx_bytes, tx_packets, tx_errs, tx_drop, tx_fifo, tx_colls, tx_carrier, tx_compressed;

        // Parse interface name (format is "  eth0: ...")
        char* colon = strchr(line, ':');
        if (!colon) {
            continue;
        }

        *colon = '\0';
        char* iface_start = line;
        while (*iface_start == ' ') {
            iface_start++;
        }

        if (strcmp(iface_start, interface) != 0) {
            continue;
        }

        // Parse stats (16 values total)
        if (sscanf(colon + 1,
                   "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &rx_bytes,
                   &rx_packets,
                   &rx_errs,
                   &rx_drop,
                   &rx_fifo,
                   &rx_frame,
                   &rx_compressed,
                   &rx_multicast,
                   &tx_bytes,
                   &tx_packets,
                   &tx_errs,
                   &tx_drop,
                   &tx_fifo,
                   &tx_colls,
                   &tx_carrier,
                   &tx_compressed) == 16) {
            stats->rx_bytes = rx_bytes;
            stats->tx_bytes = tx_bytes;
            found           = 1;
            break;
        }
    }

    fclose(fp);

    if (!found) {
        syslog(LOG_WARNING, "Interface %s not found in /proc/net/dev", interface);
        return -1;
    }

    return 0;
}

int get_primary_interface_name(char* buf, size_t size) {
    if (!buf || size == 0) {
        return -1;
    }

    FILE* fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        syslog(LOG_WARNING, "Failed to open /proc/net/dev");
        return -1;
    }

    char line[512];

    // Skip first two header lines
    if (!fgets(line, sizeof(line), fp) || !fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }

    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        // Parse interface name
        char* colon = strchr(line, ':');
        if (!colon) {
            continue;
        }

        *colon = '\0';
        char* iface_start = line;
        while (*iface_start == ' ') {
            iface_start++;
        }

        // Skip loopback interface
        if (strcmp(iface_start, "lo") == 0) {
            continue;
        }

        // Found first non-loopback interface
        snprintf(buf, size, "%s", iface_start);
        found = 1;
        break;
    }

    fclose(fp);

    if (!found) {
        syslog(LOG_WARNING, "No non-loopback interface found in /proc/net/dev");
        return -1;
    }

    return 0;
}

int get_disk_stats(const char* path, DiskStats* stats) {
    if (!path || !stats) {
        return -1;
    }

    struct statfs fs_stats;
    if (statfs(path, &fs_stats) != 0) {
        syslog(LOG_WARNING, "Failed to get filesystem stats for %s", path);
        return -1;
    }

    stats->total_bytes     = (uint64_t)fs_stats.f_blocks * (uint64_t)fs_stats.f_frsize;
    stats->available_bytes = (uint64_t)fs_stats.f_bavail * (uint64_t)fs_stats.f_frsize;

    return 0;
}

int get_process_count(void) {
    DIR* dir = opendir("/proc");
    if (!dir) {
        syslog(LOG_WARNING, "Failed to open /proc directory");
        return -1;
    }

    int count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        // Check if directory name is all digits (PID)
        int is_pid = 1;
        for (const char* p = entry->d_name; *p; p++) {
            if (!isdigit(*p)) {
                is_pid = 0;
                break;
            }
        }

        if (is_pid && entry->d_name[0] != '\0') {
            count++;
        }
    }

    closedir(dir);
    return count;
}
