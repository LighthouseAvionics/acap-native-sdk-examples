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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>

#define I2C_TIMEOUT_MS 100      // 100ms timeout for I2C operations
#define I2C_RETRY_COUNT 1       // Number of retries for failed operations
#define I2C_ALARM_SECONDS 3     // Alarm timeout in seconds per operation

// Global flag for alarm timeout
static volatile sig_atomic_t i2c_timeout_flag = 0;

// Signal handler for alarm timeout
static void i2c_alarm_handler(int sig) {
    (void)sig;
    i2c_timeout_flag = 1;
}

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

// Raw I2C transaction using I2C_RDWR ioctl
static int raw_i2c_transaction(int fd, guint8 addr __attribute__((unused)), struct i2c_msg* msgs, int num_msgs) {
    struct i2c_rdwr_ioctl_data ioctl_data = {
        .msgs = msgs,
        .nmsgs = num_msgs
    };

    // Don't use alarm() here - it interferes with the ioctl
    // Instead rely on I2C_TIMEOUT which is set on the file descriptor
    int result = ioctl(fd, I2C_RDWR, &ioctl_data);

    return result;
}

// Raw I2C read: read N bytes from device
static int do_raw_read(int bus_num, guint8 addr, int count) {
    gchar bus_path[32];
    g_snprintf(bus_path, sizeof(bus_path), "/dev/i2c-%d", bus_num);

    int fd = open(bus_path, O_RDWR);
    if (fd < 0) {
        printf("Error: Failed to open %s: %s\n", bus_path, strerror(errno));
        return EXIT_FAILURE;
    }

    // Set I2C timeout and retries
    unsigned long timeout = (I2C_TIMEOUT_MS + 9) / 10;
    ioctl(fd, I2C_TIMEOUT, timeout);
    ioctl(fd, I2C_RETRIES, I2C_RETRY_COUNT);

    guint8 read_buf[256];
    if (count > 256) count = 256;

    struct i2c_msg msgs[1] = {
        {
            .addr = addr,
            .flags = I2C_M_RD,
            .len = count,
            .buf = read_buf
        }
    };

    int result = raw_i2c_transaction(fd, addr, msgs, 1);
    close(fd);

    if (result < 0) {
        printf("Error: Failed to read: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("Raw read from bus %d, addr 0x%02x (%d bytes):\n", bus_num, addr, count);
    for (int i = 0; i < count; i++) {
        printf("%02x ", read_buf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (count % 16 != 0) printf("\n");

    return EXIT_SUCCESS;
}

// Raw I2C write: write N bytes to device
static int do_raw_write(int bus_num, guint8 addr, guint8* data, int count) {
    gchar bus_path[32];
    g_snprintf(bus_path, sizeof(bus_path), "/dev/i2c-%d", bus_num);

    int fd = open(bus_path, O_RDWR);
    if (fd < 0) {
        printf("Error: Failed to open %s: %s\n", bus_path, strerror(errno));
        return EXIT_FAILURE;
    }

    // Set I2C timeout and retries
    unsigned long timeout = (I2C_TIMEOUT_MS + 9) / 10;
    ioctl(fd, I2C_TIMEOUT, timeout);
    ioctl(fd, I2C_RETRIES, I2C_RETRY_COUNT);

    struct i2c_msg msgs[1] = {
        {
            .addr = addr,
            .flags = 0,
            .len = count,
            .buf = data
        }
    };

    int result = raw_i2c_transaction(fd, addr, msgs, 1);
    close(fd);

    if (result < 0) {
        printf("Error: Failed to write: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("Raw write to bus %d, addr 0x%02x (%d bytes): ", bus_num, addr, count);
    for (int i = 0; i < count; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");

    return EXIT_SUCCESS;
}

// Raw I2C write-then-read: write register, then read N bytes with repeated start
static int do_raw_write_read(int bus_num, guint8 addr, guint8 reg, int count) {
    gchar bus_path[32];
    g_snprintf(bus_path, sizeof(bus_path), "/dev/i2c-%d", bus_num);

    int fd = open(bus_path, O_RDWR);
    if (fd < 0) {
        printf("Error: Failed to open %s: %s\n", bus_path, strerror(errno));
        return EXIT_FAILURE;
    }

    // Set I2C timeout and retries
    unsigned long timeout = (I2C_TIMEOUT_MS + 9) / 10;
    ioctl(fd, I2C_TIMEOUT, timeout);
    ioctl(fd, I2C_RETRIES, I2C_RETRY_COUNT);

    guint8 read_buf[256];
    if (count > 256) count = 256;

    struct i2c_msg msgs[2] = {
        {
            .addr = addr,
            .flags = 0,
            .len = 1,
            .buf = &reg
        },
        {
            .addr = addr,
            .flags = I2C_M_RD,
            .len = count,
            .buf = read_buf
        }
    };

    int result = raw_i2c_transaction(fd, addr, msgs, 2);
    close(fd);

    if (result < 0) {
        printf("Error: Failed write-read: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("Raw write-read from bus %d, addr 0x%02x, reg 0x%02x (%d bytes):\n",
           bus_num, addr, reg, count);
    for (int i = 0; i < count; i++) {
        printf("%02x ", read_buf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (count % 16 != 0) printf("\n");

    return EXIT_SUCCESS;
}

static void print_usage(const char* prog_name) {
    printf("I2C Test Utility\n");
    printf("Usage:\n");
    printf("  %s read BUS ADDR REG           - Read byte from register (SMBUS)\n", prog_name);
    printf("  %s write BUS ADDR REG VAL      - Write byte to register (SMBUS)\n", prog_name);
    printf("  %s dump BUS ADDR [START] [END] - Dump registers (default 0x00-0xFF)\n", prog_name);
    printf("  %s raw-read BUS ADDR COUNT     - Raw I2C read COUNT bytes\n", prog_name);
    printf("  %s raw-write BUS ADDR B0 [B1...] - Raw I2C write bytes\n", prog_name);
    printf("  %s raw-wr BUS ADDR REG COUNT   - Write REG, then read COUNT bytes\n", prog_name);
    printf("\nExamples:\n");
    printf("  %s read 0 0x54 0x01            - SMBUS read register 0x01\n", prog_name);
    printf("  %s raw-wr 0 0x54 0x13 4        - Write reg 0x13, read 4 bytes\n", prog_name);
    printf("  %s raw-write 0 0x54 0x28 0x00 0x00 0x00 0x00 - Write 5 bytes\n", prog_name);
    printf("  %s raw-read 0 0x54 4           - Read 4 bytes without register write\n", prog_name);
}

static int do_read(int bus_num, guint8 addr, guint8 reg) {
    gchar bus_path[32];
    g_snprintf(bus_path, sizeof(bus_path), "/dev/i2c-%d", bus_num);

    int fd = open(bus_path, O_RDWR);
    if (fd < 0) {
        printf("Error: Failed to open %s: %s\n", bus_path, strerror(errno));
        return EXIT_FAILURE;
    }

    // Set I2C timeout and retries
    unsigned long timeout = (I2C_TIMEOUT_MS + 9) / 10;
    ioctl(fd, I2C_TIMEOUT, timeout);
    ioctl(fd, I2C_RETRIES, I2C_RETRY_COUNT);

    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        printf("Error: Failed to set I2C slave address 0x%02x: %s\n", addr, strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    // Setup alarm handler
    struct sigaction sa;
    sa.sa_handler = i2c_alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    // Set alarm for this operation
    i2c_timeout_flag = 0;
    alarm(I2C_ALARM_SECONDS);

    __s32 result = i2c_smbus_read_byte_data(fd, reg);

    // Cancel alarm
    alarm(0);
    signal(SIGALRM, SIG_DFL);
    close(fd);

    if (result < 0 || i2c_timeout_flag) {
        if (i2c_timeout_flag) {
            printf("Error: Timeout reading from register 0x%02x\n", reg);
        } else {
            printf("Error: Failed to read from register 0x%02x: %s\n", reg, strerror(errno));
        }
        return EXIT_FAILURE;
    }

    printf("Read from bus %d, addr 0x%02x, reg 0x%02x: 0x%02x (%d)\n",
           bus_num, addr, reg, result, result);

    return EXIT_SUCCESS;
}

static int do_write(int bus_num, guint8 addr, guint8 reg, guint8 value) {
    gchar bus_path[32];
    g_snprintf(bus_path, sizeof(bus_path), "/dev/i2c-%d", bus_num);

    int fd = open(bus_path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        printf("Error: Failed to open %s: %s\n", bus_path, strerror(errno));
        return EXIT_FAILURE;
    }

    // Set I2C timeout and retries to prevent hanging
    unsigned long timeout = (I2C_TIMEOUT_MS + 9) / 10;  // Convert ms to 10ms units
    ioctl(fd, I2C_TIMEOUT, timeout);
    ioctl(fd, I2C_RETRIES, I2C_RETRY_COUNT);

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

    // Set I2C timeout and retries
    unsigned long timeout = (I2C_TIMEOUT_MS + 9) / 10;
    ioctl(fd, I2C_TIMEOUT, timeout);
    ioctl(fd, I2C_RETRIES, I2C_RETRY_COUNT);

    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        printf("Error: Failed to set I2C slave address 0x%02x: %s\n", addr, strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    // Setup alarm handler
    struct sigaction sa;
    sa.sa_handler = i2c_alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    printf("Dumping registers 0x%02x-0x%02x from bus %d, addr 0x%02x:\n",
           start_reg, end_reg, bus_num, addr);
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    printf("(XX = read error/timeout)\n");

    int consecutive_errors = 0;
    for (int reg = start_reg; reg <= end_reg; reg++) {
        if ((reg % 16) == 0) {
            printf("%02x: ", reg);
            fflush(stdout);
        }

        // Set alarm for this operation
        i2c_timeout_flag = 0;
        alarm(I2C_ALARM_SECONDS);

        __s32 result = i2c_smbus_read_byte_data(fd, reg);

        // Cancel alarm
        alarm(0);

        if (result < 0 || i2c_timeout_flag) {
            if (i2c_timeout_flag) {
                printf("TO ");  // Timeout
                syslog(LOG_WARNING, "I2C read timeout on register 0x%02x", reg);
            } else {
                printf("XX ");  // Error
            }
            consecutive_errors++;

            if (consecutive_errors > 32) {
                printf("\n\nWarning: Too many consecutive errors, stopping dump.\n");
                break;
            }
        } else {
            printf("%02x ", result);
            consecutive_errors = 0;
        }

        if ((reg % 16) == 15) {
            printf("\n");
            fflush(stdout);
        }
    }

    if ((end_reg % 16) != 15) {
        printf("\n");
    }

    // Restore default signal handler
    signal(SIGALRM, SIG_DFL);
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
    } else if (g_strcmp0(cmd, "raw-read") == 0 && argc >= 5) {
        int count = atoi(argv[4]);
        return do_raw_read(bus_num, addr, count);
    } else if (g_strcmp0(cmd, "raw-write") == 0 && argc >= 5) {
        guint8 data[256];
        int count = argc - 4;
        if (count > 256) count = 256;
        for (int i = 0; i < count; i++) {
            data[i] = (guint8)strtol(argv[4 + i], NULL, 0);
        }
        return do_raw_write(bus_num, addr, data, count);
    } else if (g_strcmp0(cmd, "raw-wr") == 0 && argc >= 6) {
        guint8 reg = (guint8)strtol(argv[4], NULL, 0);
        int count = atoi(argv[5]);
        return do_raw_write_read(bus_num, addr, reg, count);
    } else {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
}
