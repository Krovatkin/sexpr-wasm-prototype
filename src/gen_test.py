import sys
from enum import IntEnum, unique

@unique
class FIELD(IntEnum):
    RTYPE = 0
    TYPE1 = 1
    TYPE2 = 2
    MEMORY = 3
    ENCODING = 4
    OPCODE = 5
    TEXT = 6


assert len(sys.argv) >= 2

#V(F32X4, F32, ___, 0, 0xc1, F32X4_SPLAT, "f32x4.splat")               \

WASM_VOID = "V(___"


opcodes = []
values = {"F32" : "(f32.const -1.0)" , 
    "I32" : "(i32.const 7)" , 
    "F32X4" : "(f32x4.const (f32.const -0.0) (f32.const -0.0) (f32.const -0.0) (f32.const -0.0))", 
    "___" : " " }


for l in open(sys.argv[1]):
    tokens = l.split(",")
    if len(tokens) < 2:
        continue
    opcode = tokens[FIELD.TEXT]

    
    t = (tokens[FIELD.TYPE1].strip(), tokens[FIELD.TYPE2].strip(), opcode[opcode.index('"')+1:opcode.rindex('"')], tokens[FIELD.RTYPE].strip())
    opcodes.append(t)
    #print("<i> {}            {{ OPCODE({}); RETURN(BINARY); }}".format(], tokens[FIELD.OPCODE].strip()))
    
for op in opcodes:
    assert ((op[0] in values) and (op[1] in values))
    if op[2] == "f32x4.replaceLane":
        continue
    if op[3] == WASM_VOID:
        print("({} {} {})".format (op[2], values[op[0]], values[op[1]]))
    else:
        print("(drop ({} {} {}))".format (op[2], values[op[0]], values[op[1]]))