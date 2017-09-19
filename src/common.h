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

#ifndef WASM_COMMON_H_
#define WASM_COMMON_H_

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"

#ifdef __cplusplus
#define WASM_EXTERN_C extern "C"
#define WASM_EXTERN_C_BEGIN extern "C" {
#define WASM_EXTERN_C_END }
#else
#define WASM_EXTERN_C
#define WASM_EXTERN_C_BEGIN
#define WASM_EXTERN_C_END
#endif

#define WASM_FATAL(...) fprintf(stderr, __VA_ARGS__), exit(1)
#define WASM_ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define WASM_ZERO_MEMORY(var) memset((void*)&(var), 0, sizeof(var))
#define WASM_USE(x) (void)x

#define SIMD_VEC_SIZE_IN_DBWORDS 4
#define WASM_UNKNOWN_OFFSET ((uint32_t)~0)
#define WASM_PAGE_SIZE 0x10000 /* 64k */
#define WASM_MAX_PAGES 0x10000 /* # of pages that fit in 32-bit address space */
#define WASM_BYTES_TO_PAGES(x) ((x) >> 16)
#define WASM_ALIGN_UP_TO_PAGE(x) \
  (((x) + WASM_PAGE_SIZE - 1) & ~(WASM_PAGE_SIZE - 1))

#define PRIstringslice "%.*s"
#define WASM_PRINTF_STRING_SLICE_ARG(x) (int)((x).length), (x).start

#define WASM_DEFAULT_SNPRINTF_ALLOCA_BUFSIZE 128
#define WASM_SNPRINTF_ALLOCA(buffer, len, format)                          \
  va_list args;                                                            \
  va_list args_copy;                                                       \
  va_start(args, format);                                                  \
  va_copy(args_copy, args);                                                \
  char fixed_buf[WASM_DEFAULT_SNPRINTF_ALLOCA_BUFSIZE];                    \
  char* buffer = fixed_buf;                                                \
  size_t len = wasm_vsnprintf(fixed_buf, sizeof(fixed_buf), format, args); \
  va_end(args);                                                            \
  if (len + 1 > sizeof(fixed_buf)) {                                       \
    buffer = alloca(len + 1);                                              \
    len = wasm_vsnprintf(buffer, len + 1, format, args_copy);              \
  }                                                                        \
  va_end(args_copy)

struct WasmAllocator;

typedef enum WasmBool {
  WASM_FALSE,
  WASM_TRUE,
} WasmBool;

typedef enum WasmResult {
  WASM_OK,
  WASM_ERROR,
} WasmResult;

#define WASM_SUCCEEDED(x) ((x) == WASM_OK)
#define WASM_FAILED(x) ((x) == WASM_ERROR)

typedef struct WasmStringSlice {
  const char* start;
  size_t length;
} WasmStringSlice;

typedef struct WasmLocation {
  const char* filename;
  int line;
  int first_column;
  int last_column;
} WasmLocation;

typedef void (*WasmSourceErrorCallback)(const WasmLocation*,
                                        const char* error,
                                        const char* source_line,
                                        size_t source_line_length,
                                        size_t source_line_column_offset,
                                        void* user_data);

typedef struct WasmSourceErrorHandler {
  WasmSourceErrorCallback on_error;
  /* on_error will be called with with source_line trimmed to this length */
  size_t source_line_max_length;
  void* user_data;
} WasmSourceErrorHandler;

#define WASM_SOURCE_LINE_MAX_LENGTH_DEFAULT 80
#define WASM_SOURCE_ERROR_HANDLER_DEFAULT                                    \
  {                                                                          \
    wasm_default_source_error_callback, WASM_SOURCE_LINE_MAX_LENGTH_DEFAULT, \
        NULL                                                                 \
  }

typedef void (*WasmBinaryErrorCallback)(uint32_t offset,
                                        const char* error,
                                        void* user_data);

typedef struct WasmBinaryErrorHandler {
  WasmBinaryErrorCallback on_error;
  void* user_data;
} WasmBinaryErrorHandler;

#define WASM_BINARY_ERROR_HANDLER_DEFAULT \
  { wasm_default_binary_error_callback, NULL }

/* This data structure is not required; it is just used by the default error
 * handler callbacks. */
typedef enum WasmPrintErrorHeader {
  WASM_PRINT_ERROR_HEADER_NEVER,
  WASM_PRINT_ERROR_HEADER_ONCE,
  WASM_PRINT_ERROR_HEADER_ALWAYS,
} WasmPrintErrorHeader;

typedef struct WasmDefaultErrorHandlerInfo {
  const char* header;
  FILE* out_file;
  WasmPrintErrorHeader print_header;
} WasmDefaultErrorHandlerInfo;

/* matches binary format, do not change */
typedef enum WasmType {
	  WASM_TYPE_I32 = -0x01,
	  WASM_TYPE_I64 = -0x02,
	  WASM_TYPE_F32 = -0x03,
	  WASM_TYPE_F64 = -0x04,
	  WASM_TYPE_M128 = -0x5,
	  //WASM_TYPE_B2 = -0x06,
	  //WASM_TYPE_B4 = -0x07,
	  //WASM_TYPE_B8 = -0x08,
	  //WASM_TYPE_B16 = -0x09,
	  WASM_TYPE_ANYFUNC = -0x10,
	  WASM_TYPE_FUNC  = -0x20,
	  WASM_TYPE_VOID  = -0x40,

	  WASM_TYPE____ = WASM_TYPE_VOID, /* convenient for the opcode table below */
	  WASM_TYPE_ANY = 0, /* Not actually specified, but useful for type-checking */
} WasmType;

