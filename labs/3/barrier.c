#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 5

pthread_barrier_t barrier;

void* thread_work(void* arg) {
    int thread_num = *(int*)arg;
    printf("Поток %d: Фаза 1 – выполняется работа...\n", thread_num);
    
    // Имитация работы первой фазы
    sleep(1 + thread_num % 3);
    printf("Поток %d: достиг барьера.\n", thread_num);
    
    // Синхронизация: ожидание всех потоков
    int rc = pthread_barrier_wait(&barrier);
    if (rc == PTHREAD_BARRIER_SERIAL_THREAD) {
        printf("Поток %d: специальный поток (serial) на барьере выполняет дополнительные действия.\n", thread_num);
    }
    
    printf("Поток %d: Фаза 2 – продолжается выполнение...\n", thread_num);
    free(arg);
    pthread_exit(NULL);
}

int main() {
    pthread_t threads[NUM_THREADS];

    // Инициализация барьера с числом потоков, равным NUM_THREADS
    if (pthread_barrier_init(&barrier, NULL, NUM_THREADS) != 0) {
        perror("Barrier init failed");
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
        if (pthread_create(&threads[i], NULL, thread_work, num) != 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }
    }
    
    // Ожидание завершения потоков
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Уничтожение барьера
    pthread_barrier_destroy(&barrier);
    return 0;
}
