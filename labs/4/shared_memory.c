#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>        // shm_open
#include <sys/mman.h>     // mmap, shm_unlink
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>

#define NCHILD     2
#define MAXTEXT    256
#define MSG_COUNT  3
#define TIMEOUT_SEC 2

typedef struct {
    int  msg_id;
    char text[MAXTEXT];
} Message;

typedef struct {
    Message msg;
    sem_t    sem_msg;   // сигнал от parent: "у тебя новое msg"
    sem_t    sem_ack;   // сигнал от child:  "я принял msg"
} Slot;

void child_proc(int id, Slot *slot) {
    for (;;) {
        // ждём, пока parent выдаст новое сообщение
        sem_wait(&slot->sem_msg);

        // читаем и обрабатываем
        printf("Child %d received msg #%d: \"%s\"\n",
               id, slot->msg.msg_id, slot->msg.text);

        // подтверждаем получение
        sem_post(&slot->sem_ack);

        // последний msg_id == MSG_COUNT — выходим
        if (slot->msg.msg_id == MSG_COUNT)
            break;
    }
    exit(0);
}

int main() {
    const char *shm_name = "/shm_sem_chat";
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) { perror("shm_open"); exit(1); }
    // резервируем место под NCHILD слотов
    if (ftruncate(shm_fd, sizeof(Slot)*NCHILD) < 0) {
        perror("ftruncate"); exit(1);
    }
    // мапим
    Slot *slots = mmap(NULL, sizeof(Slot)*NCHILD,
                       PROT_READ | PROT_WRITE,
                       MAP_SHARED, shm_fd, 0);
    if (slots == MAP_FAILED) { perror("mmap"); exit(1); }

    // инициализируем семафоры и поля
    for (int i = 0; i < NCHILD; i++) {
        slots[i].msg.msg_id = 0;
        slots[i].msg.text[0] = '\0';
        sem_init(&slots[i].sem_msg, 1, 0);  // pshared=1, init=0
        sem_init(&slots[i].sem_ack, 1, 0);
    }

    // форкаем детей
    for (int i = 0; i < NCHILD; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork"); exit(1);
        } else if (pid == 0) {
            child_proc(i, &slots[i]);
        }
    }

    // parent: массив текстов
    const char *texts[MSG_COUNT] = {
        "Hello, child!",
        "How are you?",
        "Goodbye!"
    };

    // буфер абсолютного времени для sem_timedwait
    struct timespec ts;

    // рассылка и ожидание ACK
    for (int m = 1; m <= MSG_COUNT; m++) {
        int acked[NCHILD] = {0}, remaining = NCHILD;

        // 1) первый раз шлём всем
        for (int i = 0; i < NCHILD; i++) {
            slots[i].msg.msg_id = m;
            strncpy(slots[i].msg.text, texts[m-1], MAXTEXT);
            sem_post(&slots[i].sem_msg);
            printf("Parent sent msg #%d to child %d\n", m, i);
        }

        // 2) цикл: ждём ACK с таймаутом, и по таймауту — ретранслируем
        while (remaining > 0) {
            // ждём по каждому не-acked child
            for (int i = 0; i < NCHILD; i++) {
                if (acked[i]) continue;

                // готовим abs timeout
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += TIMEOUT_SEC;

                if (sem_timedwait(&slots[i].sem_ack, &ts) == 0) {
                    // пришёл ACK
                    acked[i] = 1;
                    remaining--;
                    printf("Parent received ACK for msg #%d from child %d\n", m, i);
                } else if (errno == ETIMEDOUT) {
                    // таймаут именно для этого child — ретранслируем ВСЕМ не-acked
                    printf("Timeout waiting ACK for msg #%d; retransmitting to unacked...\n", m);
                    for (int j = 0; j < NCHILD; j++) {
                        if (!acked[j]) {
                            sem_post(&slots[j].sem_msg);
                            printf("Parent re-sent msg #%d to child %d\n", m, j);
                        }
                    }
                    break;  // выйдем из for(i) и начнём ждать заново
                } else {
                    perror("sem_timedwait");
                    exit(1);
                }
            }
        }
    }

    // ждём всех детей
    for (int i = 0; i < NCHILD; i++)
        wait(NULL);

    // cleanup
    for (int i = 0; i < NCHILD; i++) {
        sem_destroy(&slots[i].sem_msg);
        sem_destroy(&slots[i].sem_ack);
    }
    munmap(slots, sizeof(Slot)*NCHILD);
    shm_unlink(shm_name);

    printf("Parent: all done.\n");
    return 0;
}
