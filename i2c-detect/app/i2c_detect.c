/**
 * Copyright (C) 2024, Axis Communications AB, Lund, Sweden
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

/**
 * - i2c_detect -
 *
 * This application scans I2C buses for connected devices and logs the results.
 * Similar to the i2cdetect utility in i2c-tools.
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>

#define I2C_BUS_MAX 10
#define SCAN_START 0x03
#define SCAN_END 0x77

/* Device status flags */
#define DEV_NOT_FOUND 0
#define DEV_FOUND_PROBE 1      /* Found via I2C probe */
#define DEV_FOUND_SYSFS 2      /* Found in sysfs (kernel-bound) */

/**
 * @brief Checks if a device exists in sysfs (kernel-bound device)
 *
 * @param bus_num I2C bus number
 * @param addr I2C device address
 *
 * @return TRUE if device exists in sysfs, FALSE otherwise
 */
static gboolean i2c_device_in_sysfs(int bus_num, guint8 addr) {
    gchar sysfs_path[256];
    g_snprintf(sysfs_path, sizeof(sysfs_path), "/sys/bus/i2c/devices/%d-%04x", bus_num, addr);
    return g_file_test(sysfs_path, G_FILE_TEST_EXISTS);
}

/**
 * @brief Reads device name from sysfs
 *
 * @param bus_num I2C bus number
 * @param addr I2C device address
 *
 * @return Device name or NULL if not found
 */
static gchar* i2c_get_device_name(int bus_num, guint8 addr) {
    gchar sysfs_path[256];
    gchar* contents = NULL;
    gchar* trimmed = NULL;

    g_snprintf(sysfs_path, sizeof(sysfs_path), "/sys/bus/i2c/devices/%d-%04x/name", bus_num, addr);

    if (g_file_get_contents(sysfs_path, &contents, NULL, NULL)) {
        trimmed = g_strchomp(contents);  /* Remove trailing newline */
        return trimmed;
    }

    return NULL;
}

/**
 * @brief Checks if an I2C bus device file exists
 *
 * @param bus_num I2C bus number
 *
 * @return TRUE if device exists, FALSE otherwise
 */
static gboolean i2c_bus_exists(int bus_num) {
    gchar bus_path[256];
    g_snprintf(bus_path, sizeof(bus_path), "/dev/i2c-%d", bus_num);
    return g_file_test(bus_path, G_FILE_TEST_EXISTS);
}

/**
 * @brief Opens an I2C bus device
 *
 * @param bus_num I2C bus number
 *
 * @return File descriptor or -1 on error
 */
static int i2c_open_bus(int bus_num) {
    gchar bus_path[256];
    int fd;

    g_snprintf(bus_path, sizeof(bus_path), "/dev/i2c-%d", bus_num);
    fd = open(bus_path, O_RDWR);

    if (fd < 0) {
        syslog(LOG_WARNING, "Failed to open %s: %s", bus_path, strerror(errno));
    }

    return fd;
}

/**
 * @brief Inline implementation of I2C SMBUS write quick
 *
 * @param fd File descriptor for the I2C bus
 * @param value Write value (0 or 1)
 *
 * @return 0 on success, -1 on error
 */
static __inline__ gint32 i2c_smbus_write_quick(int fd, guint8 value) {
    union i2c_smbus_data data;
    struct i2c_smbus_ioctl_data args;

    args.read_write = value;
    args.command    = 0;
    args.size       = I2C_SMBUS_QUICK;
    args.data       = &data;

    return ioctl(fd, I2C_SMBUS, &args);
}

/**
 * @brief Attempts to detect a device at the specified I2C address
 *
 * This function uses I2C_SMBUS quick write command to detect devices.
 * The quick write is the safest way to probe devices as it doesn't
 * actually read or write any data.
 *
 * @param fd File descriptor for the I2C bus
 * @param addr I2C address to probe
 *
 * @return TRUE if device detected, FALSE otherwise
 */