//  WASM_TYPE_F32X4 = -0x05,
//  WASM_TYPE_I32X4 = -0x06,
//  WASM_TYPE_I16X8 = -0x07,
//  WASM_TYPE_I8X16 = -0x08,
//  WASM_TYPE_B32X4 = -0x09,
//  WASM_TYPE_B16X8 = -0xa,
//  WASM_TYPE_B8X16 = -0xb,
//  WASM_TYPE_F64X2 = -0x0c,
//  WASM_TYPE_I64X2 = -0x0d,
//  WASM_TYPE_B64X2 = -0x0e,
//  WASM_TYPE_ANYFUNC = -0x10,
//  WASM_TYPE_FUNC  = -0x20,
//  WASM_TYPE_VOID  = -0x40,

/* matches binary format, do not change */
typedef enum WasmExternalKind {
  WASM_EXTERNAL_KIND_FUNC = 0,
  WASM_EXTERNAL_KIND_TABLE = 1,
  WASM_EXTERNAL_KIND_MEMORY = 2,
  WASM_EXTERNAL_KIND_GLOBAL = 3,
  WASM_NUM_EXTERNAL_KINDS,
} WasmExternalKind;

typedef struct WasmLimits {
  uint64_t initial;
  uint64_t max;
  WasmBool has_max;
} WasmLimits;

enum { WASM_USE_NATURAL_ALIGNMENT = 0xFFFFFFFF };


/*
 *   tr: result type
 *   t1: type of the 1st parameter
 *   t2: type of the 2nd parameter
 *    m: memory size of the operation, if any
 * code: opcode
 * NAME: used to generate the opcode enum
 * text: a string of the opcode name in the AST format
 *
 *  tr  t1    t2   m  code  NAME text
 *  ============================ */
