;;-------------------------------------------------------------------------------------------------------
;; Copyright (C) Microsoft. All rights reserved.
;; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;;-------------------------------------------------------------------------------------------------------
(module
  (func (export "popcount") (result f32x4) (local f32x4)
    (set_local 0 (f32x4.const (f32.const -0.0) (f32.const -0.0) (f32.const -0.0) (f32.const -0.0)))	
    (f32x4.add (f32x4.const (f32.const -0.0) (f32.const -0.0) (f32.const -0.0) (f32.const -0.0)) 
                     (get_local 0))
    )
  )

