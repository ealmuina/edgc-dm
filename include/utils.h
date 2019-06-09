#ifndef EDGC_DM_UTILS_H
#define EDGC_DM_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE  (512 * 1024)  /* 512 KB */
#define FIELD_SIZE 1024
#define FLEXMPI_INTERVAL 20 /* Time to wait before sending commands to FlexMPI controller so it can process
                             * previous commands */

char repository_url[FIELD_SIZE];

char *md5(const char *file_name);

void print_log(char *msg, int event);

void send_controller_instruction(char *instr, int report);

#endif //EDGC_DM_UTILS_H
