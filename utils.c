#include "include/utils.h"

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

void print_log(char *msg) {
    time_t rawtime;
    time(&rawtime);
    char *time = strtok(ctime(&rawtime), "\n");
    printf("[%s] %s\n", time, msg);
}

void send_controller_instruction(char *instr, int report) {
    char buffer[FIELD_SIZE];
    sprintf(buffer, "nping --udp -p 8900 -c 1 localhost --data-string \"%s\" > /dev/null 2> /dev/null", instr);
    if (report)
        printf("\t-> %s\n", buffer);
    system(buffer);
    sleep(FLEXMPI_INTERVAL);
}