#define WASM_FOREACH_OPCODE(V)                                          \
  V(___, ___, ___, 0, 0x00, UNREACHABLE, "unreachable")                 \
  V(___, ___, ___, 0, 0x01, NOP, "nop")                                 \
  V(___, ___, ___, 0, 0x02, BLOCK, "block")                             \
  V(___, ___, ___, 0, 0x03, LOOP, "loop")                               \
  V(___, ___, ___, 0, 0x04, IF, "if")                                   \
  V(___, ___, ___, 0, 0x05, ELSE, "else")                               \
  V(___, ___, ___, 0, 0x0b, END, "end")                                 \
  V(___, ___, ___, 0, 0x0c, BR, "br")                                   \
  V(___, ___, ___, 0, 0x0d, BR_IF, "br_if")                             \
  V(___, ___, ___, 0, 0x0e, BR_TABLE, "br_table")                       \
  V(___, ___, ___, 0, 0x0f, RETURN, "return")                           \
  V(___, ___, ___, 0, 0x10, CALL, "call")                               \
  V(___, ___, ___, 0, 0x11, CALL_INDIRECT, "call_indirect")             \
  V(___, ___, ___, 0, 0x1a, DROP, "drop")                               \
  V(___, ___, ___, 0, 0x1b, SELECT, "select")                           \
  V(___, ___, ___, 0, 0x20, GET_LOCAL, "get_local")                     \
  V(___, ___, ___, 0, 0x21, SET_LOCAL, "set_local")                     \
  V(___, ___, ___, 0, 0x22, TEE_LOCAL, "tee_local")                     \
  V(___, ___, ___, 0, 0x23, GET_GLOBAL, "get_global")                   \
  V(___, ___, ___, 0, 0x24, SET_GLOBAL, "set_global")                   \
  V(I32, I32, ___, 4, 0x28, I32_LOAD, "i32.load")                       \
  V(I64, I32, ___, 8, 0x29, I64_LOAD, "i64.load")                       \
  V(F32, I32, ___, 4, 0x2a, F32_LOAD, "f32.load")                       \
  V(F64, I32, ___, 8, 0x2b, F64_LOAD, "f64.load")                       \
  V(I32, I32, ___, 1, 0x2c, I32_LOAD8_S, "i32.load8_s")                 \
  V(I32, I32, ___, 1, 0x2d, I32_LOAD8_U, "i32.load8_u")                 \
  V(I32, I32, ___, 2, 0x2e, I32_LOAD16_S, "i32.load16_s")               \
  V(I32, I32, ___, 2, 0x2f, I32_LOAD16_U, "i32.load16_u")               \
  V(I64, I32, ___, 1, 0x30, I64_LOAD8_S, "i64.load8_s")                 \
  V(I64, I32, ___, 1, 0x31, I64_LOAD8_U, "i64.load8_u")                 \
  V(I64, I32, ___, 2, 0x32, I64_LOAD16_S, "i64.load16_s")               \
  V(I64, I32, ___, 2, 0x33, I64_LOAD16_U, "i64.load16_u")               \
  V(I64, I32, ___, 4, 0x34, I64_LOAD32_S, "i64.load32_s")               \
  V(I64, I32, ___, 4, 0x35, I64_LOAD32_U, "i64.load32_u")               \
  V(___, I32, I32, 4, 0x36, I32_STORE, "i32.store")                     \
  V(___, I32, I64, 8, 0x37, I64_STORE, "i64.store")                     \
  V(___, I32, F32, 4, 0x38, F32_STORE, "f32.store")                     \
  V(___, I32, F64, 8, 0x39, F64_STORE, "f64.store")                     \
  V(___, I32, I32, 1, 0x3a, I32_STORE8, "i32.store8")                   \
  V(___, I32, I32, 2, 0x3b, I32_STORE16, "i32.store16")                 \
  V(___, I32, I64, 1, 0x3c, I64_STORE8, "i64.store8")                   \
  V(___, I32, I64, 2, 0x3d, I64_STORE16, "i64.store16")                 \
  V(___, I32, I64, 4, 0x3e, I64_STORE32, "i64.store32")                 \
  V(I32, ___, ___, 0, 0x3f, CURRENT_MEMORY, "current_memory")           \
  V(I32, I32, ___, 0, 0x40, GROW_MEMORY, "grow_memory")                 \
  V(I32, ___, ___, 0, 0x41, I32_CONST, "i32.const")                     \
  V(I64, ___, ___, 0, 0x42, I64_CONST, "i64.const")                     \
  V(F32, ___, ___, 0, 0x43, F32_CONST, "f32.const")                     \
  V(F64, ___, ___, 0, 0x44, F64_CONST, "f64.const")                     \
  V(I32, I32, ___, 0, 0x45, I32_EQZ, "i32.eqz")                         \
  V(I32, I32, I32, 0, 0x46, I32_EQ, "i32.eq")                           \
  V(I32, I32, I32, 0, 0x47, I32_NE, "i32.ne")                           \
  V(I32, I32, I32, 0, 0x48, I32_LT_S, "i32.lt_s")                       \
  V(I32, I32, I32, 0, 0x49, I32_LT_U, "i32.lt_u")                       \
  V(I32, I32, I32, 0, 0x4a, I32_GT_S, "i32.gt_s")                       \
  V(I32, I32, I32, 0, 0x4b, I32_GT_U, "i32.gt_u")                       \
  V(I32, I32, I32, 0, 0x4c, I32_LE_S, "i32.le_s")                       \
  V(I32, I32, I32, 0, 0x4d, I32_LE_U, "i32.le_u")                       \
  V(I32, I32, I32, 0, 0x4e, I32_GE_S, "i32.ge_s")                       \
  V(I32, I32, I32, 0, 0x4f, I32_GE_U, "i32.ge_u")                       \
  V(I32, I64, ___, 0, 0x50, I64_EQZ, "i64.eqz")                         \
  V(I32, I64, I64, 0, 0x51, I64_EQ, "i64.eq")                           \
  V(I32, I64, I64, 0, 0x52, I64_NE, "i64.ne")                           \
  V(I32, I64, I64, 0, 0x53, I64_LT_S, "i64.lt_s")                       \
  V(I32, I64, I64, 0, 0x54, I64_LT_U, "i64.lt_u")                       \
  V(I32, I64, I64, 0, 0x55, I64_GT_S, "i64.gt_s")                       \
  V(I32, I64, I64, 0, 0x56, I64_GT_U, "i64.gt_u")                       \
  V(I32, I64, I64, 0, 0x57, I64_LE_S, "i64.le_s")                       \
  V(I32, I64, I64, 0, 0x58, I64_LE_U, "i64.le_u")                       \
  V(I32, I64, I64, 0, 0x59, I64_GE_S, "i64.ge_s")                       \
  V(I32, I64, I64, 0, 0x5a, I64_GE_U, "i64.ge_u")                       \
  V(I32, F32, F32, 0, 0x5b, F32_EQ, "f32.eq")                           \
  V(I32, F32, F32, 0, 0x5c, F32_NE, "f32.ne")                           \
  V(I32, F32, F32, 0, 0x5d, F32_LT, "f32.lt")                           \
  V(I32, F32, F32, 0, 0x5e, F32_GT, "f32.gt")                           \
  V(I32, F32, F32, 0, 0x5f, F32_LE, "f32.le")                           \
  V(I32, F32, F32, 0, 0x60, F32_GE, "f32.ge")                           \
  V(I32, F64, F64, 0, 0x61, F64_EQ, "f64.eq")                           \
  V(I32, F64, F64, 0, 0x62, F64_NE, "f64.ne")                           \
  V(I32, F64, F64, 0, 0x63, F64_LT, "f64.lt")                           \
  V(I32, F64, F64, 0, 0x64, F64_GT, "f64.gt")                           \
  V(I32, F64, F64, 0, 0x65, F64_LE, "f64.le")                           \
  V(I32, F64, F64, 0, 0x66, F64_GE, "f64.ge")                           \
  V(I32, I32, ___, 0, 0x67, I32_CLZ, "i32.clz")                         \
  V(I32, I32, ___, 0, 0x68, I32_CTZ, "i32.ctz")                         \
  V(I32, I32, ___, 0, 0x69, I32_POPCNT, "i32.popcnt")                   \
  V(I32, I32, I32, 0, 0x6a, I32_ADD, "i32.add")                         \
  V(I32, I32, I32, 0, 0x6b, I32_SUB, "i32.sub")                         \
  V(I32, I32, I32, 0, 0x6c, I32_MUL, "i32.mul")                         \
  V(I32, I32, I32, 0, 0x6d, I32_DIV_S, "i32.div_s")                     \
  V(I32, I32, I32, 0, 0x6e, I32_DIV_U, "i32.div_u")                     \
  V(I32, I32, I32, 0, 0x6f, I32_REM_S, "i32.rem_s")                     \
  V(I32, I32, I32, 0, 0x70, I32_REM_U, "i32.rem_u")                     \
  V(I32, I32, I32, 0, 0x71, I32_AND, "i32.and")                         \
  V(I32, I32, I32, 0, 0x72, I32_OR, "i32.or")                           \
  V(I32, I32, I32, 0, 0x73, I32_XOR, "i32.xor")                         \
  V(I32, I32, I32, 0, 0x74, I32_SHL, "i32.shl")                         \
  V(I32, I32, I32, 0, 0x75, I32_SHR_S, "i32.shr_s")                     \
  V(I32, I32, I32, 0, 0x76, I32_SHR_U, "i32.shr_u")                     \
  V(I32, I32, I32, 0, 0x77, I32_ROTL, "i32.rotl")                       \
  V(I32, I32, I32, 0, 0x78, I32_ROTR, "i32.rotr")                       \
  V(I64, I64, I64, 0, 0x79, I64_CLZ, "i64.clz")                         \
  V(I64, I64, I64, 0, 0x7a, I64_CTZ, "i64.ctz")                         \
  V(I64, I64, I64, 0, 0x7b, I64_POPCNT, "i64.popcnt")                   \
  V(I64, I64, I64, 0, 0x7c, I64_ADD, "i64.add")                         \
  V(I64, I64, I64, 0, 0x7d, I64_SUB, "i64.sub")                         \
  V(I64, I64, I64, 0, 0x7e, I64_MUL, "i64.mul")                         \
  V(I64, I64, I64, 0, 0x7f, I64_DIV_S, "i64.div_s")                     \
  V(I64, I64, I64, 0, 0x80, I64_DIV_U, "i64.div_u")                     \
  V(I64, I64, I64, 0, 0x81, I64_REM_S, "i64.rem_s")                     \
  V(I64, I64, I64, 0, 0x82, I64_REM_U, "i64.rem_u")                     \
  V(I64, I64, I64, 0, 0x83, I64_AND, "i64.and")                         \
  V(I64, I64, I64, 0, 0x84, I64_OR, "i64.or")                           \
  V(I64, I64, I64, 0, 0x85, I64_XOR, "i64.xor")                         \
  V(I64, I64, I64, 0, 0x86, I64_SHL, "i64.shl")                         \
  V(I64, I64, I64, 0, 0x87, I64_SHR_S, "i64.shr_s")                     \
  V(I64, I64, I64, 0, 0x88, I64_SHR_U, "i64.shr_u")                     \
  V(I64, I64, I64, 0, 0x89, I64_ROTL, "i64.rotl")                       \
  V(I64, I64, I64, 0, 0x8a, I64_ROTR, "i64.rotr")                       \
  V(F32, F32, F32, 0, 0x8b, F32_ABS, "f32.abs")                         \
  V(F32, F32, F32, 0, 0x8c, F32_NEG, "f32.neg")                         \
  V(F32, F32, F32, 0, 0x8d, F32_CEIL, "f32.ceil")                       \
  V(F32, F32, F32, 0, 0x8e, F32_FLOOR, "f32.floor")                     \
  V(F32, F32, F32, 0, 0x8f, F32_TRUNC, "f32.trunc")                     \
  V(F32, F32, F32, 0, 0x90, F32_NEAREST, "f32.nearest")                 \
  V(F32, F32, F32, 0, 0x91, F32_SQRT, "f32.sqrt")                       \
  V(F32, F32, F32, 0, 0x92, F32_ADD, "f32.add")                         \
  V(F32, F32, F32, 0, 0x93, F32_SUB, "f32.sub")                         \
  V(F32, F32, F32, 0, 0x94, F32_MUL, "f32.mul")                         \
  V(F32, F32, F32, 0, 0x95, F32_DIV, "f32.div")                         \
  V(F32, F32, F32, 0, 0x96, F32_MIN, "f32.min")                         \
  V(F32, F32, F32, 0, 0x97, F32_MAX, "f32.max")                         \
  V(F32, F32, F32, 0, 0x98, F32_COPYSIGN, "f32.copysign")               \
  V(F64, F64, F64, 0, 0x99, F64_ABS, "f64.abs")                         \
  V(F64, F64, F64, 0, 0x9a, F64_NEG, "f64.neg")                         \
  V(F64, F64, F64, 0, 0x9b, F64_CEIL, "f64.ceil")                       \
  V(F64, F64, F64, 0, 0x9c, F64_FLOOR, "f64.floor")                     \
  V(F64, F64, F64, 0, 0x9d, F64_TRUNC, "f64.trunc")                     \
  V(F64, F64, F64, 0, 0x9e, F64_NEAREST, "f64.nearest")                 \
  V(F64, F64, F64, 0, 0x9f, F64_SQRT, "f64.sqrt")                       \
  V(F64, F64, F64, 0, 0xa0, F64_ADD, "f64.add")                         \
  V(F64, F64, F64, 0, 0xa1, F64_SUB, "f64.sub")                         \
  V(F64, F64, F64, 0, 0xa2, F64_MUL, "f64.mul")                         \
  V(F64, F64, F64, 0, 0xa3, F64_DIV, "f64.div")                         \
  V(F64, F64, F64, 0, 0xa4, F64_MIN, "f64.min")                         \
  V(F64, F64, F64, 0, 0xa5, F64_MAX, "f64.max")                         \
  V(F64, F64, F64, 0, 0xa6, F64_COPYSIGN, "f64.copysign")               \
  V(I32, I64, ___, 0, 0xa7, I32_WRAP_I64, "i32.wrap/i64")               \
  V(I32, F32, ___, 0, 0xa8, I32_TRUNC_S_F32, "i32.trunc_s/f32")         \
  V(I32, F32, ___, 0, 0xa9, I32_TRUNC_U_F32, "i32.trunc_u/f32")         \
  V(I32, F64, ___, 0, 0xaa, I32_TRUNC_S_F64, "i32.trunc_s/f64")         \
  V(I32, F64, ___, 0, 0xab, I32_TRUNC_U_F64, "i32.trunc_u/f64")         \
  V(I64, I32, ___, 0, 0xac, I64_EXTEND_S_I32, "i64.extend_s/i32")       \
  V(I64, I32, ___, 0, 0xad, I64_EXTEND_U_I32, "i64.extend_u/i32")       \
  V(I64, F32, ___, 0, 0xae, I64_TRUNC_S_F32, "i64.trunc_s/f32")         \
  V(I64, F32, ___, 0, 0xaf, I64_TRUNC_U_F32, "i64.trunc_u/f32")         \
  V(I64, F64, ___, 0, 0xb0, I64_TRUNC_S_F64, "i64.trunc_s/f64")         \
  V(I64, F64, ___, 0, 0xb1, I64_TRUNC_U_F64, "i64.trunc_u/f64")         \
  V(F32, I32, ___, 0, 0xb2, F32_CONVERT_S_I32, "f32.convert_s/i32")     \
  V(F32, I32, ___, 0, 0xb3, F32_CONVERT_U_I32, "f32.convert_u/i32")     \
  V(F32, I64, ___, 0, 0xb4, F32_CONVERT_S_I64, "f32.convert_s/i64")     \
  V(F32, I64, ___, 0, 0xb5, F32_CONVERT_U_I64, "f32.convert_u/i64")     \
  V(F32, F64, ___, 0, 0xb6, F32_DEMOTE_F64, "f32.demote/f64")           \
  V(F64, I32, ___, 0, 0xb7, F64_CONVERT_S_I32, "f64.convert_s/i32")     \
  V(F64, I32, ___, 0, 0xb8, F64_CONVERT_U_I32, "f64.convert_u/i32")     \
  V(F64, I64, ___, 0, 0xb9, F64_CONVERT_S_I64, "f64.convert_s/i64")     \
  V(F64, I64, ___, 0, 0xba, F64_CONVERT_U_I64, "f64.convert_u/i64")     \
  V(F64, F32, ___, 0, 0xbb, F64_PROMOTE_F32, "f64.promote/f32")         \
  V(I32, F32, ___, 0, 0xbc, I32_REINTERPRET_F32, "i32.reinterpret/f32") \
  V(I64, F64, ___, 0, 0xbd, I64_REINTERPRET_F64, "i64.reinterpret/f64") \
  V(F32, I32, ___, 0, 0xbe, F32_REINTERPRET_I32, "f32.reinterpret/i32") \
  V(F64, I64, ___, 0, 0xbf, F64_REINTERPRET_I64, "f64.reinterpret/i64") \
  V(M128, ___, ___, 3,  0x100, M128_CONST, "m128.const") \
  V(M128, I32, ___, 4,  0x101, M128_LOAD, "m128.load") \
  V(___, I32, M128, 4,  0x102, M128_STORE, "m128.store") \
  V(M128, I32, ___, 0,  0x103, I8X16_SPLAT, "i8x16.splat") \
  V(M128, I32, ___, 0,  0x104, I16X8_SPLAT, "i16x8.splat") \
  V(M128, I32, ___, 0,  0x105, I32X4_SPLAT, "i32x4.splat") \
  V(M128, I64, ___, 0,  0x106, I64X2_SPLAT, "i64x2.splat") \
  V(M128, F32, ___, 0,  0x107, F32X4_SPLAT, "f32x4.splat") \
  V(M128, F64, ___, 0,  0x108, F64X2_SPLAT, "f64x2.splat") \
  V(I32, M128, ___, 16,  0x109, I8X16_EXTRACT_S, "i8x16.extract_s") \
  V(I32, M128, ___, 16,  0x10a, I8X16_EXTRACT_U, "i8x16.extract_u") \
  V(I32, M128, ___, 8,  0x10b, I16X8_EXTRACT_S, "i16x8.extract_s") \
  V(I32, M128, ___, 8,  0x10c, I16X8_EXTRACT_U, "i16x8.extract_u") \
  V(I32, M128, ___, 4,  0x10d, I32X4_EXTRACT, "i32x4.extract") \
  V(I64, M128, ___, 2,  0x10e, I64X2_EXTRACT, "i64x2.extract") \
  V(F32, M128, ___, 4,  0x10f, F32X4_EXTRACT, "f32x4.extract") \
  V(F64, M128, ___, 2,  0x110, F64X2_EXTRACT, "f64x2.extract") \
  V(M128, M128, I32, 16,  0x111, I8X16_REPLACE, "i8x16.replace") \
  V(M128, M128, I32, 8,  0x112, I16X8_REPLACE, "i16x8.replace") \
  V(M128, M128, I32, 4,  0x113, I32X4_REPLACE, "i32x4.replace") \
  V(M128, M128, I64, 2,  0x114, I64X2_REPLACE, "i64x2.replace") \
  V(M128, M128, F32, 4,  0x115, F32X4_REPLACE, "f32x4.replace") \
  V(M128, M128, F64, 2,  0x116, F64X2_REPLACE, "f64x2.replace") \
  V(M128, M128, I32, 16,  0x117, V8X16_SHUFFLE, "v8x16.shuffle") \
  V(M128, M128, M128, 0,  0x118, I8X16_ADD, "i8x16.add") \
  V(M128, M128, M128, 0,  0x119, I16X8_ADD, "i16x8.add") \
  V(M128, M128, M128, 0,  0x11a, I32X4_ADD, "i32x4.add") \
  V(M128, M128, M128, 0,  0x11b, I64X2_ADD, "i64x2.add") \
  V(M128, M128, M128, 0,  0x11c, I8X16_SUB, "i8x16.sub") \
  V(M128, M128, M128, 0,  0x11d, I16X8_SUB, "i16x8.sub") \
  V(M128, M128, M128, 0,  0x11e, I32X4_SUB, "i32x4.sub") \
  V(M128, M128, M128, 0,  0x11f, I64X2_SUB, "i64x2.sub") \
  V(M128, M128, M128, 0,  0x120, I8X16_MUL, "i8x16.mul") \
  V(M128, M128, M128, 0,  0x121, I16X8_MUL, "i16x8.mul") \
  V(M128, M128, M128, 0,  0x122, I32X4_MUL, "i32x4.mul") \
  V(M128, M128, ___, 0,  0x123, I8X16_NEG, "i8x16.neg") \
  V(M128, M128, ___, 0,  0x124, I16X8_NEG, "i16x8.neg") \
  V(M128, M128, ___, 0,  0x125, I32X4_NEG, "i32x4.neg") \
  V(M128, M128, ___, 0,  0x126, I64X2_NEG, "i64x2.neg") \
  V(M128, M128, M128, 0,  0x127, I8X16_ADDSATURATE_S, "i8x16.addsaturate_s") \
  V(M128, M128, M128, 0,  0x128, I8X16_ADDSATURATE_U, "i8x16.addsaturate_u") \
  V(M128, M128, M128, 0,  0x129, I16X8_ADDSATURATE_S, "i16x8.addsaturate_s") \
  V(M128, M128, M128, 0,  0x12a, I16X8_ADDSATURATE_U, "i16x8.addsaturate_u") \
  V(M128, M128, M128, 0,  0x12b, I8X16_SUBSATURATE_S, "i8x16.subsaturate_s") \
  V(M128, M128, M128, 0,  0x12c, I8X16_SUBSATURATE_U, "i8x16.subsaturate_u") \
  V(M128, M128, M128, 0,  0x12d, I16X8_SUBSATURATE_S, "i16x8.subsaturate_s") \
  V(M128, M128, M128, 0,  0x12e, I16X8_SUBSATURATE_U, "i16x8.subsaturate_u") \
  V(M128, M128, I32, 0,  0x12f, I8X16_SHL, "i8x16.shl") \
  V(M128, M128, I32, 0,  0x130, I16X8_SHL, "i16x8.shl") \
  V(M128, M128, I32, 0,  0x131, I32X4_SHL, "i32x4.shl") \
  V(M128, M128, I32, 0,  0x132, I64X2_SHL, "i64x2.shl") \
  V(M128, M128, I32, 0,  0x133, I8X16_SHR_S, "i8x16.shr_s") \
  V(M128, M128, I32, 0,  0x134, I8X16_SHR_U, "i8x16.shr_u") \
  V(M128, M128, I32, 0,  0x135, I16X8_SHR_S, "i16x8.shr_s") \
  V(M128, M128, I32, 0,  0x136, I16X8_SHR_U, "i16x8.shr_u") \
  V(M128, M128, I32, 0,  0x137, I32X4_SHR_S, "i32x4.shr_s") \
  V(M128, M128, I32, 0,  0x138, I32X4_SHR_U, "i32x4.shr_u") \
  V(M128, M128, I32, 0,  0x139, I64X2_SHR_S, "i64x2.shr_s") \
  V(M128, M128, I32, 0,  0x13a, I64X2_SHR_U, "i64x2.shr_u") \
  V(M128, M128, M128, 3,  0x13b, M128_AND, "m128.and") \
  V(M128, M128, M128, 3,  0x13c, M128_OR, "m128.or") \
  V(M128, M128, M128, 3,  0x13d, M128_XOR, "m128.xor") \
  V(M128, M128, M128, 3,  0x13e, M128_NOT, "m128.not") \
  V(M128, M128, M128, 3,  0x13f, M128_BITSELECT, "m128.bitselect") \
  V(I32, M128, M128, 3,  0x140, I8X16_ANY_TRUE, "i8x16.any_true") \
  V(I32, M128, M128, 3,  0x141, I16X8_ANY_TRUE, "i16x8.any_true") \
  V(I32, M128, M128, 3,  0x142, I32X4_ANY_TRUE, "i32x4.any_true") \
  V(I32, M128, M128, 3,  0x143, I64X2_ANY_TRUE, "i64x2.any_true") \
  V(I32, M128, M128, 3,  0x144, I8X16_ALL_TRUE, "i8x16.all_true") \
  V(I32, M128, M128, 3,  0x145, I16X8_ALL_TRUE, "i16x8.all_true") \
  V(I32, M128, M128, 3,  0x146, I32X4_ALL_TRUE, "i32x4.all_true") \
  V(I32, M128, M128, 3,  0x147, I64X2_ALL_TRUE, "i64x2.all_true") \
  V(M128, M128, M128, 0,  0x148, I8X16_EQ, "i8x16.eq") \
  V(M128, M128, M128, 0,  0x149, I16X8_EQ, "i16x8.eq") \
  V(M128, M128, M128, 0,  0x14a, I32X4_EQ, "i32x4.eq") \
  V(M128, M128, M128, 0,  0x14b, F32X4_EQ, "f32x4.eq") \
  V(M128, M128, M128, 0,  0x14c, F64X2_EQ, "f64x2.eq") \
  V(M128, M128, M128, 0,  0x14d, I8X16_NE, "i8x16.ne") \
  V(M128, M128, M128, 0,  0x14e, I16X8_NE, "i16x8.ne") \
  V(M128, M128, M128, 0,  0x14f, I32X4_NE, "i32x4.ne") \
  V(M128, M128, M128, 0,  0x150, F32X4_NE, "f32x4.ne") \
  V(M128, M128, M128, 0,  0x151, F64X2_NE, "f64x2.ne") \
  V(M128, M128, M128, 0,  0x152, I8X16_LT_S, "i8x16.lt_s") \
  V(M128, M128, M128, 0,  0x153, I8X16_LT_U, "i8x16.lt_u") \
  V(M128, M128, M128, 0,  0x154, I16X8_LT_S, "i16x8.lt_s") \
  V(M128, M128, M128, 0,  0x155, I16X8_LT_U, "i16x8.lt_u") \
  V(M128, M128, M128, 0,  0x156, I32X4_LT_S, "i32x4.lt_s") \
  V(M128, M128, M128, 0,  0x157, I32X4_LT_U, "i32x4.lt_u") \
  V(M128, M128, M128, 0,  0x158, F32X4_LT, "f32x4.lt") \
  V(M128, M128, M128, 0,  0x159, F64X2_LT, "f64x2.lt") \
  V(M128, M128, M128, 0,  0x15a, I8X16_LE_S, "i8x16.le_s") \
  V(M128, M128, M128, 0,  0x15b, I8X16_LE_U, "i8x16.le_u") \
  V(M128, M128, M128, 0,  0x15c, I16X8_LE_S, "i16x8.le_s") \
  V(M128, M128, M128, 0,  0x15d, I16X8_LE_U, "i16x8.le_u") \
  V(M128, M128, M128, 0,  0x15e, I32X4_LE_S, "i32x4.le_s") \
  V(M128, M128, M128, 0,  0x15f, I32X4_LE_U, "i32x4.le_u") \
  V(M128, M128, M128, 0,  0x160, F32X4_LE, "f32x4.le") \
  V(M128, M128, M128, 0,  0x161, F64X2_LE, "f64x2.le") \
  V(M128, M128, M128, 0,  0x162, I8X16_GT_S, "i8x16.gt_s") \
  V(M128, M128, M128, 0,  0x163, I8X16_GT_U, "i8x16.gt_u") \
  V(M128, M128, M128, 0,  0x164, I16X8_GT_S, "i16x8.gt_s") \
  V(M128, M128, M128, 0,  0x165, I16X8_GT_U, "i16x8.gt_u") \
  V(M128, M128, M128, 0,  0x166, I32X4_GT_S, "i32x4.gt_s") \
  V(M128, M128, M128, 0,  0x167, I32X4_GT_U, "i32x4.gt_u") \
  V(M128, M128, M128, 0,  0x168, F32X4_GT, "f32x4.gt") \
  V(M128, M128, M128, 0,  0x169, F64X2_GT, "f64x2.gt") \
  V(M128, M128, M128, 0,  0x16a, I8X16_GE_S, "i8x16.ge_s") \
  V(M128, M128, M128, 0,  0x16b, I8X16_GE_U, "i8x16.ge_u") \
  V(M128, M128, M128, 0,  0x16c, I16X8_GE_S, "i16x8.ge_s") \
  V(M128, M128, M128, 0,  0x16d, I16X8_GE_U, "i16x8.ge_u") \
  V(M128, M128, M128, 0,  0x16e, I32X4_GE_S, "i32x4.ge_s") \
  V(M128, M128, M128, 0,  0x16f, I32X4_GE_U, "i32x4.ge_u") \
  V(M128, M128, M128, 0,  0x170, F32X4_GE, "f32x4.ge") \
  V(M128, M128, M128, 0,  0x171, F64X2_GE, "f64x2.ge") \
  V(M128, M128, ___, 0,  0x172, F32X4_NEG, "f32x4.neg") \
  V(M128, M128, ___, 0,  0x173, F64X2_NEG, "f64x2.neg") \
  V(M128, M128, ___, 0,  0x174, F32X4_ABS, "f32x4.abs") \
  V(M128, M128, ___, 0,  0x175, F64X2_ABS, "f64x2.abs") \
  V(M128, M128, M128, 0,  0x176, F32X4_MIN, "f32x4.min") \
  V(M128, M128, M128, 0,  0x177, F64X2_MIN, "f64x2.min") \
  V(M128, M128, M128, 0,  0x178, F32X4_MAX, "f32x4.max") \
  V(M128, M128, M128, 0,  0x179, F64X2_MAX, "f64x2.max") \
  V(M128, M128, M128, 0,  0x17a, F32X4_ADD, "f32x4.add") \
  V(M128, M128, M128, 0,  0x17b, F64X2_ADD, "f64x2.add") \
  V(M128, M128, M128, 0,  0x17c, F32X4_SUB, "f32x4.sub") \
  V(M128, M128, M128, 0,  0x17d, F64X2_SUB, "f64x2.sub") \
  V(M128, M128, M128, 0,  0x17e, F32X4_DIV, "f32x4.div") \
  V(M128, M128, M128, 0,  0x17f, F64X2_DIV, "f64x2.div") \
  V(M128, M128, M128, 0,  0x180, F32X4_MUL, "f32x4.mul") \
  V(M128, M128, M128, 0,  0x181, F64X2_MUL, "f64x2.mul") \
  V(M128, M128, ___, 0,  0x182, F32X4_SQRT, "f32x4.sqrt") \
  V(M128, M128, ___, 0,  0x183, F64X2_SQRT, "f64x2.sqrt") \
  V(M128, M128, ___, 3,  0x184, F32X4_CONVERT_S, "f32x4.convert_s") \
  V(M128, M128, ___, 3,  0x185, F32X4_CONVERT_U, "f32x4.convert_u") \
  V(M128, M128, ___, 3,  0x186, F64X2_CONVERT_S, "f64x2.convert_s") \
  V(M128, M128, ___, 3,  0x187, F64X2_CONVERT_U, "f64x2.convert_u") \
  V(M128, M128, ___, 3,  0x188, I32X4_TRUNC_S, "i32x4.trunc_s") \
  V(M128, M128, ___, 3,  0x189, I32X4_TRUNC_U, "i32x4.trunc_u") \
  V(M128, M128, ___, 3,  0x18a, I64X2_TRUNC_S, "i64x2.trunc_s") \
  V(M128, M128, ___, 3,  0x18b, I64X2_TRUNC_U, "i64x2.trunc_u")

