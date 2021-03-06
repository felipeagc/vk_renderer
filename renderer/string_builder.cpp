#include "string_builder.h"

#include <string.h>
#include <stdarg.h>
#include "array.h"
#include "allocator.h"
#include "format.h"

struct EgStringBuilder
{
    EgAllocator *allocator;
    EgArray(char) arr;
};

extern "C" EgStringBuilder *egStringBuilderCreate(EgAllocator *allocator)
{
    EgStringBuilder *sb = (EgStringBuilder*)egAllocate(allocator, sizeof(*sb));
    *sb = {};
    sb->allocator = allocator;
    sb->arr = egArrayCreate(allocator, char);
    egArrayEnsure(&sb->arr, 1 << 13);
    return sb;
}

extern "C" void egStringBuilderDestroy(EgStringBuilder *sb)
{
    egArrayFree(&sb->arr);
    egFree(sb->allocator, sb);
}

extern "C" void egStringBuilderAppend(EgStringBuilder *sb, const char *str)
{
    char c;
    while ((c = *str))
    {
        egArrayPush(&sb->arr, c);
        ++str;
    }
}

extern "C" void egStringBuilderAppendLen(EgStringBuilder *sb, const char *str, size_t length)
{
    for (const char *s = str; s != str + length; ++s)
    {
        egArrayPush(&sb->arr, *s);
    }
}

extern "C" void egStringBuilderAppendFormat(EgStringBuilder *sb, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const char *string = egVsprintf(sb->allocator, format, args);
    va_end(args);

    egStringBuilderAppend(sb, string);

    egFree(sb->allocator, (void*)string);
}

extern "C" const char *egStringBuilderBuild(EgStringBuilder *sb, EgAllocator *allocator)
{
    char *new_str = (char*)egAllocate(allocator, egArrayLength(sb->arr)+1);
    memcpy(new_str, sb->arr, egArrayLength(sb->arr));
    new_str[egArrayLength(sb->arr)] = '\0';
    return new_str;
}
