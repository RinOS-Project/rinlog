/* SPDX-License-Identifier: MIT */
#ifndef RINLOG_LOG_H
#define RINLOG_LOG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIN_LOG_EVENT_VERSION_1 1u
#define RIN_LOG_EVENT_FLAG_SENSITIVE_MESSAGE 0x00000001u
#define RIN_LOG_FIELD_FLAG_SENSITIVE 0x00000001u

#define RIN_LOG_MAX_COMPONENT_BYTES 64u
#define RIN_LOG_MAX_CATEGORY_BYTES 64u
#define RIN_LOG_MAX_EVENT_BYTES 64u
#define RIN_LOG_MAX_MESSAGE_BYTES 1024u
#define RIN_LOG_MAX_FIELDS 16u
#define RIN_LOG_MAX_FIELD_KEY_BYTES 64u
#define RIN_LOG_MAX_FIELD_VALUE_BYTES 256u
#define RIN_LOG_MAX_RENDERED_BYTES 8192u

typedef enum RinLogSeverity {
    RIN_LOG_TRACE = 0u,
    RIN_LOG_DEBUG = 1u,
    RIN_LOG_INFO = 2u,
    RIN_LOG_NOTICE = 3u,
    RIN_LOG_WARNING = 4u,
    RIN_LOG_ERROR = 5u,
    RIN_LOG_CRITICAL = 6u
} RinLogSeverity;

typedef enum RinLogStatus {
    RIN_LOG_OK = 0,
    RIN_LOG_INVALID_ARGUMENT = -1,
    RIN_LOG_BAD_VERSION = -2,
    RIN_LOG_BAD_FLAGS = -3,
    RIN_LOG_BAD_SEVERITY = -4,
    RIN_LOG_BAD_TEXT = -5,
    RIN_LOG_LIMIT_EXCEEDED = -6,
    RIN_LOG_DUPLICATE_FIELD = -7,
    RIN_LOG_BUFFER_TOO_SMALL = -8,
    RIN_LOG_SINK_FAILED = -9
} RinLogStatus;

typedef struct RinLogFieldV1 {
    const char* key;
    size_t key_size;
    const char* value;
    size_t value_size;
    uint32_t flags;
} RinLogFieldV1;

typedef struct RinLogEventV1 {
    uint32_t struct_size;
    uint16_t version;
    uint16_t reserved;
    uint32_t severity;
    uint32_t flags;
    uint64_t timestamp_ns;
    uint64_t process_id;
    uint64_t thread_id;
    const char* component;
    size_t component_size;
    const char* category;
    size_t category_size;
    const char* event;
    size_t event_size;
    const char* message;
    size_t message_size;
    const RinLogFieldV1* fields;
    size_t field_count;
} RinLogEventV1;

typedef int (*RinLogSinkFn)(void* context, const char* line, size_t line_size);

int rin_log_event_validate(const RinLogEventV1* event);
int rin_log_event_format(const RinLogEventV1* event,
                         char* output, size_t output_capacity,
                         size_t* output_size);
int rin_log_emit(const RinLogEventV1* event,
                 char* scratch, size_t scratch_capacity,
                 RinLogSinkFn sink, void* context);

#ifdef __cplusplus
}
#endif

#endif /* RINLOG_LOG_H */