typedef enum WasmOpcode {
#define V(rtype, type1, type2, mem_size, code, NAME, text) \
  WASM_OPCODE_##NAME = code,
  WASM_FOREACH_OPCODE(V)
#undef V
  WASM_EXTENDED_START = 256,
  WASM_SIMD_START =   0xFD,
  WASM_EXTENDED_START2 = 512,
  WASM_HOST_START = 720,
  WASM_EXTENDED_OPCODE = 6,
  WASM_EXTENDED_OPCODE2 = 7,
  WASM_NUM_OPCODES = 760
} WasmOpcode;

typedef struct WasmOpcodeInfo {
  const char* name;
  WasmType result_type;
  WasmType param1_type;
  WasmType param2_type;
  int memory_size;
} WasmOpcodeInfo;

typedef enum WasmLiteralType {
  WASM_LITERAL_TYPE_INT,
  WASM_LITERAL_TYPE_FLOAT,
  WASM_LITERAL_TYPE_HEXFLOAT,
  WASM_LITERAL_TYPE_INFINITY,
  WASM_LITERAL_TYPE_NAN,
} WasmLiteralType;

typedef struct WasmLiteral {
  WasmLiteralType type;
  WasmStringSlice text;
} WasmLiteral;

WASM_EXTERN_C_BEGIN
/* return 1 if |alignment| matches the alignment of |opcode|, or if |alignment|
 * is WASM_USE_NATURAL_ALIGNMENT */
