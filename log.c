/* SPDX-License-Identifier: MIT */
#include "include/rinlog/log.h"

#include <stdint.h>

typedef struct RinLogWriter {
    char* output;
    size_t capacity;
    size_t position;
} RinLogWriter;

static int valid_bytes(const char* value, size_t size, size_t maximum,
                       int printable_ascii)
{
    size_t index;
    if (size > maximum || (size != 0u && value == NULL)) return 0;
    for (index = 0u; index < size; ++index) {
        unsigned char byte = (unsigned char)value[index];
        if (byte == 0u) return 0;
        if (printable_ascii && (byte < 0x20u || byte > 0x7eu)) return 0;
    }
    return 1;
}

static int field_keys_equal(const RinLogFieldV1* left,
                            const RinLogFieldV1* right)
{
    size_t index;
    if (left->key_size != right->key_size) return 0;
    for (index = 0u; index < left->key_size; ++index) {
        if (left->key[index] != right->key[index]) return 0;
    }
    return 1;
}

int rin_log_event_validate(const RinLogEventV1* event)
{
    size_t index;
    size_t other;
    if (event == NULL) return RIN_LOG_INVALID_ARGUMENT;
    if (event->struct_size != sizeof(*event) ||
        event->version != RIN_LOG_EVENT_VERSION_1 || event->reserved != 0u) {
        return RIN_LOG_BAD_VERSION;
    }
    if ((event->flags & ~RIN_LOG_EVENT_FLAG_SENSITIVE_MESSAGE) != 0u) {
        return RIN_LOG_BAD_FLAGS;
    }
    if (event->severity > RIN_LOG_CRITICAL) return RIN_LOG_BAD_SEVERITY;
    if (!valid_bytes(event->component, event->component_size,
                     RIN_LOG_MAX_COMPONENT_BYTES, 1) ||
        !valid_bytes(event->category, event->category_size,
                     RIN_LOG_MAX_CATEGORY_BYTES, 1) ||
        !valid_bytes(event->event, event->event_size,
                     RIN_LOG_MAX_EVENT_BYTES, 1) ||
        !valid_bytes(event->message, event->message_size,
                     RIN_LOG_MAX_MESSAGE_BYTES, 0)) {
        return RIN_LOG_BAD_TEXT;
    }
    if (event->field_count > RIN_LOG_MAX_FIELDS ||
        (event->field_count != 0u && event->fields == NULL)) {
        return RIN_LOG_LIMIT_EXCEEDED;
    }
    for (index = 0u; index < event->field_count; ++index) {
        const RinLogFieldV1* field = &event->fields[index];
        if ((field->flags & ~RIN_LOG_FIELD_FLAG_SENSITIVE) != 0u) {
            return RIN_LOG_BAD_FLAGS;
        }
        if (field->key_size == 0u ||
            !valid_bytes(field->key, field->key_size,
                         RIN_LOG_MAX_FIELD_KEY_BYTES, 1) ||
            !valid_bytes(field->value, field->value_size,
                         RIN_LOG_MAX_FIELD_VALUE_BYTES, 0)) {
            return RIN_LOG_BAD_TEXT;
        }
        for (other = 0u; other < index; ++other) {
            if (field_keys_equal(field, &event->fields[other])) {
                return RIN_LOG_DUPLICATE_FIELD;
            }
        }
    }
    return RIN_LOG_OK;
}

static void writer_byte(RinLogWriter* writer, char byte)
{
    if (writer->position < writer->capacity) {
        writer->output[writer->position] = byte;
    }
    ++writer->position;
}

static void writer_bytes(RinLogWriter* writer, const char* bytes, size_t size)
{
    size_t index;
    for (index = 0u; index < size; ++index) writer_byte(writer, bytes[index]);
}

static void writer_literal(RinLogWriter* writer, const char* text)
{
    size_t size = 0u;
    while (text[size] != '\0') ++size;
    writer_bytes(writer, text, size);
}

static void writer_u64(RinLogWriter* writer, uint64_t value)
{
    char digits[20];
    size_t count = 0u;
    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);
    while (count != 0u) writer_byte(writer, digits[--count]);
}

static void writer_hex_byte(RinLogWriter* writer, unsigned char byte)
{
    static const char digits[] = "0123456789abcdef";
    writer_byte(writer, digits[(byte >> 4u) & 0x0fu]);
    writer_byte(writer, digits[byte & 0x0fu]);
}

