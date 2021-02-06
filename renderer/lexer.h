#pragma once

#include <stddef.h>

typedef struct Allocator Allocator;

typedef enum TokenType
{
    TOKEN_ERROR,
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
} TokenType;

typedef struct Token
{
    TokenType type;
    size_t pos;

    const char *str;
    size_t str_length;
} Token;

typedef struct TokenizerState
{
    const char *text;
    size_t length;
    size_t pos;
} TokenizerState;

TokenizerState NewTokenizerState(const char *text, size_t length);
TokenizerState NextToken(Allocator *allocator, TokenizerState state, Token *token);
