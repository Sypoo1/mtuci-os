#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

// Функция для вывода информации о процессе
void print_info(const char *Name) {
    char timeStr[9]; // Формат "HH:MM:SS"
    time_t now = time(NULL);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime(&now));
    printf("%s: PID = %d, PPID = %d, Время = %s\n",
        Name, getpid(), getppid(), timeStr);
}

int main(void) {
    pid_t pid1, pid2;

    // Создаём первый дочерний процесс
    pid1 = fork();

    if (pid1 == 0) {
        print_info("Первый дочерний процесс");
        system("arp -a");
        exit(EXIT_SUCCESS);
    }

    // Создаём второй дочерний процесс
    pid2 = fork();

    if (pid2 == 0) {
        print_info("Второй дочерний процесс");
        exit(EXIT_SUCCESS);
    }

    print_info("Родительский процесс");

    system("ps -x");

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 0;
}