static void writer_json_string(RinLogWriter* writer, const char* value,
                               size_t size)
{
    size_t index;
    writer_byte(writer, '"');
    for (index = 0u; index < size; ++index) {
        unsigned char byte = (unsigned char)value[index];
        switch (byte) {
        case '"': writer_literal(writer, "\\\""); break;
        case '\\': writer_literal(writer, "\\\\"); break;
        case '\b': writer_literal(writer, "\\b"); break;
        case '\f': writer_literal(writer, "\\f"); break;
        case '\n': writer_literal(writer, "\\n"); break;
        case '\r': writer_literal(writer, "\\r"); break;
        case '\t': writer_literal(writer, "\\t"); break;
        default:
            if (byte < 0x20u) {
                writer_literal(writer, "\\u00");
                writer_hex_byte(writer, byte);
            } else {
                writer_byte(writer, (char)byte);
            }
            break;
        }
    }
    writer_byte(writer, '"');
}

static const char* severity_name(uint32_t severity)
{
    static const char* const names[] = {
        "trace", "debug", "info", "notice", "warning", "error",
        "critical"
    };
    return names[severity];
}

int rin_log_event_format(const RinLogEventV1* event,
                         char* output, size_t output_capacity,
                         size_t* output_size)
{
    RinLogWriter writer;
    size_t index;
    int status = rin_log_event_validate(event);
    if (output_size != NULL) *output_size = 0u;
    if (status != RIN_LOG_OK) return status;
    if (output == NULL && output_capacity != 0u) return RIN_LOG_INVALID_ARGUMENT;
    writer.output = output;
    writer.capacity = output_capacity == 0u ? 0u : output_capacity - 1u;
    writer.position = 0u;
    writer_literal(&writer, "{\"severity\":");
    writer_json_string(&writer, severity_name(event->severity),
                       event->severity == RIN_LOG_CRITICAL ? 8u :
                       event->severity == RIN_LOG_WARNING ? 7u :
                       event->severity == RIN_LOG_NOTICE ? 6u :
                       event->severity == RIN_LOG_TRACE ? 5u :
                       event->severity == RIN_LOG_DEBUG ? 5u :
                       event->severity == RIN_LOG_ERROR ? 5u : 4u);
    writer_literal(&writer, ",\"component\":");
    writer_json_string(&writer, event->component, event->component_size);
    writer_literal(&writer, ",\"timestamp_ns\":");
    writer_u64(&writer, event->timestamp_ns);
    writer_literal(&writer, ",\"process\":");
    writer_u64(&writer, event->process_id);
    writer_literal(&writer, ",\"thread\":");
    writer_u64(&writer, event->thread_id);
    writer_literal(&writer, ",\"category\":");
    writer_json_string(&writer, event->category, event->category_size);
    writer_literal(&writer, ",\"event\":");
    writer_json_string(&writer, event->event, event->event_size);
    writer_literal(&writer, ",\"message\":");
    if ((event->flags & RIN_LOG_EVENT_FLAG_SENSITIVE_MESSAGE) != 0u) {
        writer_literal(&writer, "\"<redacted>\"");
    } else {
        writer_json_string(&writer, event->message, event->message_size);
    }
    writer_literal(&writer, ",\"fields\":{");
    for (index = 0u; index < event->field_count; ++index) {
        const RinLogFieldV1* field = &event->fields[index];
        if (index != 0u) writer_byte(&writer, ',');
        writer_json_string(&writer, field->key, field->key_size);
        writer_byte(&writer, ':');
        if ((field->flags & RIN_LOG_FIELD_FLAG_SENSITIVE) != 0u) {
            writer_literal(&writer, "\"<redacted>\"");
        } else {
            writer_json_string(&writer, field->value, field->value_size);
        }
    }
    writer_literal(&writer, "}}\n");
    if (output_size != NULL) *output_size = writer.position;
    if (writer.position > RIN_LOG_MAX_RENDERED_BYTES) {
        if (output != NULL && output_capacity != 0u) output[output_capacity - 1u] = '\0';
        return RIN_LOG_LIMIT_EXCEEDED;
    }
    if (output_capacity == 0u || writer.position >= output_capacity) {
        if (output != NULL && output_capacity != 0u) output[output_capacity - 1u] = '\0';
        return RIN_LOG_BUFFER_TOO_SMALL;
    }
    output[writer.position] = '\0';
    return RIN_LOG_OK;
}

int rin_log_emit(const RinLogEventV1* event,
                 char* scratch, size_t scratch_capacity,
                 RinLogSinkFn sink, void* context)
{
    size_t line_size = 0u;
    int status;
    if (sink == NULL) return RIN_LOG_INVALID_ARGUMENT;
    status = rin_log_event_format(event, scratch, scratch_capacity, &line_size);
    if (status != RIN_LOG_OK) return status;
    return sink(context, scratch, line_size) == 0 ? RIN_LOG_OK : RIN_LOG_SINK_FAILED;
}
