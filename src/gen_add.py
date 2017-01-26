


"""
  V(I32X4, I32, ___, 4,  0xdf, I32X4_CONST, "i32x4.const")               \
  V(I16X8, I32, ___, 8,  0xe0, I16X8_CONST, "i16x8.const")               \
  V(I8X16, I32, ___, 16, 0xe1, I8X16_CONST, "i8x16.const")               \
  V(B32X4, I32, ___, 4,  0xe2, B32X4_CONST, "b32x4.const")               \
  V(B16X8, I32, ___, 8,  0xe3, B16X8_CONST, "b16x8.const")               \
  V(B8X16, I32, ___, 16, 0xe4, B8X16_CONST, "b8x16.const")               \
  V(U32X4, I32, ___, 4,  0xe5, U32X4_CONST, "u32x4.const")               \
  V(U16X8, I32, ___, 8,  0xe6, U16X8_CONST, "u16x8.const")               \
  V(U8X16, I32, ___, 16, 0xe7, U8X16_CONST, "u8x16.const")               \
  """
  
"""  
WASM_UNARY__OPCODE(F4Splat, 0xc1, F4_F, Simd128_Splat_F4, false)
WASM_BINARY_OPCODE(F4Add,   0xd9, F4X3, Simd128_Add_F4, false)
WASM_BINARY_OPCODE(I4Add, 0xe8, I4X3, Simd128_Add_I4, false)
WASM_BINARY_OPCODE(I8Add, 0xe9, I8X3, Simd128_Add_I8, false)
WASM_BINARY_OPCODE(I16Add, 0xea, I16X3, Simd128_Add_I16, false)
WASM_BINARY_OPCODE(U4Add, 0xee, U4X3, Simd128_Add_U4, false)
WASM_BINARY_OPCODE(U8Add, 0xef, U8X3, Simd128_Add_U8, false)
WASM_BINARY_OPCODE(U16Add, 0xf0, U16X3, Simd128_Add_U16, false)

Simd128_ExtractLane_I4

V(I32, I32X4, I32, 0, 0xcd, I32X4_EXTRACT, "i32x4.extract")               \
V(I32, I16X8, I32, 0, 0xce, I16X8_EXTRACT, "i16x8.extract")               \
V(I32, I8X16, I32, 0, 0xcf, I8X16_EXTRACT, "i8x16.extract")               \
V(I32, B32X4, I32, 0, 0xd0, B32X4_EXTRACT, "b32x4.extract")               \
V(I32, B16X8, I32, 0, 0xd1, B16X8_EXTRACT, "b16x8.extract")               \
V(I32, B8X16, I32, 0, 0xd2, B8X16_EXTRACT, "b8x16.extract")               \
V(I32, U32X4, I32, 0, 0xd3, U32X4_EXTRACT, "u32x4.extract")               \
V(I32, U16X8, I32, 0, 0xd4, U16X8_EXTRACT, "u16x8.extract")               \
V(I32, U8X16, I32, 0, 0xd5, U8X16_EXTRACT, "u8x16.extract")               \
V(F32, F32X4, I32, 0, 0xd6, F32X4_EXTRACT, "f32x4.extract")               \

WASM_BINARY_OPCODE(I32ExtractLane, 205, I32_I, Simd128_ExtractLane_I32, false)
WASM_BINARY_OPCODE(I16ExtractLane, 206, I16_I, Simd128_ExtractLane_I16, false)
WASM_BINARY_OPCODE(I8ExtractLane, 207, I8_I, Simd128_ExtractLane_I8, false)
WASM_BINARY_OPCODE(B32ExtractLane, 208, B32_I, Simd128_ExtractLane_B32, false)
WASM_BINARY_OPCODE(B16ExtractLane, 209, B16_I, Simd128_ExtractLane_B16, false)
WASM_BINARY_OPCODE(B8ExtractLane, 210, B8_I, Simd128_ExtractLane_B8, false)
WASM_BINARY_OPCODE(U32ExtractLane, 211, U32_I, Simd128_ExtractLane_U32, false)
WASM_BINARY_OPCODE(U16ExtractLane, 212, U16_I, Simd128_ExtractLane_U16, false)
WASM_BINARY_OPCODE(U8ExtractLane, 213, U8_I, Simd128_ExtractLane_U8, false)

WASM_BINARY_OPCODE(I32ExtractLane, 0xcd, I32_I, Simd128_ExtractLane_I32, false)
WASM_BINARY_OPCODE(I16ExtractLane, 0xce, I16_I, Simd128_ExtractLane_I16, false)
WASM_BINARY_OPCODE(I8ExtractLane, 0xcf, I8_I, Simd128_ExtractLane_I8, false)
WASM_BINARY_OPCODE(B32ExtractLane, 0xd0, B32_I, Simd128_ExtractLane_B32, false)
WASM_BINARY_OPCODE(B16ExtractLane, 0xd1, B16_I, Simd128_ExtractLane_B16, false)
WASM_BINARY_OPCODE(B8ExtractLane, 0xd2, B8_I, Simd128_ExtractLane_B8, false)
WASM_BINARY_OPCODE(U32ExtractLane, 0xd3, U32_I, Simd128_ExtractLane_U32, false)
WASM_BINARY_OPCODE(U16ExtractLane, 0xd4, U16_I, Simd128_ExtractLane_U16, false)
WASM_BINARY_OPCODE(U8ExtractLane, 0xd5, U8_I, Simd128_ExtractLane_U8, false)


"""

