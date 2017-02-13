/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ast-lexer.h"

#include <assert.h>
#include <stdio.h>

#include "config.h"

#include "allocator.h"
#include "ast-parser.h"
#include "ast-parser-lexer-shared.h"
#include "vector.h"

/* must be included after so some typedefs will be defined */
#include "ast-parser-gen.h"

/*!max:re2c */

#define INITIAL_LEXER_BUFFER_SIZE (64 * 1024)

#define YY_USER_ACTION                        \
  {                                           \
    loc->filename = lexer->filename;          \
    loc->line = lexer->line;                  \
    loc->first_column = COLUMN(lexer->token); \
    loc->last_column = COLUMN(lexer->cursor); \
  }

#define RETURN(name) \
  YY_USER_ACTION;    \
  return WASM_TOKEN_TYPE_##name

#define ERROR(...) \
  YY_USER_ACTION;  \
  wasm_ast_parser_error(loc, lexer, parser, __VA_ARGS__)

#define BEGIN(c) \
  do {           \
    cond = c;    \
  } while (0)
#define FILL(n)                                     \
  do {                                              \
    if (WASM_FAILED(fill(loc, lexer, parser, n))) { \
      RETURN(EOF);                                  \
      continue;                                     \
    }                                               \
  } while (0)

#define yytext (lexer->token)
#define yyleng (lexer->cursor - lexer->token)

/* p must be a pointer somewhere in the lexer buffer */
#define FILE_OFFSET(p) ((p) - (lexer->buffer) + lexer->buffer_file_offset)
#define COLUMN(p) (FILE_OFFSET(p) - lexer->line_file_offset + 1)

#define COMMENT_NESTING (lexer->comment_nesting)
#define NEWLINE                                           \
  do {                                                    \
    lexer->line++;                                        \
    lexer->line_file_offset = FILE_OFFSET(lexer->cursor); \
  } while (0)

#define TEXT                 \
  lval->text.start = yytext; \
  lval->text.length = yyleng

#define TEXT_AT(offset)               \
  lval->text.start = yytext + offset; \
  lval->text.length = yyleng - offset

#define TYPE(type_) lval->type = WASM_TYPE_##type_

#define OPCODE(name) lval->opcode = WASM_OPCODE_##name

#define LITERAL(type_)                      \
  lval->literal.type = WASM_LITERAL_TYPE_##type_; \
  lval->literal.text.start = yytext;              \
  lval->literal.text.length = yyleng

static WasmResult fill(WasmLocation* loc,
                       WasmAstLexer* lexer,
                       WasmAstParser* parser,
                       size_t need) {
  if (lexer->eof)
    return WASM_ERROR;
  size_t free = lexer->token - lexer->buffer;
  assert((size_t)(lexer->cursor - lexer->buffer) >= free);
  /* our buffer is too small, need to realloc */
  if (free < need) {
    char* old_buffer = lexer->buffer;
    size_t old_buffer_size = lexer->buffer_size;
    size_t new_buffer_size =
        old_buffer_size ? old_buffer_size * 2 : INITIAL_LEXER_BUFFER_SIZE;
    /* make sure there is enough space for the bytes requested (need) and an
     * additional YYMAXFILL bytes which is needed for the re2c lexer
     * implementation when the eof is reached */
    while ((new_buffer_size - old_buffer_size) + free < need + YYMAXFILL)
      new_buffer_size *= 2;

    /* TODO(binji): could just alloc instead, because we know we'll need to
     * memmove below */
    char* new_buffer = wasm_realloc(lexer->allocator, lexer->buffer,
                                    new_buffer_size, WASM_DEFAULT_ALIGN);
    if (new_buffer == NULL) {
      wasm_ast_parser_error(loc, lexer, parser,
                            "unable to reallocate lexer buffer.");
      return WASM_ERROR;
    }
    memmove(new_buffer, lexer->token, lexer->limit - lexer->token);
    lexer->buffer = new_buffer;
    lexer->buffer_size = new_buffer_size;
    lexer->token = new_buffer + (lexer->token - old_buffer) - free;
    lexer->marker = new_buffer + (lexer->marker - old_buffer) - free;
    lexer->cursor = new_buffer + (lexer->cursor - old_buffer) - free;
    lexer->limit = new_buffer + (lexer->limit - old_buffer) - free;
    lexer->buffer_file_offset += free;
    free += new_buffer_size - old_buffer_size;
  } else {
    /* shift everything down to make more room in the buffer */
    memmove(lexer->buffer, lexer->token, lexer->limit - lexer->token);
    lexer->token -= free;
    lexer->marker -= free;
    lexer->cursor -= free;
    lexer->limit -= free;
    lexer->buffer_file_offset += free;
  }
  /* read the new data into the buffer */
  if (lexer->source.type == WASM_LEXER_SOURCE_TYPE_FILE) {
    lexer->limit += fread(lexer->limit, 1, free, lexer->source.file);
  } else {
    /* TODO(binji): could lex directly from buffer */
    assert(lexer->source.type == WASM_LEXER_SOURCE_TYPE_BUFFER);
    size_t read_size = free;
    size_t offset = lexer->source.buffer.read_offset;
    size_t bytes_left = lexer->source.buffer.size - offset;
    if (read_size > bytes_left)
      read_size = bytes_left;
    memcpy(lexer->buffer, (char*)lexer->source.buffer.data + offset,
           read_size);
    lexer->source.buffer.read_offset += read_size;
    lexer->limit += read_size;
  }
  /* if at the end of file, need to fill YYMAXFILL more characters with "fake
   * characters", that are not a lexeme nor a lexeme suffix. see
   * http://re2c.org/examples/example_03.html */
  if (lexer->limit < lexer->buffer + lexer->buffer_size - YYMAXFILL) {
    lexer->eof = WASM_TRUE;
    memset(lexer->limit, 0, YYMAXFILL);
    lexer->limit += YYMAXFILL;
  }
  return WASM_OK;
}

