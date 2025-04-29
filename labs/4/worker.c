#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#define SOCKET_PATH "/tmp/msg_socket"

typedef struct {
    int message_id;
    char text[256];
} Message;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <fifo_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int fifo = open(argv[1], O_RDONLY);
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    
    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
        .sun_path = SOCKET_PATH
    };
    
    connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    Message msg;
    while (read(fifo, &msg, sizeof(msg)) > 0) {
        printf("[Worker %d] Received: %s\n", getpid(), msg.text);
        write(sock, &msg.message_id, sizeof(msg.message_id));
    }

    close(fifo);
    close(sock);
    return EXIT_SUCCESS;
}