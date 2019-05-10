#ifndef EDGC_DM_HTTP_H
#define EDGC_DM_HTTP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/stat.h>

#include "utils.h"

struct write_result {
    char *data;
    int pos;
};

struct write_this {
    const char *readptr;
    size_t sizeleft;
};

size_t read_callback(void *dest, size_t size, size_t nmemb, void *userp);

size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream);

size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream);

void download(const char *url, const char *filename);

char *get(const char *url, long *code);

char *post(const char *url, const char *data);

void upload(const char *url, const char *filename);

#endif //EDGC_DM_HTTP_H
