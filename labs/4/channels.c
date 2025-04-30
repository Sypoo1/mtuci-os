#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define NCHILD   2
#define MAXTEXT  256
#define MSG_COUNT 3       // число сообщений для отправки
#define TIMEOUT_SEC 2     // таймаут ожидания ACK в секундах

typedef struct {
    int  msg_id;
    char text[MAXTEXT];
} Message;

void child_proc(int id, int read_fd, int write_fd) {
    Message msg;


    while (1) {
        
        // ждём сообщение от parent
        ssize_t r = read(read_fd, &msg, sizeof(msg));
        if (r == 0) {
            // канала нет — parent закрыл концы
            break;
        } else if (r < 0) {
            if (errno == EINTR) continue;
            perror("child read");
            exit(1);
        }
        // обработали сообщение
        printf("Child %d received msg #%d: \"%s\"\n", id, msg.msg_id, msg.text);
        // отправляем ACK обратно
        if (write(write_fd, &msg.msg_id, sizeof(int)) < 0) {
            perror("child write ACK");
            exit(1);
        }
    }
    close(read_fd);
    close(write_fd);
    exit(0);
}

int main() {
    int p2c[NCHILD][2], c2p[NCHILD][2];
    pid_t pid;
    int i;

    // создаём пары каналов
    for (i = 0; i < NCHILD; i++) {
        if (pipe(p2c[i]) < 0 || pipe(c2p[i]) < 0) {
            perror("pipe");
            exit(1);
        }
    }

    // форкаем детей
    for (i = 0; i < NCHILD; i++) {
        pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            // в child: закрываем ненужные концы
            close(p2c[i][1]);    // не пишем в свой p2c
            close(c2p[i][0]);    // не читаем из c2p
            // закрываем каналы других детей
            for (int j = 0; j < NCHILD; j++) if (j != i) {
                close(p2c[j][0]); close(p2c[j][1]);
                close(c2p[j][0]); close(c2p[j][1]);
            }
            child_proc(i, p2c[i][0], c2p[i][1]);
        }
    }

    // в parent: закрываем ненужные концы
    for (i = 0; i < NCHILD; i++) {
        close(p2c[i][0]);    // parent только пишет в p2c
        close(c2p[i][1]);    // parent только читает из c2p
    }

    // готовим массив сообщений
    Message msgs[MSG_COUNT] = {
        { .msg_id = 1, .text = "Hello, child!" },
        { .msg_id = 2, .text = "How are you?" },
        { .msg_id = 3, .text = "Goodbye!" }
    };

    // отправка сообщений и ожидание ACK 
    for (int m = 0; m < MSG_COUNT; m++) {
        int acked[NCHILD] = {0};
        int remaining = NCHILD;

        // 1) Первый раз шлём всем
        for (i = 0; i < NCHILD; i++) {
            write(p2c[i][1], &msgs[m], sizeof(Message));
            printf("Parent sent msg #%d to child %d\n", msgs[m].msg_id, i);
        }

        // 2) Цикл: ждём таймаут или ACK, и только по таймауту шлём повторно
        while (remaining > 0) {
            struct timeval tv = { .tv_sec = TIMEOUT_SEC, .tv_usec = 0 };
            fd_set readfds;
            FD_ZERO(&readfds);
            int maxfd = -1;
            for (i = 0; i < NCHILD; i++) {
                if (!acked[i]) {
                    FD_SET(c2p[i][0], &readfds);
                    if (c2p[i][0] > maxfd) maxfd = c2p[i][0];
                }
            }

            int ready = select(maxfd + 1, &readfds, NULL, NULL, &tv);
            if (ready < 0) {
                if (errno == EINTR) continue;
                perror("select");
                exit(1);
            }

            if (ready == 0) {
                // полный таймаут — повторяем отправку тем, кто ещё не подтвердил
                printf("Timeout waiting ACK for msg #%d; retransmitting to unacked...\n",
                       msgs[m].msg_id);
                for (i = 0; i < NCHILD; i++) {
                    if (!acked[i]) {
                        write(p2c[i][1], &msgs[m], sizeof(Message));
                        printf("Parent re-sent msg #%d to child %d\n",
                               msgs[m].msg_id, i);
                    }
                }
            } else {
                // пришли одни или несколько ACK — читаем их, но НЕ шлём новых сообщений
                for (i = 0; i < NCHILD; i++) {
                    if (!acked[i] && FD_ISSET(c2p[i][0], &readfds)) {
                        int ack_id;
                        if (read(c2p[i][0], &ack_id, sizeof(int)) > 0
                            && ack_id == msgs[m].msg_id) {
                            acked[i] = 1;
                            remaining--;
                            printf("Parent received ACK for msg #%d from child %d\n",
                                   ack_id, i);
                        }
                    }
                }
                // сразу же возвращаемся в select: если ещё есть 'remaining',
                // мы либо дождёмся следующего ACK (до истечения tv),
                // либо timeout — и тогда ретранслируем.
            }
        }
    }


    // закрываем каналы parent → child (чтобы дети вышли из read)
    for (i = 0; i < NCHILD; i++) {
        close(p2c[i][1]);
    }

    // ждём завершения детей
    for (i = 0; i < NCHILD; i++) {
        wait(NULL);
    }

    // закрываем оставшиеся каналы
    for (i = 0; i < NCHILD; i++) {
        close(c2p[i][0]);
    }

    printf("Parent: all done.\n");
    return 0;
}
