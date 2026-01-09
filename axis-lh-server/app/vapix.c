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

#include "vapix.h"
#include <curl/curl.h>
#include <gio/gio.h>
#include <glib.h>
#include <jansson.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

/* Cache structures */
typedef struct {
    double value;
    time_t timestamp;
    int valid;
    int ttl_seconds;
} CachedDouble;

typedef struct {
    DeviceInfo value;
    time_t timestamp;
    int valid;
    int ttl_seconds;
} CachedDeviceInfo;

typedef struct {
    char* data;
    size_t size;
} MemoryBuffer;

typedef struct {
    char username[128];
    char password[256];
} VAPIXCredentials;

/* Global state */
static CachedDouble temperature_cache     = {.ttl_seconds = 60};
static CachedDeviceInfo device_info_cache = {.ttl_seconds = 300};
static pthread_mutex_t cache_mutex        = PTHREAD_MUTEX_INITIALIZER;
static VAPIXCredentials g_vapix_creds     = {0};
static int g_vapix_initialized            = 0;

/**
 * Callback for curl to write response data
 */
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize       = size * nmemb;
    MemoryBuffer* buffer  = (MemoryBuffer*)userp;

    char* ptr = realloc(buffer->data, buffer->size + realsize + 1);
    if (!ptr) {
        syslog(LOG_ERR, "VAPIX: Out of memory in write_callback");
        return 0;
    }

    buffer->data = ptr;
    memcpy(&(buffer->data[buffer->size]), contents, realsize);
    buffer->size += realsize;
    buffer->data[buffer->size] = '\0';

    return realsize;
}

/**
 * Acquire VAPIX credentials via D-Bus
 */
static int acquire_vapix_credentials(void) {
    GError* error           = NULL;
    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);

    if (!connection) {
        syslog(LOG_ERR, "VAPIX: Failed to connect to system D-Bus: %s", error ? error->message : "unknown");
        if (error) {
            g_error_free(error);
        }
        return -1;
    }

    GVariant* result = g_dbus_connection_call_sync(connection,
                                                     "com.axis.HTTPConf1",
                                                     "/com/axis/HTTPConf1/VAPIXServiceAccounts1",
                                                     "com.axis.HTTPConf1.VAPIXServiceAccounts1",
                                                     "GetCredentials",
                                                     g_variant_new("(s)", "axis-lh-server"),
                                                     G_VARIANT_TYPE("(ss)"),
                                                     G_DBUS_CALL_FLAGS_NONE,
                                                     -1,
                                                     NULL,
                                                     &error);

    if (!result) {
        syslog(LOG_ERR, "VAPIX: Failed to acquire credentials via D-Bus: %s", error ? error->message : "unknown");
        if (error) {
            g_error_free(error);
        }
        g_object_unref(connection);
        return -1;
    }

    const char* username = NULL;
    const char* password = NULL;
    g_variant_get(result, "(&s&s)", &username, &password);

    if (!username || !password) {
        syslog(LOG_ERR, "VAPIX: Invalid credentials returned from D-Bus");
        g_variant_unref(result);
        g_object_unref(connection);
        return -1;
    }

    snprintf(g_vapix_creds.username, sizeof(g_vapix_creds.username), "%s", username);
    snprintf(g_vapix_creds.password, sizeof(g_vapix_creds.password), "%s", password);

    g_variant_unref(result);
    g_object_unref(connection);

    syslog(LOG_INFO, "VAPIX: Credentials acquired successfully");
    return 0;
}