int wasm_ast_lexer_lex(WASM_AST_PARSER_STYPE* lval,
                       WASM_AST_PARSER_LTYPE* loc,
                       WasmAstLexer* lexer,
                       WasmAstParser* parser) {
  enum {
    YYCOND_INIT,
    YYCOND_BAD_TEXT,
    YYCOND_LINE_COMMENT,
    YYCOND_BLOCK_COMMENT,
    YYCOND_i = YYCOND_INIT,
  } cond = YYCOND_INIT;

  for (;;) {
    lexer->token = lexer->cursor;
  /*!re2c
    re2c:condprefix = YYCOND_;
    re2c:condenumprefix = YYCOND_;
    re2c:define:YYCTYPE = "unsigned char";
    re2c:define:YYCURSOR = lexer->cursor;
    re2c:define:YYMARKER = lexer->marker;
    re2c:define:YYLIMIT = lexer->limit;
    re2c:define:YYFILL = "FILL";
    re2c:define:YYGETCONDITION = "cond";
    re2c:define:YYGETCONDITION:naked = 1;
    re2c:define:YYSETCONDITION = "BEGIN";

    space =     [ \t];
    digit =     [0-9];
    digits =    [0-9]+;
    hexdigit =  [0-9a-fA-F];
    letter =    [a-zA-Z];
    symbol =    [+\-*\/\\\^~=<>!?@#$%&|:`.];
    tick =      "'";
    escape =    [nt\\'"];
    character = [^"\\\x00-\x1f\x7f] | "\\" escape | "\\" hexdigit hexdigit;
    sign =      [+-];
    num =       digit+;
    hexnum =    "0x" hexdigit+;
    nat =       num | hexnum;
    int =       sign nat;
    float0 =    sign? num "." digit*;
    float1 =    sign? num ("." digit*)? [eE] sign? num;
    hexfloat =  sign? "0x" hexdigit+ "."? hexdigit* "p" sign? digit+;
    infinity =  sign? ("inf" | "infinity");
    nan =       sign? "nan" | sign? "nan:0x" hexdigit+;
    float =     float0 | float1;
    text =      '"' character* '"';
    atom =      (letter | digit | "_" | tick | symbol)+;
    name =      "$" atom;
    EOF =       "\x00";

    <i> "("                   { RETURN(LPAR); }
    <i> ")"                   { RETURN(RPAR); }
    <i> nat                   { LITERAL(INT); RETURN(NAT); }
    <i> int                   { LITERAL(INT); RETURN(INT); }
    <i> float                 { LITERAL(FLOAT); RETURN(FLOAT); }
    <i> hexfloat              { LITERAL(HEXFLOAT); RETURN(FLOAT); }
    <i> infinity              { LITERAL(INFINITY); RETURN(FLOAT); }
    <i> nan                   { LITERAL(NAN); RETURN(FLOAT); }
    <i> text                  { TEXT; RETURN(TEXT); }
    <i> '"' => BAD_TEXT       { continue; }
    <BAD_TEXT> character      { continue; }
    <BAD_TEXT> "\n" => i      { ERROR("newline in string"); NEWLINE; continue; }
    <BAD_TEXT> "\\".          { ERROR("bad escape \"%.*s\"", (int)yyleng, yytext);
                                continue; }
    <BAD_TEXT> '"' => i       { TEXT; RETURN(TEXT); }
    <BAD_TEXT> EOF            { ERROR("unexpected EOF"); RETURN(EOF); }
    <BAD_TEXT> [^]            { ERROR("illegal character in string"); continue; }
    <i> "i32"                 { TYPE(I32); RETURN(VALUE_TYPE); }
    <i> "i64"                 { TYPE(I64); RETURN(VALUE_TYPE); }
    <i> "f32"                 { TYPE(F32); RETURN(VALUE_TYPE); }
    <i> "f64"                 { TYPE(F64); RETURN(VALUE_TYPE); }
    <i> "f32x4"               { TYPE(F32X4); RETURN(VALUE_TYPE); }
    <i> "i32x4"               { TYPE(I32X4); RETURN(VALUE_TYPE); }
    <i> "i16x8"               { TYPE(I16X8); RETURN(VALUE_TYPE); }
    <i> "i8x16"               { TYPE(I8X16); RETURN(VALUE_TYPE); }
    <i> "b32x4"               { TYPE(B32X4); RETURN(VALUE_TYPE); }
    <i> "b16x8"               { TYPE(B16X8); RETURN(VALUE_TYPE); }
    <i> "b8x16"               { TYPE(B8X16); RETURN(VALUE_TYPE); }
    <i> "b64x2"               { TYPE(B64X2); RETURN(VALUE_TYPE); }
    <i> "i64x2"               { TYPE(I64X2); RETURN(VALUE_TYPE); }
    <i> "f64x2"               { TYPE(F64X2); RETURN(VALUE_TYPE); }
    <i> "anyfunc"             { RETURN(ANYFUNC); }
    <i> "mut"                 { RETURN(MUT); }
    <i> "nop"                 { RETURN(NOP); }
    <i> "block"               { RETURN(BLOCK); }
    <i> "if"                  { RETURN(IF); }
    <i> "if_else"             { RETURN(IF); }
    <i> "then"                { RETURN(THEN); }
    <i> "else"                { RETURN(ELSE); }
    <i> "loop"                { RETURN(LOOP); }
    <i> "br"                  { RETURN(BR); }
    <i> "br_if"               { RETURN(BR_IF); }
    <i> "br_table"            { RETURN(BR_TABLE); }
    <i> "call"                { RETURN(CALL); }
    <i> "call_import"         { RETURN(CALL_IMPORT); }
    <i> "call_indirect"       { RETURN(CALL_INDIRECT); }
    <i> "drop"                { RETURN(DROP); }
    <i> "end"                 { RETURN(END); }
    <i> "return"              { RETURN(RETURN); }
    <i> "get_local"           { RETURN(GET_LOCAL); }
    <i> "set_local"           { RETURN(SET_LOCAL); }
    <i> "tee_local"           { RETURN(TEE_LOCAL); }
    <i> "get_global"          { RETURN(GET_GLOBAL); }
    <i> "set_global"          { RETURN(SET_GLOBAL); }
    <i> "i32.load"            { OPCODE(I32_LOAD); RETURN(LOAD); }
    <i> "i64.load"            { OPCODE(I64_LOAD); RETURN(LOAD); }
    <i> "f32.load"            { OPCODE(F32_LOAD); RETURN(LOAD); }
    <i> "f64.load"            { OPCODE(F64_LOAD); RETURN(LOAD); }
    <i> "i32.store"           { OPCODE(I32_STORE); RETURN(STORE); }
    <i> "i64.store"           { OPCODE(I64_STORE); RETURN(STORE); }
    <i> "f32.store"           { OPCODE(F32_STORE); RETURN(STORE); }
    <i> "f64.store"           { OPCODE(F64_STORE); RETURN(STORE); }
    <i> "i32.load8_s"         { OPCODE(I32_LOAD8_S); RETURN(LOAD); }
    <i> "i64.load8_s"         { OPCODE(I64_LOAD8_S); RETURN(LOAD); }
    <i> "i32.load8_u"         { OPCODE(I32_LOAD8_U); RETURN(LOAD); }
    <i> "i64.load8_u"         { OPCODE(I64_LOAD8_U); RETURN(LOAD); }
    <i> "i32.load16_s"        { OPCODE(I32_LOAD16_S); RETURN(LOAD); }
    <i> "i64.load16_s"        { OPCODE(I64_LOAD16_S); RETURN(LOAD); }
    <i> "i32.load16_u"        { OPCODE(I32_LOAD16_U); RETURN(LOAD); }
    <i> "i64.load16_u"        { OPCODE(I64_LOAD16_U); RETURN(LOAD); }
    <i> "i64.load32_s"        { OPCODE(I64_LOAD32_S); RETURN(LOAD); }
    <i> "i64.load32_u"        { OPCODE(I64_LOAD32_U); RETURN(LOAD); }
    <i> "i32.store8"          { OPCODE(I32_STORE8); RETURN(STORE); }
    <i> "i64.store8"          { OPCODE(I64_STORE8); RETURN(STORE); }
    <i> "i32.store16"         { OPCODE(I32_STORE16); RETURN(STORE); }
    <i> "i64.store16"         { OPCODE(I64_STORE16); RETURN(STORE); }
    <i> "i64.store32"         { OPCODE(I64_STORE32); RETURN(STORE); }
    <i> "offset=" nat         { TEXT_AT(7); RETURN(OFFSET_EQ_NAT); }
    <i> "align=" nat          { TEXT_AT(6); RETURN(ALIGN_EQ_NAT); }
    <i> "i32.const"           { TYPE(I32); RETURN(CONST); }
    <i> "i64.const"           { TYPE(I64); RETURN(CONST); }
    <i> "f32.const"           { TYPE(F32); RETURN(CONST); }
    <i> "f64.const"           { TYPE(F64); RETURN(CONST); }
    <i> "i32.eqz"             { OPCODE(I32_EQZ); RETURN(CONVERT); }
    <i> "i64.eqz"             { OPCODE(I64_EQZ); RETURN(CONVERT); }
    <i> "i32.clz"             { OPCODE(I32_CLZ); RETURN(UNARY); }
    <i> "i64.clz"             { OPCODE(I64_CLZ); RETURN(UNARY); }
    <i> "i32.ctz"             { OPCODE(I32_CTZ); RETURN(UNARY); }
    <i> "i64.ctz"             { OPCODE(I64_CTZ); RETURN(UNARY); }
    <i> "i32.popcnt"          { OPCODE(I32_POPCNT); RETURN(UNARY); }
    <i> "i64.popcnt"          { OPCODE(I64_POPCNT); RETURN(UNARY); }
    <i> "f32.neg"             { OPCODE(F32_NEG); RETURN(UNARY); }
    <i> "f64.neg"             { OPCODE(F64_NEG); RETURN(UNARY); }
    <i> "f32.abs"             { OPCODE(F32_ABS); RETURN(UNARY); }
    <i> "f64.abs"             { OPCODE(F64_ABS); RETURN(UNARY); }
    <i> "f32.sqrt"            { OPCODE(F32_SQRT); RETURN(UNARY); }
    <i> "f64.sqrt"            { OPCODE(F64_SQRT); RETURN(UNARY); }
    <i> "f32.ceil"            { OPCODE(F32_CEIL); RETURN(UNARY); }
    <i> "f64.ceil"            { OPCODE(F64_CEIL); RETURN(UNARY); }
    <i> "f32.floor"           { OPCODE(F32_FLOOR); RETURN(UNARY); }
    <i> "f64.floor"           { OPCODE(F64_FLOOR); RETURN(UNARY); }
    <i> "f32.trunc"           { OPCODE(F32_TRUNC); RETURN(UNARY); }
    <i> "f64.trunc"           { OPCODE(F64_TRUNC); RETURN(UNARY); }
    <i> "f32.nearest"         { OPCODE(F32_NEAREST); RETURN(UNARY); }
    <i> "f64.nearest"         { OPCODE(F64_NEAREST); RETURN(UNARY); }
    <i> "i32.add"             { OPCODE(I32_ADD); RETURN(BINARY); }
    <i> "i64.add"             { OPCODE(I64_ADD); RETURN(BINARY); }
    <i> "i32.sub"             { OPCODE(I32_SUB); RETURN(BINARY); }
    <i> "i64.sub"             { OPCODE(I64_SUB); RETURN(BINARY); }
    <i> "i32.mul"             { OPCODE(I32_MUL); RETURN(BINARY); }
    <i> "i64.mul"             { OPCODE(I64_MUL); RETURN(BINARY); }
    <i> "i32.div_s"           { OPCODE(I32_DIV_S); RETURN(BINARY); }
    <i> "i64.div_s"           { OPCODE(I64_DIV_S); RETURN(BINARY); }
    <i> "i32.div_u"           { OPCODE(I32_DIV_U); RETURN(BINARY); }
    <i> "i64.div_u"           { OPCODE(I64_DIV_U); RETURN(BINARY); }
    <i> "i32.rem_s"           { OPCODE(I32_REM_S); RETURN(BINARY); }
    <i> "i64.rem_s"           { OPCODE(I64_REM_S); RETURN(BINARY); }
    <i> "i32.rem_u"           { OPCODE(I32_REM_U); RETURN(BINARY); }
    <i> "i64.rem_u"           { OPCODE(I64_REM_U); RETURN(BINARY); }
    <i> "i32.and"             { OPCODE(I32_AND); RETURN(BINARY); }
    <i> "i64.and"             { OPCODE(I64_AND); RETURN(BINARY); }
    <i> "i32.or"              { OPCODE(I32_OR); RETURN(BINARY); }
    <i> "i64.or"              { OPCODE(I64_OR); RETURN(BINARY); }
    <i> "i32.xor"             { OPCODE(I32_XOR); RETURN(BINARY); }
    <i> "i64.xor"             { OPCODE(I64_XOR); RETURN(BINARY); }
    <i> "i32.shl"             { OPCODE(I32_SHL); RETURN(BINARY); }
    <i> "i64.shl"             { OPCODE(I64_SHL); RETURN(BINARY); }
    <i> "i32.shr_s"           { OPCODE(I32_SHR_S); RETURN(BINARY); }
    <i> "i64.shr_s"           { OPCODE(I64_SHR_S); RETURN(BINARY); }
    <i> "i32.shr_u"           { OPCODE(I32_SHR_U); RETURN(BINARY); }
    <i> "i64.shr_u"           { OPCODE(I64_SHR_U); RETURN(BINARY); }
    <i> "i32.rotl"            { OPCODE(I32_ROTL); RETURN(BINARY); }
    <i> "i64.rotl"            { OPCODE(I64_ROTL); RETURN(BINARY); }
    <i> "i32.rotr"            { OPCODE(I32_ROTR); RETURN(BINARY); }
    <i> "i64.rotr"            { OPCODE(I64_ROTR); RETURN(BINARY); }
    <i> "f32.add"             { OPCODE(F32_ADD); RETURN(BINARY); }
    <i> "f64.add"             { OPCODE(F64_ADD); RETURN(BINARY); }
    <i> "f32.sub"             { OPCODE(F32_SUB); RETURN(BINARY); }
    <i> "f64.sub"             { OPCODE(F64_SUB); RETURN(BINARY); }
    <i> "f32.mul"             { OPCODE(F32_MUL); RETURN(BINARY); }
    <i> "f64.mul"             { OPCODE(F64_MUL); RETURN(BINARY); }
    <i> "f32.div"             { OPCODE(F32_DIV); RETURN(BINARY); }
    <i> "f64.div"             { OPCODE(F64_DIV); RETURN(BINARY); }
    <i> "f32.min"             { OPCODE(F32_MIN); RETURN(BINARY); }
    <i> "f64.min"             { OPCODE(F64_MIN); RETURN(BINARY); }
    <i> "f32.max"             { OPCODE(F32_MAX); RETURN(BINARY); }
    <i> "f64.max"             { OPCODE(F64_MAX); RETURN(BINARY); }
    <i> "f32.copysign"        { OPCODE(F32_COPYSIGN); RETURN(BINARY); }
    <i> "f64.copysign"        { OPCODE(F64_COPYSIGN); RETURN(BINARY); }
    <i> "i32.eq"              { OPCODE(I32_EQ); RETURN(COMPARE); }
    <i> "i64.eq"              { OPCODE(I64_EQ); RETURN(COMPARE); }
    <i> "i32.ne"              { OPCODE(I32_NE); RETURN(COMPARE); }
    <i> "i64.ne"              { OPCODE(I64_NE); RETURN(COMPARE); }
    <i> "i32.lt_s"            { OPCODE(I32_LT_S); RETURN(COMPARE); }
    <i> "i64.lt_s"            { OPCODE(I64_LT_S); RETURN(COMPARE); }
    <i> "i32.lt_u"            { OPCODE(I32_LT_U); RETURN(COMPARE); }
    <i> "i64.lt_u"            { OPCODE(I64_LT_U); RETURN(COMPARE); }
    <i> "i32.le_s"            { OPCODE(I32_LE_S); RETURN(COMPARE); }
    <i> "i64.le_s"            { OPCODE(I64_LE_S); RETURN(COMPARE); }
    <i> "i32.le_u"            { OPCODE(I32_LE_U); RETURN(COMPARE); }
    <i> "i64.le_u"            { OPCODE(I64_LE_U); RETURN(COMPARE); }
    <i> "i32.gt_s"            { OPCODE(I32_GT_S); RETURN(COMPARE); }
    <i> "i64.gt_s"            { OPCODE(I64_GT_S); RETURN(COMPARE); }
    <i> "i32.gt_u"            { OPCODE(I32_GT_U); RETURN(COMPARE); }
    <i> "i64.gt_u"            { OPCODE(I64_GT_U); RETURN(COMPARE); }
    <i> "i32.ge_s"            { OPCODE(I32_GE_S); RETURN(COMPARE); }
    <i> "i64.ge_s"            { OPCODE(I64_GE_S); RETURN(COMPARE); }
    <i> "i32.ge_u"            { OPCODE(I32_GE_U); RETURN(COMPARE); }
    <i> "i64.ge_u"            { OPCODE(I64_GE_U); RETURN(COMPARE); }
    <i> "f32.eq"              { OPCODE(F32_EQ); RETURN(COMPARE); }
    <i> "f64.eq"              { OPCODE(F64_EQ); RETURN(COMPARE); }
    <i> "f32.ne"              { OPCODE(F32_NE); RETURN(COMPARE); }
    <i> "f64.ne"              { OPCODE(F64_NE); RETURN(COMPARE); }
    <i> "f32.lt"              { OPCODE(F32_LT); RETURN(COMPARE); }
    <i> "f64.lt"              { OPCODE(F64_LT); RETURN(COMPARE); }
    <i> "f32.le"              { OPCODE(F32_LE); RETURN(COMPARE); }
    <i> "f64.le"              { OPCODE(F64_LE); RETURN(COMPARE); }
    <i> "f32.gt"              { OPCODE(F32_GT); RETURN(COMPARE); }
    <i> "f64.gt"              { OPCODE(F64_GT); RETURN(COMPARE); }
    <i> "f32.ge"              { OPCODE(F32_GE); RETURN(COMPARE); }
    <i> "f64.ge"              { OPCODE(F64_GE); RETURN(COMPARE); }
    <i> "i64.extend_s/i32"    { OPCODE(I64_EXTEND_S_I32); RETURN(CONVERT); }
    <i> "i64.extend_u/i32"    { OPCODE(I64_EXTEND_U_I32); RETURN(CONVERT); }
    <i> "i32.wrap/i64"        { OPCODE(I32_WRAP_I64); RETURN(CONVERT); }
    <i> "i32.trunc_s/f32"     { OPCODE(I32_TRUNC_S_F32); RETURN(CONVERT); }
    <i> "i64.trunc_s/f32"     { OPCODE(I64_TRUNC_S_F32); RETURN(CONVERT); }
    <i> "i32.trunc_s/f64"     { OPCODE(I32_TRUNC_S_F64); RETURN(CONVERT); }
    <i> "i64.trunc_s/f64"     { OPCODE(I64_TRUNC_S_F64); RETURN(CONVERT); }
    <i> "i32.trunc_u/f32"     { OPCODE(I32_TRUNC_U_F32); RETURN(CONVERT); }
    <i> "i64.trunc_u/f32"     { OPCODE(I64_TRUNC_U_F32); RETURN(CONVERT); }
    <i> "i32.trunc_u/f64"     { OPCODE(I32_TRUNC_U_F64); RETURN(CONVERT); }
    <i> "i64.trunc_u/f64"     { OPCODE(I64_TRUNC_U_F64); RETURN(CONVERT); }
    <i> "f32.convert_s/i32"   { OPCODE(F32_CONVERT_S_I32); RETURN(CONVERT); }
    <i> "f64.convert_s/i32"   { OPCODE(F64_CONVERT_S_I32); RETURN(CONVERT); }
    <i> "f32.convert_s/i64"   { OPCODE(F32_CONVERT_S_I64); RETURN(CONVERT); }
    <i> "f64.convert_s/i64"   { OPCODE(F64_CONVERT_S_I64); RETURN(CONVERT); }
    <i> "f32.convert_u/i32"   { OPCODE(F32_CONVERT_U_I32); RETURN(CONVERT); }
    <i> "f64.convert_u/i32"   { OPCODE(F64_CONVERT_U_I32); RETURN(CONVERT); }
    <i> "f32.convert_u/i64"   { OPCODE(F32_CONVERT_U_I64); RETURN(CONVERT); }
    <i> "f64.convert_u/i64"   { OPCODE(F64_CONVERT_U_I64); RETURN(CONVERT); }
    <i> "f64.promote/f32"     { OPCODE(F64_PROMOTE_F32); RETURN(CONVERT); }
    <i> "f32.demote/f64"      { OPCODE(F32_DEMOTE_F64); RETURN(CONVERT); }
    <i> "f32.reinterpret/i32" { OPCODE(F32_REINTERPRET_I32); RETURN(CONVERT); }
    <i> "i32.reinterpret/f32" { OPCODE(I32_REINTERPRET_F32); RETURN(CONVERT); }
    <i> "f64.reinterpret/i64" { OPCODE(F64_REINTERPRET_I64); RETURN(CONVERT); }
    <i> "i64.reinterpret/f64" { OPCODE(I64_REINTERPRET_F64); RETURN(CONVERT); }

    <i> "b8x16.splat"         { OPCODE(B8X16_SPLAT); RETURN(UNARY); }
    <i> "b8x16.extractlane"         { OPCODE(B8X16_EXTRACT); RETURN(BINARY); }
    <i> "b8x16.and"         { OPCODE(B8X16_AND); RETURN(BINARY); }
    <i> "b8x16.or"         { OPCODE(B8X16_OR); RETURN(BINARY); }
    <i> "b8x16.xor"         { OPCODE(B8X16_XOR); RETURN(BINARY); }
    <i> "b8x16.not"         { OPCODE(B8X16_NOT); RETURN(UNARY); }
    <i> "b8x16.anytrue"         { OPCODE(B8X16_ANYTRUE); RETURN(UNARY); }
    <i> "b8x16.alltrue"         { OPCODE(B8X16_ALLTRUE); RETURN(UNARY); }
    <i> "b16x8.splat"         { OPCODE(B16X8_SPLAT); RETURN(UNARY); }
    <i> "b16x8.extractlane"         { OPCODE(B16X8_EXTRACT); RETURN(BINARY); }
    <i> "b16x8.and"         { OPCODE(B16X8_AND); RETURN(BINARY); }
    <i> "b16x8.or"         { OPCODE(B16X8_OR); RETURN(BINARY); }
    <i> "b16x8.xor"         { OPCODE(B16X8_XOR); RETURN(BINARY); }
    <i> "b16x8.not"         { OPCODE(B16X8_NOT); RETURN(UNARY); }
    <i> "b16x8.anytrue"         { OPCODE(B16X8_ANYTRUE); RETURN(UNARY); }
    <i> "b16x8.alltrue"         { OPCODE(B16X8_ALLTRUE); RETURN(UNARY); }
    <i> "b32x4.splat"         { OPCODE(B32X4_SPLAT); RETURN(UNARY); }
    <i> "b32x4.extractlane"         { OPCODE(B32X4_EXTRACT); RETURN(BINARY); }
    <i> "b32x4.and"         { OPCODE(B32X4_AND); RETURN(BINARY); }
    <i> "b32x4.or"         { OPCODE(B32X4_OR); RETURN(BINARY); }
    <i> "b32x4.xor"         { OPCODE(B32X4_XOR); RETURN(BINARY); }
    <i> "b32x4.not"         { OPCODE(B32X4_NOT); RETURN(UNARY); }
    <i> "b32x4.anytrue"         { OPCODE(B32X4_ANYTRUE); RETURN(UNARY); }
    <i> "b32x4.alltrue"         { OPCODE(B32X4_ALLTRUE); RETURN(UNARY); }
    <i> "b64x2.splat"         { OPCODE(B64X2_SPLAT); RETURN(UNARY); }
    <i> "b64x2.extractlane"         { OPCODE(B64X2_EXTRACT); RETURN(BINARY); }
    <i> "b64x2.and"         { OPCODE(B64X2_AND); RETURN(BINARY); }
    <i> "b64x2.or"         { OPCODE(B64X2_OR); RETURN(BINARY); }
    <i> "b64x2.xor"         { OPCODE(B64X2_XOR); RETURN(BINARY); }
    <i> "b64x2.not"         { OPCODE(B64X2_NOT); RETURN(UNARY); }
    <i> "b64x2.anytrue"         { OPCODE(B64X2_ANYTRUE); RETURN(UNARY); }
    <i> "b64x2.alltrue"         { OPCODE(B64X2_ALLTRUE); RETURN(UNARY); }
    <i> "i8x16.reinterpret_i16x8"         { OPCODE(I8X16_REINTERPRET_I16X8); RETURN(UNARY); }
    <i> "i8x16.reinterpret_i32x4"         { OPCODE(I8X16_REINTERPRET_I32X4); RETURN(UNARY); }
    <i> "i8x16.reinterpret_i64x2"         { OPCODE(I8X16_REINTERPRET_I64X2); RETURN(UNARY); }
    <i> "i8x16.reinterpret_f32x4"         { OPCODE(I8X16_REINTERPRET_F32X4); RETURN(UNARY); }
    <i> "i8x16.reinterpret_f64x2"         { OPCODE(I8X16_REINTERPRET_F64X2); RETURN(UNARY); }
    <i> "i8x16.splat"         { OPCODE(I8X16_SPLAT); RETURN(UNARY); }
    <i> "i8x16.extractlane_s"         { OPCODE(I8X16_EXTRACT_S); RETURN(BINARY); }
    <i> "i8x16.extractlane_u"         { OPCODE(I8X16_EXTRACT_U); RETURN(BINARY); }
    <i> "i8x16.add"         { OPCODE(I8X16_ADD); RETURN(BINARY); }
    <i> "i8x16.sub"         { OPCODE(I8X16_SUB); RETURN(BINARY); }
    <i> "i8x16.mul"         { OPCODE(I8X16_MUL); RETURN(BINARY); }
    <i> "i8x16.neg"         { OPCODE(I8X16_NEG); RETURN(UNARY); }
    <i> "i8x16.addsaturate_s"         { OPCODE(I8X16_ADDSATURATE_S); RETURN(BINARY); }
    <i> "i8x16.addsaturate_u"         { OPCODE(I8X16_ADDSATURATE_U); RETURN(BINARY); }
    <i> "i8x16.subsaturate_s"         { OPCODE(I8X16_SUBSATURATE_S); RETURN(BINARY); }
    <i> "i8x16.subsaturate_u"         { OPCODE(I8X16_SUBSATURATE_U); RETURN(BINARY); }
    <i> "i8x16.shl"         { OPCODE(I8X16_SHL); RETURN(BINARY); }
    <i> "i8x16.shr_s"         { OPCODE(I8X16_SHR_S); RETURN(BINARY); }
    <i> "i8x16.shr_u"         { OPCODE(I8X16_SHR_U); RETURN(BINARY); }
    <i> "i8x16.and"         { OPCODE(I8X16_AND); RETURN(BINARY); }
    <i> "i8x16.or"         { OPCODE(I8X16_OR); RETURN(BINARY); }
    <i> "i8x16.xor"         { OPCODE(I8X16_XOR); RETURN(BINARY); }
    <i> "i8x16.not"         { OPCODE(I8X16_NOT); RETURN(UNARY); }
    <i> "i8x16.eq"         { OPCODE(I8X16_EQ); RETURN(BINARY); }
    <i> "i8x16.ne"         { OPCODE(I8X16_NE); RETURN(BINARY); }
    <i> "i8x16.lt_s"         { OPCODE(I8X16_LT_S); RETURN(BINARY); }
    <i> "i8x16.lt_u"         { OPCODE(I8X16_LT_U); RETURN(BINARY); }
    <i> "i8x16.le_s"         { OPCODE(I8X16_LE_S); RETURN(BINARY); }
    <i> "i8x16.le_u"         { OPCODE(I8X16_LE_U); RETURN(BINARY); }
    <i> "i8x16.gt_s"         { OPCODE(I8X16_GT_S); RETURN(BINARY); }
    <i> "i8x16.gt_u"         { OPCODE(I8X16_GT_U); RETURN(BINARY); }
    <i> "i8x16.ge_s"         { OPCODE(I8X16_GE_S); RETURN(BINARY); }
    <i> "i8x16.ge_u"         { OPCODE(I8X16_GE_U); RETURN(BINARY); }
    <i> "i8x16.load"         { OPCODE(I8X16_LOAD); RETURN(LOAD); }
    <i> "i8x16.store"         { OPCODE(I8X16_STORE); RETURN(STORE); }
    <i> "i16x8.reinterpret_i8x16"         { OPCODE(I16X8_REINTERPRET_I8X16); RETURN(UNARY); }
    <i> "i16x8.reinterpret_i32x4"         { OPCODE(I16X8_REINTERPRET_I32X4); RETURN(UNARY); }
    <i> "i16x8.reinterpret_i64x2"         { OPCODE(I16X8_REINTERPRET_I64X2); RETURN(UNARY); }
    <i> "i16x8.reinterpret_f32x4"         { OPCODE(I16X8_REINTERPRET_F32X4); RETURN(UNARY); }
    <i> "i16x8.reinterpret_f64x2"         { OPCODE(I16X8_REINTERPRET_F64X2); RETURN(UNARY); }
    <i> "i16x8.splat"         { OPCODE(I16X8_SPLAT); RETURN(UNARY); }
    <i> "i16x8.extractlane_s"         { OPCODE(I16X8_EXTRACT_S); RETURN(BINARY); }
    <i> "i16x8.extractlane_u"         { OPCODE(I16X8_EXTRACT_U); RETURN(BINARY); }
    <i> "i16x8.add"         { OPCODE(I16X8_ADD); RETURN(BINARY); }
    <i> "i16x8.sub"         { OPCODE(I16X8_SUB); RETURN(BINARY); }
    <i> "i16x8.mul"         { OPCODE(I16X8_MUL); RETURN(BINARY); }
    <i> "i16x8.neg"         { OPCODE(I16X8_NEG); RETURN(UNARY); }
    <i> "i16x8.addsaturate_s"         { OPCODE(I16X8_ADDSATURATE_S); RETURN(BINARY); }
    <i> "i16x8.addsaturate_u"         { OPCODE(I16X8_ADDSATURATE_U); RETURN(BINARY); }
    <i> "i16x8.subsaturate_s"         { OPCODE(I16X8_SUBSATURATE_S); RETURN(BINARY); }
    <i> "i16x8.subsaturate_u"         { OPCODE(I16X8_SUBSATURATE_U); RETURN(BINARY); }
    <i> "i16x8.shl"         { OPCODE(I16X8_SHL); RETURN(BINARY); }
    <i> "i16x8.shr_s"         { OPCODE(I16X8_SHR_S); RETURN(BINARY); }
    <i> "i16x8.shr_u"         { OPCODE(I16X8_SHR_U); RETURN(BINARY); }
    <i> "i16x8.and"         { OPCODE(I16X8_AND); RETURN(BINARY); }
    <i> "i16x8.or"         { OPCODE(I16X8_OR); RETURN(BINARY); }
    <i> "i16x8.xor"         { OPCODE(I16X8_XOR); RETURN(BINARY); }
    <i> "i16x8.not"         { OPCODE(I16X8_NOT); RETURN(UNARY); }
    <i> "i16x8.eq"         { OPCODE(I16X8_EQ); RETURN(BINARY); }
    <i> "i16x8.ne"         { OPCODE(I16X8_NE); RETURN(BINARY); }
    <i> "i16x8.lt_s"         { OPCODE(I16X8_LT_S); RETURN(BINARY); }
    <i> "i16x8.lt_u"         { OPCODE(I16X8_LT_U); RETURN(BINARY); }
    <i> "i16x8.le_s"         { OPCODE(I16X8_LE_S); RETURN(BINARY); }
    <i> "i16x8.le_u"         { OPCODE(I16X8_LE_U); RETURN(BINARY); }
    <i> "i16x8.gt_s"         { OPCODE(I16X8_GT_S); RETURN(BINARY); }
    <i> "i16x8.gt_u"         { OPCODE(I16X8_GT_U); RETURN(BINARY); }
    <i> "i16x8.ge_s"         { OPCODE(I16X8_GE_S); RETURN(BINARY); }
    <i> "i16x8.ge_u"         { OPCODE(I16X8_GE_U); RETURN(BINARY); }
    <i> "i16x8.load"         { OPCODE(I16X8_LOAD); RETURN(LOAD); }
    <i> "i16x8.store"         { OPCODE(I16X8_STORE); RETURN(STORE); }
    <i> "i32x4.reinterpret_i8x16"         { OPCODE(I32X4_REINTERPRET_I8X16); RETURN(UNARY); }
    <i> "i32x4.reinterpret_i16x8"         { OPCODE(I32X4_REINTERPRET_I16X8); RETURN(UNARY); }
    <i> "i32x4.reinterpret_i64x2"         { OPCODE(I32X4_REINTERPRET_I64X2); RETURN(UNARY); }
    <i> "i32x4.reinterpret_f32x4"         { OPCODE(I32X4_REINTERPRET_F32X4); RETURN(UNARY); }
    <i> "i32x4.reinterpret_f64x2"         { OPCODE(I32X4_REINTERPRET_F64X2); RETURN(UNARY); }
    <i> "i32x4.splat"         { OPCODE(I32X4_SPLAT); RETURN(UNARY); }
    <i> "i32x4.extractlane"         { OPCODE(I32X4_EXTRACT); RETURN(BINARY); }
    <i> "i32x4.add"         { OPCODE(I32X4_ADD); RETURN(BINARY); }
    <i> "i32x4.sub"         { OPCODE(I32X4_SUB); RETURN(BINARY); }
    <i> "i32x4.mul"         { OPCODE(I32X4_MUL); RETURN(BINARY); }
    <i> "i32x4.neg"         { OPCODE(I32X4_NEG); RETURN(UNARY); }
    <i> "i32x4.shl"         { OPCODE(I32X4_SHL); RETURN(BINARY); }
    <i> "i32x4.shr_s"         { OPCODE(I32X4_SHR_S); RETURN(BINARY); }
    <i> "i32x4.shr_u"         { OPCODE(I32X4_SHR_U); RETURN(BINARY); }
    <i> "i32x4.and"         { OPCODE(I32X4_AND); RETURN(BINARY); }
    <i> "i32x4.or"         { OPCODE(I32X4_OR); RETURN(BINARY); }
    <i> "i32x4.xor"         { OPCODE(I32X4_XOR); RETURN(BINARY); }
    <i> "i32x4.not"         { OPCODE(I32X4_NOT); RETURN(UNARY); }
    <i> "i32x4.eq"         { OPCODE(I32X4_EQ); RETURN(BINARY); }
    <i> "i32x4.ne"         { OPCODE(I32X4_NE); RETURN(BINARY); }
    <i> "i32x4.lt_s"         { OPCODE(I32X4_LT_S); RETURN(BINARY); }
    <i> "i32x4.lt_u"         { OPCODE(I32X4_LT_U); RETURN(BINARY); }
    <i> "i32x4.le_s"         { OPCODE(I32X4_LE_S); RETURN(BINARY); }
    <i> "i32x4.le_u"         { OPCODE(I32X4_LE_U); RETURN(BINARY); }
    <i> "i32x4.gt_s"         { OPCODE(I32X4_GT_S); RETURN(BINARY); }
    <i> "i32x4.gt_u"         { OPCODE(I32X4_GT_U); RETURN(BINARY); }
    <i> "i32x4.ge_s"         { OPCODE(I32X4_GE_S); RETURN(BINARY); }
    <i> "i32x4.ge_u"         { OPCODE(I32X4_GE_U); RETURN(BINARY); }
    <i> "i32x4.load"         { OPCODE(I32X4_LOAD); RETURN(LOAD); }
    <i> "i32x4.store"         { OPCODE(I32X4_STORE); RETURN(STORE); }
    <i> "i32x4.load1"         { OPCODE(I32X4_LOAD1); RETURN(LOAD); }
    <i> "i32x4.load2"         { OPCODE(I32X4_LOAD2); RETURN(LOAD); }
    <i> "i32x4.load3"         { OPCODE(I32X4_LOAD3); RETURN(LOAD); }
    <i> "i32x4.store1"         { OPCODE(I32X4_STORE1); RETURN(STORE); }
    <i> "i32x4.store2"         { OPCODE(I32X4_STORE2); RETURN(STORE); }
    <i> "i32x4.store3"         { OPCODE(I32X4_STORE3); RETURN(STORE); }
    <i> "i32x4.trunc_s"         { OPCODE(I32X4_TRUNC_S); RETURN(UNARY); }
    <i> "i32x4.trunc_u"         { OPCODE(I32X4_TRUNC_U); RETURN(UNARY); }
    <i> "i64x2.reinterpret_i8x16"         { OPCODE(I64X2_REINTERPRET_I8X16); RETURN(UNARY); }
    <i> "i64x2.reinterpret_i16x8"         { OPCODE(I64X2_REINTERPRET_I16X8); RETURN(UNARY); }
    <i> "i64x2.reinterpret_i32x4"         { OPCODE(I64X2_REINTERPRET_I32X4); RETURN(UNARY); }
    <i> "i64x2.reinterpret_f32x4"         { OPCODE(I64X2_REINTERPRET_F32X4); RETURN(UNARY); }
    <i> "i64x2.reinterpret_f64x2"         { OPCODE(I64X2_REINTERPRET_F64X2); RETURN(UNARY); }
    <i> "i64x2.splat"         { OPCODE(I64X2_SPLAT); RETURN(UNARY); }
    <i> "i64x2.extractlane"         { OPCODE(I64X2_EXTRACT); RETURN(BINARY); }
    <i> "i64x2.add"         { OPCODE(I64X2_ADD); RETURN(BINARY); }
    <i> "i64x2.sub"         { OPCODE(I64X2_SUB); RETURN(BINARY); }
    <i> "i64x2.mul"         { OPCODE(I64X2_MUL); RETURN(BINARY); }
    <i> "i64x2.neg"         { OPCODE(I64X2_NEG); RETURN(UNARY); }
    <i> "i64x2.shl"         { OPCODE(I64X2_SHL); RETURN(BINARY); }
    <i> "i64x2.shr_s"         { OPCODE(I64X2_SHR_S); RETURN(BINARY); }
    <i> "i64x2.shr_u"         { OPCODE(I64X2_SHR_U); RETURN(BINARY); }
    <i> "i64x2.and"         { OPCODE(I64X2_AND); RETURN(BINARY); }
    <i> "i64x2.or"         { OPCODE(I64X2_OR); RETURN(BINARY); }
    <i> "i64x2.xor"         { OPCODE(I64X2_XOR); RETURN(BINARY); }
    <i> "i64x2.not"         { OPCODE(I64X2_NOT); RETURN(UNARY); }
    <i> "i64x2.eq"         { OPCODE(I64X2_EQ); RETURN(BINARY); }
    <i> "i64x2.ne"         { OPCODE(I64X2_NE); RETURN(BINARY); }
    <i> "i64x2.lt_s"         { OPCODE(I64X2_LT_S); RETURN(BINARY); }
    <i> "i64x2.lt_u"         { OPCODE(I64X2_LT_U); RETURN(BINARY); }
    <i> "i64x2.le_s"         { OPCODE(I64X2_LE_S); RETURN(BINARY); }
    <i> "i64x2.le_u"         { OPCODE(I64X2_LE_U); RETURN(BINARY); }
    <i> "i64x2.gt_s"         { OPCODE(I64X2_GT_S); RETURN(BINARY); }
    <i> "i64x2.gt_u"         { OPCODE(I64X2_GT_U); RETURN(BINARY); }
    <i> "i64x2.ge_s"         { OPCODE(I64X2_GE_S); RETURN(BINARY); }
    <i> "i64x2.ge_u"         { OPCODE(I64X2_GE_U); RETURN(BINARY); }
    <i> "i64x2.load"         { OPCODE(I64X2_LOAD); RETURN(LOAD); }
    <i> "i64x2.store"         { OPCODE(I64X2_STORE); RETURN(STORE); }
    <i> "i64x2.trunc_s"         { OPCODE(I64X2_TRUNC_S); RETURN(UNARY); }
    <i> "i64x2.trunc_u"         { OPCODE(I64X2_TRUNC_U); RETURN(UNARY); }
    <i> "f32x4.reinterpret_i8x16"         { OPCODE(F32X4_REINTERPRET_I8X16); RETURN(UNARY); }
    <i> "f32x4.reinterpret_i16x8"         { OPCODE(F32X4_REINTERPRET_I16X8); RETURN(UNARY); }
    <i> "f32x4.reinterpret_i32x4"         { OPCODE(F32X4_REINTERPRET_I32X4); RETURN(UNARY); }
    <i> "f32x4.reinterpret_i64x2"         { OPCODE(F32X4_REINTERPRET_I64X2); RETURN(UNARY); }
    <i> "f32x4.reinterpret_f64x2"         { OPCODE(F32X4_REINTERPRET_F64X2); RETURN(UNARY); }
    <i> "f32x4.splat"         { OPCODE(F32X4_SPLAT); RETURN(UNARY); }
    <i> "f32x4.extractlane"         { OPCODE(F32X4_EXTRACT); RETURN(BINARY); }
    <i> "f32x4.add"         { OPCODE(F32X4_ADD); RETURN(BINARY); }
    <i> "f32x4.sub"         { OPCODE(F32X4_SUB); RETURN(BINARY); }
    <i> "f32x4.mul"         { OPCODE(F32X4_MUL); RETURN(BINARY); }
    <i> "f32x4.neg"         { OPCODE(F32X4_NEG); RETURN(UNARY); }
    <i> "f32x4.and"         { OPCODE(F32X4_AND); RETURN(BINARY); }
    <i> "f32x4.or"         { OPCODE(F32X4_OR); RETURN(BINARY); }
    <i> "f32x4.xor"         { OPCODE(F32X4_XOR); RETURN(BINARY); }
    <i> "f32x4.not"         { OPCODE(F32X4_NOT); RETURN(UNARY); }
    <i> "f32x4.eq"         { OPCODE(F32X4_EQ); RETURN(BINARY); }
    <i> "f32x4.ne"         { OPCODE(F32X4_NE); RETURN(BINARY); }
    <i> "f32x4.lt"         { OPCODE(F32X4_LT); RETURN(BINARY); }
    <i> "f32x4.le"         { OPCODE(F32X4_LE); RETURN(BINARY); }
    <i> "f32x4.gt"         { OPCODE(F32X4_GT); RETURN(BINARY); }
    <i> "f32x4.ge"         { OPCODE(F32X4_GE); RETURN(BINARY); }
    <i> "f32x4.load"         { OPCODE(F32X4_LOAD); RETURN(LOAD); }
    <i> "f32x4.store"         { OPCODE(F32X4_STORE); RETURN(STORE); }
    <i> "f32x4.load1"         { OPCODE(F32X4_LOAD1); RETURN(LOAD); }
    <i> "f32x4.load2"         { OPCODE(F32X4_LOAD2); RETURN(LOAD); }
    <i> "f32x4.load3"         { OPCODE(F32X4_LOAD3); RETURN(LOAD); }
    <i> "f32x4.store1"         { OPCODE(F32X4_STORE1); RETURN(STORE); }
    <i> "f32x4.store2"         { OPCODE(F32X4_STORE2); RETURN(STORE); }
    <i> "f32x4.store3"         { OPCODE(F32X4_STORE3); RETURN(STORE); }
    <i> "f32x4.abs"         { OPCODE(F32X4_ABS); RETURN(UNARY); }
    <i> "f32x4.min"         { OPCODE(F32X4_MIN); RETURN(BINARY); }
    <i> "f32x4.max"         { OPCODE(F32X4_MAX); RETURN(BINARY); }
    <i> "f32x4.div"         { OPCODE(F32X4_DIV); RETURN(BINARY); }
    <i> "f32x4.sqrt"         { OPCODE(F32X4_SQRT); RETURN(UNARY); }
    <i> "f32x4.rcp"         { OPCODE(F32X4_RCP); RETURN(UNARY); }
    <i> "f32x4.rsqrt"         { OPCODE(F32X4_RSQRT); RETURN(UNARY); }
    <i> "f32x4.convert_s"         { OPCODE(F32X4_CONVERT_S); RETURN(UNARY); }
    <i> "f32x4.convert_u"         { OPCODE(F32X4_CONVERT_U); RETURN(UNARY); }
    <i> "f64x2.reinterpret_i8x16"         { OPCODE(F64X2_REINTERPRET_I8X16); RETURN(UNARY); }
    <i> "f64x2.reinterpret_i16x8"         { OPCODE(F64X2_REINTERPRET_I16X8); RETURN(UNARY); }
    <i> "f64x2.reinterpret_i32x4"         { OPCODE(F64X2_REINTERPRET_I32X4); RETURN(UNARY); }
    <i> "f64x2.reinterpret_i64x2"         { OPCODE(F64X2_REINTERPRET_I64X2); RETURN(UNARY); }
    <i> "f64x2.reinterpret_f32x4"         { OPCODE(F64X2_REINTERPRET_F32X4); RETURN(UNARY); }
    <i> "f64x2.splat"         { OPCODE(F64X2_SPLAT); RETURN(UNARY); }
    <i> "f64x2.extractlane"         { OPCODE(F64X2_EXTRACT); RETURN(BINARY); }
    <i> "f64x2.add"         { OPCODE(F64X2_ADD); RETURN(BINARY); }
    <i> "f64x2.sub"         { OPCODE(F64X2_SUB); RETURN(BINARY); }
    <i> "f64x2.mul"         { OPCODE(F64X2_MUL); RETURN(BINARY); }
    <i> "f64x2.neg"         { OPCODE(F64X2_NEG); RETURN(UNARY); }
    <i> "f64x2.and"         { OPCODE(F64X2_AND); RETURN(BINARY); }
    <i> "f64x2.or"         { OPCODE(F64X2_OR); RETURN(BINARY); }
    <i> "f64x2.xor"         { OPCODE(F64X2_XOR); RETURN(BINARY); }
    <i> "f64x2.not"         { OPCODE(F64X2_NOT); RETURN(UNARY); }
    <i> "f64x2.eq"         { OPCODE(F64X2_EQ); RETURN(BINARY); }
    <i> "f64x2.ne"         { OPCODE(F64X2_NE); RETURN(BINARY); }
    <i> "f64x2.lt"         { OPCODE(F64X2_LT); RETURN(BINARY); }
    <i> "f64x2.le"         { OPCODE(F64X2_LE); RETURN(BINARY); }
    <i> "f64x2.gt"         { OPCODE(F64X2_GT); RETURN(BINARY); }
    <i> "f64x2.ge"         { OPCODE(F64X2_GE); RETURN(BINARY); }
    <i> "f64x2.load"         { OPCODE(F64X2_LOAD); RETURN(LOAD); }
    <i> "f64x2.store"         { OPCODE(F64X2_STORE); RETURN(STORE); }
    <i> "f64x2.abs"         { OPCODE(F64X2_ABS); RETURN(UNARY); }
    <i> "f64x2.min"         { OPCODE(F64X2_MIN); RETURN(BINARY); }
    <i> "f64x2.max"         { OPCODE(F64X2_MAX); RETURN(BINARY); }
    <i> "f64x2.div"         { OPCODE(F64X2_DIV); RETURN(BINARY); }
    <i> "f64x2.sqrt"         { OPCODE(F64X2_SQRT); RETURN(UNARY); }
    <i> "f64x2.rcp"         { OPCODE(F64X2_RCP); RETURN(UNARY); }
    <i> "f64x2.rsqrt"         { OPCODE(F64X2_RSQRT); RETURN(UNARY); }
    <i> "f64x2.convert_s"         { OPCODE(F64X2_CONVERT_S); RETURN(UNARY); }
    <i> "f64x2.convert_u"         { OPCODE(F64X2_CONVERT_U); RETURN(UNARY); }
    <i> "f64x2.swizzle"         { OPCODE(F64X2_SWIZZLE); RETURN(SIMD_SWIZZLE); }

    <i> "i32x4.build"            { OPCODE(I32X4_BUILD); RETURN(SIMD_BUILD); }
    <i> "i32x4.const"            { OPCODE(I32X4_CONST); RETURN(SIMD_CONST); }
    <i> "i16x8.build"            { OPCODE(I16X8_BUILD); RETURN(SIMD_BUILD); }
    <i> "i16x8.const"            { OPCODE(I16X8_CONST); RETURN(SIMD_CONST); }
    <i> "i8x16.build"            { OPCODE(I8X16_BUILD); RETURN(SIMD_BUILD); }
    <i> "i8x16.const"            { OPCODE(I8X16_CONST); RETURN(SIMD_CONST); }
    <i> "b32x4.build"            { OPCODE(B32X4_BUILD); RETURN(SIMD_BUILD); }
    <i> "b32x4.const"            { OPCODE(B32X4_CONST); RETURN(SIMD_CONST); }
    <i> "b16x8.build"            { OPCODE(B16X8_BUILD); RETURN(SIMD_BUILD); }
    <i> "b16x8.const"            { OPCODE(B16X8_CONST); RETURN(SIMD_CONST); }
    <i> "b8x16.build"            { OPCODE(B8X16_BUILD); RETURN(SIMD_BUILD); }
    <i> "b8x16.const"            { OPCODE(B8X16_CONST); RETURN(SIMD_CONST); }
    <i> "f32x4.build"            { OPCODE(F32X4_BUILD); RETURN(SIMD_BUILD); }
    <i> "f32x4.const"            { OPCODE(F32X4_CONST); RETURN(SIMD_CONST); }
    <i> "i64x2.build"            { OPCODE(I64X2_BUILD); RETURN(SIMD_BUILD); }
    <i> "i64x2.const"            { OPCODE(I64X2_CONST); RETURN(SIMD_CONST); }
    <i> "f64x2.build"            { OPCODE(F64X2_BUILD); RETURN(SIMD_BUILD); }
    <i> "f64x2.const"            { OPCODE(F64X2_CONST); RETURN(SIMD_CONST); }
    <i> "b64x2.build"            { OPCODE(B64X2_BUILD); RETURN(SIMD_BUILD); }
    <i> "b64x2.const"            { OPCODE(B64X2_CONST); RETURN(SIMD_CONST); }

    <i> "select"              { RETURN(SELECT); }
    <i> "unreachable"         { RETURN(UNREACHABLE); }
    <i> "current_memory"      { RETURN(CURRENT_MEMORY); }
    <i> "grow_memory"         { RETURN(GROW_MEMORY); }
    <i> "type"                { RETURN(TYPE); }
    <i> "func"                { RETURN(FUNC); }
    <i> "param"               { RETURN(PARAM); }
    <i> "result"              { RETURN(RESULT); }
    <i> "local"               { RETURN(LOCAL); }
    <i> "global"              { RETURN(GLOBAL); }
    <i> "module"              { RETURN(MODULE); }
    <i> "table"               { RETURN(TABLE); }
    <i> "memory"              { RETURN(MEMORY); }
    <i> "start"               { RETURN(START); }
    <i> "elem"                { RETURN(ELEM); }
    <i> "data"                { RETURN(DATA); }
    <i> "offset"              { RETURN(OFFSET); }
    <i> "import"              { RETURN(IMPORT); }
    <i> "export"              { RETURN(EXPORT); }
    <i> "register"            { RETURN(REGISTER); }
    <i> "invoke"              { RETURN(INVOKE); }
    <i> "get"                 { RETURN(GET); }
    <i> "assert_malformed"    { RETURN(ASSERT_MALFORMED); }
    <i> "assert_invalid"      { RETURN(ASSERT_INVALID); }
    <i> "assert_unlinkable"   { RETURN(ASSERT_UNLINKABLE); }
    <i> "assert_return"       { RETURN(ASSERT_RETURN); }
    <i> "assert_return_nan"   { RETURN(ASSERT_RETURN_NAN); }
    <i> "assert_trap"         { RETURN(ASSERT_TRAP); }
    <i> "input"               { RETURN(INPUT); }
    <i> "output"              { RETURN(OUTPUT); }
    <i> name                  { TEXT; RETURN(VAR); }

    <i> ";;" => LINE_COMMENT  { continue; }
    <LINE_COMMENT> "\n" => i  { NEWLINE; continue; }
    <LINE_COMMENT> [^\n]*     { continue; }
    <i> "(;" => BLOCK_COMMENT { COMMENT_NESTING = 1; continue; }
    <BLOCK_COMMENT> "(;"      { COMMENT_NESTING++; continue; }
    <BLOCK_COMMENT> ";)"      { if (--COMMENT_NESTING == 0)
                                  BEGIN(YYCOND_INIT);
                                continue; }
    <BLOCK_COMMENT> "\n"      { NEWLINE; continue; }
    <BLOCK_COMMENT> EOF       { ERROR("unexpected EOF"); RETURN(EOF); }
    <BLOCK_COMMENT> [^]       { continue; }
    <i> "\n"                  { NEWLINE; continue; }
    <i> [ \t\r]+              { continue; }
    <i> atom                  { ERROR("unexpected token \"%.*s\"",
                                      (int)yyleng, yytext);
                                continue; }
    <*> EOF                   { RETURN(EOF); }
    <*> [^]                   { ERROR("unexpected char"); continue; }
   */
  }
}

