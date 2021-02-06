#include "format.h"

#include <stdarg.h>
#include <stdio.h>
#include "allocator.h"

const char* Sprintf(Allocator *allocator, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    size_t str_size = vsnprintf(NULL, 0, format, args) + 1;
    va_end(args);

    char *str = (char*)Allocate(allocator, str_size);

    va_start(args, format);
    vsnprintf(str, str_size, format, args);
    va_end(args);

    str[str_size-1] = '\0';

    return str;
}
