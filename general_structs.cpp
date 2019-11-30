#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "base64.c"

#include "general_structs.h"
#include "poet_shared_functions.h"
#include "poet_common_definitions.h"

#define UPPERCASE(x) ((x) & ((unsigned char)0xEF))
#define LOWERCASE(x) ((x) | ((unsigned char)0x10))

const char *node_t_to_json(const node_t *node) {
    assert(node != nullptr);
    char *buffer = (char *) malloc(BUFFER_SIZE);
    char *wbuffer = nullptr;
    if (buffer != nullptr) {
        sprintf(buffer,
                R"({"node_id": %u, "sgx_time": %u, "arrival_time": %u, "time_left": %u, "n_leadership": %u})",
                node->node_id, node->sgx_time, node->arrival_time, node->time_left, node->n_leadership);
        size_t len = strlen(buffer);
        wbuffer = (char *) (malloc(len + 1));
        if (wbuffer != nullptr) {
            strcpy(wbuffer, buffer);
            free(buffer);
        } else {
            wbuffer = buffer;
            perror("node_t_to_json -> memory efficiency");
            ERR("for some reason could not allocate more memory\n");
        }
    } else {
        perror("node_t_to_json");
    }

    return wbuffer;
}

int json_to_node_t(const json_value *json, node_t *node) {
    assert(json != nullptr);
    assert(json->type == json_object);

    auto *root = (json_value *) json;

    int state = 1;
    json_value *value = nullptr;

    value = find_value(root, "node_id");
    state = state && value != nullptr;
    if (state) node->node_id = std::max(value->u.integer, (long) 0);

    value = state ? find_value(root, "sgx_time") : nullptr;
    state = state && value != nullptr;
    if (state) node->sgx_time = std::max(value->u.integer, (long) 0);

    value = state ? find_value(root, "n_leadership") : nullptr;
    state = state && value != nullptr;
    if (state) node->n_leadership = std::max(value->u.integer, (long) 0);

    value = state ? find_value(root, "time_left") : nullptr;
    state = state && value != nullptr;
    if (state) node->time_left = std::min((long) node->sgx_time, std::max(value->u.integer, (long) 0));

    value = state ? find_value(root, "arrival_time") : nullptr;
    state = state && value != nullptr;
    if (state) node->arrival_time = std::max(value->u.integer, (long) 0);

    return state;
}

void free_poet_context(struct poet_context *context) {
    assert(context != nullptr);

    if (context->node != nullptr) {
        free(context->node);
        context->node = nullptr;
    }

    if (context->public_key != nullptr) {
        free(context->public_key);
        context->public_key = nullptr;
    }

    if (context->signature != nullptr) {
        free(context->signature);
        context->signature = nullptr;
    }
}

char *encode_hex(void *buffer, size_t buffer_len) {
    size_t wbuffer_len = buffer_len * 2 + 10;
    char *wbuffer = (char *) (malloc(wbuffer_len));
    if (wbuffer == nullptr) {
        perror("malloc");
        goto error;
    }
    memset(wbuffer, 0, wbuffer_len);

    for (size_t i = 0; i < buffer_len; i++) {
        assert(2 * i < wbuffer_len);
        sprintf(wbuffer + 2 * i, "%02x", *((unsigned char *) buffer + i));
    }

    goto terminate;

    error:
    fprintf(stderr, "Fatal error, can not proceed with sending message");
    if (wbuffer != nullptr) {
        free(wbuffer);
    }
    wbuffer = nullptr;

    terminate:
    return wbuffer;
}

unsigned char *encode_64base(const void *buffer, size_t buffer_len) {
    size_t out_len;
    unsigned char *out = base64_encode((unsigned char *) buffer, buffer_len, &out_len);
    if (out != nullptr) {
        out[out_len - 1] = '\0';
    }
    return out;
}

void *decode_64base(const char *buffer, size_t buffer_len, size_t *out_len) {
    assert(out_len != nullptr);
    return base64_decode((const unsigned char *) buffer, buffer_len, out_len);
}

unsigned char hexchr2bin(const char hex) {
    unsigned char c = 0;

    if (hex >= '0' && hex <= '9') {
        c = hex - '0';
    } else if (hex >= 'A' && hex <= 'F') {
        c = hex - 'A' + 10;
    } else if (hex >= 'a' && hex <= 'f') {
        c = hex - 'a' + 10;
    }

    return c;
}

void *decode_hex(const char *buffer, size_t buffer_len) {
    assert(buffer != nullptr);
    assert(buffer_len > 0);

    unsigned char c;
    size_t pos = 0;

    size_t wbuff_len = buffer_len / 2 + 1;
    char *wbuffer = (char *) (malloc(wbuff_len));
    if (wbuffer == nullptr) {
        perror("malloc");
        goto error;
    }
    memset(wbuffer, 0, wbuff_len);

    for (size_t i = 0; i < buffer_len; i += 2) {
        assert(pos < wbuff_len);
        c = hexchr2bin(buffer[i]);
        c <<= 4;
        c |= hexchr2bin(buffer[i + 1]);
        wbuffer[pos++] = c;
    }

    goto terminate;

    error:
    fprintf(stderr, "Fatal error, can not proceed with converting hex message to raw bytes");
    if (wbuffer != nullptr) {
        free(wbuffer);
    }
    wbuffer = nullptr;

    terminate:
    return wbuffer;
}