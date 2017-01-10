
def generate_n_consts(count):
  #return "(i32.const 0) "*count
  #"\s".join(["(i32.const {})".format(i) for i in range(1,count)])
  return " ".join(["(i32.const {})".format(i) for i in range(1,count+1)])

header = """
;;-------------------------------------------------------------------------------------------------------
;; Copyright (C) Microsoft. All rights reserved.
;; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;;-------------------------------------------------------------------------------------------------------
(module
\t(func (export "popcount") 
"""

footer = """
\t)
)
"""

types = [ "i32x4" , "i16x8", "i8x16", "b32x4", "b16x8", "b8x16", "u32x4", "u16x8", "u8x16"]
size = 4 

print(header)

for t in types:
  args = generate_n_consts(size)
  print("\t\t(drop \n\t\t\t({}.add \n\t\t\t\t({}.const {}) \n\t\t\t\t({}.const {})\n\t\t\t)\n\t\t)".format(t,t,args,t,args))
  if size == 16:
    size = 4
  else:
    size *= 2

print(footer)