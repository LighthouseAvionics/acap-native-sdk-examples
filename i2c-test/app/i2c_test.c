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

// Inline SMBUS functions (from i2c-detect pattern)
static inline __s32 i2c_smbus_access(int file, char read_write, __u8 command,
                                      int size, union i2c_smbus_data *data)
{
    struct i2c_smbus_ioctl_data args;
    args.read_write = read_write;
    args.command = command;
    args.size = size;
    args.data = data;
    return ioctl(file, I2C_SMBUS, &args);
}

static inline __s32 i2c_smbus_read_byte_data(int file, __u8 command)
{
    union i2c_smbus_data data;
    if (i2c_smbus_access(file, I2C_SMBUS_READ, command, I2C_SMBUS_BYTE_DATA, &data))
        return -1;
    else
        return 0x0FF & data.byte;
}

static inline __s32 i2c_smbus_write_byte_data(int file, __u8 command, __u8 value)
{
    union i2c_smbus_data data;
    data.byte = value;
    return i2c_smbus_access(file, I2C_SMBUS_WRITE, command, I2C_SMBUS_BYTE_DATA, &data);
}

static inline __s32 i2c_smbus_read_i2c_block_data(int file, __u8 command, __u8 length, __u8 *values)
{
    union i2c_smbus_data data;
    int i;
    if (length > I2C_SMBUS_BLOCK_MAX)
        length = I2C_SMBUS_BLOCK_MAX;
    data.block[0] = length;
    if (i2c_smbus_access(file, I2C_SMBUS_READ, command, I2C_SMBUS_I2C_BLOCK_DATA, &data))
        return -1;
    else {
        for (i = 1; i <= data.block[0]; i++)
            values[i-1] = data.block[i];
        return data.block[0];
    }
}

static void print_usage(const char* prog_name) {
    printf("I2C Test Utility\n");
    printf("Usage:\n");
    printf("  %s read BUS ADDR REG           - Read byte from register\n", prog_name);
    printf("  %s write BUS ADDR REG VAL      - Write byte to register\n", prog_name);
    printf("  %s dump BUS ADDR [START] [END] - Dump registers (default 0x00-0xFF)\n", prog_name);
    printf("\nExamples:\n");
    printf("  %s read 0 0x54 0x00            - Read register 0x00 from device 0x54 on bus 0\n", prog_name);
    printf("  %s write 0 0x54 0x10 0xAB      - Write 0xAB to register 0x10\n", prog_name);
    printf("  %s dump 0 0x54                 - Dump all registers from device 0x54\n", prog_name);
    printf("  %s dump 0 0x54 0x00 0x0F       - Dump registers 0x00 to 0x0F\n", prog_name);
}

static int do_read(int bus_num, guint8 addr, guint8 reg) {
    gchar bus_path[32];
    g_snprintf(bus_path, sizeof(bus_path), "/dev/i2c-%d", bus_num);

    int fd = open(bus_path, O_RDWR);
    if (fd < 0) {
        printf("Error: Failed to open %s: %s\n", bus_path, strerror(errno));
        return EXIT_FAILURE;
    }

    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        printf("Error: Failed to set I2C slave address 0x%02x: %s\n", addr, strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    __s32 result = i2c_smbus_read_byte_data(fd, reg);
    close(fd);

    if (result < 0) {
        printf("Error: Failed to read from register 0x%02x: %s\n", reg, strerror(errno));
        return EXIT_FAILURE;
    }

    printf("Read from bus %d, addr 0x%02x, reg 0x%02x: 0x%02x (%d)\n",
           bus_num, addr, reg, result, result);

    return EXIT_SUCCESS;
}

static int do_write(int bus_num, guint8 addr, guint8 reg, guint8 value) {
    gchar bus_path[32];
    g_snprintf(bus_path, sizeof(bus_path), "/dev/i2c-%d", bus_num);

    int fd = open(bus_path, O_RDWR);
    if (fd < 0) {
        printf("Error: Failed to open %s: %s\n", bus_path, strerror(errno));
        return EXIT_FAILURE;
    }

    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        printf("Error: Failed to set I2C slave address 0x%02x: %s\n", addr, strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    __s32 result = i2c_smbus_write_byte_data(fd, reg, value);
    close(fd);

    if (result < 0) {
        printf("Error: Failed to write to register 0x%02x: %s\n", reg, strerror(errno));
        return EXIT_FAILURE;
    }

    printf("Wrote to bus %d, addr 0x%02x, reg 0x%02x: 0x%02x (%d)\n",
           bus_num, addr, reg, value, value);

    return EXIT_SUCCESS;
}

static int do_dump(int bus_num, guint8 addr, guint8 start_reg, guint8 end_reg) {
    gchar bus_path[32];
    g_snprintf(bus_path, sizeof(bus_path), "/dev/i2c-%d", bus_num);

    int fd = open(bus_path, O_RDWR);
    if (fd < 0) {
        printf("Error: Failed to open %s: %s\n", bus_path, strerror(errno));
        return EXIT_FAILURE;
    }

    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        printf("Error: Failed to set I2C slave address 0x%02x: %s\n", addr, strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Dumping registers 0x%02x-0x%02x from bus %d, addr 0x%02x:\n",
           start_reg, end_reg, bus_num, addr);
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");

    for (int reg = start_reg; reg <= end_reg; reg++) {
        if ((reg % 16) == 0) {
            printf("%02x: ", reg);
        }

        __s32 result = i2c_smbus_read_byte_data(fd, reg);
        if (result < 0) {
            printf("XX ");
        } else {
            printf("%02x ", result);
        }

        if ((reg % 16) == 15) {
            printf("\n");
        }
    }

    if ((end_reg % 16) != 15) {
        printf("\n");
    }

    close(fd);
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    openlog("i2c_test", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Starting I2C test utility");

    if (argc < 4) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const gchar* cmd = argv[1];
    int bus_num = atoi(argv[2]);
    guint8 addr = (guint8)strtol(argv[3], NULL, 0);

    if (g_strcmp0(cmd, "read") == 0 && argc >= 5) {
        guint8 reg = (guint8)strtol(argv[4], NULL, 0);
        return do_read(bus_num, addr, reg);
    } else if (g_strcmp0(cmd, "write") == 0 && argc >= 6) {
        guint8 reg = (guint8)strtol(argv[4], NULL, 0);
        guint8 value = (guint8)strtol(argv[5], NULL, 0);
        return do_write(bus_num, addr, reg, value);
    } else if (g_strcmp0(cmd, "dump") == 0) {
        guint8 start_reg = 0x00;
        guint8 end_reg = 0xFF;
        if (argc >= 5) {
            start_reg = (guint8)strtol(argv[4], NULL, 0);
        }
        if (argc >= 6) {
            end_reg = (guint8)strtol(argv[5], NULL, 0);
        }
        return do_dump(bus_num, addr, start_reg, end_reg);
    } else {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
}
