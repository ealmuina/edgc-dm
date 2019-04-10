#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <openssl/md5.h>
#include <jansson.h>

#define BUFFER_SIZE  (256 * 1024)  /* 256 KB */

struct write_result {
    char *data;
    int pos;
};

struct WriteThis {
    const char *readptr;
    size_t sizeleft;
};

static size_t read_callback(void *dest, size_t size, size_t nmemb, void *userp) {
    struct WriteThis *wt = (struct WriteThis *) userp;
    size_t buffer_size = size * nmemb;

    if (wt->sizeleft) {
        /* copy as much as possible from the source to the destination */
        size_t copy_this_much = wt->sizeleft;
        if (copy_this_much > buffer_size)
            copy_this_much = buffer_size;
        memcpy(dest, wt->readptr, copy_this_much);

        wt->readptr += copy_this_much;
        wt->sizeleft -= copy_this_much;
        return copy_this_much; /* we copied this many bytes */
    }

    return 0; /* no more data left to deliver */
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *) stream);
    return written;
}

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {
    struct write_result *result = (struct write_result *) stream;

    if (result->pos + size * nmemb >= BUFFER_SIZE - 1) {
        fprintf(stderr, "error: too small buffer\n");
        return 0;
    }

    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;

    return size * nmemb;
}

static void download(const char *url, const char *filename) {
    CURL *curl;
    FILE *file;

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl = curl_easy_init();

    /* set URL to get here */
    curl_easy_setopt(curl, CURLOPT_URL, url);

    /* disable progress meter, set to 0L to enable and disable debug output */
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    /* send all data to this function  */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

    /* open the file */
    file = fopen(filename, "wb");
    if (file) {

        /* write the page body to this file handle */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

        /* get it! */
        curl_easy_perform(curl);

        /* close the header file */
        fclose(file);
    }

    /* cleanup curl stuff */
    curl_easy_cleanup(curl);

    curl_global_cleanup();
}

static char *get(const char *url) {
    CURL *curl = NULL;
    CURLcode status;
    char *data = NULL;
    long code;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (!curl)
        goto error;

    data = malloc(BUFFER_SIZE);
    if (!data)
        goto error;

    struct write_result write_result = {
            .data = data,
            .pos = 0
    };

    curl_easy_setopt(curl, CURLOPT_URL, url);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

    status = curl_easy_perform(curl);
    if (status != 0) {
        fprintf(stderr, "error: unable to get data from %s:\n", url);
        fprintf(stderr, "%s\n", curl_easy_strerror(status));
        goto error;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if (code != 200) {
        fprintf(stderr, "error: server responded with code %ld\n", code);
        goto error;
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    /* zero-terminate the result */
    data[write_result.pos] = '\0';

    return data;

    error:
    if (data)
        free(data);
    if (curl)
        curl_easy_cleanup(curl);
    curl_global_cleanup();
    return NULL;
}

static char *post(const char *url, const char *data) {
    CURL *curl;
    CURLcode res;

    struct curl_slist *headers = NULL;
    struct WriteThis wt;

    wt.readptr = data;
    wt.sizeleft = strlen(data);

    char *result = malloc(BUFFER_SIZE);

    struct write_result write_result = {
            .data = result,
            .pos = 0
    };

    /* In windows, this will init the winsock stuff */
    res = curl_global_init(CURL_GLOBAL_DEFAULT);
    /* Check for errors */
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_global_init() failed: %s\n",
                curl_easy_strerror(res));
        return NULL;
    }

    /* get a curl handle */
    curl = curl_easy_init();
    if (curl) {
        /* First set the URL that is about to receive our POST. */
        curl_easy_setopt(curl, CURLOPT_URL, url);

        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        /* Now specify we want to POST data */
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        /* we want to use our own read function */
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
        curl_easy_setopt(curl, CURLOPT_READDATA, &wt);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

        /* Set the expected POST size. If you want to POST large amounts of data,
           consider CURLOPT_POSTFIELDSIZE_LARGE */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) wt.sizeleft);

        /* Perform the get, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    curl_slist_free_all(headers);
    curl_global_cleanup();

    return result;
}

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
