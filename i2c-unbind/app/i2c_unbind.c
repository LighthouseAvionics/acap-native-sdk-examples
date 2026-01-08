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

static gboolean i2c_device_in_sysfs(int bus_num, guint8 addr) {
    gchar sysfs_path[256];
    g_snprintf(sysfs_path, sizeof(sysfs_path), "/sys/bus/i2c/devices/%d-%04x", bus_num, addr);
    return g_file_test(sysfs_path, G_FILE_TEST_EXISTS);
}

static gchar* i2c_get_device_name(int bus_num, guint8 addr) {
    gchar sysfs_path[256];
    gchar* contents = NULL;

    g_snprintf(sysfs_path, sizeof(sysfs_path), "/sys/bus/i2c/devices/%d-%04x/name", bus_num, addr);

    if (g_file_get_contents(sysfs_path, &contents, NULL, NULL)) {
        return g_strchomp(contents);
    }

    return NULL;
}

static gchar* i2c_get_driver_name(int bus_num, guint8 addr) {
    gchar sysfs_path[256];
    gchar link_path[512];
    ssize_t len;

    g_snprintf(sysfs_path, sizeof(sysfs_path), "/sys/bus/i2c/devices/%d-%04x/driver", bus_num, addr);

    len = readlink(sysfs_path, link_path, sizeof(link_path) - 1);
    if (len > 0) {
        link_path[len] = '\0';
        gchar* driver_name = g_path_get_basename(link_path);
        return driver_name;
    }

    return NULL;
}

static gboolean unbind_device(int bus_num, guint8 addr) {
    gchar* driver_name = i2c_get_driver_name(bus_num, addr);
    if (!driver_name) {
        syslog(LOG_WARNING, "Device %d-%04x has no driver bound", bus_num, addr);
        printf("Device %d-%04x has no driver bound\n", bus_num, addr);
        return FALSE;
    }

    gchar unbind_path[512];
    g_snprintf(unbind_path, sizeof(unbind_path), "/sys/bus/i2c/drivers/%s/unbind", driver_name);

    gchar device_id[32];
    g_snprintf(device_id, sizeof(device_id), "%d-%04x", bus_num, addr);

    // sysfs requires direct write, not g_file_set_contents which tries to create temp file
    int fd = open(unbind_path, O_WRONLY);
    if (fd < 0) {
        gchar* err_msg = g_strdup_printf("Failed to open %s: %s", unbind_path, strerror(errno));
        syslog(LOG_ERR, "%s", err_msg);
        printf("%s\n", err_msg);
        g_free(err_msg);
        g_free(driver_name);
        return FALSE;
    }

    ssize_t len = strlen(device_id);
    ssize_t written = write(fd, device_id, len);
    close(fd);

    gboolean success = (written == len);

    if (success) {
        syslog(LOG_INFO, "Unbound device %s from driver %s", device_id, driver_name);
    } else {
        gchar* err_msg = g_strdup_printf("Failed to unbind device %s from driver %s: %s",
                                         device_id, driver_name,
                                         written < 0 ? strerror(errno) : "incomplete write");
        syslog(LOG_ERR, "%s", err_msg);
        printf("%s\n", err_msg);
        g_free(err_msg);
    }

    g_free(driver_name);
    return success;
}

static gboolean rebind_device(int bus_num, guint8 addr, const gchar* driver_name) {
    gchar bind_path[512];
    g_snprintf(bind_path, sizeof(bind_path), "/sys/bus/i2c/drivers/%s/bind", driver_name);

    gchar device_id[32];
    g_snprintf(device_id, sizeof(device_id), "%d-%04x", bus_num, addr);

    // sysfs requires direct write, not g_file_set_contents which tries to create temp file
    int fd = open(bind_path, O_WRONLY);
    if (fd < 0) {
        gchar* err_msg = g_strdup_printf("Failed to open %s: %s", bind_path, strerror(errno));
        syslog(LOG_ERR, "%s", err_msg);
        printf("%s\n", err_msg);
        g_free(err_msg);
        return FALSE;
    }

    ssize_t len = strlen(device_id);
    ssize_t written = write(fd, device_id, len);
    close(fd);

    gboolean success = (written == len);

    if (success) {
        syslog(LOG_INFO, "Bound device %s to driver %s", device_id, driver_name);
    } else {
        gchar* err_msg = g_strdup_printf("Failed to bind device %s to driver %s: %s",
                                         device_id, driver_name,
                                         written < 0 ? strerror(errno) : "incomplete write");
        syslog(LOG_ERR, "%s", err_msg);
        printf("%s\n", err_msg);
        g_free(err_msg);
    }

    return success;
}

