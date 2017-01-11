


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

types = [ "i32x4" , "i16x8", "i8x16", "b32x4", "b16x8", "b8x16", "u32x4", "u16x8", "u8x16"]

start_opcode = int('0xf0',16)
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
    print("V({}, {}, {}, {},  {}, {}_BUILD, \"{}.build\")               \\".format(t.upper(), "I32", "___", size, hex(start_opcode), t.upper(), t))
    if size == 16:
       size = 4
    else:
       size *= 2

    #print("<i> \"{}.build\"            {{ OPCODE({}_BUILD); RETURN(SIMD_BUILD); }}".format(t, t.upper()))
