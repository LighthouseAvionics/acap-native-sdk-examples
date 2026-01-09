#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <errno.h>
#include <string.h>

static inline int i2c_smbus_write_quick(int fd, unsigned char value) {
    union i2c_smbus_data data;
    struct i2c_smbus_ioctl_data args;

    args.read_write = value;
    args.command = 0;
    args.size = I2C_SMBUS_QUICK;
    args.data = &data;

    return ioctl(fd, I2C_SMBUS, &args);
}

int main() {
    int fd = open("/dev/i2c-0", O_RDWR);
    if (fd < 0) {
        printf("Failed to open /dev/i2c-0: %s\n", strerror(errno));
        return 1;
    }

    printf("Testing I2C probe on bus 0:\n");

    // Test address 0x32 (should be kernel-bound rx8130)
    printf("Testing 0x32 (rx8130 - kernel bound): ");
    if (ioctl(fd, I2C_SLAVE, 0x32) < 0) {
        printf("Failed to set slave: %s\n", strerror(errno));
    } else {
        int result = i2c_smbus_write_quick(fd, I2C_SMBUS_WRITE);
        if (result == 0) {
            printf("RESPONDS\n");
        } else {
            printf("No response: %s\n", strerror(errno));
        }
    }

    // Test address 0x10 (should be empty)
    printf("Testing 0x10 (should be empty): ");
    if (ioctl(fd, I2C_SLAVE, 0x10) < 0) {
        printf("Failed to set slave: %s\n", strerror(errno));
    } else {
        int result = i2c_smbus_write_quick(fd, I2C_SMBUS_WRITE);
        if (result == 0) {
            printf("RESPONDS (unexpected!)\n");
        } else {
            printf("No response: %s (expected)\n", strerror(errno));
        }
    }

    close(fd);
    return 0;
}