static WasmAstLexer* wasm_new_lexer(WasmAllocator* allocator,
                                    WasmAstLexerSourceType type,
                                    const char* filename) {
  WasmAstLexer* lexer =
      wasm_alloc_zero(allocator, sizeof(WasmAstLexer), WASM_DEFAULT_ALIGN);
  lexer->allocator = allocator;
  lexer->line = 1;
  lexer->filename = filename;
  lexer->source.type = type;
  return lexer;
}

WasmAstLexer* wasm_new_ast_file_lexer(WasmAllocator* allocator,
                                      const char* filename) {
  WasmAstLexer* lexer =
      wasm_new_lexer(allocator, WASM_LEXER_SOURCE_TYPE_FILE, filename);
  lexer->source.file = fopen(filename, "rb");
  if (!lexer->source.file) {
    wasm_destroy_ast_lexer(lexer);
    return NULL;
  }
  return lexer;
}

WasmAstLexer* wasm_new_ast_buffer_lexer(WasmAllocator* allocator,
                                        const char* filename,
                                        const void* data,
                                        size_t size) {
  WasmAstLexer* lexer =
      wasm_new_lexer(allocator, WASM_LEXER_SOURCE_TYPE_BUFFER, filename);
  lexer->source.buffer.data = data;
  lexer->source.buffer.size = size;
  lexer->source.buffer.read_offset = 0;
  return lexer;
}