static gboolean i2c_probe_address(int fd, guint8 addr) {
    /* Use I2C_SLAVE to set the slave address */
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        return FALSE;
    }

    /* Try a quick write - safest way to detect device presence */
    if (i2c_smbus_write_quick(fd, I2C_SMBUS_WRITE) < 0) {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Scans an I2C bus for connected devices
 *
 * @param bus_num I2C bus number to scan
 */
static void scan_i2c_bus(int bus_num) {
    int fd;
    guint8 addr;
    GString* scan_result;
    GString* line = NULL;
    GString* device_list = NULL;
    int devices_found = 0;
    int kernel_devices = 0;
    guint8 device_status[0x80];

    syslog(LOG_INFO, "Scanning I2C bus %d...", bus_num);

    /* Initialize device status array */
    memset(device_status, DEV_NOT_FOUND, sizeof(device_status));

    /* First, check for kernel-bound devices in sysfs */
    for (addr = SCAN_START; addr <= SCAN_END; addr++) {
        if (i2c_device_in_sysfs(bus_num, addr)) {
            device_status[addr] = DEV_FOUND_SYSFS;
            kernel_devices++;
        }
    }

    /* Then try probing devices not already found */
    fd = i2c_open_bus(bus_num);
    if (fd >= 0) {
        for (addr = SCAN_START; addr <= SCAN_END; addr++) {
            if (device_status[addr] == DEV_NOT_FOUND && i2c_probe_address(fd, addr)) {
                device_status[addr] = DEV_FOUND_PROBE;
                devices_found++;
            }
        }
        close(fd);
    }

    /* Build the scan result grid */
    scan_result = g_string_new("");
    g_string_append_printf(scan_result, "\n=== I2C Bus %d Scan Results ===\n", bus_num);
    g_string_append(scan_result, "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");

    for (addr = 0; addr < 0x80; addr++) {
        if (addr % 16 == 0) {
            if (addr > 0) {
                syslog(LOG_INFO, "%s", line->str);
                g_string_append(scan_result, line->str);
                g_string_append(scan_result, "\n");
                g_string_free(line, TRUE);
            }
            line = g_string_new("");
            g_string_append_printf(line, "%02x: ", addr);
        }

        if (addr < SCAN_START || addr > SCAN_END) {
            g_string_append(line, "   ");
        } else if (device_status[addr] == DEV_FOUND_SYSFS) {
            g_string_append_printf(line, "UU ");  /* UU = in use by kernel driver */
        } else if (device_status[addr] == DEV_FOUND_PROBE) {
            g_string_append_printf(line, "%02x ", addr);
        } else {
            g_string_append(line, "-- ");
        }
    }

    if (line != NULL) {
        syslog(LOG_INFO, "%s", line->str);
        g_string_append(scan_result, line->str);
        g_string_append(scan_result, "\n");
        g_string_free(line, TRUE);
    }

    g_string_append(scan_result, "================================\n");
    syslog(LOG_INFO, "%s", scan_result->str);

    /* List devices with names */
    if (kernel_devices > 0) {
        device_list = g_string_new("Kernel-bound devices:\n");
        for (addr = SCAN_START; addr <= SCAN_END; addr++) {
            if (device_status[addr] == DEV_FOUND_SYSFS) {
                gchar* dev_name = i2c_get_device_name(bus_num, addr);
                if (dev_name) {
                    g_string_append_printf(device_list, "  0x%02x: %s\n", addr, dev_name);
                    g_free(dev_name);
                } else {
                    g_string_append_printf(device_list, "  0x%02x: (unknown)\n", addr);
                }
            }
        }
        syslog(LOG_INFO, "%s", device_list->str);
        g_string_free(device_list, TRUE);
    }

    syslog(LOG_INFO, "Found %d device(s) via probe, %d kernel-bound device(s) on I2C bus %d",
           devices_found, kernel_devices, bus_num);

    g_string_free(scan_result, TRUE);
}

/**
 * @brief Main function
 *
 * Scans all available I2C buses for connected devices
 */
int main(void) {
    int bus_num;
    int buses_found = 0;

    openlog("i2c_detect", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Starting I2C Detect application");

    /* Scan for available I2C buses */
    for (bus_num = 0; bus_num < I2C_BUS_MAX; bus_num++) {
        if (i2c_bus_exists(bus_num)) {
            buses_found++;
            scan_i2c_bus(bus_num);
        }
    }

    if (buses_found == 0) {
        syslog(LOG_WARNING, "No I2C buses found on this system");
        syslog(LOG_INFO, "Note: I2C functionality may require specific hardware or kernel modules");
    } else {
        syslog(LOG_INFO, "Scan complete. Found %d I2C bus(es)", buses_found);
    }

    syslog(LOG_INFO, "I2C Detect application finished");

    return EXIT_SUCCESS;
}
