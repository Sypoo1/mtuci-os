#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <pwd.h>

#define BUFFER_SIZE 256

int main() {
    char hostname[BUFFER_SIZE];
    char username[BUFFER_SIZE];
    struct utsname sys_info;
    time_t current_time;
    struct tm *time_info;
    struct sysinfo si;

    printf("\n=== Системная информация ===\n\n");

    // Получение имени компьютера
    if (gethostname(hostname, BUFFER_SIZE) == 0) {
        printf("Имя компьютера: %s\n", hostname);
    } else {
        printf("Ошибка при получении имени компьютера\n");
    }

    // Получение имени пользователя
    if (getlogin_r(username, BUFFER_SIZE) == 0) {
        printf("Имя пользователя: %s\n", username);
    } else {
        printf("Ошибка при получении имени пользователя\n");
    }

    // Получение информации об ОС
    if (uname(&sys_info) == 0) {
        printf("\n--- Информация об ОС ---\n");
        printf("Операционная система: %s\n", sys_info.sysname);
        printf("Версия ядра: %s\n", sys_info.release);
        printf("Версия ОС: %s\n", sys_info.version);
        printf("Архитектура: %s\n", sys_info.machine);
    }

    // Получение системных метрик
    if (sysinfo(&si) == 0) {
        printf("\n--- Системные метрики ---\n");
        printf("Время работы системы: %ld дней, %ld часов, %ld минут\n",
               si.uptime / 86400, (si.uptime % 86400) / 3600, (si.uptime % 3600) / 60);
        printf("Всего ОЗУ: %lu МБ\n", si.totalram / 1024 / 1024);
        printf("Свободно ОЗУ: %lu МБ\n", si.freeram / 1024 / 1024);
    }

    // Работа со временем
    time(&current_time);
    time_info = localtime(&current_time);
    printf("\n--- Информация о времени ---\n");
    printf("Текущее локальное время: %s", asctime(time_info));
    time_info = gmtime(&current_time);
    printf("Текущее время UTC: %s", asctime(time_info));

    // Дополнительная информация через system()
    printf("\n--- Дополнительная информация ---\n");
    printf("Информация о процессоре:\n");
    system("cat /proc/cpuinfo | grep 'model name' | head -n 1");
    printf("\nИнформация о дисковом пространстве:\n");
    system("df -h /");
    printf("\nСписок активных процессов:\n");
    system("ps aux | head -n 2");
    printf("\nИнформация о сетевых интерфейсах:\n");
    system("ip addr show | grep 'inet ' | head -n 5");

    return 0;
}
