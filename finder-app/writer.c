#include <stdio.h>
#include <syslog.h>
#include <string.h>

#define USER_LOG_INFO(format, ...) do { \
                                syslog(LOG_DEBUG , format, ##__VA_ARGS__); \
                            } while (0)

#define USER_LOG_ERR(format, ...) do { \
                                syslog(LOG_ERR , format, ##__VA_ARGS__); \
                            } while (0)

#define USER_LOG_INIT() openlog("finder-app", LOG_PID | LOG_CONS, LOG_USER)




int main (int argc, char *argv[]) {
    char *filename;
    char *message;
    FILE *file_to_write;

    USER_LOG_INIT();

    if (argc != 3){
        USER_LOG_ERR("Usage: writer.sh <file> <string>");
        return 1;
    }

    if (strlen(argv[1]) == 0) {
        USER_LOG_ERR("File path is null");
        return 1;
    }

    if (strlen(argv[2]) == 0) {
        USER_LOG_ERR("Write string is null");
        return 1;
    }

    filename = argv[1];
    message = argv[2];

    file_to_write = fopen(filename, "w");

    if (file_to_write == NULL) {
        USER_LOG_ERR("Error opening file %s", filename);
        return 1;
    }

    size_t ret = fwrite((void *)message, sizeof(char), strlen(message), file_to_write);

    if (ret != strlen(message)) {
        USER_LOG_ERR("Error writing to file %s", filename);
        fclose(file_to_write);
        return 1;
    }

    USER_LOG_INFO ("Writing %s to %s)", message, filename);

    fclose(file_to_write);
    closelog();

    return 0;

}