#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>

#define BUFFER_SIZE  (256 * 1024)  /* 256 KB */

static char *md5(const char *file_name) {
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