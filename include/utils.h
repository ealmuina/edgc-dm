#ifndef EDGC_DM_UTILS_H
#define EDGC_DM_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE  (256 * 1024)  /* 256 KB */
#define FIELD_SIZE 1024
#define FLEXMPI_INTERVAL 10 /* Time to wait before sending commands to FlexMPI controller so it can process
                             * previous commands */

char repository_url[FIELD_SIZE];

char *md5(const char *file_name);

void print_log(char *msg);

void send_controller_instruction(char *instr, int report);

#endif //EDGC_DM_UTILS_H
