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

#include "i2c_lrf.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>

static __inline__ gint32 i2c_smbus_read_i2c_block_data(int fd,
                                                        guint8 command,
                                                        guint8 length,
                                                        guint8* values) {
    union i2c_smbus_data data;
    int i, err;

    if (length > I2C_SMBUS_BLOCK_MAX) {
        length = I2C_SMBUS_BLOCK_MAX;
    }

    data.block[0] = length;

    struct i2c_smbus_ioctl_data args;
    args.read_write = I2C_SMBUS_READ;
    args.command    = command;
    args.size       = I2C_SMBUS_I2C_BLOCK_DATA;
    args.data       = &data;

    err = ioctl(fd, I2C_SMBUS, &args);
    if (err < 0) {
        return err;
    }

    for (i = 1; i <= data.block[0]; i++) {
        values[i - 1] = data.block[i];
    }

    return data.block[0];
}

static __inline__ gint32 i2c_smbus_write_byte(int fd, guint8 value) {
    union i2c_smbus_data data;
    struct i2c_smbus_ioctl_data args;

    args.read_write = I2C_SMBUS_WRITE;
    args.command    = value;
    args.size       = I2C_SMBUS_BYTE;
    args.data       = &data;

    return ioctl(fd, I2C_SMBUS, &args);
}

LrfDevice* lrf_open(int bus_num, guint8 addr) {
    gchar bus_path[256];
    int fd;

    g_snprintf(bus_path, sizeof(bus_path), "/dev/i2c-%d", bus_num);
    fd = open(bus_path, O_RDWR);

    if (fd < 0) {
        syslog(LOG_ERR, "Failed to open %s: %s", bus_path, strerror(errno));
        return NULL;
    }

    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        syslog(LOG_ERR, "Failed to set I2C slave address 0x%02x: %s", addr, strerror(errno));
        close(fd);
        return NULL;
    }

    LrfDevice* dev = g_malloc(sizeof(LrfDevice));
    dev->fd        = fd;
    dev->bus_num   = bus_num;
    dev->addr      = addr;

    syslog(LOG_INFO, "Opened LRF device on bus %d at address 0x%02x", bus_num, addr);
    return dev;
}

void lrf_close(LrfDevice* dev) {
    if (dev) {
        if (dev->fd >= 0) {
            close(dev->fd);
        }
        g_free(dev);
    }
}

gboolean lrf_read_distance(LrfDevice* dev, float* distance_m) {
    if (!dev || dev->fd < 0) {
        return FALSE;
    }

    guint8 buffer[4];
    gint32 result = i2c_smbus_read_i2c_block_data(dev->fd, 0x00, 4, buffer);

    if (result < 0) {
        syslog(LOG_WARNING, "Failed to read distance from LRF: %s", strerror(errno));
        return FALSE;
    }

    if (result < 4) {
        syslog(LOG_WARNING, "Insufficient data read from LRF: got %d bytes, expected 4", result);
        return FALSE;
    }

    guint32 distance_mm = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
    *distance_m         = distance_mm / 1000.0f;

    return TRUE;
}

gboolean lrf_send_command(LrfDevice* dev, guint8 cmd, guint8* response, gsize len) {
    if (!dev || dev->fd < 0) {
        return FALSE;
    }

    if (i2c_smbus_write_byte(dev->fd, cmd) < 0) {
        syslog(LOG_WARNING, "Failed to send command 0x%02x to LRF: %s", cmd, strerror(errno));
        return FALSE;
    }

    usleep(50000);

    if (response && len > 0) {
        gint32 result = i2c_smbus_read_i2c_block_data(dev->fd, 0x00, len, response);
        if (result < 0) {
            syslog(LOG_WARNING, "Failed to read response from LRF: %s", strerror(errno));
            return FALSE;
        }
    }

    return TRUE;
}
