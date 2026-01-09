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

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <glib.h>

typedef struct HttpRequest {
    gchar* method;
    gchar* path;
    gchar* body;
    gsize body_len;
} HttpRequest;

typedef void (*HttpHandler)(int client_fd, HttpRequest* request, gpointer user_data);

typedef struct HttpServer HttpServer;

HttpServer* http_server_new(int port);
void http_server_free(HttpServer* server);
void http_server_add_handler(HttpServer* server, const gchar* path, HttpHandler handler, gpointer user_data);
gboolean http_server_start(HttpServer* server);
void http_server_stop(HttpServer* server);
void http_server_run(HttpServer* server);

void http_send_json(int client_fd, int status_code, const gchar* json_body);
void http_send_error(int client_fd, int status_code, const gchar* message);

#endif