void wasm_destroy_ast_lexer(WasmAstLexer* lexer) {
  if (lexer->source.type == WASM_LEXER_SOURCE_TYPE_FILE && lexer->source.file)
    fclose(lexer->source.file);
  wasm_free(lexer->allocator, lexer->buffer);
  wasm_free(lexer->allocator, lexer);
}

WasmAllocator* wasm_ast_lexer_get_allocator(WasmAstLexer* lexer) {
  return lexer->allocator;
}

typedef enum WasmLineOffsetPosition {
  WASM_LINE_OFFSET_POSITION_START,
  WASM_LINE_OFFSET_POSITION_END,
} WasmLineOffsetPosition;

static WasmResult scan_forward_for_line_offset_in_buffer(
    const char* buffer_start,
    const char* buffer_end,
    int buffer_line,
    size_t buffer_file_offset,
    WasmLineOffsetPosition find_position,
    int find_line,
    int* out_line,
    size_t* out_line_offset) {
  int line = buffer_line;
  int line_offset = 0;
  const char* p;
  WasmBool is_previous_carriage = 0;
  for (p = buffer_start; p < buffer_end; ++p) {
    if (*p == '\n') {
      if (find_position == WASM_LINE_OFFSET_POSITION_START) {
        if (++line == find_line) {
          line_offset = buffer_file_offset + (p - buffer_start) + 1;
          break;
        }
      } else {
        if (line++ == find_line) {
          line_offset = buffer_file_offset + (p - buffer_start) - is_previous_carriage;
          break;
        }
      }
    }
    is_previous_carriage = *p == '\r';
  }

  WasmResult result = WASM_OK;
  if (p == buffer_end) {
    /* end of buffer */
    if (find_position == WASM_LINE_OFFSET_POSITION_START) {
      result = WASM_ERROR;
    } else {
      line_offset = buffer_file_offset + (buffer_end - buffer_start);
    }
  }

  *out_line = line;
  *out_line_offset = line_offset;
  return result;
}