WasmBool wasm_is_naturally_aligned(WasmOpcode opcode, uint32_t alignment);

/* if |alignment| is WASM_USE_NATURAL_ALIGNMENT, return the alignment of
 * |opcode|, else return |alignment| */
uint32_t wasm_get_opcode_alignment(WasmOpcode opcode, uint32_t alignment);

WasmStringSlice wasm_empty_string_slice(void);
WasmBool wasm_string_slice_eq_cstr(const WasmStringSlice* s1, const char* s2);
WasmBool wasm_string_slice_startswith(const WasmStringSlice* s1, const char* s2);
WasmStringSlice wasm_string_slice_from_cstr(const char* string);
WasmBool wasm_string_slice_is_empty(const WasmStringSlice*);
WasmBool wasm_string_slices_are_equal(const WasmStringSlice*,
                                      const WasmStringSlice*);
void wasm_destroy_string_slice(struct WasmAllocator*, WasmStringSlice*);
WasmResult wasm_read_file(struct WasmAllocator* allocator,
                          const char* filename,
                          void** out_data,
                          size_t* out_size);

void wasm_default_source_error_callback(const WasmLocation*,
                                        const char* error,
                                        const char* source_line,
                                        size_t source_line_length,
                                        size_t source_line_column_offset,
                                        void* user_data);

