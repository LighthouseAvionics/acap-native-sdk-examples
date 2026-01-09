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

#ifndef METRICS_HELPERS_H
#define METRICS_HELPERS_H

#include <stddef.h>
#include <stdint.h>

/**
 * Memory information structure
 */
typedef struct {
    uint64_t total_bytes;
    uint64_t available_bytes;
} MemoryInfo;

/**
 * CPU statistics structure
 */
typedef struct {
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
} CPUStats;

/**
 * Network statistics structure
 */
typedef struct {
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} NetworkStats;

/**
 * Disk statistics structure
 */
typedef struct {
    uint64_t total_bytes;
    uint64_t available_bytes;
} DiskStats;

/**
 * Get system uptime in seconds
 * @return uptime in seconds, or -1.0 on error
 */
double get_uptime(void);

/**
 * Get memory information from /proc/meminfo
 * @param mem pointer to MemoryInfo structure to fill
 * @return 0 on success, -1 on error
 */
int get_memory_info(MemoryInfo* mem);

/**
 * Get CPU statistics from /proc/stat
 * @param stats pointer to CPUStats structure to fill
 * @return 0 on success, -1 on error
 */
int get_cpu_stats(CPUStats* stats);

/**
 * Calculate CPU usage percentage from two CPU stat samples
 * @param prev previous CPU stats
 * @param curr current CPU stats
 * @return CPU usage percentage (0.0-100.0), or -1.0 on error
 */
double calculate_cpu_usage(const CPUStats* prev, const CPUStats* curr);

/**
 * Get 1-minute load average from /proc/loadavg
 * @return load average, or -1.0 on error
 */
double get_load_average_1m(void);

/**
 * Get network statistics for a specific interface
 * @param stats pointer to NetworkStats structure to fill
 * @param interface interface name (e.g., "eth0")
 * @return 0 on success, -1 on error
 */
int get_network_stats(NetworkStats* stats, const char* interface);

/**
 * Get the name of the primary network interface
 * @param buf buffer to store interface name
 * @param size size of buffer
 * @return 0 on success, -1 on error
 */
int get_primary_interface_name(char* buf, size_t size);

/**
 * Get disk statistics for a mount point
 * @param path mount point path (e.g., "/")
 * @param stats pointer to DiskStats structure to fill
 * @return 0 on success, -1 on error
 */
int get_disk_stats(const char* path, DiskStats* stats);

/**
 * Get number of running processes
 * @return process count, or -1 on error
 */
int get_process_count(void);

#endif /* METRICS_HELPERS_H */
