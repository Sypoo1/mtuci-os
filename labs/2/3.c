#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

// Функция для вывода информации: имя, id потока и PID родительского процесса,
// а также текущее время в формате ЧЧ:ММ:СС.
void print_info(const char *name) {
    char timeStr[9]; // Формат "HH:MM:SS"
    time_t now = time(NULL);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime(&now));
    printf("%s: ID = %lu, PPID = %d, Время = %s\n",
           name, pthread_self(), getppid(), timeStr);
}

// Функция, выполняемая дочерним потоком.
void *child_thread(void *arg) {
    char *name = (char *)arg;
    print_info(name);
    pthread_exit(NULL);
}

int main(void) {
    pthread_t tid1, tid2;

    // Вывод информации родительского процесса.
    print_info("Родительский процесс");

    // Создание первого дочернего потока.
    pthread_create(&tid1, NULL, child_thread, "Первый дочерний поток");


    // Создание второго дочернего потока.
    pthread_create(&tid2, NULL, child_thread, "Второй дочерний поток");


    // Ожидаем завершения обоих дочерних потоков.
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    return 0;
}
