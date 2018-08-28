#include <curl/curl.h>

enum NetError {
    NetError_None,
    NetError_Again,
    NetError_CurlInit,
    NetError_VaporCloudAuth,
    NetError_Generic
};

enum NetError InitCurl() {
    static b32 curlInit = false;
    if (curlInit) return NetError_None;

    CURLcode err;
    err = curl_global_init(CURL_GLOBAL_ALL);
    if (err != CURLE_OK)
        return NetError_CurlInit;

    curlInit = true;
    return NetError_None;
}

#define RESPONSE_BUFFER_MAX_SIZE (1024*1024)

enum HTTPMethod {
    Method_Get,
    Method_Post,
    Method_Patch,
    Method_Delete,

    _METHOD_COUNT
};

const char *HTTPMethodDescriptions[_METHOD_COUNT] = {
    [Method_Get] =    "GET",
    [Method_Post] =   "POST",
    [Method_Patch] =  "PATCH",
    [Method_Delete] = "DELETE"
};

char urlScratchBuffer[1024];

struct CurlRequest {
    CURL *handle;

    const char *url;
    enum HTTPMethod method;
    const char *request;
    u32 requestLenLeft;

    char *response;
    u32 len;
};

static size_t writeFunc(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realSize = size * nmemb;
    struct CurlRequest *req = (struct CurlRequest *)userp;

    if (req->len + realSize >= RESPONSE_BUFFER_MAX_SIZE) {
        return 0;
    }

    memcpy(&req->response[req->len], contents, realSize);
    req->len += realSize;
    req->response[req->len] = '\0';

    return realSize;
}

static size_t readFunc(void *dest, size_t size, size_t nmemb, void *userp) {
    size_t bufferSize = size * nmemb;
    struct CurlRequest *req = (struct CurlRequest *)userp;

    size_t count = req->requestLenLeft;
    if (count) {
        count = count < bufferSize ? count : bufferSize;
        memcpy(dest, req->request, count);
        req->request += count;
        req->requestLenLeft -= count;
    }

    return count;
}

b32 extractString(
    const char *key,
    jsmntok_t field,
    jsmntok_t *child,
    const char *json,
    const char **out
) {
    if (field.type != JSMN_STRING)
        return 0;

    int fieldLen = field.end - field.start;
    for (size_t i = 0; i < fieldLen; i += 1) {
        if (key[i] != json[field.start+i])
            return 0;
    }

    const char *new = strndup(json+child->start, child->end-child->start);
    new = Unescape(new);

    *out = new;
    return 1;
}

void skipTokens(jsmntok_t *tokens, int *index) {
    switch (tokens[*index].type) {
    case JSMN_PRIMITIVE:
    case JSMN_STRING:
    case JSMN_UNDEFINED:
        *index = (*index)+1;
        break;

    case JSMN_ARRAY: {
        int start = *index;
        jsmntok_t arry = tokens[start++];

        for (size_t i = 0; i < arry.size; i++) {
            skipTokens(tokens, &start);
        }

        *index = start;
    } break;

    case JSMN_OBJECT: {
        int start = *index;
        jsmntok_t obj = tokens[start++];

        for (size_t i = 0; i < obj.size; i++) {
            skipTokens(tokens, &start); // key
            skipTokens(tokens, &start); // value
        }

        *index = start;
    } break;
    }
}

b32 GetVaporCloudKeys(const char **refresh, const char **access) {
    const char *home = getenv("HOME");
    if (!home) {
        return 1;
    }

    char buffer[1024];

    snprintf(&buffer[0], sizeof(buffer), "%s/.vapor/token.json", home);
    FILE *file = fopen(&buffer[0], "r");
    if (!file) {
        return 1;
    }

    int read = fread(&buffer[0], 1, sizeof(buffer), file);
    fclose(file);

    if (read <= 0 || read >= sizeof(buffer)) {
        return 1;
    }

    buffer[read] = '\0';

    jsmn_parser parser;
    jsmntok_t tokens[0x100];

    jsmn_init(&parser);
    int count;
    count = jsmn_parse(&parser, &buffer[0], read, &tokens[0], sizeof(tokens)/sizeof(tokens[0]));
    if (count < 0) {
        return 1;
    }

    if (tokens[0].type != JSMN_OBJECT) {
        return 1;
    }

    int fields = tokens[0].size;
    int offset = 1;

    for (size_t i = 0; i < fields; i += 1) {
        jsmntok_t field = tokens[offset++];

        extractString("refresh", field, &tokens[offset], &buffer[0], refresh);
        extractString("access", field, &tokens[offset], &buffer[0], access);

        skipTokens(tokens, &offset);
    }

    return 0;
}

struct KeyValue {
    const char *key;
    const char *value;
};

