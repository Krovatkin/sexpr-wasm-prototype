import sys
import re
import functools


pprint = functools.partial(print, end=" ")

ident = 0

def help():
    print ("wast-beautifier.py in_file\n")

if len(sys.argv) != 2:
    help()
    exit(1)

in_file = sys.argv[1]

simple_tokens = ["(i32.const", "(i64.const", "(f32.const", "(f64.const", "(export", "(get_local", "(result", "(local", "(param"]

simple_mode = False

for l in open(in_file):
    if l.startswith(";;"):
        pprint(l)
        continue
    l = l.replace(")", ") ")
    tokens = re.split("[\t ]+", l)

    for tok in tokens:
        tok = tok.strip()
        if tok == "":
            continue
        if simple_mode:
            pprint(tok+" ")
            if tok.endswith(")"):
                simple_mode = False
        elif tok in simple_tokens:
            simple_mode = True
            ident_str = "".ljust(ident, "\t")
            pprint("\n"+ident_str+tok+" ")
        elif tok.startswith("("):
            ident_str = "".ljust(ident, "\t")
            if tok.startswith("(func)"):
                pprint("\n")
            pprint("\n"+ident_str+tok)
            ident += 1
        elif tok.endswith(")"):
            if tok == ")":
                ident -= 1
                ident_str = "".ljust(ident, "\t")
                pprint("\n"+ident_str+")")
            else:
                ident_str = "".ljust(ident, "\t")
                pprint("\n"+ident_str+tok[:-1])
                ident -= 1
                ident_str = "".ljust(ident, "\t")
                pprint("\n"+ident_str+")")
        else:
            ident_str = "".ljust(ident, "\t")
            pprint("\n"+ident_str+tok)
print("")

