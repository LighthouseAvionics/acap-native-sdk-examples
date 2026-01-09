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

#include "metrics.h"
#include "metrics_helpers.h"
#include "vapix.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

/* Global counters */
uint64_t g_http_requests_total = 0;
uint64_t g_i2c_errors_total    = 0;

/**
 * Helper function to append a gauge metric with labels
 */
static void append_metric_gauge(GString* output,
                                 const char* name,
                                 const char* help,
                                 double value,
                                 const char* labels) {
    g_string_append_printf(output, "# HELP %s %s\n", name, help);
    g_string_append_printf(output, "# TYPE %s gauge\n", name);

    if (labels) {
        g_string_append_printf(output, "%s{%s} %.2f\n", name, labels, value);
    } else {
        g_string_append_printf(output, "%s %.2f\n", name, value);
    }
}

/**
 * Helper function to append a counter metric with labels
 */
static void append_metric_counter(GString* output,
                                   const char* name,
                                   const char* help,
                                   uint64_t value,
                                   const char* labels) {
    g_string_append_printf(output, "# HELP %s %s\n", name, help);
    g_string_append_printf(output, "# TYPE %s counter\n", name);

    if (labels) {
        g_string_append_printf(output, "%s{%s} %llu\n", name, labels, value);
    } else {
        g_string_append_printf(output, "%s %llu\n", name, value);
    }
}

void collect_system_metrics(GString* output) {
    /* Uptime */
    double uptime = get_uptime();
    if (uptime >= 0) {
        append_metric_gauge(output, "ptz_uptime_seconds", "PTZ camera system uptime", uptime, NULL);
    } else {
        syslog(LOG_WARNING, "Failed to collect uptime metric");
    }

    /* Memory */
    MemoryInfo mem;
    if (get_memory_info(&mem) == 0) {
        append_metric_gauge(output,
                            "ptz_memory_total_bytes",
                            "Total memory in bytes",
                            (double)mem.total_bytes,
                            NULL);
        append_metric_gauge(output,
                            "ptz_memory_available_bytes",
                            "Available memory in bytes",
                            (double)mem.available_bytes,
                            NULL);
    } else {
        syslog(LOG_WARNING, "Failed to collect memory metrics");
    }

    /* Load average */
    double load_avg = get_load_average_1m();
    if (load_avg >= 0) {
        append_metric_gauge(output, "ptz_load_average_1m", "1-minute load average", load_avg, NULL);
    } else {
        syslog(LOG_WARNING, "Failed to collect load average metric");
    }

    /* CPU usage (requires delta calculation) */
    static CPUStats prev_cpu = {0};
    static int first_sample  = 1;

    CPUStats curr_cpu;
    if (get_cpu_stats(&curr_cpu) == 0) {
        if (!first_sample) {
            double cpu_usage = calculate_cpu_usage(&prev_cpu, &curr_cpu);
            if (cpu_usage >= 0) {
                append_metric_gauge(output,
                                    "ptz_cpu_usage_percent",
                                    "CPU utilization percentage",
                                    cpu_usage,
                                    NULL);
            }
        }
        prev_cpu     = curr_cpu;
        first_sample = 0;
    } else {
        syslog(LOG_WARNING, "Failed to collect CPU stats");
    }
}

void collect_network_metrics(GString* output) {
    char interface[64];
    if (get_primary_interface_name(interface, sizeof(interface)) != 0) {
        syslog(LOG_WARNING, "Failed to detect primary network interface");
        return;
    }

    NetworkStats net_stats;
    if (get_network_stats(&net_stats, interface) != 0) {
        syslog(LOG_WARNING, "Failed to collect network stats for %s", interface);
        return;
    }

    char labels[128];
    snprintf(labels, sizeof(labels), "interface=\"%s\"", interface);

    append_metric_counter(output,
                          "ptz_network_rx_bytes_total",
                          "Total bytes received",
                          net_stats.rx_bytes,
                          labels);
    append_metric_counter(output,
                          "ptz_network_tx_bytes_total",
                          "Total bytes transmitted",
                          net_stats.tx_bytes,
                          labels);
}

void collect_disk_metrics(GString* output) {
    DiskStats disk_stats;
    if (get_disk_stats("/", &disk_stats) != 0) {
        syslog(LOG_WARNING, "Failed to collect disk stats");
        return;
    }

    append_metric_gauge(output,
                        "ptz_disk_total_bytes",
                        "Total disk space in bytes",
                        (double)disk_stats.total_bytes,
                        NULL);
    append_metric_gauge(output,
                        "ptz_disk_free_bytes",
                        "Free disk space in bytes",
                        (double)disk_stats.available_bytes,
                        NULL);
}

void collect_service_metrics(GString* output) {
    /* Application counters */
    append_metric_counter(output,
                          "ptz_http_requests_total",
                          "Total HTTP requests handled",
                          g_http_requests_total,
                          NULL);
    append_metric_counter(output,
                          "ptz_i2c_errors_total",
                          "Total I2C communication errors",
                          g_i2c_errors_total,
                          NULL);

    /* Process count */
    int proc_count = get_process_count();
    if (proc_count >= 0) {
        append_metric_gauge(output,
                            "ptz_process_count",
                            "Number of running processes",
                            (double)proc_count,
                            NULL);
    } else {
        syslog(LOG_WARNING, "Failed to collect process count");
    }
}

void collect_vapix_metrics(GString* output) {
    double temperature;
    if (get_cached_temperature(&temperature) == 0) {
        append_metric_gauge(output,
                            "ptz_temperature_celsius",
                            "Camera temperature in Celsius",
                            temperature,
                            NULL);
    }
    /* If VAPIX is unavailable, we simply skip the temperature metric */
}

void metrics_handler(int client_fd, HttpRequest* request, gpointer user_data) {
    (void)user_data;

    /* Increment request counter */
    g_http_requests_total++;

    /* Validate HTTP method */
    if (g_strcmp0(request->method, "GET") != 0) {
        http_send_error(client_fd, 405, "Method not allowed");
        return;
    }

    /* Collect all metrics */
    GString* metrics = g_string_new("");

    collect_system_metrics(metrics);
    collect_network_metrics(metrics);
    collect_disk_metrics(metrics);
    collect_service_metrics(metrics);
    collect_vapix_metrics(metrics);

    /* Ensure trailing newline */
    if (metrics->len > 0 && metrics->str[metrics->len - 1] != '\n') {
        g_string_append_c(metrics, '\n');
    }

    /* Build HTTP response header */
    char header[512];
    snprintf(header,
             sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
             "Content-Length: %zu\r\n"
             "\r\n",
             metrics->len);

    /* Send header and body */
    if (send(client_fd, header, strlen(header), 0) < 0) {
        syslog(LOG_WARNING, "Failed to send metrics response header");
        g_string_free(metrics, TRUE);
        return;
    }

    if (send(client_fd, metrics->str, metrics->len, 0) < 0) {
        syslog(LOG_WARNING, "Failed to send metrics response body");
    }

    g_string_free(metrics, TRUE);
}