static WasmResult scan_forward_for_line_offset_in_file(
    WasmAstLexer* lexer,
    int line,
    size_t line_start_offset,
    WasmLineOffsetPosition find_position,
    int find_line,
    size_t* out_line_offset) {
  FILE* lexer_file = lexer->source.file;
  WasmResult result = WASM_ERROR;
  long old_offset = ftell(lexer_file);
  if (old_offset == -1)
    return WASM_ERROR;
  size_t buffer_file_offset = line_start_offset;
  if (fseek(lexer_file, buffer_file_offset, SEEK_SET) == -1)
    goto cleanup;

  while (1) {
    char buffer[8 * 1024];
    const size_t buffer_size = WASM_ARRAY_SIZE(buffer);
    size_t read_bytes = fread(buffer, 1, buffer_size, lexer_file);
    if (read_bytes == 0) {
      /* end of buffer */
      if (find_position == WASM_LINE_OFFSET_POSITION_START) {
        result = WASM_ERROR;
      } else {
        *out_line_offset = buffer_file_offset + read_bytes;
        result = WASM_OK;
      }
      goto cleanup;
    }

    const char* buffer_end = buffer + read_bytes;
    result = scan_forward_for_line_offset_in_buffer(
        buffer, buffer_end, line, buffer_file_offset, find_position, find_line,
        &line, out_line_offset);
    if (result == WASM_OK)
      goto cleanup;

    buffer_file_offset += read_bytes;
  }

cleanup:
  /* if this fails, we're screwed */
  if (fseek(lexer_file, old_offset, SEEK_SET) == -1)
    return WASM_ERROR;
  return result;
}

