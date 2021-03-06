#include "format.h"

#include <stdarg.h>
#include <stdio.h>
#include "allocator.h"

const char* egSprintf(EgAllocator *allocator, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    size_t str_size = vsnprintf(NULL, 0, format, args) + 1;
    va_end(args);

    char *str = (char*)egAllocate(allocator, str_size);

    va_start(args, format);
    vsnprintf(str, str_size, format, args);
    va_end(args);

    str[str_size-1] = '\0';

    return str;
}

const char* egVsprintf(EgAllocator *allocator, const char *format, va_list args)
{
    va_list va1;
    va_copy(va1, args);
    size_t str_size = vsnprintf(NULL, 0, format, va1) + 1;
    va_end(va1);

    char *str = (char*)egAllocate(allocator, str_size);

    vsnprintf(str, str_size, format, args);

    str[str_size-1] = '\0';

    return str;
}