types = [ "i32x4" , "i16x8", "i8x16", "b32x4", "b16x8", "b8x16", "u32x4", "u16x8", "u8x16"]

start_opcode = int('0xcc',16)
size = 4
#<i> \"i16x8.const\"         { OPCODE(I16X8_CONST); RETURN(SIMD_BUILD); }

for t in types:
    start_opcode += 1
    #print("V({}, {}, {}, {},  {}, {}_ADD, \"{}.add\")               \\".format(t.upper(), t.upper(), t.upper(), 0, hex(start_opcode), t.upper(), t))
    #print("<i> \"{}.const\"         {{ OPCODE({}_CONST); RETURN(SIMD_BUILD); }}".format(t, t.upper()))
    #print("<i> \"{}.add\"            {{ OPCODE({}_ADD); RETURN(BINARY); }}".format(t, t.upper()))
    #print ("case WASM_TYPE_"+t.upper()+":")
    #print ("\topcode = WASM_OPCODE_"+t.upper()+"_CONST;")
    #print ("\tbreak;")
    #print("V({}, {}, {}, {},  {}, {}_EXTRACT, \"{}.extract\")               \\".format(t.upper(), "I32", "___", size, hex(start_opcode), t.upper(), t))
    #print("V({}, {}, {}, {},  {}, {}_EXTRACT, \"{}.extract\")               \\".format("I32", t.upper() , "I32", 0, hex(start_opcode), t.upper(), t))
    #print("<i> \"{}.extract\"            {{ OPCODE({}_EXTRACT); RETURN(BINARY); }}".format(t, t.upper()))
    #print("<i> \"{}\"               {{ TYPE({}); RETURN(VALUE_TYPE); }}".format(t, t.upper()))
    #s = t[:t.find('x')].upper() + str(size)
    s = t[0].upper() + str(size)

    #WASM_BINARY_OPCODE(U8ExtractLane, 0xd5, U8_I, Simd128_ExtractLane_I16, false)

    print ("WASM_BINARY_OPCODE({}ExtractLane, {}, {}_{}_I, Simd128_ExtractLane_{}, false)".format(s, hex(start_opcode),s, s, s))


    if size == 16:
       size = 4
    else:
       size *= 2

    #print("<i> \"{}.build\"            {{ OPCODE({}_BUILD); RETURN(SIMD_BUILD); }}".format(t, t.upper()))
