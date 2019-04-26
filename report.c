#include "report.h"

void *report_func(void *args) {
    int domain_id = *(int *) args;
    int sockfd, len, id;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(REPORT_PORT);

    bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        int n = recvfrom(sockfd, &id, sizeof(int), 0, (struct sockaddr *) &cli_addr, &len);

        // Find task index
        int index;
        pthread_mutex_lock(&tasks_lock);
        for (index = 0; index < TASKS_MAX; ++index) {
            if (tasks[index].active && tasks[index].id == id)
                break;
        }
        // Report task and set it to inactive
        system(tasks[index].pack);
        char buffer[BUFFER_SIZE];
        sprintf(buffer, "%s?taskId=%d&domainId=%d", REPORT_ADDR, tasks[index].id, domain_id);
        upload(buffer, tasks[index].output);
        tasks[index].active = 0;
        // Report to log
        sprintf(buffer, "Result of task %d successfully reported.", tasks[index].id);
        print_log(buffer);
        pthread_mutex_unlock(&tasks_lock);
    }
#pragma clang diagnostic pop
}

void start_reporter(int domain_id) {
    pthread_t m;
    pthread_create(&m, NULL, report_func, &domain_id);
}
