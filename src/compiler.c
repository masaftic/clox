#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "chunk.h"
#include "scanner.h"
#include "object.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,       // least
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY     // highest
} Precedence;

typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;
Chunk *compilingChunk;

static Chunk *current_chunk()
{
    return compilingChunk;
}

static void errorAt(Token *token, const char *message)
{
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR) {
        // Nothing.
    }
    else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char *message) 
{
    errorAt(&parser.previous, message);
}

static void error_at_current(const char *message)
{
    errorAt(&parser.current, message);
}

static void advance()
{
    parser.previous = parser.current;

    for (;;) {
        parser.current = scan_token();
        if (parser.current.type != TOKEN_ERROR) break;

        error_at_current(parser.current.start);
    }
}

static bool check(TokenType type)
{
    return parser.current.type == type;
}

static void consume(TokenType type, const char *message)
{
    if (check(type)) {
        advance();
        return;
    }

    error_at_current(message);
}

static bool match(TokenType type)
{
    if (!check(type)) return false;
    advance();
    return true;
}

static void emit_byte(uint8_t byte)
{
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

static void emit_return()
{
    emit_byte(OP_RETURN);
}

/// @brief pushes a constant {value} into the current chunk
/// @param value 
/// @return constant's index 
static uint8_t make_constant(Value value)
{
    int constant_index = add_constant(current_chunk(), value);
    if (constant_index > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant_index;
}

static void emit_constant(Value value)
{
    emit_bytes(OP_CONSTANT, make_constant(value));
}

static void end_compiler()
{
    emit_return();

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassemble_chunk(current_chunk(), "code");
    }
#endif
}


static void expression();
static void statement();
static void declaration();
static ParseRule* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);


/// @brief add token.lexeme into chunk's constants
/// @param name 
/// @return index of that constant
static uint8_t identifier_constant(Token *name)
{
    return make_constant(OBJ_VAL(copy_string(name->start, name->length)));
}

static uint8_t parse_variable(const char *errorMessage)
{
    consume(TOKEN_IDENTIFIER, errorMessage);
    return identifier_constant(&parser.previous);
}

static void define_variable(uint8_t global)
{
    emit_bytes(OP_DEFINE_GLOBAL, global);
}

static void binary()
{
    TokenType operatorType = parser.previous.type;
    ParseRule *rule = get_rule(operatorType);
    parse_precedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
    case TOKEN_BANG_EQUAL:    
        emit_bytes(OP_EQUAL, OP_NOT); 
        break;
    case TOKEN_EQUAL_EQUAL:   
        emit_byte(OP_EQUAL); 
        break;
    case TOKEN_GREATER:       
        emit_byte(OP_GREATER); 
        break;
    case TOKEN_GREATER_EQUAL: 
        emit_bytes(OP_LESS, OP_NOT); 
        break;
    case TOKEN_LESS:          
        emit_byte(OP_LESS); 
        break;
    case TOKEN_LESS_EQUAL:    
        emit_bytes(OP_GREATER, OP_NOT); 
        break;

    case TOKEN_PLUS:
        emit_byte(OP_ADD);
        break;
    case TOKEN_MINUS:
        emit_byte(OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        emit_byte(OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emit_byte(OP_DIVIDE);
        break;
    
    default:
        return;
    }
}

static void literal()
{
    switch (parser.previous.type)
    {
    case TOKEN_FALSE:
        emit_byte(OP_FALSE);
        break;
    case TOKEN_NIL:
        emit_byte(OP_NIL);
        break;
    case TOKEN_TRUE:
        emit_byte(OP_TRUE);
        break;

    default:
        return; // Unreachable
    }
}

static void synchronize()
{
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
        case TOKEN_CLASS:
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_PRINT:
        case TOKEN_RETURN:
            return;

        default:
            ; // Do nothing.
        }
        
        advance();
    }
}

static void expression()
{
    parse_precedence(PREC_ASSIGNMENT);
}

static void var_declaration() {
    uint8_t global = parse_variable("Expect variable name.");
    
    if (match(TOKEN_EQUAL)) {
        expression();
    }
    else {
        emit_byte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    define_variable(global);
}

static void print_statement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(OP_PRINT);
}

static void expression_statement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emit_byte(OP_POP);
}

static void declaration()
{
    if (match(TOKEN_VAR)) {
        var_declaration();
    }
    else {
        statement();
    }

    if (parser.panicMode) synchronize();
}

static void statement()
{
    if (match(TOKEN_PRINT)) {
        print_statement();
    }
    else {
        expression_statement();
    }
}

static void grouping()
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void number()
{
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void string()
{
    emit_constant(OBJ_VAL(copy_string(parser.previous.start + 1, 
                                      parser.previous.length - 2)));
}

static void named_variable(Token name)
{
    uint8_t arg = identifier_constant(&name);
    emit_bytes(OP_GET_GLOBAL, arg);
}

static void variable()
{
    named_variable(parser.previous);
}

static void unary()
{
    TokenType operaterType = parser.previous.type;

    parse_precedence(PREC_UNARY);

    switch (operaterType) {
    case TOKEN_BANG:
        emit_byte(OP_NOT);
        break;
    
    case TOKEN_MINUS:
        emit_byte(OP_NEGATE);
        break;
 
    
    default:
        return;
    }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
    [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static ParseRule *get_rule(TokenType type)
{
    return &rules[type];
}

static void parse_precedence(Precedence precedence)
{
    advance();
    ParseFn prefixRule = get_rule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression");
        return;
    }

    prefixRule();

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = get_rule(parser.previous.type)->infix;
        infixRule();
    }
    
}



bool compile(const char *source, Chunk *chunk) 
{
    init_scanner(source);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }
    end_compiler();
    return !parser.hadError;
}