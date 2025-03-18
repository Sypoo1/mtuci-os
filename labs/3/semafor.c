#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define NUM_THREADS 10
#define NUM_PRINTERS 3

sem_t printer_sem;

void* use_printer(void* arg) {
    int thread_num = *(int*)arg;
    printf("Поток %d: ожидает освобождения принтера...\n", thread_num);
    
    // Ожидание доступного принтера
    sem_wait(&printer_sem);
    printf("Поток %d: получил доступ к принтеру. Выполняется печать...\n", thread_num);
    
    sleep(1); // имитация печати
    printf("Поток %d: завершил печать. Освобождает принтер.\n", thread_num);
    
    // Освобождение принтера
    sem_post(&printer_sem);
    free(arg);
    pthread_exit(NULL);
}

int main() {
    pthread_t threads[NUM_THREADS];

    // Инициализация семафора, начальное значение - число доступных принтеров
    if (sem_init(&printer_sem, 0, NUM_PRINTERS) != 0) {
        perror("Semaphore init failed");
        exit(EXIT_FAILURE);
    }
    
    // Создание потоков
    for (int i = 0; i < NUM_THREADS; i++) {
        int *num = malloc(sizeof(int));
        if (!num) {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        *num = i;
        if (pthread_create(&threads[i], NULL, use_printer, num) != 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }
    }

    // Ожидание завершения потоков
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Уничтожение семафора
    sem_destroy(&printer_sem);
    return 0;
}