void wasm_default_binary_error_callback(uint32_t offset,
                                        const char* error,
                                        void* user_data);

void wasm_init_stdio();

/* opcode info */
extern WasmOpcodeInfo g_wasm_opcode_info[];

static WASM_INLINE WasmOpcode get_simd_const_opcode(WasmType type) {

    uint32_t opcode = 0;
    switch (type) {
    case WASM_TYPE_M128:
        opcode = WASM_OPCODE_M128_CONST;
        break;
    default:
        assert (0);
    }

    return (WasmOpcode)opcode;
}

static WASM_INLINE const char* wasm_get_opcode_name(WasmOpcode opcode) {
  assert(opcode < WASM_NUM_OPCODES);
  return g_wasm_opcode_info[opcode].name;
}


static WASM_INLINE WasmType wasm_get_opcode_result_type(WasmOpcode opcode) {
  assert(opcode < WASM_NUM_OPCODES);
  return g_wasm_opcode_info[opcode].result_type;
}

static WASM_INLINE WasmType wasm_get_opcode_param_type_1(WasmOpcode opcode) {
  assert(opcode < WASM_NUM_OPCODES);
  return g_wasm_opcode_info[opcode].param1_type;
}

static WASM_INLINE WasmType wasm_get_opcode_param_type_2(WasmOpcode opcode) {
  assert(opcode < WASM_NUM_OPCODES);
  return g_wasm_opcode_info[opcode].param2_type;
}

static WASM_INLINE int wasm_get_opcode_memory_size(WasmOpcode opcode) {
  assert(opcode < WASM_NUM_OPCODES);
  return g_wasm_opcode_info[opcode].memory_size;
}

/* external kind */

extern const char* g_wasm_kind_name[];

static WASM_INLINE const char* wasm_get_kind_name(WasmExternalKind kind) {
  assert(kind < WASM_NUM_EXTERNAL_KINDS);
  return g_wasm_kind_name[kind];
}

/* type */

static WASM_INLINE const char* wasm_get_type_name(WasmType type) {
  switch (type) {
    case WASM_TYPE_I32: return "i32";
    case WASM_TYPE_I64: return "i64";
    case WASM_TYPE_F32: return "f32";
    case WASM_TYPE_F64: return "f64";
    case WASM_TYPE_M128: return "m128";
    case WASM_TYPE_ANYFUNC: return "anyfunc";
    case WASM_TYPE_FUNC: return "func";
    case WASM_TYPE_VOID: return "void";
    case WASM_TYPE_ANY: return "any";
    default: return NULL;
  }
}

WASM_EXTERN_C_END

#endif /* WASM_COMMON_H_ */
