#include "lexer.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "allocator.h"
#include "format.h"

static inline int64_t LengthLeft(EgTokenizerState state, size_t offset)
{
    return ((int64_t)state.length) - (int64_t)(state.pos + offset);
}

static inline bool IsWhitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline bool IsAlpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

static inline bool IsAlphaNum(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') || (c >= '0' && c <= '9');
}

EgTokenizerState egTokenizerCreate(const char *text, size_t length)
{
    EgTokenizerState state = {};
    state.text = text;
    state.length = length;
    state.pos = 0;
    return state;
}

EgTokenizerState egTokenizerNextToken(EgAllocator *allocator, EgTokenizerState state, EgToken *token)
{
    (void)allocator;
    *token = {};

    // Skip whitespace
    for (size_t i = state.pos; i < state.length; ++i)
    {
        if (IsWhitespace(state.text[i])) state.pos++;
        else break;
    }

    token->pos = state.pos;

    if (LengthLeft(state, 0) <= 0)
    {
        token->type = TOKEN_EOF;
        return state;
    }

    char c = state.text[state.pos];
    switch (c)
    {
    case '\"':
    {
        // String
        state.pos++;

        const char *string = &state.text[state.pos];

        size_t content_length = 0;
        while (LengthLeft(state, content_length) > 0 &&
               state.text[state.pos + content_length] != '\"')
        {
            content_length++;
        }

        state.pos += content_length;

        if (LengthLeft(state, 0) > 0 &&
            state.text[state.pos] == '\"')
        {
            state.pos++;
        }
        else
        {
            token->type = TOKEN_ERROR;
            token->str = egStrdup(allocator, "unclosed string");
            break;
        }

        token->type = TOKEN_STRING;
        token->str = egNullTerminate(allocator, string, content_length);

        break;
    }

    case '{': state.pos++; token->type = TOKEN_LCURLY; break;
    case '}': state.pos++; token->type = TOKEN_RCURLY; break;
    case '[': state.pos++; token->type = TOKEN_LBRACKET; break;
    case ']': state.pos++; token->type = TOKEN_RBRACKET; break;
    case '(': state.pos++; token->type = TOKEN_LPAREN; break;
    case ')': state.pos++; token->type = TOKEN_RPAREN; break;

    case ':': state.pos++; token->type = TOKEN_COLON; break;
    case ';': state.pos++; token->type = TOKEN_SEMICOLON; break;
    case '.': state.pos++; token->type = TOKEN_DOT; break;
    case ',': state.pos++; token->type = TOKEN_COMMA; break;

    default:
    {
        if (IsAlpha(c))
        {
            // Identifier
            size_t ident_length = 0;
            while (LengthLeft(state, ident_length) > 0 &&
                   IsAlphaNum(state.text[state.pos + ident_length]))
            {
                ident_length++;
            }

            token->type = TOKEN_IDENT;
            token->str = egNullTerminate(allocator, &state.text[state.pos], ident_length);
            state.pos += ident_length;
        }
        else
        {
            token->type = TOKEN_ERROR;
            token->str = egSprintf(allocator, "unknown token: '%c'", state.text[state.pos]);
            state.pos++;
        }
        break;
    }
    }

    return state;
}

void egTokenizerFreeToken(EgAllocator *allocator, EgToken token)
{
    switch (token.type)
    {
    case TOKEN_STRING:
    case TOKEN_IDENT:
    {
        egFree(allocator, (void*)token.str);
        break;
    }
    default: break;
    }
}
