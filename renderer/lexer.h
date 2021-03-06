#pragma once

#include <stddef.h>

typedef struct EgAllocator EgAllocator;

typedef enum EgTokenType
{
    TOKEN_ERROR = 0,
    TOKEN_LCURLY,
    TOKEN_RCURLY,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_COLON,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_IDENT,
    TOKEN_STRING,
    TOKEN_EOF,
} EgTokenType;

typedef struct EgToken
{
    EgTokenType type;
    size_t pos;
    const char *str;
} EgToken;

typedef struct EgTokenizerState
{
    const char *text;
    size_t length;
    size_t pos;
} EgTokenizerState;

EgTokenizerState egTokenizerCreate(const char *text, size_t length);
EgTokenizerState egTokenizerNextToken(EgAllocator *allocator, EgTokenizerState state, EgToken *token);
void egTokenizerFreeToken(EgAllocator *allocator, EgToken token);
