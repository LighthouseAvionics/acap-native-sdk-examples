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

#include "http_server.h"
#include "i2c_lrf.h"
#include <jansson.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define PORT 8080
#define I2C_BUS 0
#define LRF_ADDR 0x48

static HttpServer* http_server    = NULL;
static LrfDevice* lrf_device      = NULL;

__attribute__((noreturn)) __attribute__((format(printf, 1, 2))) static void
panic(const char* format, ...) {
    va_list arg;
    va_start(arg, format);
    vsyslog(LOG_ERR, format, arg);
    va_end(arg);
    exit(1);
}

static void stop_application(int status) {
    (void)status;
    if (http_server) {
        http_server_stop(http_server);
    }
}

static void distance_handler(int client_fd, HttpRequest* request, gpointer user_data __attribute__((unused))) {
    if (g_strcmp0(request->method, "GET") != 0) {
        http_send_error(client_fd, 405, "Method not allowed");
        return;
    }

    float distance_m;
    if (!lrf_read_distance(lrf_device, &distance_m)) {
        http_send_error(client_fd, 500, "Failed to read distance from LRF");
        return;
    }

    json_t* json = json_object();
    json_object_set_new(json, "distance_m", json_real(distance_m));
    json_object_set_new(json, "status", json_string("ok"));

    gchar* json_str = json_dumps(json, JSON_COMPACT);
    http_send_json(client_fd, 200, json_str);

    free(json_str);
    json_decref(json);
}

static void command_handler(int client_fd, HttpRequest* request, gpointer user_data __attribute__((unused))) {
    if (g_strcmp0(request->method, "POST") != 0) {
        http_send_error(client_fd, 405, "Method not allowed");
        return;
    }

    if (!request->body || request->body_len == 0) {
        http_send_error(client_fd, 400, "No request body");
        return;
    }

    json_error_t json_error;
    json_t* json = json_loads(request->body, 0, &json_error);
    if (!json) {
        http_send_error(client_fd, 400, "Invalid JSON");
        return;
    }

    json_t* cmd_json = json_object_get(json, "cmd");
    if (!cmd_json || !json_is_integer(cmd_json)) {
        http_send_error(client_fd, 400, "Missing or invalid 'cmd' field");
        json_decref(json);
        return;
    }

    guint8 cmd      = (guint8)json_integer_value(cmd_json);
    guint8 response[32];
    memset(response, 0, sizeof(response));

    if (!lrf_send_command(lrf_device, cmd, response, sizeof(response))) {
        http_send_error(client_fd, 500, "Failed to send command to LRF");
        json_decref(json);
        return;
    }

    json_t* response_json = json_object();
    json_object_set_new(response_json, "status", json_string("ok"));

    json_t* response_array = json_array();
    for (gsize i = 0; i < sizeof(response); i++) {
        json_array_append_new(response_array, json_integer(response[i]));
    }
    json_object_set_new(response_json, "response", response_array);

    gchar* json_str = json_dumps(response_json, JSON_COMPACT);
    http_send_json(client_fd, 200, json_str);

    free(json_str);
    json_decref(response_json);
    json_decref(json);
}

static void status_handler(int client_fd, HttpRequest* request, gpointer user_data __attribute__((unused))) {
    if (g_strcmp0(request->method, "GET") != 0) {
        http_send_error(client_fd, 405, "Method not allowed");
        return;
    }

    json_t* json       = json_object();
    gboolean connected = (lrf_device != NULL && lrf_device->fd >= 0);

    json_object_set_new(json, "connected", json_boolean(connected));
    json_object_set_new(json, "bus", json_integer(I2C_BUS));

    gchar addr_str[16];
    g_snprintf(addr_str, sizeof(addr_str), "0x%02x", LRF_ADDR);
    json_object_set_new(json, "addr", json_string(addr_str));

    gchar* json_str = json_dumps(json, JSON_COMPACT);
    http_send_json(client_fd, 200, json_str);

    free(json_str);
    json_decref(json);
}

int main(void) {
    signal(SIGTERM, stop_application);
    signal(SIGINT, stop_application);

    syslog(LOG_INFO, "LRF Controller starting on port %d", PORT);

    lrf_device = lrf_open(I2C_BUS, LRF_ADDR);
    if (!lrf_device) {
        syslog(LOG_WARNING,
               "Failed to open LRF device on bus %d at address 0x%02x. Server will start but "
               "requests will fail.",
               I2C_BUS,
               LRF_ADDR);
    }

    http_server = http_server_new(PORT);
    if (!http_server) {
        if (lrf_device) {
            lrf_close(lrf_device);
        }
        panic("Failed to create HTTP server");
    }

    http_server_add_handler(http_server, "/distance", distance_handler, NULL);
    http_server_add_handler(http_server, "/command", command_handler, NULL);
    http_server_add_handler(http_server, "/status", status_handler, NULL);

    if (!http_server_start(http_server)) {
        http_server_free(http_server);
        if (lrf_device) {
            lrf_close(lrf_device);
        }
        panic("Failed to start HTTP server");
    }

    syslog(LOG_INFO, "LRF Controller server started successfully");

    http_server_run(http_server);

    syslog(LOG_INFO, "LRF Controller shutting down");

    http_server_free(http_server);

    if (lrf_device) {
        lrf_close(lrf_device);
    }

    return EXIT_SUCCESS;
}
