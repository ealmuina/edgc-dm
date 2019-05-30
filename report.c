#include "include/report.h"

void *report_func(void *args) {
    char buffer[FIELD_SIZE];
    int domain_id = *(int *) args;
    int sockfd;
    socklen_t len;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(REPORT_PORT);

    bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

    print_log("Report thread initialized.", 0);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        recvfrom(sockfd, buffer, FIELD_SIZE, 0, (struct sockaddr *) &cli_addr, &len);
        int id = atoi(buffer);

        printf(">>>>>>>>>>>>>>>>>>> %d\n", id);

        // Find task index
        int index;
        pthread_mutex_lock(&tasks_lock);
        for (index = 0; index < TASKS_MAX; ++index) {
            if (tasks[index].active && tasks[index].flexmpi_id == id)
                break;
        }

        // Report task and set it to inactive
        system(tasks[index].pack);
        sprintf(buffer, "%s/%s?taskId=%d&domainId=%d", repository_url, REPORT_URL, tasks[index].id, domain_id);
        upload(buffer, tasks[index].output);
        tasks[index].active = 0;

        // Report to log
        sprintf(buffer, "Result of task %d successfully reported.", tasks[index].id);
        print_log(buffer, 5);
        pthread_mutex_unlock(&tasks_lock);

        // Clean processes information regarding that task in all nodes
        pthread_mutex_lock(&nodes_lock);
        for (int i = 0; i < NODES_MAX; ++i) {
            if (nodes[i].active) {
                nodes[i].processes[index] = 0;
                nodes[i].root_task[index] = 0;
            }
        }
        pthread_mutex_unlock(&nodes_lock);
    }
#pragma clang diagnostic pop
}

void start_reporter(int domain_id) {
    pthread_t m;
    pthread_create(&m, NULL, report_func, &domain_id);
}
