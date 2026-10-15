/* Experimental build adaptation; not an original-source claim.
 * Prepared source snapshot; compiled directly without a generation step.
 * Existing allocation failure, truncation and overflow behavior is preserved.
 * SDK vsnprintf replaces the original LDBL dispatch stub: equivalence untested.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recovered-types.h"
void history_add(RecoveredHistory *history, const char *text);
void history_reset(RecoveredHistory *history);
char *history_prev(RecoveredHistory *history);
char *history_next(RecoveredHistory *history);
void ds_init(RecoveredDString *str);
void ds_free(RecoveredDString *str);
void ds_clear(RecoveredDString *str);
void ds_reserve(RecoveredDString *str, unsigned int requested);
void ds_append(RecoveredDString *str, const char *text);
void ds_appendn(RecoveredDString *str, const char *text, unsigned int length);
void ds_appendf(RecoveredDString *str, const char *format, ...);
void ds_truncate(RecoveredDString *str, unsigned int length);
char *ds_cstr(RecoveredDString *str);
void ds_copy(RecoveredDString *destination, RecoveredDString *source);
void queue_init(RecoveredQueue *queue);
int queue_empty(RecoveredQueue *queue);
int queue_full(RecoveredQueue *queue);
void queue_push(RecoveredQueue *queue, const char *text);
char *queue_pop(RecoveredQueue *queue);

/* Analyst reconstruction of history_add at 0x00008064. See index/manual-review.json. */
void history_add(RecoveredHistory *history, const char *text)
{
    int index;
    if (text == 0 || text[0] == '\0') return;
    if (history->count > 0) {
        index = (history->count + history->first - 1) % 256;
        if (strcmp(history->entries[index], text) == 0) return;
    }
    index = (history->count + history->first) % 256;
    strncpy(history->entries[index], text, 32767);
    history->entries[index][32767] = '\0';
    if (history->count < 256) ++history->count;
    else history->first = (history->first + 1) % 256;
    history->cursor = -1;
}

/* Analyst reconstruction of history_reset at 0x00008160. See index/manual-review.json. */
void history_reset(RecoveredHistory *history)
{
    /* Reset navigation only: stored history is intentionally preserved. */
    history->cursor = -1;
}

/* Analyst reconstruction of history_prev at 0x00008170. See index/manual-review.json. */
char *history_prev(RecoveredHistory *history)
{
    int index;
    if (history->count == 0) return 0;
    if (history->cursor == -1)
        index = (history->count + history->first - 1) % 256;
    else
        index = (history->cursor + 255) % 256;
    if (index < 0) index += 256;
    if (history->count >= 256 || index < history->count) {
        history->cursor = index;
        return history->entries[index];
    }
    return 0;
}

/* Analyst reconstruction of history_next at 0x000081f8. See index/manual-review.json. */
char *history_next(RecoveredHistory *history)
{
    int index;
    if (history->count == 0 || history->cursor == -1) return 0;
    index = (history->cursor + 1) % 256;
    if (index != (history->count + history->first) % 256 &&
        history->entries[index][0] != '\0') {
        history->cursor = index;
        return history->entries[index];
    }
    history->cursor = -1;
    return 0;
}

/* Analyst reconstruction of ds_init at 0x00016fec. See index/manual-review.json. */
void ds_init(RecoveredDString *str)
{
    if (str == 0) return;
    str->data = 0; str->length = 0; str->capacity = 0;
}

/* Analyst reconstruction of ds_free at 0x00017008. See index/manual-review.json. */
void ds_free(RecoveredDString *str)
{
    if (str == 0) return;
    free(str->data);
    ds_init(str);
}

/* Analyst reconstruction of ds_clear at 0x00017044. See index/manual-review.json. */
void ds_clear(RecoveredDString *str)
{
    if (str == 0) return;
    if (str->data != 0) str->data[0] = '\0';
    str->length = 0;
}

/* Analyst reconstruction of ds_reserve at 0x0001706c. See index/manual-review.json. */
void ds_reserve(RecoveredDString *str, unsigned int requested)
{
    unsigned int capacity;
    char *allocation;
    if (str == 0 || requested == 0 || requested <= str->capacity) return;
    capacity = str->capacity;
    if (capacity == 0) capacity = 8;
    /* Unsigned 32-bit doubling; no overflow guard exists in the binary. */
    while (capacity < requested) capacity <<= 1;
    allocation = realloc(str->data, capacity);
    if (allocation == 0) {
        /* The binary discards the old buffer on allocation failure. */
        free(str->data);
        str->data = 0; str->capacity = 0; str->length = 0;
    } else {
        str->data = allocation; str->capacity = capacity;
    }
}

