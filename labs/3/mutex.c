#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 5
#define NUM_ITERATIONS 1000000

int counter = 0;
pthread_mutex_t counter_mutex;

void* increment_counter(void* arg) {
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        pthread_mutex_lock(&counter_mutex);
        counter++;  // критическая секция
        pthread_mutex_unlock(&counter_mutex);
    }
    pthread_exit(NULL);
}

int main() {
    pthread_t threads[NUM_THREADS];
    
    // Инициализация мьютекса
    if (pthread_mutex_init(&counter_mutex, NULL) != 0) {
        perror("Mutex init failed");
        exit(EXIT_FAILURE);
    }

    // Создание потоков
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, increment_counter, NULL) != 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }
    }

    // Ожидание завершения потоков
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Final counter value: %d\n", counter);

    // Уничтожение мьютекса
    pthread_mutex_destroy(&counter_mutex);
    return 0;
}
