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

#ifndef HEALTH_H
#define HEALTH_H

#include "http_server.h"
#include <glib.h>
#include <jansson.h>
#include <stdbool.h>
#include <time.h>

/**
 * Health status enumeration
 */
typedef enum {
    HEALTH_STATUS_HEALTHY   = 0,
    HEALTH_STATUS_DEGRADED  = 1,
    HEALTH_STATUS_UNHEALTHY = 2
} HealthStatus;

/**
 * Threshold type for health checks
 */
typedef enum {
    THRESHOLD_TYPE_LOWER_BAD,  // Lower values are bad (e.g., memory, disk)
    THRESHOLD_TYPE_HIGHER_BAD  // Higher values are bad (e.g., temperature, CPU)
} ThresholdType;

/**
 * Health check structure
 */
typedef struct {
    char name[64];
    double value;
    double warning_threshold;
    double critical_threshold;
    ThresholdType threshold_type;
    HealthStatus status;
} HealthCheck;

/**
 * Dependency check structure
 */
typedef struct {
    char service[64];
    bool reachable;
    HealthStatus status;
} DependencyCheck;

/**
 * Health report structure
 */
typedef struct {
    char service_name[64];
    HealthStatus overall_status;
    char timestamp[32];
    HealthCheck* checks;
    size_t check_count;
    DependencyCheck* dependencies;
    size_t dependency_count;
} HealthReport;

/**
 * Calculate health status for a single check
 * @param check pointer to HealthCheck structure
 * @return calculated health status
 */
HealthStatus calculate_check_status(const HealthCheck* check);

/**
 * Calculate overall health status from all checks and dependencies
 * @param checks array of health checks
 * @param check_count number of checks
 * @param deps array of dependency checks
 * @param dep_count number of dependencies
 * @return overall health status (worst-case)
 */
HealthStatus calculate_overall_status(HealthCheck* checks,
                                       size_t check_count,
                                       DependencyCheck* deps,
                                       size_t dep_count);

/**
 * Convert health status to string
 * @param status health status enum value
 * @return status string ("healthy", "degraded", "unhealthy")
 */
const char* health_status_to_string(HealthStatus status);

/**
 * Convert health status to severity level
 * @param status health status enum value
 * @return severity string ("info", "warning", "critical")
 */
const char* health_status_to_severity(HealthStatus status);

/**
 * Get current timestamp in ISO 8601 format
 * @param buffer buffer to store timestamp
 * @param buffer_size size of buffer
 */
void get_iso8601_timestamp(char* buffer, size_t buffer_size);

/**
 * Generate health report with all checks
 * @param report pointer to HealthReport structure to fill
 */
void generate_health_report(HealthReport* report);

/**
 * Convert health report to JSON
 * @param report pointer to HealthReport structure
 * @return JSON object (must be freed with json_decref)
 */
json_t* health_report_to_json(const HealthReport* report);

/**
 * HTTP handler for /health endpoint
 * Returns JSON health status report
 * @param client_fd client socket file descriptor
 * @param request HTTP request structure
 * @param user_data user data (unused)
 */
void health_handler(int client_fd, HttpRequest* request, gpointer user_data);

#endif /* HEALTH_H */