/* Analyst reconstruction of ds_append at 0x00017100. See index/manual-review.json. */
void ds_append(RecoveredDString *str, const char *text)
{
    if (str != 0 && text != 0) ds_appendn(str, text, strlen(text));
}

/* Analyst reconstruction of ds_appendn at 0x00017150. See index/manual-review.json. */
void ds_appendn(RecoveredDString *str, const char *text, unsigned int length)
{
    if (str == 0 || text == 0 || length == 0) return;
    ds_reserve(str, str->length + length + 1);
    if (str->data == 0) return;
    memcpy(str->data + str->length, text, length);
    str->length += length;
    str->data[str->length] = '\0';
}

/* Analyst reconstruction of ds_appendf at 0x000171dc. See index/manual-review.json. */
void ds_appendf(RecoveredDString *str, const char *format, ...)
{
    char small[256];
    char *buffer = 0;
    unsigned int capacity;
    int length;
    va_list original, attempt;
    if (str == 0 || format == 0) return;
    /* va_list denotes the Tiger PPC parameter-save-area cursor. The machine
       spills r5..r10 at original-SP+32..52 and passes that cursor in r6. */
    va_start(original, format);
    va_copy(attempt, original);
    length = vsnprintf(small, 256, format, attempt);
    va_end(attempt);
    if (length < 0) { va_end(original); return; }
    if ((unsigned int)length < 256) {
        ds_reserve(str, str->length + (unsigned int)length + 1);
        if (str->data != 0) {
            memcpy(str->data + str->length, small, (unsigned int)length + 1);
            str->length += (unsigned int)length;
        }
        va_end(original);
        return;
    }
    /* Preserve the observed retry from 256 and doubling, even though the
       first formatting result already reported a longer required length. */
    capacity = 256;
    for (;;) {
        char *resized = realloc(buffer, capacity);
        if (resized == 0) { free(buffer); break; }
        buffer = resized;
        va_copy(attempt, original);
        length = vsnprintf(buffer, capacity, format, attempt);
        va_end(attempt);
        if (length < 0) { free(buffer); break; }
        if ((unsigned int)length >= capacity) { capacity <<= 1; continue; }
        ds_reserve(str, str->length + (unsigned int)length + 1);
        if (str->data != 0) {
            memcpy(str->data + str->length, buffer, (unsigned int)length + 1);
            str->length += (unsigned int)length;
        }
        free(buffer);
        break;
    }
    va_end(original);
}

/* Analyst reconstruction of ds_truncate at 0x00017364. See index/manual-review.json. */
void ds_truncate(RecoveredDString *str, unsigned int length)
{
    if (str == 0 || str->length <= length) return;
    str->length = length;
    if (str->data != 0) str->data[length] = '\0';
}

/* Analyst reconstruction of ds_cstr at 0x00017394. See index/manual-review.json. */
char *ds_cstr(RecoveredDString *str)
{
    return str != 0 && str->data != 0 ? str->data : "";
}

/* Analyst reconstruction of ds_copy at 0x000173c8. See index/manual-review.json. */
void ds_copy(RecoveredDString *destination, RecoveredDString *source)
{
    if (destination == 0 || destination == source) return;
    ds_clear(destination);
    if (source != 0) ds_appendn(destination, source->data, source->length);
}

/* Analyst reconstruction of queue_init at 0x00017a00. See index/manual-review.json. */
void queue_init(RecoveredQueue *queue)
{
    queue->count = 0; queue->first = 0;
}

/* Analyst reconstruction of queue_empty at 0x00017a14. See index/manual-review.json. */
int queue_empty(RecoveredQueue *queue)
{
    return queue->count == 0;
}

/* Analyst reconstruction of queue_full at 0x00017a28. See index/manual-review.json. */
int queue_full(RecoveredQueue *queue)
{
    return queue->count > 7;
}

/* Analyst reconstruction of queue_push at 0x00017a40. See index/manual-review.json. */
void queue_push(RecoveredQueue *queue, const char *text)
{
    int index;
    if (queue_full(queue)) return;
    index = (queue->first + queue->count) % 8;
    strncpy(queue->entries[index], text, 262143);
    queue->entries[index][262143] = '\0';
    ++queue->count;
}

/* Analyst reconstruction of queue_pop at 0x00017ac8. See index/manual-review.json. */
char *queue_pop(RecoveredQueue *queue)
{
    char *text;
    if (queue_empty(queue)) return 0;
    text = queue->entries[queue->first];
    queue->first = (queue->first + 1) % 8;
    --queue->count;
    return text;
}
