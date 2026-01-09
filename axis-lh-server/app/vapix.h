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

#ifndef VAPIX_H
#define VAPIX_H

#include <time.h>

/**
 * Device information structure
 */
typedef struct {
    char serial_number[64];
    char firmware_version[64];
    char model[64];
    char architecture[32];
    char soc[64];
} DeviceInfo;

/**
 * Initialize VAPIX client
 * Acquires credentials via D-Bus and initializes libcurl
 * @return 0 on success, -1 on error
 */
int vapix_init(void);

/**
 * Cleanup VAPIX client resources
 * Zeros credentials and cleans up libcurl
 */
void vapix_cleanup(void);

/**
 * Get cached temperature value
 * Returns cached value if valid and within TTL (60s),
 * otherwise fetches fresh value from VAPIX API
 * @param temperature pointer to store temperature value
 * @return 0 on success, -1 on error
 */
int get_cached_temperature(double* temperature);

/**
 * Get cached device information
 * Returns cached value if valid and within TTL (300s),
 * otherwise fetches fresh value from VAPIX API
 * @param info pointer to DeviceInfo structure to fill
 * @return 0 on success, -1 on error
 */
int get_cached_device_info(DeviceInfo* info);

#endif /* VAPIX_H */
