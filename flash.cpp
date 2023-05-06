#include <cstdio>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#define DRY_RUN
// #define SERIAL_DEBUG
// #define VERIFY_ADDRESS_ONLY

#define FLASH_ADDRESS 0x1080000
#define HEX_PREFIX    ""
#define LINE_FEED     "\r"
#define READ_TIMEOUT  10

#if defined(DRY_RUN) && !defined(SERIAL_DEBUG)
    #define SERIAL_DEBUG
#endif

bool parseHex(const char *s, uint32_t *r) {
    *r = 0;
    for (uint32_t i = 0; i < 8; ++i, ++s) {
        // Shift result
        *r <<= 4;

        // Check for digit
        if ((*s >= '0') && (*s <= '9')) {
            *r += *s - '0';
        }
        // Check for lower hex digit
        else if ((*s >= 'a') && (*s <= 'f')) {
            *r += *s - 'a' + 10;
        }
        // Check for upper hex digit
        else if ((*s >= 'A') && (*s <= 'F')) {
            *r += *s - 'A' + 10;
        } else {
            // Invalid char
            return false;
        }
    }
    return true;
}

bool processLine(uint32_t *address_ref, uint32_t *value_ref, char *line_buffer) {
    // Check if line is empty
    if (*line_buffer == 0) {
        return true;
    }

    // Check for write command
    if (strncmp(line_buffer, "mw.q ", 4) == 0) {
        return true;
    }

    // Check for dump command
    if (strncmp(line_buffer, "md.l ", 4) == 0) {
        return true;
    }

    // Check for device name
    if (strncmp(line_buffer, "axg_s420_v1_gva#", 16) == 0) {
        return true;
    }

    // Check string length
    if (strlen(line_buffer) != 65) {
        printf("ERROR: Invalid result line => (%s) => (%d)\n", line_buffer, strlen(line_buffer));
        return false;
    }

    // Check address
    uint32_t tmp;
    if (!parseHex(line_buffer, &tmp)) {
        printf("ERROR: Failed to parse flash address => (%s) => (%d)\n", line_buffer, strlen(line_buffer));
        return false;
    }
    if (tmp != *address_ref) {
        printf("ERROR: Invalid flash address. Expected (0x%X), got (0x%X) => (%s) => (%d)\n",
               *address_ref,
               tmp,
               line_buffer,
               strlen(line_buffer));
        return false;
    }

#if !defined(VERIFY_ADDRESS_ONLY)
    // Check first dword
    if (!parseHex(line_buffer + 10, &tmp)) {
        printf("ERROR: Failed to parse first dword => (%s) => (%d)\n", line_buffer, strlen(line_buffer));
        return false;
    }
    if (tmp != value_ref[0]) {
        printf("ERROR: Incorrect first dword. Expected (0x%08X), got (0x%08X) => (%s) => (%d)\n",
               value_ref[0],
               tmp,
               line_buffer,
               strlen(line_buffer));
        return false;
    }

    // Check second dword
    if (!parseHex(line_buffer + 19, &tmp)) {
        printf("ERROR: Failed to parse second dword => (%s) => (%d)\n", line_buffer, strlen(line_buffer));
        return false;
    }
    if (tmp != value_ref[1]) {
        printf("ERROR: Incorrect second dword. Expected (0x%08X), got (0x%08X) => (%s) => (%d)\n",
               value_ref[1],
               tmp,
               line_buffer,
               strlen(line_buffer));
        return false;
    }

    // Check third dword
    if (!parseHex(line_buffer + 28, &tmp)) {
        printf("ERROR: Failed to parse third dword => (%s) => (%d)\n", line_buffer, strlen(line_buffer));
        return false;
    }
    if (tmp != value_ref[2]) {
        printf("ERROR: Incorrect third dword. Expected (0x%08X), got (0x%08X) => (%s) => (%d)\n",
               value_ref[2],
               tmp,
               line_buffer,
               strlen(line_buffer));
        return false;
    }

    // Check fourth dword
    if (!parseHex(line_buffer + 37, &tmp)) {
        printf("ERROR: Failed to parse fourth dword => (%s) => (%d)\n", line_buffer, strlen(line_buffer));
        return false;
    }
    if (tmp != value_ref[3]) {
        printf("ERROR: Incorrect fourth dword. Expected (0x%08X), got (0x%08X) => (%s) => (%d)\n",
               value_ref[0],
               tmp,
               line_buffer,
               strlen(line_buffer));
        return false;
    }
#endif

    // Update next address
    *address_ref += 16;

    // Succes
    return true;
}

bool getResponse(const char *msg) {
    // Wait for response
    int c = 0;
    while ((c != 'y') && (c != 'Y') && (c != 'n') && (c != 'N')) {
        printf("\n%s (y/n)? ", msg);
        c = getchar();
    }

    // Return result
    return (c == 'y') || (c == 'Y');
}

