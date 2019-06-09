#include "include/utils.h"

FILE *eventsfd;

char *md5(const char *file_name) {
    unsigned char c[MD5_DIGEST_LENGTH];

    FILE *inFile = fopen(file_name, "rb");
    MD5_CTX mdContext;

    int bytes;
    unsigned char data[1024];
    char current[1024];
    char *hash = malloc(1024);
    memset(hash, 0, 1024);

    MD5_Init(&mdContext);
    while ((bytes = fread(data, 1, 1024, inFile)) != 0)
        MD5_Update(&mdContext, data, bytes);
    MD5_Final(c, &mdContext);

    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(current, "%02x", c[i]);
        strcat(hash, current);
    }

    fclose(inFile);
    return hash;
}

void print_log(char *msg, int event) {
    time_t rawtime;
    time(&rawtime);
    char *time = strtok(ctime(&rawtime), "\n");
    printf("[%s] %s\n", time, msg);
    fflush(stdout);

    if (event) {
        // If it has event type report it to events log file
        /* EVENT TYPES:
         * 1: Register in repository
         * 2: Requested execution of a task
         * 3: Reduced load of a task
         * 4: Increased load of a task
         * 5: Reported result of a task
         * 6: Detected node
         * */
        if (!eventsfd)
            eventsfd = fopen("dm-events.log", "w");
        char entry[FIELD_SIZE];
        sprintf(entry, "%ld\t%d\n", rawtime, event);
        fwrite(entry, sizeof(char), strlen(entry), eventsfd);
        fflush(eventsfd);
    }
}

void send_controller_instruction(char *instr, int report) {
    pthread_mutex_lock(&controller_lock);
    char buffer[FIELD_SIZE];
    sprintf(buffer, "nping --udp -p 8900 -c 1 localhost --data-string \"%s\" > /dev/null 2> /dev/null", instr);
    if (report)
        printf("\t-> %s\n", buffer);
    system(buffer);
    sleep(FLEXMPI_INTERVAL);
    pthread_mutex_unlock(&controller_lock);
}