static WasmResult scan_forward_for_line_offset(
    WasmAstLexer* lexer,
    int line,
    size_t line_start_offset,
    WasmLineOffsetPosition find_position,
    int find_line,
    size_t* out_line_offset) {
  assert(line <= find_line);
  if (lexer->source.type == WASM_LEXER_SOURCE_TYPE_BUFFER) {
    const char* source_buffer = lexer->source.buffer.data;
    const char* buffer_start = source_buffer + line_start_offset;
    const char* buffer_end = source_buffer + lexer->source.buffer.size;
    return scan_forward_for_line_offset_in_buffer(
        buffer_start, buffer_end, line, line_start_offset, find_position,
        find_line, &line, out_line_offset);
  } else {
    assert(lexer->source.type == WASM_LEXER_SOURCE_TYPE_FILE);
    return scan_forward_for_line_offset_in_file(lexer, line, line_start_offset,
                                                find_position, find_line,
                                                out_line_offset);
  }
}

static WasmResult get_line_start_offset(WasmAstLexer* lexer,
                                        int line,
                                        size_t* out_offset) {
  int first_line = 1;
  size_t first_offset = 0;
  int current_line = lexer->line;
  size_t current_offset = lexer->line_file_offset;

  if (line == current_line) {
    *out_offset = current_offset;
    return WASM_OK;
  } else if (line == first_line) {
    *out_offset = first_offset;
    return WASM_OK;
  } else if (line > current_line) {
    return scan_forward_for_line_offset(lexer, current_line, current_offset,
                                        WASM_LINE_OFFSET_POSITION_START, line,
                                        out_offset);
  } else {
    /* TODO(binji): optimize by storing more known line/offset pairs */
    return scan_forward_for_line_offset(lexer, first_line, first_offset,
                                        WASM_LINE_OFFSET_POSITION_START, line,
                                        out_offset);
  }
}

