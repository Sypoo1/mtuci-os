// sockets.c
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>       // offsetof
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>     // struct timeval

#define NCHILD      2
#define MAXTEXT     256
#define MSG_COUNT   3
#define TIMEOUT_SEC 2

typedef struct {
    int  msg_id;
    char text[MAXTEXT];
} Message;

typedef struct {
    int child_id;
    int msg_id;
} AckMessage;

static void cleanup_socket(const char *path) {
    unlink(path);
}

static void child_proc(int id) {
    int sock;
    struct sockaddr_un src, dst;
    char path[64];

    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) { perror("child socket"); exit(1); }

    // свой адрес
    snprintf(path, sizeof(path), "/tmp/child%d.sock", id);
    cleanup_socket(path);
    memset(&src, 0, sizeof(src));
    src.sun_family = AF_UNIX;
    strcpy(src.sun_path, path);
    if (bind(sock, (struct sockaddr*)&src,
             offsetof(struct sockaddr_un, sun_path) + strlen(path)) < 0) {
        perror("child bind"); exit(1);
    }

    // адрес родителя
    memset(&dst, 0, sizeof(dst));
    dst.sun_family = AF_UNIX;
    strcpy(dst.sun_path, "/tmp/parent.sock");

    Message msg;
    while (1) {
        ssize_t r = recvfrom(sock, &msg, sizeof(msg), 0, NULL, NULL);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("child recvfrom"); exit(1);
        }
        printf("Child %d received msg #%d: \"%s\"\n",
               id, msg.msg_id, msg.text);

        // отправляем ACK с child_id и msg_id
        AckMessage ack = { .child_id = id, .msg_id = msg.msg_id };
        if (sendto(sock, &ack, sizeof(ack), 0,
                   (struct sockaddr*)&dst,
                   offsetof(struct sockaddr_un, sun_path) + strlen(dst.sun_path)) < 0) {
            perror("child sendto ACK"); exit(1);
        }
        if (msg.msg_id == MSG_COUNT) break;
    }

    close(sock);
    cleanup_socket(path);
    exit(0);
}

int main() {
    int sock;
    struct sockaddr_un parent_addr;
    pid_t pid;

    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) { perror("parent socket"); exit(1); }

    cleanup_socket("/tmp/parent.sock");
    memset(&parent_addr, 0, sizeof(parent_addr));
    parent_addr.sun_family = AF_UNIX;
    strcpy(parent_addr.sun_path, "/tmp/parent.sock");
    if (bind(sock, (struct sockaddr*)&parent_addr,
             offsetof(struct sockaddr_un, sun_path) + strlen(parent_addr.sun_path)) < 0) {
        perror("parent bind"); exit(1);
    }

    struct timeval tv = { .tv_sec = TIMEOUT_SEC, .tv_usec = 0 };
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt SO_RCVTIMEO"); exit(1);
    }

    for (int i = 0; i < NCHILD; i++) {
        pid = fork();
        if (pid < 0) {
            perror("fork"); exit(1);
        } else if (pid == 0) {
            child_proc(i);
        }
    }

    sleep(1);

    Message msgs[MSG_COUNT] = {
        {1, "Hello, child!"},
        {2, "How are you?"},
        {3, "Goodbye!"}
    };

    struct sockaddr_un child_addr[NCHILD];
    socklen_t child_len[NCHILD];
    char path[64];
    for (int i = 0; i < NCHILD; i++) {
        memset(&child_addr[i], 0, sizeof(child_addr[i]));
        child_addr[i].sun_family = AF_UNIX;
        snprintf(path, sizeof(path), "/tmp/child%d.sock", i);
        strcpy(child_addr[i].sun_path, path);
        child_len[i] = offsetof(struct sockaddr_un, sun_path) + strlen(path);
    }

    for (int m = 0; m < MSG_COUNT; m++) {
        int acked[NCHILD] = {0}, remaining = NCHILD;

        for (int i = 0; i < NCHILD; i++) {
            if (sendto(sock, &msgs[m], sizeof(msgs[m]), 0,
                       (struct sockaddr*)&child_addr[i], child_len[i]) < 0) {
                perror("parent sendto"); exit(1);
            }
            printf("Parent sent msg #%d to child %d\n",
                   msgs[m].msg_id, i);
        }

        while (remaining > 0) {
            AckMessage ack;
            ssize_t r = recvfrom(sock, &ack, sizeof(ack), 0, NULL, NULL);
            if (r < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    printf("Timeout waiting ACK for msg #%d; retransmitting...\n",
                           msgs[m].msg_id);
                    for (int i = 0; i < NCHILD; i++) {
                        if (!acked[i]) {
                            sendto(sock, &msgs[m], sizeof(msgs[m]), 0,
                                   (struct sockaddr*)&child_addr[i], child_len[i]);
                            printf("Parent re-sent msg #%d to child %d\n",
                                   msgs[m].msg_id, i);
                        }
                    }
                    continue;
                } else if (errno == EINTR) {
                    continue;
                } else {
                    perror("parent recvfrom"); exit(1);
                }
            }
            if (ack.msg_id == msgs[m].msg_id && ack.child_id >= 0 && ack.child_id < NCHILD && !acked[ack.child_id]) {
                acked[ack.child_id] = 1;
                remaining--;
                printf("Parent received ACK for msg #%d from child %d\n",
                       ack.msg_id, ack.child_id);
            }
        }
    }

    for (int i = 0; i < NCHILD; i++) wait(NULL);
    close(sock);
    cleanup_socket("/tmp/parent.sock");
    printf("Parent: all done.\n");
    return 0;
}
