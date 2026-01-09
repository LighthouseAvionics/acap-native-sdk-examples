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

#include "log_buffer.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

/* Global log buffer */
static LogBuffer g_log_buffer        = {0};
static bool g_log_buffer_initialized = false;

void log_buffer_init(LogBuffer* buffer) {
    memset(buffer, 0, sizeof(LogBuffer));
    pthread_mutex_init(&buffer->lock, NULL);
    g_log_buffer_initialized = true;
}

void log_buffer_destroy(LogBuffer* buffer) {
    pthread_mutex_destroy(&buffer->lock);
    g_log_buffer_initialized = false;
}

void log_event(const char* severity, const char* format, ...) {
    /* Initialize buffer if needed */
    if (!g_log_buffer_initialized) {
        log_buffer_init(&g_log_buffer);
    }

    pthread_mutex_lock(&g_log_buffer.lock);

    /* Get next entry slot */
    LogEntry* entry = &g_log_buffer.entries[g_log_buffer.head];

    /* Set timestamp */
    entry->timestamp = time(NULL);

    /* Copy severity */
    strncpy(entry->severity, severity, MAX_SEVERITY_LENGTH - 1);
    entry->severity[MAX_SEVERITY_LENGTH - 1] = '\0';

    /* Format message */
    va_list args;
    va_start(args, format);
    vsnprintf(entry->message, MAX_MESSAGE_LENGTH, format, args);
    va_end(args);

    /* Advance head pointer (circular) */
    g_log_buffer.head = (g_log_buffer.head + 1) % MAX_LOG_ENTRIES;

    /* Update count (saturate at MAX) */
    if (g_log_buffer.count < MAX_LOG_ENTRIES) {
        g_log_buffer.count++;
    }

    pthread_mutex_unlock(&g_log_buffer.lock);

    /* Also log to syslog */
    syslog(severity_to_syslog(severity), "[%s] %s", severity, entry->message);
}

void get_recent_logs(json_t* logs_array) {
    get_recent_logs_limited(logs_array, MAX_LOG_ENTRIES);
}

void get_recent_logs_limited(json_t* logs_array, size_t max_entries) {
    if (!g_log_buffer_initialized) {
        return;
    }

    pthread_mutex_lock(&g_log_buffer.lock);

    size_t entries_to_export = (max_entries < g_log_buffer.count) ? max_entries : g_log_buffer.count;

    /* Calculate start index (oldest entry) */
    size_t start_idx = (g_log_buffer.head + MAX_LOG_ENTRIES - entries_to_export) % MAX_LOG_ENTRIES;

    /* Export entries in chronological order (oldest to newest) */
    for (size_t i = 0; i < entries_to_export; i++) {
        size_t idx            = (start_idx + i) % MAX_LOG_ENTRIES;
        const LogEntry* entry = &g_log_buffer.entries[idx];

        /* Format timestamp as ISO 8601 */
        char timestamp_str[32];
        struct tm* tm_info = gmtime(&entry->timestamp);
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", tm_info);

        /* Create JSON object for this log entry */
        json_t* log_obj = json_pack("{s:s, s:s, s:s}",
                                     "timestamp",
                                     timestamp_str,
                                     "severity",
                                     entry->severity,
                                     "message",
                                     entry->message);

        json_array_append_new(logs_array, log_obj);
    }

    pthread_mutex_unlock(&g_log_buffer.lock);
}

int severity_to_syslog(const char* severity) {
    if (strcmp(severity, "critical") == 0) {
        return LOG_CRIT;
    } else if (strcmp(severity, "warning") == 0) {
        return LOG_WARNING;
    } else if (strcmp(severity, "info") == 0) {
        return LOG_INFO;
    } else if (strcmp(severity, "debug") == 0) {
        return LOG_DEBUG;
    } else {
        return LOG_NOTICE;
    }
}