static WasmResult get_offsets_from_line(WasmAstLexer* lexer,
                                        int line,
                                        size_t* out_line_start,
                                        size_t* out_line_end) {
  size_t line_start;
  if (WASM_FAILED(get_line_start_offset(lexer, line, &line_start)))
    return WASM_ERROR;

  size_t line_end;
  if (WASM_FAILED(scan_forward_for_line_offset(lexer, line, line_start,
                                               WASM_LINE_OFFSET_POSITION_END,
                                               line, &line_end)))
    return WASM_ERROR;
  *out_line_start = line_start;
  *out_line_end = line_end;
  return WASM_OK;
}

static void clamp_source_line_offsets_to_location(size_t line_start,
                                                  size_t line_end,
                                                  int first_column,
                                                  int last_column,
                                                  size_t max_line_length,
                                                  size_t* out_new_line_start,
                                                  size_t* out_new_line_end) {
  size_t line_length = line_end - line_start;
  if (line_length > max_line_length) {
    size_t column_range = last_column - first_column;
    size_t center_on;
    if (column_range > max_line_length) {
      /* the column range doesn't fit, just center on first_column */
      center_on = first_column - 1;
    } else {
      /* the entire range fits, display it all in the center */
      center_on = (first_column + last_column) / 2 - 1;
    }
    if (center_on > max_line_length / 2)
      line_start += center_on - max_line_length / 2;
    if (line_start > line_end - max_line_length)
      line_start = line_end - max_line_length;
    line_end = line_start + max_line_length;
  }

  *out_new_line_start = line_start;
  *out_new_line_end = line_end;
}

