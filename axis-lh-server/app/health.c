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

#include "health.h"
#include "log_buffer.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>
#include <syslog.h>
#include <unistd.h>

/* Helper function to get memory info */
static int get_memory_info(double* available_mb) {
    FILE* fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        return -1;
    }

    char line[256];
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        unsigned long value_kb;
        if (sscanf(line, "MemAvailable: %lu kB", &value_kb) == 1) {
            *available_mb = (double)value_kb / 1024.0;
            found         = 1;
            break;
        }
    }

    fclose(fp);
    return found ? 0 : -1;
}

/* Helper function to get disk stats */
static int get_disk_stats(const char* path, double* free_mb) {
    struct statvfs stat;
    if (statvfs(path, &stat) != 0) {
        return -1;
    }

    unsigned long long free_bytes = (unsigned long long)stat.f_bavail * (unsigned long long)stat.f_frsize;
    *free_mb                      = (double)free_bytes / (1024.0 * 1024.0);

    return 0;
}

/* Helper function to get temperature */
static double get_temperature(void) {
    int fd = open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY);
    if (fd < 0) {
        return -1.0;
    }

    char buf[32];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) {
        return -1.0;
    }

    buf[n] = '\0';
    long temp_millidegrees;
    if (sscanf(buf, "%ld", &temp_millidegrees) != 1) {
        return -1.0;
    }

    return (double)temp_millidegrees / 1000.0;
}

/* Helper function to check I2C bus availability */
static bool check_i2c_bus_available(void) {
    int fd = open("/dev/i2c-0", O_RDWR);
    if (fd < 0) {
        return false;
    }
    close(fd);
    return true;
}

HealthStatus calculate_check_status(const HealthCheck* check) {
    if (!check) {
        return HEALTH_STATUS_UNHEALTHY;
    }

    if (check->threshold_type == THRESHOLD_TYPE_LOWER_BAD) {
        if (check->value < check->critical_threshold) {
            return HEALTH_STATUS_UNHEALTHY;
        }
        if (check->value < check->warning_threshold) {
            return HEALTH_STATUS_DEGRADED;
        }
        return HEALTH_STATUS_HEALTHY;
    } else { /* THRESHOLD_TYPE_HIGHER_BAD */
        if (check->value > check->critical_threshold) {
            return HEALTH_STATUS_UNHEALTHY;
        }
        if (check->value > check->warning_threshold) {
            return HEALTH_STATUS_DEGRADED;
        }
        return HEALTH_STATUS_HEALTHY;
    }
}

HealthStatus calculate_overall_status(HealthCheck* checks,
                                       size_t check_count,
                                       DependencyCheck* deps,
                                       size_t dep_count) {
    HealthStatus overall = HEALTH_STATUS_HEALTHY;

    /* Calculate status for all checks */
    for (size_t i = 0; i < check_count; i++) {
        checks[i].status = calculate_check_status(&checks[i]);
        if (checks[i].status > overall) {
            overall = checks[i].status;
        }
    }

    /* Check dependencies */
    for (size_t i = 0; i < dep_count; i++) {
        deps[i].status = deps[i].reachable ? HEALTH_STATUS_HEALTHY : HEALTH_STATUS_DEGRADED;
        if (deps[i].status > overall) {
            overall = deps[i].status;
        }
    }

    return overall;
}

const char* health_status_to_string(HealthStatus status) {
    switch (status) {
    case HEALTH_STATUS_HEALTHY:
        return "healthy";
    case HEALTH_STATUS_DEGRADED:
        return "degraded";
    case HEALTH_STATUS_UNHEALTHY:
        return "unhealthy";
    default:
        return "unknown";
    }
}

const char* health_status_to_severity(HealthStatus status) {
    switch (status) {
    case HEALTH_STATUS_HEALTHY:
        return "info";
    case HEALTH_STATUS_DEGRADED:
        return "warning";
    case HEALTH_STATUS_UNHEALTHY:
        return "critical";
    default:
        return "unknown";
    }
}

void get_iso8601_timestamp(char* buffer, size_t buffer_size) {
    time_t now           = time(NULL);
    struct tm* tm_info = gmtime(&now);
    strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", tm_info);
}