b32 parseConfigs(struct KeyValue **out, const char *json, size_t length) {
    jsmn_parser parser;
    jsmn_init(&parser);

    int tokenCount;
    tokenCount = jsmn_parse(&parser, json, length, NULL, 0);
    if (tokenCount < 1) {
        printf("Failed to parse json: %d\n", tokenCount);
        return -1;
    }

    jsmntok_t *tokens = calloc(tokenCount, sizeof(jsmntok_t));
    if (!tokens) {
        return -1;
    }

    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, json, length, tokens, tokenCount);
    if (tokenCount < 1) {
        printf("Failed to load tokens: %d\n", tokenCount);
        return -1;
    }

    if (tokens[0].type != JSMN_ARRAY) {
        printf("Malformed json response\n");
        return -1;
    }

    int configsCount = tokens[0].size;
    struct KeyValue *configs = calloc(configsCount, sizeof(struct KeyValue));

    int offset = 1;
    for (size_t configIndex = 0; configIndex < configsCount; configIndex += 1) {
        struct KeyValue *config = &configs[configIndex];
        jsmntok_t obj = tokens[offset++];
        if (obj.type != JSMN_OBJECT) {
            return -1;
        }

        int fields = obj.size;
        for (size_t i = 0; i < fields; i += 1) {
            jsmntok_t field = tokens[offset++];

            extractString("key", field, &tokens[offset], json, &config->key);
            extractString("value", field, &tokens[offset], json, &config->value);

            skipTokens(tokens, &offset);
        }
    }

    *out = configs;
    return configsCount;
}

void refreshTokenUrl(CURL *handle) {
    curl_easy_setopt(
        handle, CURLOPT_URL,
        "https://api.vapor.cloud/admin/refresh"
    );
}

void cacheTokens(const char *refresh, const char *access) {
    const char *home = getenv("HOME");
    if (!home) return;

    char buffer[1024];
    snprintf(&buffer[0], sizeof(buffer), "%s/.vapor/token.json", home);
    FILE *file = fopen(&buffer[0], "w");
    if (!file) return;

    int count = snprintf(
        &buffer[0],
        sizeof(buffer),
        "{\"access\": \"%s\", \"refresh\": \"%s\"}",
        access, refresh
    );

    fwrite(&buffer[0], 1, count, file);
    fclose(file);
}

b32 refreshToken(const char *refreshToken, const char **accessOut) {
    CURL *handle = curl_easy_init();
    refreshTokenUrl(handle);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeFunc);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
    if (FlagVerbose)
        curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);

    char authBuffer[1024];
    snprintf(authBuffer, sizeof(authBuffer), "Authorization: Bearer %s", refreshToken);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, &authBuffer[0]);
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    struct CurlRequest req = {
        .response = malloc(1024*1024),
        .len = 0
    };

    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&req);

    CURLcode code;
    code = curl_easy_perform(handle);
    if (code != CURLE_OK) {
        return NetError_Generic;
    }

    const char *json = req.response;
    u32 jsonLen = req.len;

    jsmn_parser parser;
    jsmn_init(&parser);

    int tokenCount;
    tokenCount = jsmn_parse(&parser, json, jsonLen, NULL, 0);
    if (tokenCount < 1) {
        printf("Failed to parse json: %d\n", tokenCount);
        return NetError_Generic;
    }

    jsmntok_t *tokens = calloc(tokenCount, sizeof(jsmntok_t));
    if (!tokens) {
        return NetError_Generic;
    }

    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, json, jsonLen, tokens, tokenCount);
    if (tokenCount < 1) {
        printf("Failed to load tokens: %d\n", tokenCount);
        return NetError_Generic;
    }

    if (tokens[0].type != JSMN_OBJECT) {
        return -1;
    }

    int fields = tokens[0].size;

    const char *access = NULL;

    int offset = 1;
    for (size_t i = 0; i < fields; i += 1) {
        jsmntok_t field = tokens[offset++];

        extractString("accessToken", field, &tokens[offset], json, &access);

        skipTokens(tokens, &offset);
    }

    cacheTokens(refreshToken, access);

    *accessOut = access ?: "";
    return NetError_None;
}

const char *vaporCloudConfigUrl(const char *app, const char *env) {
    snprintf(
        &urlScratchBuffer[0],
        sizeof(urlScratchBuffer),
        "https://api.vapor.cloud/application/applications/%s/hosting/environments/%s/configurations",
        app, env
    );
    return &urlScratchBuffer[0];
}

enum NetError vaporCloudReq(struct CurlRequest *req) {
    CURL *handle = curl_easy_init();
    req->handle = handle;

