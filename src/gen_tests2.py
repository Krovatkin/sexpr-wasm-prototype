


header = """
;;-------------------------------------------------------------------------------------------------------
;; Copyright (C) Microsoft. All rights reserved.
;; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;;-------------------------------------------------------------------------------------------------------
(module
"""


# (func (export "popcount") (param) (result f32x4) (local f32x4)
#)
def generate_n_consts(count, sep=" "):
  #return "(i32.const 0) "*count
  #"\s".join(["(i32.const {})".format(i) for i in range(1,count)])
  return sep.join(["(i32.const {})".format(i) for i in range(1,count+1)])

def generate_sig (args, name):
  if (args is None or len(args) == 0):
    return " "
  args_string = " ".join(args)
  return "({} {})".format(name, args)

def generate_simd_const(simd_type, count):
  consts_str = generate_n_consts(count)
  return "({}.const {})".format(simd_type, consts_str)

def generate_build_function_with_consts (simd_type, size):
  args_str = generate_n_consts(size, " ")
  return "({}.build {}\n)".format(simd_type, args_str)

def generate_function_header(name, params = None, result = None, locals = None):
  params_string = generate_sig(params, "param")
  result_string = generate_sig(result, "result")
  locals_string = generate_sig(locals, "local")
  return "(func (export \"{}\") {} {} {}".format(name, params_string, result_string, locals_string)


#print("\t\t(drop \n\t\t\t({}.build \n\t\t\t\t{}\n\t\t\t)\n\t\t)".format(t,args))

def generate_function_footer():
  return "\t)"

footer = """
\t)
)
"""

types = [ "i32x4" , "i16x8", "i8x16", "b32x4", "b16x8", "b8x16", "u32x4", "u16x8", "u8x16"]
#types = [ "i32x4"]

size = 4

print(header)

for t in types:

  for lane in range(0, size):
    print(generate_function_header("func_"+t+"_"+str(lane), None, "i32", t))
    print("(set_local 0 {})".format(generate_build_function_with_consts(t,size)))
    print("({}.extract (get_local 0) (i32.const {}))".format(t, lane))
    print(generate_function_footer())

  #args = generate_n_consts(size, "\n\t\t\t\t")
  #print("\t\t(drop \n\t\t\t({}.build \n\t\t\t\t{}\n\t\t\t)\n\t\t)".format(t,args))
  if size == 16:
    size = 4
  else:
    size *= 2

print(generate_function_footer())