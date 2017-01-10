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

# V(F32X4, F32, ___, 0, 0xc1, F32X4_SPLAT, "f32x4.splat")               \
#
for l in open(sys.argv[1]):
    tokens = l.split(",")
    if len(tokens) < 2:
        continue
    opcode = tokens[FIELD.TEXT]
    #
    print("<i> {}            {{ OPCODE({}); RETURN(BINARY); }}".format(opcode[:opcode.rindex('"')], tokens[FIELD.OPCODE].strip()))