int vapix_init(void) {
    if (g_vapix_initialized) {
        return 0;
    }

    if (acquire_vapix_credentials() != 0) {
        syslog(LOG_WARNING, "VAPIX: Failed to acquire credentials, VAPIX features unavailable");
        return -1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    g_vapix_initialized = 1;

    syslog(LOG_INFO, "VAPIX: Client initialized successfully");
    return 0;
}

void vapix_cleanup(void) {
    if (!g_vapix_initialized) {
        return;
    }

    memset(&g_vapix_creds, 0, sizeof(g_vapix_creds));
    curl_global_cleanup();
    g_vapix_initialized = 0;

    syslog(LOG_INFO, "VAPIX: Client cleaned up");
}

/**
 * Parse temperature from VAPIX response
 */
static double parse_temperature_response(const char* response) {
    if (!response || strlen(response) == 0) {
        return -1.0;
    }

    double temperature;
    if (sscanf(response, "%lf", &temperature) != 1) {
        syslog(LOG_WARNING, "VAPIX: Failed to parse temperature response");
        return -1.0;
    }

    /* Sanity check */
    if (temperature < -50.0 || temperature > 100.0) {
        syslog(LOG_WARNING, "VAPIX: Temperature value out of range: %.2f", temperature);
    }

    return temperature;
}

/**
 * Fetch temperature from VAPIX API
 */
static int vapix_get_temperature(double* temperature) {
    if (!g_vapix_initialized) {
        return -1;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        syslog(LOG_ERR, "VAPIX: Failed to initialize curl");
        return -1;
    }

    MemoryBuffer buffer = {.data = NULL, .size = 0};

    char auth[512];
    snprintf(auth, sizeof(auth), "%s:%s", g_vapix_creds.username, g_vapix_creds.password);

    curl_easy_setopt(curl, CURLOPT_URL,
                     "http://127.0.0.1/axis-cgi/temperaturecontrol.cgi?device=sensor&id=2&action=query&temperatureunit=celsius");
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC | CURLAUTH_DIGEST);
    curl_easy_setopt(curl, CURLOPT_USERPWD, auth);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&buffer);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        syslog(LOG_WARNING, "VAPIX: Temperature request failed: %s", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(buffer.data);
        return -1;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code != 200) {
        syslog(LOG_WARNING, "VAPIX: Temperature request returned HTTP %ld", http_code);
        curl_easy_cleanup(curl);
        free(buffer.data);
        return -1;
    }

    double temp = parse_temperature_response(buffer.data);
    curl_easy_cleanup(curl);
    free(buffer.data);

    if (temp < 0) {
        return -1;
    }

    *temperature = temp;
    return 0;
}

/**
 * Parse device info from VAPIX response
 */
static int parse_device_info_response(const char* response, DeviceInfo* info) {
    json_error_t error;
    json_t* root = json_loads(response, 0, &error);

    if (!root) {
        syslog(LOG_WARNING, "VAPIX: Failed to parse device info JSON: %s", error.text);
        return -1;
    }

    json_t* data = json_object_get(root, "data");
    if (!data) {
        json_decref(root);
        return -1;
    }

    json_t* property_list = json_object_get(data, "propertyList");
    if (!property_list) {
        json_decref(root);
        return -1;
    }

    const char* serial = json_string_value(json_object_get(property_list, "SerialNumber"));
    const char* version = json_string_value(json_object_get(property_list, "Version"));
    const char* model = json_string_value(json_object_get(property_list, "ProdNbr"));
    const char* arch = json_string_value(json_object_get(property_list, "Architecture"));
    const char* soc = json_string_value(json_object_get(property_list, "Soc"));

    if (serial) {
        snprintf(info->serial_number, sizeof(info->serial_number), "%s", serial);
    }
    if (version) {
        snprintf(info->firmware_version, sizeof(info->firmware_version), "%s", version);
    }
    if (model) {
        snprintf(info->model, sizeof(info->model), "%s", model);
    }
    if (arch) {
        snprintf(info->architecture, sizeof(info->architecture), "%s", arch);
    }
    if (soc) {
        snprintf(info->soc, sizeof(info->soc), "%s", soc);
    }

    json_decref(root);
    return 0;
}

/**
 * Fetch device info from VAPIX API
 */
