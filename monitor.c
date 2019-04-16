#include "monitor.h"

void *monitor_func(void *args) {
    int sockfd, newsockfd, portno, clilen;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in serv_addr, cli_addr;
    int n, len;

    sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int broadcastEnable = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 9910;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &cli_addr, &len);
        buffer[n] = '\0';

        if (!cli_addr.sin_addr.s_addr) continue; //skip address 0

        time_t rawtime;
        time(&rawtime);

        int index = 0;

        pthread_mutex_lock(&monitor_lock);
        // Search item in nodes list
        for (int i = 0; i < NODES_MAX; ++i) {
            if (nodes[index].active) index = i; // index will be the first empty position
            if (nodes[i].active && nodes[i].addr == cli_addr.sin_addr.s_addr) {
                index = i; // found!
                break;
            }
        }
        if (index != NODES_MAX) { // There is still space for this node
            nodes[index].active = 1;
            strcpy(nodes[index].stats, buffer);
            nodes[index].last_seen = rawtime;
            nodes[index].addr = cli_addr.sin_addr.s_addr;
        }
        pthread_mutex_unlock(&monitor_lock);
    }
#pragma clang diagnostic pop
}

void start_monitor() {
    pthread_mutex_init(&monitor_lock, NULL);
    pthread_t m;
    pthread_create(&m, NULL, monitor_func, NULL);
}