int main(int argc, const char *argv[]) {
    // Check number of commandline arguments
    if (argc != 3) {
        printf("USAGE: %s __SERIAL_PORT__ __SYSTEM_IMAGE_FILE__\n", argv[0]);
        return 1;
    }

#if !defined(DRY_RUN)
    // Open serial port
    int serial = open(argv[1], O_RDWR);
    if (serial < 0) {
        printf("ERROR: Failed to open serial port. %d => %s\n", errno, strerror(errno));
        return 1;
    }

    // Check if serial port is valid
    if (isatty(serial) != 1) {
        close(serial);
        printf("ERROR: Serial port is invalid.\n");
        return 1;
    }

    // Get serial port settings
    struct termios tty {};
    if (tcgetattr(serial, &tty) != 0) {
        close(serial);
        printf("ERROR: Failed to get serial port settings. %d => %s\n", errno, strerror(errno));
        return 1;
    }

    // Set control flags
    tty.c_cflag &= ~PARENB;        // Clear parity bit, disabling parity
    tty.c_cflag &= ~CSTOPB;        // Clear stop field, only one stop bit used in communication
    tty.c_cflag &= ~CSIZE;         // Clear all bits that set the data size
    tty.c_cflag |= CS8;            // 8 bits per byte
    tty.c_cflag &= ~CRTSCTS;       // Disable RTS/CTS hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;                  // Disable echo
    tty.c_lflag &= ~ECHOE;                 // Disable erasure
    tty.c_lflag &= ~ECHONL;                // Disable new-line echo
    tty.c_lflag &= ~ISIG;                  // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag |= (IXON | IXOFF | IXANY); // Turn on s/w flow ctrl
    tty.c_iflag &=
        ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable any special handling of received bytes
    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

    // Set wait times
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN]  = 0;

    // Set baud rates
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    // Apply serial port settings
    if (tcsetattr(serial, TCSANOW, &tty) != 0) {
        close(serial);
        printf("ERROR: Failed to set serial port settings. %d => %s\n", errno, strerror(errno));
        return 1;
    }