static int vapix_get_device_info(DeviceInfo* info) {
    if (!g_vapix_initialized) {
        return -1;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        syslog(LOG_ERR, "VAPIX: Failed to initialize curl");
        return -1;
    }

    MemoryBuffer buffer = {.data = NULL, .size = 0};

    char auth[512];
    snprintf(auth, sizeof(auth), "%s:%s", g_vapix_creds.username, g_vapix_creds.password);

    const char* json_payload = "{\"apiVersion\":\"1.0\",\"context\":\"axis-lh-server\",\"method\":\"getAllProperties\"}";

    struct curl_slist* headers = NULL;
    headers                    = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1/axis-cgi/basicdeviceinfo.cgi");
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC | CURLAUTH_DIGEST);
    curl_easy_setopt(curl, CURLOPT_USERPWD, auth);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&buffer);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        syslog(LOG_WARNING, "VAPIX: Device info request failed: %s", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(buffer.data);
        return -1;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code != 200) {
        syslog(LOG_WARNING, "VAPIX: Device info request returned HTTP %ld", http_code);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(buffer.data);
        return -1;
    }

    int result = parse_device_info_response(buffer.data, info);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(buffer.data);

    return result;
}

int get_cached_temperature(double* temperature) {
    if (!temperature) {
        return -1;
    }

    pthread_mutex_lock(&cache_mutex);

    time_t now = time(NULL);

    /* Check if cache is valid and within TTL */
    if (temperature_cache.valid && (now - temperature_cache.timestamp) < temperature_cache.ttl_seconds) {
        *temperature = temperature_cache.value;
        pthread_mutex_unlock(&cache_mutex);
        return 0;
    }

    pthread_mutex_unlock(&cache_mutex);

    /* Fetch fresh data */
    double temp;
    if (vapix_get_temperature(&temp) == 0) {
        pthread_mutex_lock(&cache_mutex);
        temperature_cache.value     = temp;
        temperature_cache.timestamp = now;
        temperature_cache.valid     = 1;
        pthread_mutex_unlock(&cache_mutex);

        *temperature = temp;
        return 0;
    }

    /* Try serving stale cache if available */
    pthread_mutex_lock(&cache_mutex);
    if (temperature_cache.valid) {
        *temperature = temperature_cache.value;
        pthread_mutex_unlock(&cache_mutex);
        syslog(LOG_WARNING, "VAPIX: Serving stale temperature cache");
        return 0;
    }
    pthread_mutex_unlock(&cache_mutex);

    return -1;
}

int get_cached_device_info(DeviceInfo* info) {
    if (!info) {
        return -1;
    }

    pthread_mutex_lock(&cache_mutex);

    time_t now = time(NULL);

    /* Check if cache is valid and within TTL */
    if (device_info_cache.valid && (now - device_info_cache.timestamp) < device_info_cache.ttl_seconds) {
        memcpy(info, &device_info_cache.value, sizeof(DeviceInfo));
        pthread_mutex_unlock(&cache_mutex);
        return 0;
    }

    pthread_mutex_unlock(&cache_mutex);

    /* Fetch fresh data */
    DeviceInfo fresh_info;
    memset(&fresh_info, 0, sizeof(DeviceInfo));

    if (vapix_get_device_info(&fresh_info) == 0) {
        pthread_mutex_lock(&cache_mutex);
        memcpy(&device_info_cache.value, &fresh_info, sizeof(DeviceInfo));
        device_info_cache.timestamp = now;
        device_info_cache.valid     = 1;
        pthread_mutex_unlock(&cache_mutex);

        memcpy(info, &fresh_info, sizeof(DeviceInfo));
        return 0;
    }

    /* Try serving stale cache if available */
    pthread_mutex_lock(&cache_mutex);
    if (device_info_cache.valid) {
        memcpy(info, &device_info_cache.value, sizeof(DeviceInfo));
        pthread_mutex_unlock(&cache_mutex);
        syslog(LOG_WARNING, "VAPIX: Serving stale device info cache");
        return 0;
    }
    pthread_mutex_unlock(&cache_mutex);

    return -1;
}