    curl_easy_setopt(handle, CURLOPT_URL, req->url);

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeFunc);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, req);

    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
    if (FlagVerbose) {
        curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(handle, CURLOPT_STDERR, stdout);
    }

    switch (req->method) {
        case Method_Get:
            curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
            break;
        case Method_Post:
            curl_easy_setopt(handle, CURLOPT_POST, 1L);
            break;
        default:
            curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, HTTPMethodDescriptions[req->method]);
    }

    const char *refresh, *access;
    if (GetVaporCloudKeys(&refresh, &access)) {
        fprintf(stderr, "Unable to locate Vapor Cloud key. Please refresh your token or login\n");
        return NetError_VaporCloudAuth;
    }

    char authBuffer[1024];
    snprintf(authBuffer, sizeof(authBuffer), "Authorization: Bearer %s", access);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, &authBuffer[0]);

    if (req->request && req->requestLenLeft) {
        curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, (long)req->requestLenLeft);
        curl_easy_setopt(handle, CURLOPT_READFUNCTION, readFunc);
        curl_easy_setopt(handle, CURLOPT_READDATA,req);
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }

    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    req->len = 0;
    
    CURLcode code;
    code = curl_easy_perform(handle);
    if (code != CURLE_OK) {
        int status;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
        if (status == 401) {
            return NetError_VaporCloudAuth;
        }
        printf("failed: %s\n", curl_easy_strerror(code));

        return NetError_Generic;
    }

    return NetError_None;
}

struct CurlRequest netGet(const char *url) {
    struct CurlRequest req = {0};
    req.url = url;
    req.method = Method_Get;
    req.response = malloc(1024*1024);
    return req;
}

struct CurlRequest netPatch(const char *url, const char *data, u32 len) {
    struct CurlRequest req = {0};
    req.url = url;
    req.method = Method_Patch;
    req.response = malloc(1024*1024);
    req.request = data;
    req.requestLenLeft = len;
    return req;
}

i32 configToJson(struct KeyValue *configs, u32 count, char **out) {
    char *json = malloc(1024*1024);

    json[0] = '{';
    i32 offset = 1;

    for (size_t i = 0; i < count; i += 1) {
        offset += snprintf(
            json+offset,(1024*1024)-offset,
            "\"%s\": \"%s\",",
            configs[i].key, configs[i].value
        );

        if (offset >= 1024*1024) {
            free(json);
            return -1;
        }
    }

    // NOTE: replaces the final `,` with `}`
    json[offset-1] = '}';
    *out = json;
    return offset;
}

enum NetError SetVaporCloudConfig(
    const char *app,
    const char *env,
    struct KeyValue **configs,
    u32 *count
) {
    char *json;
    u32 len = configToJson(*configs, *count, &json);

    struct CurlRequest req = netPatch(
        vaporCloudConfigUrl(app, env),
        json,
        len
    );

    enum NetError err = vaporCloudReq(&req);
    if (err)
        return err;

    struct KeyValue *newConfigs;
    int newCount = parseConfigs(&newConfigs, req.response, req.len);
    if (newCount < 0) {
        printf("Something went wrong: %d\n", newCount);
        return NetError_Generic;
    }

    *count = newCount;
    *configs = newConfigs;

    return NetError_None;
}

enum NetError GetVaporCloudConfig(const char *app, const char *env, struct KeyValue **out, u32 *outCount) {
    const char *refresh, *access;

    b32 error;
    error = GetVaporCloudKeys(&refresh, &access);
    if (error) {
        fprintf(stderr, "Unable to locate Vapor Cloud key. Please refresh your token or login\n");
        return NetError_VaporCloudAuth;
    }

    char *buffer = malloc(1024*1024);
    struct CurlRequest req = {
        .url = vaporCloudConfigUrl(app, env),
        .method = Method_Get,
        .response = buffer,
        .len = 0
    };

    CURL *handle = curl_easy_init();
    b32 hasTried = false;

tryAgain:
    curl_easy_setopt(handle, CURLOPT_URL, req.url);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeFunc);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
    if (FlagVerbose) {
        curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(handle, CURLOPT_STDERR, stdout);
    }

    char authBuffer[1024];
    snprintf(authBuffer, sizeof(authBuffer), "Authorization: Bearer %s", access);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, &authBuffer[0]);

    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    req.len = 0;
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&req);
    
    CURLcode code;
    code = curl_easy_perform(handle);
    if (code != CURLE_OK) {
        int status;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
        if (status == 401) {
            if (!hasTried) {
                printf("Refreshing Vapor Cloud token...\n");
                hasTried = true;
                b32 err = refreshToken(refresh, &access);
                curl_easy_reset(handle);
                goto tryAgain;
            }
            return NetError_VaporCloudAuth;
        }

        return NetError_Generic;
    }

    struct KeyValue *configs;
    int count = parseConfigs(&configs, req.response, req.len);
    if (count < 0) {
        printf("Something went wrong: %d\n", count);
        return NetError_Generic;
    }

    *out = configs;
    *outCount = count;

    return NetError_None;
} 
