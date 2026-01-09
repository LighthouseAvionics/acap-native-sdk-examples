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

#ifndef I2C_LRF_H
#define I2C_LRF_H

#include <glib.h>

typedef struct {
    int fd;
    int bus_num;
    guint8 addr;
} LrfDevice;

LrfDevice* lrf_open(int bus_num, guint8 addr);
void lrf_close(LrfDevice* dev);
gboolean lrf_read_distance(LrfDevice* dev, float* distance_m);
gboolean lrf_send_command(LrfDevice* dev, guint8 cmd, guint8* response, gsize len);

#endif
