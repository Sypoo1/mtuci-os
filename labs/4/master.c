#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>

#define MAX_MESSAGES 100
#define TIMEOUT_SEC 2
#define SOCKET_PATH "/tmp/msg_socket"
#define SHM_NAME "/msg_shm"
#define SEM_NAME "/msg_sem"

typedef struct {
    int message_id;
    char text[256];
} Message;

typedef struct {
    int message_id;
    pid_t receiver_pid;
    time_t sent_time;
    int acknowledged;
    char text[256];
} MessageStatus;

typedef struct {
    MessageStatus statuses[MAX_MESSAGES];
    int message_counter;
    int count;
} SharedData;

void cleanup() {
    unlink("/tmp/process1_fifo");
    unlink("/tmp/process2_fifo");
    unlink(SOCKET_PATH);
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);
}

void handle_signal(int sig) {
    exit(EXIT_SUCCESS);
}

int main() {
    signal(SIGINT, handle_signal);
    atexit(cleanup);

    // Создание FIFO
    mkfifo("/tmp/process1_fifo", 0666);
    mkfifo("/tmp/process2_fifo", 0666);

    // Инициализация сокета
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
        .sun_path = SOCKET_PATH
    };
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock, 5);

    // Инициализация разделяемой памяти
    int fd_shm = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(fd_shm, sizeof(SharedData));
    SharedData* shared_data = mmap(NULL, sizeof(SharedData), 
        PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    memset(shared_data, 0, sizeof(SharedData));

    // Семафор для синхронизации
    sem_t* sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);

    // Запуск worker процессов
    pid_t pids[2];
    for (int i = 0; i < 2; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            char fifo_path[32];
            snprintf(fifo_path, sizeof(fifo_path), "/tmp/process%d_fifo", i+1);
            execl("./worker", "worker", fifo_path, NULL);
            perror("execl failed");
            exit(EXIT_FAILURE);
        }
    }

    // Отправка сообщений
    int fifos[2] = {
        open("/tmp/process1_fifo", O_WRONLY),
        open("/tmp/process2_fifo", O_WRONLY)
    };

    sem_wait(sem);
    int current_id = shared_data->message_counter++;
    sem_post(sem);

    Message msg = {
        .message_id = current_id,
        .text = "Broadcast message to all!"
    };

    sem_wait(sem);
    for (int i = 0; i < 2; i++) {
        if (shared_data->count >= MAX_MESSAGES) {
            fprintf(stderr, "Message buffer full!\n");
            break;
        }
        
        write(fifos[i], &msg, sizeof(msg));
        shared_data->statuses[shared_data->count] = (MessageStatus){
            .message_id = msg.message_id,
            .receiver_pid = pids[i],
            .sent_time = time(NULL),
            .acknowledged = 0
        };
        snprintf(shared_data->statuses[shared_data->count].text, 
                sizeof(shared_data->statuses[shared_data->count].text),
                "%s", msg.text);
        shared_data->count++;
    }
    sem_post(sem);

    close(fifos[0]);
    close(fifos[1]);

    // Обработка подтверждений
    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        
        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
        
        if (select(sock+1, &fds, NULL, NULL, &tv) > 0) {
            int client = accept(sock, NULL, NULL);
            int ack_id;
            read(client, &ack_id, sizeof(ack_id));
            
            sem_wait(sem);
            for (int i = 0; i < shared_data->count; i++) {
                if (shared_data->statuses[i].message_id == ack_id) {
                    shared_data->statuses[i].acknowledged = 1;
                    printf("Message %d confirmed by PID %d\n", 
                          ack_id, shared_data->statuses[i].receiver_pid);
                    break;
                }
            }
            sem_post(sem);
            close(client);
        }

        // Проверка таймаутов
        sem_wait(sem);
        time_t now = time(NULL);
        for (int i = 0; i < shared_data->count; i++) {
            if (!shared_data->statuses[i].acknowledged && 
                (now - shared_data->statuses[i].sent_time) > TIMEOUT_SEC) {
                
                char fifo_path[32];
                snprintf(fifo_path, sizeof(fifo_path), "/tmp/process%d_fifo",
                    (shared_data->statuses[i].receiver_pid == pids[0]) ? 1 : 2);
                
                int fifo = open(fifo_path, O_WRONLY);
                
                Message resend_msg = {
                    .message_id = shared_data->statuses[i].message_id,
                };
                snprintf(resend_msg.text, sizeof(resend_msg.text),
                    "RESEND: %s", shared_data->statuses[i].text);
                
                write(fifo, &resend_msg, sizeof(resend_msg));
                shared_data->statuses[i].sent_time = now;
                close(fifo);
                
                printf("Resent message %d to PID %d\n", 
                      resend_msg.message_id, shared_data->statuses[i].receiver_pid);
            }
        }
        sem_post(sem);
    }

    return EXIT_SUCCESS;
}