void generate_health_report(HealthReport* report) {
    /* Set service name */
    snprintf(report->service_name, sizeof(report->service_name), "axis-lh-server");

    /* Set timestamp */
    get_iso8601_timestamp(report->timestamp, sizeof(report->timestamp));

    /* Create health checks */
    static HealthCheck checks[4];
    report->checks     = checks;
    report->check_count = 4;

    /* Check 1: Memory Available */
    double mem_value = 0;
    snprintf(checks[0].name, sizeof(checks[0].name), "memory_available_mb");
    if (get_memory_info(&mem_value) == 0) {
        checks[0].value = mem_value;
    } else {
        checks[0].value = -1.0;
    }
    checks[0].warning_threshold  = 50.0;
    checks[0].critical_threshold = 20.0;
    checks[0].threshold_type     = THRESHOLD_TYPE_LOWER_BAD;

    /* Check 2: Disk Free Space */
    double disk_value = 0;
    snprintf(checks[1].name, sizeof(checks[1].name), "disk_free_mb");
    if (get_disk_stats("/", &disk_value) == 0) {
        checks[1].value = disk_value;
    } else {
        checks[1].value = -1.0;
    }
    checks[1].warning_threshold  = 100.0;
    checks[1].critical_threshold = 50.0;
    checks[1].threshold_type     = THRESHOLD_TYPE_LOWER_BAD;

    /* Check 3: Temperature */
    double temp_value = get_temperature();
    snprintf(checks[2].name, sizeof(checks[2].name), "temperature_celsius");
    checks[2].value               = temp_value;
    checks[2].warning_threshold   = 70.0;
    checks[2].critical_threshold  = 80.0;
    checks[2].threshold_type      = THRESHOLD_TYPE_HIGHER_BAD;

    /* Check 4: CPU Usage (placeholder) */
    snprintf(checks[3].name, sizeof(checks[3].name), "cpu_usage_percent");
    checks[3].value               = 0.0; /* TODO: Implement CPU usage monitoring */
    checks[3].warning_threshold   = 80.0;
    checks[3].critical_threshold  = 95.0;
    checks[3].threshold_type      = THRESHOLD_TYPE_HIGHER_BAD;

    /* Create dependency checks */
    static DependencyCheck deps[1];
    report->dependencies     = deps;
    report->dependency_count = 1;

    snprintf(deps[0].service, sizeof(deps[0].service), "i2c-bus-0");
    deps[0].reachable = check_i2c_bus_available();

    /* Calculate overall status */
    report->overall_status = calculate_overall_status(report->checks,
                                                       report->check_count,
                                                       report->dependencies,
                                                       report->dependency_count);
}

json_t* health_report_to_json(const HealthReport* report) {
    json_t* root = json_object();

    /* Root fields */
    json_object_set_new(root, "service", json_string(report->service_name));
    json_object_set_new(root, "status", json_string(health_status_to_string(report->overall_status)));
    json_object_set_new(root, "severity", json_string(health_status_to_severity(report->overall_status)));
    json_object_set_new(root, "timestamp", json_string(report->timestamp));

    /* Checks array */
    json_t* checks_array = json_array();
    for (size_t i = 0; i < report->check_count; i++) {
        const HealthCheck* check = &report->checks[i];
        json_t* check_obj        = json_object();

        json_object_set_new(check_obj, "name", json_string(check->name));
        json_object_set_new(check_obj, "value", json_real(check->value));
        json_object_set_new(check_obj, "warning", json_real(check->warning_threshold));
        json_object_set_new(check_obj, "critical", json_real(check->critical_threshold));
        json_object_set_new(check_obj, "status", json_string(health_status_to_string(check->status)));

        json_array_append_new(checks_array, check_obj);
    }
    json_object_set_new(root, "checks", checks_array);

    /* Dependencies array */
    json_t* deps_array = json_array();
    for (size_t i = 0; i < report->dependency_count; i++) {
        const DependencyCheck* dep = &report->dependencies[i];
        json_t* dep_obj            = json_object();

        json_object_set_new(dep_obj, "service", json_string(dep->service));
        json_object_set_new(dep_obj, "reachable", json_boolean(dep->reachable));
        json_object_set_new(dep_obj, "status", json_string(health_status_to_string(dep->status)));

        json_array_append_new(deps_array, dep_obj);
    }
    json_object_set_new(root, "dependencies", deps_array);

    return root;
}

void health_handler(int client_fd, HttpRequest* request, gpointer user_data) {
    (void)user_data;

    /* Validate HTTP method */
    if (g_strcmp0(request->method, "GET") != 0) {
        http_send_error(client_fd, 405, "Method not allowed");
        return;
    }

    /* Generate health report */
    HealthReport report;
    generate_health_report(&report);

    /* Convert to JSON */
    json_t* json = health_report_to_json(&report);
    char* json_str = json_dumps(json, JSON_INDENT(2));

    /* Send response */
    http_send_json(client_fd, 200, json_str);

    /* Cleanup */
    free(json_str);
    json_decref(json);
}