#endif

    // Open file
    FILE *file = fopen(argv[2], "rb");
    if (file == NULL) {
#if !defined(DRY_RUN)
        close(serial);
#endif
        printf("ERROR: Failed to open file\n");
        return 1;
    }

    // Get file size
    fseek(file, 0L, SEEK_END);
    auto file_size = ftell(file);
    if ((file_size % 2048) != 0) {
#if !defined(DRY_RUN)
        close(serial);
#endif
        fclose(file);
        printf("ERROR: Invalid file size\n");
        return 0;
    }
    fseek(file, 0L, SEEK_SET);

    // Write data
    char     line_buffer[100];
    char     read_buffer[16];
    char     serial_buffer[256];
    uint32_t ref_address = FLASH_ADDRESS;
    memset(serial_buffer, 0, sizeof(serial_buffer));
    printf("Writing image to RAM\n");
    for (uint32_t i = 0; i < file_size; i += 16) {
        // Print progress
        printf("\r%u / %u", i, file_size);
        fflush(stdout);

        // Read next 16 bytes from file
        fread(read_buffer, 16, 1, file);

        // Write two 8 byte chunks
        for (int k = 0; k < 16; k += 8) {
            // Write command to serial
#if !defined(SERIAL_DEBUG)
            dprintf(serial,
                    "mw.q " HEX_PREFIX "%X " HEX_PREFIX "%016LX" LINE_FEED,
                    FLASH_ADDRESS + i + k,
                    *((uint64_t *)(read_buffer + k)));
            fsync(serial);
#else
            sprintf(serial_buffer,
                    "mw.q " HEX_PREFIX "%X " HEX_PREFIX "%016LX",
                    FLASH_ADDRESS + i + k,
                    *((uint64_t *)(read_buffer + k)));
            printf("%s\n", serial_buffer);
    #if !defined(DRY_RUN)
            dprintf(serial, "%s" LINE_FEED, serial_buffer);
            fsync(serial);
    #endif
#endif
        }

        // Send dump command
#if !defined(SERIAL_DEBUG)
        dprintf(serial, "md.l " HEX_PREFIX "%X " HEX_PREFIX "4" LINE_FEED, FLASH_ADDRESS + i);
        fsync(serial);
#else
        sprintf(serial_buffer, "md.l " HEX_PREFIX "%X " HEX_PREFIX "4", FLASH_ADDRESS + i);
        printf("%s\n", serial_buffer);
    #if !defined(DRY_RUN)
        dprintf(serial, "%s" LINE_FEED, serial_buffer);
        fsync(serial);
    #endif
#endif

#if !defined(DRY_RUN)
        // Read dump result
        uint32_t exp_address  = FLASH_ADDRESS + i + 16;
        uint32_t line_index   = 0;
        int      read_timeout = READ_TIMEOUT;
        while ((read_timeout > 0) && (ref_address != exp_address)) {
            // Check if we got data
            fd_set fd;
            FD_ZERO(&fd);
            FD_SET(serial, &fd);
            struct timeval tv {};
            tv.tv_sec  = 0;
            tv.tv_usec = 10000;
            if (select(serial + 1, &fd, nullptr, nullptr, &tv) <= 0) {
                --read_timeout;
                continue;
            }
            read_timeout = READ_TIMEOUT;

            // Read from serial
            int bytes_read = read(serial, &serial_buffer, sizeof(serial_buffer));

            // Process bytes
            for (int k = 0; k < bytes_read; ++k) {
                // Check for line end
                if ((serial_buffer[k] == 10) || (serial_buffer[k] == 13)) {
                    // Terminate string
                    line_buffer[line_index] = 0;
                    line_index              = 0;

                    // Process line
                    if (!processLine(&ref_address, (uint32_t *)read_buffer, line_buffer)) {
                        read_timeout = -1;
                        break;
                    }
                } else {
                    // Copy char to line buffer
                    line_buffer[line_index++] = serial_buffer[k];
                }
            }
        }

        // Parse remaining of line
        if ((read_timeout >= 0) && (line_index > 0)) {
            line_buffer[line_index] = 0;
            if (!processLine(&ref_address, (uint32_t *)read_buffer, line_buffer)) {
                read_timeout = -1;
            }
        }

        // Check reference address
        if ((read_timeout >= 0) && (ref_address != exp_address)) {
            printf("ERROR: Invalid reference flash address after read. Expected (0x%08X), got (0x%08X)\n",
                   exp_address,
                   ref_address);
            read_timeout = -1;
        }

        // Check for read error
        if (read_timeout < 0) {
            tcflush(serial, TCIOFLUSH);
            close(serial);
            fclose(file);
            return 1;
        }
#endif
    }
    printf("\r%u / %u\nDone", file_size, file_size);

    // Write to system partition?
    if (getResponse("Write system partition to NAND")) {
        // Erase system partition
        printf("Erasing system partition\n");
#if !defined(SERIAL_DEBUG)
        dprintf(serial, "nand erase.part system" LINE_FEED);
        fsync(serial);
#else
        sprintf(serial_buffer, "nand erase.part system");
        printf("%s\n", serial_buffer);
    #if !defined(DRY_RUN)
        dprintf(serial, "%s" LINE_FEED, serial_buffer);
        fsync(serial);
    #endif
#endif

        // Wait for command to finish
#if !defined(DRY_RUN)
        int read_timeout = READ_TIMEOUT;
        while (read_timeout > 0) {
            fd_set fd;
            FD_ZERO(&fd);
            FD_SET(serial, &fd);
            struct timeval tv {};
            tv.tv_sec  = 1;
            tv.tv_usec = 0;
            if (select(serial + 1, &fd, nullptr, nullptr, &tv) <= 0) {
                --read_timeout;
            } else {
                read(serial, &serial_buffer, sizeof(serial_buffer));
                read_timeout = READ_TIMEOUT;
            }
        }
#endif

        // Write to system partition
        printf("Writing to system partition\n");
#if !defined(SERIAL_DEBUG)
        dprintf(serial, "nand write " HEX_PREFIX "%X system " HEX_PREFIX "%X" LINE_FEED, FLASH_ADDRESS, file_size);
        fsync(serial);
#else
        sprintf(serial_buffer, "nand write " HEX_PREFIX "%X system " HEX_PREFIX "%X", FLASH_ADDRESS, file_size);
        printf("%s\n", serial_buffer);
    #if !defined(DRY_RUN)
        dprintf(serial, "%s" LINE_FEED, serial_buffer);
        fsync(serial);
    #endif
#endif

        // Wait for command to finish
#if !defined(DRY_RUN)
        read_timeout = READ_TIMEOUT;
        while (read_timeout > 0) {
            fd_set fd;
            FD_ZERO(&fd);
            FD_SET(serial, &fd);
            struct timeval tv {};
            tv.tv_sec  = 1;
            tv.tv_usec = 0;
            if (select(serial + 1, &fd, nullptr, nullptr, &tv) <= 0) {
                --read_timeout;
            } else {
                read(serial, &serial_buffer, sizeof(serial_buffer));
                read_timeout = READ_TIMEOUT;
            }
        }
#endif

        // Reboot?
        if (getResponse("Reboot")) {
            // Reboot
            printf("Rebooting\n");
#if !defined(SERIAL_DEBUG)
            dprintf(serial, "reboot" LINE_FEED);
            fsync(serial);
#else
            sprintf(serial_buffer, "reboot");
            printf("%s\n", serial_buffer);
    #if !defined(DRY_RUN)
            dprintf(serial, "%s" LINE_FEED, serial_buffer);
            fsync(serial);
    #endif
#endif
        }
    }

    // Cleanup
#if !defined(DRY_RUN)
    tcflush(serial, TCIOFLUSH);
    close(serial);
#endif
    fclose(file);
    return 0;
}
