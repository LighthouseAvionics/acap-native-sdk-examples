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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

typedef struct {
    gchar* path;
    HttpHandler handler;
    gpointer user_data;
} HandlerEntry;

struct HttpServer {
    int port;
    int server_fd;
    gboolean running;
    GList* handlers;
};

HttpServer* http_server_new(int port) {
    HttpServer* server = g_malloc0(sizeof(HttpServer));
    server->port       = port;
    server->server_fd  = -1;
    server->running    = FALSE;
    server->handlers   = NULL;
    return server;
}

void http_server_free(HttpServer* server) {
    if (!server) {
        return;
    }

    if (server->server_fd >= 0) {
        close(server->server_fd);
    }

    for (GList* l = server->handlers; l != NULL; l = l->next) {
        HandlerEntry* entry = l->data;
        g_free(entry->path);
        g_free(entry);
    }
    g_list_free(server->handlers);

    g_free(server);
}

void http_server_add_handler(HttpServer* server, const gchar* path, HttpHandler handler, gpointer user_data) {
    HandlerEntry* entry = g_malloc(sizeof(HandlerEntry));
    entry->path         = g_strdup(path);
    entry->handler      = handler;
    entry->user_data    = user_data;
    server->handlers    = g_list_append(server->handlers, entry);
}

gboolean http_server_start(HttpServer* server) {
    struct sockaddr_in addr;

    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        syslog(LOG_ERR, "Failed to create socket");
        return FALSE;
    }

    int opt = 1;
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        syslog(LOG_WARNING, "setsockopt SO_REUSEADDR failed");
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(server->port);

    if (bind(server->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "Failed to bind to port %d", server->port);
        close(server->server_fd);
        server->server_fd = -1;
        return FALSE;
    }

    if (listen(server->server_fd, 10) < 0) {
        syslog(LOG_ERR, "Failed to listen on port %d", server->port);
        close(server->server_fd);
        server->server_fd = -1;
        return FALSE;
    }

    server->running = TRUE;
    syslog(LOG_INFO, "HTTP server listening on port %d", server->port);
    return TRUE;
}

void http_server_stop(HttpServer* server) {
    if (server) {
        server->running = FALSE;
    }
}

static HttpRequest* parse_request(const gchar* buffer, gsize len __attribute__((unused))) {
    HttpRequest* req = g_malloc0(sizeof(HttpRequest));

    gchar** lines      = g_strsplit(buffer, "\r\n", -1);
    gchar** request_line = g_strsplit(lines[0], " ", 3);

    if (request_line[0] && request_line[1]) {
        req->method = g_strdup(request_line[0]);
        req->path   = g_strdup(request_line[1]);
    }

    gsize content_length = 0;
    for (int i = 1; lines[i] && lines[i][0] != '\0'; i++) {
        if (g_str_has_prefix(lines[i], "Content-Length:")) {
            content_length = atoi(lines[i] + 15);
        }
    }

    if (content_length > 0) {
        const gchar* body_start = strstr(buffer, "\r\n\r\n");
        if (body_start) {
            body_start += 4;
            req->body     = g_strndup(body_start, content_length);
            req->body_len = content_length;
        }
    }

    g_strfreev(request_line);
    g_strfreev(lines);

    return req;
}

static void free_request(HttpRequest* req) {
    if (req) {
        g_free(req->method);
        g_free(req->path);
        g_free(req->body);
        g_free(req);
    }
}

void http_server_run(HttpServer* server) {
    gchar buffer[4096];

    while (server->running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server->server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (server->running) {
                syslog(LOG_WARNING, "Accept failed");
            }
            continue;
        }

        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            close(client_fd);
            continue;
        }

        buffer[bytes_read] = '\0';

        HttpRequest* req = parse_request(buffer, bytes_read);

        if (req->method && req->path) {
            gboolean handled = FALSE;

            for (GList* l = server->handlers; l != NULL; l = l->next) {
                HandlerEntry* entry = l->data;
                if (g_strcmp0(entry->path, req->path) == 0) {
                    entry->handler(client_fd, req, entry->user_data);
                    handled = TRUE;
                    break;
                }
            }

            if (!handled) {
                http_send_error(client_fd, 404, "Not Found");
            }
        } else {
            http_send_error(client_fd, 400, "Bad Request");
        }

        free_request(req);
        close(client_fd);
    }
}

void http_send_json(int client_fd, int status_code, const gchar* json_body) {
    const gchar* status_text = (status_code == 200) ? "OK" : "Error";
    gsize body_len           = strlen(json_body);

    gchar* response = g_strdup_printf("HTTP/1.1 %d %s\r\n"
                                      "Content-Type: application/json\r\n"
                                      "Content-Length: %zu\r\n"
                                      "Connection: close\r\n"
                                      "\r\n"
                                      "%s",
                                      status_code,
                                      status_text,
                                      body_len,
                                      json_body);

    gsize response_len = strlen(response);
    ssize_t written    = write(client_fd, response, response_len);
    if (written < 0 || (gsize)written != response_len) {
        syslog(LOG_WARNING, "Failed to write full HTTP response");
    }
    g_free(response);
}

void http_send_error(int client_fd, int status_code, const gchar* message) {
    gchar* json = g_strdup_printf("{\"error\":\"%s\"}", message);
    http_send_json(client_fd, status_code, json);
    g_free(json);
}
