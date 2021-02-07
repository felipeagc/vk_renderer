#include "string_builder.h"

#include <assert.h>
#include <string.h>
#include "array.hpp"
#include "allocator.h"
#include "format.h"

struct StringBuilder
{
    Allocator *allocator;
    Array<char> arr;
};

extern "C" StringBuilder *StringBuilderCreate(Allocator *allocator)
{
    StringBuilder *sb = (StringBuilder*)Allocate(allocator, sizeof(*sb));
    *sb = {};
    sb->allocator = allocator;
    sb->arr = Array<char>::create(allocator);
    sb->arr.ensure(1 << 13);
    return sb;
}

extern "C" void StringBuilderDestroy(StringBuilder *sb)
{
    sb->arr.free();
    Free(sb->allocator, sb);
}

extern "C" void StringBuilderAppend(StringBuilder *sb, const char *str)
{
    char c;
    while ((c = *str))
    {
        sb->arr.push_back(c);
        ++str;
    }
}

extern "C" void StringBuilderAppendLen(StringBuilder *sb, const char *str, size_t length)
{
    for (const char *s = str; s != str + length; ++s)
    {
        sb->arr.push_back(*s);
    }
}

extern "C" void StringBuilderAppendFormat(StringBuilder *sb, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const char *string = Vsprintf(sb->allocator, format, args);
    va_end(args);

    StringBuilderAppend(sb, string);

    Free(sb->allocator, (void*)string);
}

extern "C" const char *StringBuilderBuild(StringBuilder *sb, Allocator *allocator)
{
    char *new_str = (char*)Allocate(allocator, sb->arr.length+1);
    memcpy(new_str, sb->arr.ptr, sb->arr.length);
    new_str[sb->arr.length] = '\0';
    return new_str;
}
