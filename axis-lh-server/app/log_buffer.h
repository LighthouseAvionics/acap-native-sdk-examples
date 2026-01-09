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

#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include <jansson.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define MAX_LOG_ENTRIES 100
#define MAX_MESSAGE_LENGTH 256
#define MAX_SEVERITY_LENGTH 16

/**
 * Log entry structure
 */
typedef struct {
    time_t timestamp;
    char severity[MAX_SEVERITY_LENGTH];
    char message[MAX_MESSAGE_LENGTH];
} LogEntry;

/**
 * Circular log buffer structure
 */
typedef struct {
    LogEntry entries[MAX_LOG_ENTRIES];
    size_t head;
    size_t count;
    pthread_mutex_t lock;
} LogBuffer;

/**
 * Initialize log buffer
 * @param buffer pointer to LogBuffer structure
 */
void log_buffer_init(LogBuffer* buffer);

/**
 * Destroy log buffer and cleanup resources
 * @param buffer pointer to LogBuffer structure
 */
void log_buffer_destroy(LogBuffer* buffer);

/**
 * Log an event to the circular buffer and syslog
 * @param severity severity level string ("info", "warning", "critical", "debug")
 * @param format printf-style format string
 * @param ... format arguments
 */
void log_event(const char* severity, const char* format, ...);

/**
 * Get recent logs as JSON array
 * @param logs_array JSON array to append log entries to
 */
void get_recent_logs(json_t* logs_array);

/**
 * Get recent logs with limit
 * @param logs_array JSON array to append log entries to
 * @param max_entries maximum number of entries to return
 */
void get_recent_logs_limited(json_t* logs_array, size_t max_entries);

/**
 * Convert severity string to syslog priority
 * @param severity severity string
 * @return syslog priority level
 */
int severity_to_syslog(const char* severity);

#endif /* LOG_BUFFER_H */
