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

#ifndef METRICS_H
#define METRICS_H

#include "http_server.h"
#include <glib.h>
#include <stdint.h>

/**
 * Global counters for application metrics
 * These should be incremented by the application as events occur
 */
extern uint64_t g_http_requests_total;
extern uint64_t g_i2c_errors_total;

/**
 * Collect system metrics (uptime, memory, CPU, load average)
 * @param output GString to append metrics to
 */
void collect_system_metrics(GString* output);

/**
 * Collect network metrics (RX/TX bytes)
 * @param output GString to append metrics to
 */
void collect_network_metrics(GString* output);

/**
 * Collect disk metrics (total/free bytes)
 * @param output GString to append metrics to
 */
void collect_disk_metrics(GString* output);

/**
 * Collect service metrics (HTTP requests, I2C errors, process count)
 * @param output GString to append metrics to
 */
void collect_service_metrics(GString* output);

/**
 * Collect VAPIX metrics (temperature)
 * @param output GString to append metrics to
 */
void collect_vapix_metrics(GString* output);

/**
 * HTTP handler for /metrics endpoint
 * Returns Prometheus text format metrics
 * @param client_fd client socket file descriptor
 * @param request HTTP request structure
 * @param user_data user data (unused)
 */
void metrics_handler(int client_fd, HttpRequest* request, gpointer user_data);

#endif /* METRICS_H */