static void list_bound_devices(void) {
    int bus_num;
    guint8 addr;
    int found = 0;

    syslog(LOG_INFO, "=== Kernel-Bound I2C Devices ===");
    printf("=== Kernel-Bound I2C Devices ===\n");

    for (bus_num = 0; bus_num < I2C_BUS_MAX; bus_num++) {
        for (addr = SCAN_START; addr <= SCAN_END; addr++) {
            if (i2c_device_in_sysfs(bus_num, addr)) {
                gchar* dev_name = i2c_get_device_name(bus_num, addr);
                gchar* driver_name = i2c_get_driver_name(bus_num, addr);

                gchar info[256];
                g_snprintf(info,
                           sizeof(info),
                           "Bus %d, Addr 0x%02x: %s (driver: %s)",
                           bus_num,
                           addr,
                           dev_name ? dev_name : "unknown",
                           driver_name ? driver_name : "none");

                syslog(LOG_INFO, "%s", info);
                printf("%s\n", info);

                g_free(dev_name);
                g_free(driver_name);
                found++;
            }
        }
    }

    gchar summary[128];
    g_snprintf(summary, sizeof(summary), "Found %d kernel-bound I2C device(s)", found);
    syslog(LOG_INFO, "%s", summary);
    printf("%s\n", summary);
}

static void unbind_all_on_bus(int bus_num) {
    guint8 addr;
    int unbound = 0;

    syslog(LOG_INFO, "Unbinding all devices on bus %d", bus_num);

    for (addr = SCAN_START; addr <= SCAN_END; addr++) {
        if (i2c_device_in_sysfs(bus_num, addr)) {
            if (unbind_device(bus_num, addr)) {
                unbound++;
            }
        }
    }

    syslog(LOG_INFO, "Unbound %d device(s) on bus %d", unbound, bus_num);
}

int main(int argc, char* argv[]) {
    syslog(LOG_INFO, "Starting I2C Unbind utility");

    if (argc < 2) {
        printf("Usage:\n");
        printf("  %s list              - List all kernel-bound I2C devices\n", argv[0]);
        printf("  %s unbind BUS ADDR   - Unbind device at BUS-ADDR (hex)\n", argv[0]);
        printf("  %s rebind BUS ADDR DRIVER - Rebind device to DRIVER\n", argv[0]);
        printf("  %s unbind-bus BUS    - Unbind all devices on BUS\n", argv[0]);
        printf("\nExamples:\n");
        printf("  %s list\n", argv[0]);
        printf("  %s unbind 8 0x52\n", argv[0]);
        printf("  %s rebind 8 0x52 motor\n", argv[0]);
        printf("  %s unbind-bus 8\n", argv[0]);
        return EXIT_SUCCESS;
    }

    if (g_strcmp0(argv[1], "list") == 0) {
        list_bound_devices();
    } else if (g_strcmp0(argv[1], "unbind") == 0 && argc >= 4) {
        int bus_num = atoi(argv[2]);
        guint8 addr = (guint8)strtol(argv[3], NULL, 0);

        if (unbind_device(bus_num, addr)) {
            printf("Successfully unbound device %d-0x%02x\n", bus_num, addr);
        } else {
            printf("Failed to unbind device %d-0x%02x\n", bus_num, addr);
            return EXIT_FAILURE;
        }
    } else if (g_strcmp0(argv[1], "rebind") == 0 && argc >= 5) {
        int bus_num = atoi(argv[2]);
        guint8 addr = (guint8)strtol(argv[3], NULL, 0);
        const gchar* driver = argv[4];

        if (rebind_device(bus_num, addr, driver)) {
            printf("Successfully bound device %d-0x%02x to driver %s\n", bus_num, addr, driver);
        } else {
            printf("Failed to bind device %d-0x%02x to driver %s\n", bus_num, addr, driver);
            return EXIT_FAILURE;
        }
    } else if (g_strcmp0(argv[1], "unbind-bus") == 0 && argc >= 3) {
        int bus_num = atoi(argv[2]);
        unbind_all_on_bus(bus_num);
        printf("Unbound all devices on bus %d\n", bus_num);
    } else {
        printf("Invalid command. Run without arguments to see usage.\n");
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "I2C Unbind utility finished");
    return EXIT_SUCCESS;
}