WasmResult wasm_ast_lexer_get_source_line(WasmAstLexer* lexer,
                                          const WasmLocation* loc,
                                          size_t line_max_length,
                                          char* line,
                                          size_t* out_line_length,
                                          int* out_column_offset) {
  WasmResult result;
  size_t line_start; /* inclusive */
  size_t line_end;   /* exclusive */
  result = get_offsets_from_line(lexer, loc->line, &line_start, &line_end);
  if (WASM_FAILED(result))
    return result;

  size_t new_line_start;
  size_t new_line_end;
  clamp_source_line_offsets_to_location(line_start, line_end, loc->first_column,
                                        loc->last_column, line_max_length,
                                        &new_line_start, &new_line_end);
  WasmBool has_start_ellipsis = line_start != new_line_start;
  WasmBool has_end_ellipsis = line_end != new_line_end;

  char* write_start = line;
  size_t line_length = new_line_end - new_line_start;
  size_t read_start = new_line_start;
  size_t read_length = line_length;
  if (has_start_ellipsis) {
    memcpy(line, "...", 3);
    read_start += 3;
    write_start += 3;
    read_length -= 3;
  }
  if (has_end_ellipsis) {
    memcpy(line + line_length - 3, "...", 3);
    read_length -= 3;
  }

  if (lexer->source.type == WASM_LEXER_SOURCE_TYPE_BUFFER) {
    char* buffer_read_start = (char*)lexer->source.buffer.data + read_start;
    memcpy(write_start, buffer_read_start, read_length);
  } else {
    assert(lexer->source.type == WASM_LEXER_SOURCE_TYPE_FILE);
    FILE* lexer_file = lexer->source.file;
    long old_offset = ftell(lexer_file);
    if (old_offset == -1)
      return WASM_ERROR;
    if (fseek(lexer_file, read_start, SEEK_SET) == -1)
      return WASM_ERROR;
    if (fread(write_start, 1, read_length, lexer_file) < read_length)
      return WASM_ERROR;
    if (fseek(lexer_file, old_offset, SEEK_SET) == -1)
      return WASM_ERROR;
  }

  line[line_length] = '\0';

  *out_line_length = line_length;
  *out_column_offset = new_line_start - line_start;
  return WASM_OK;
}
