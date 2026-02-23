#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

#include <sys/ioctl.h>
#include "aesd_ioctl.h"

#define PORT 9000
#define BUFFER_SIZE 1024

int sockfd, new_fd;
volatile sig_atomic_t exit_requested = 0;

void signal_handler(int signo) {
    syslog(LOG_INFO, "Señal capturada, saliendo");
    exit_requested = 1;
    shutdown(sockfd, SHUT_RDWR);
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    bool daemon_flag = false;

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_flag = true;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        syslog(LOG_ERR, "Error en socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "Error en bind: %s", strerror(errno));
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (daemon_flag) daemonize();

    listen(sockfd, 10);

    while (!exit_requested) {
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (new_fd < 0) {
            if (exit_requested) break;
            syslog(LOG_ERR, "Error en accept: %s", strerror(errno));
            continue;
        }

        syslog(LOG_INFO, "Conexión aceptada desde %s", inet_ntoa(client_addr.sin_addr));

        /* Abrir el driver aesdchar */
        int driver_fd = open("/dev/aesdchar", O_RDWR);
        if (driver_fd < 0) {
            syslog(LOG_ERR, "Error abriendo /dev/aesdchar: %s", strerror(errno));
            close(new_fd);
            continue;
        }

        ssize_t bytes;
        char *recv_buf = NULL;
        size_t recv_size = 0;

        /* Recibir todo del cliente hasta '\n' */
        while ((bytes = recv(new_fd, buffer, BUFFER_SIZE, 0)) > 0) {
            recv_buf = realloc(recv_buf, recv_size + bytes);
            memcpy(recv_buf + recv_size, buffer, bytes);
            recv_size += bytes;
            if (memchr(buffer, '\n', bytes))
                break;
        }

        if (recv_buf) {
            bool is_ioc_cmd = false;

            /* Verificar si el buffer completo es un comando ioctl exacto */
            if (strncmp(recv_buf, "AESDCHAR_IOCSEEKTO:", 19) == 0
                && recv_buf[recv_size - 1] == '\n'
                && strchr(recv_buf + 19, '\n') == (recv_buf + recv_size - 1)) {

                unsigned int write_cmd = 0;
                unsigned int write_offset = 0;
                if (sscanf(recv_buf + 19, "%u,%u", &write_cmd, &write_offset) == 2) {
                    struct aesd_seekto seekto;
                    seekto.write_cmd = write_cmd;
                    seekto.write_cmd_offset = write_offset;

                    if (ioctl(driver_fd, AESDCHAR_IOCSEEKTO, &seekto) < 0) {
                        syslog(LOG_ERR, "ioctl AESDCHAR_IOCSEEKTO failed: %s", strerror(errno));
                    }
                    is_ioc_cmd = true;
                }
            }

            /* Si NO fue comando ioctl, escribir en el driver */
            if (!is_ioc_cmd) {
                ssize_t w = write(driver_fd, recv_buf, recv_size);
                if (w < 0) {
                    syslog(LOG_ERR, "write to /dev/aesdchar failed: %s", strerror(errno));
                }
            }

            free(recv_buf);
        }

        /* Leer desde driver (usando f_pos actual) y enviar al cliente */
        while ((bytes = read(driver_fd, buffer, BUFFER_SIZE)) > 0) {
            send(new_fd, buffer, bytes, 0);
        }

        syslog(LOG_INFO, "Conexión cerrada desde %s", inet_ntoa(client_addr.sin_addr));

        close(driver_fd);
        close(new_fd);
    }

    close(sockfd);
    closelog();
    return 0;
}