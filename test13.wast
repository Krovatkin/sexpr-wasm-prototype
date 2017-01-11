;;-------------------------------------------------------------------------------------------------------
;; Copyright (C) Microsoft. All rights reserved.
;; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;;-------------------------------------------------------------------------------------------------------
(module
  (func (export "popcount") (local i32 i32)
    (drop (i32x4.build (i32.const 2) (i32.const 2) (i32.const 2) (i32.const 2)))
  )
) 
