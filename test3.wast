;;-------------------------------------------------------------------------------------------------------
;; Copyright (C) Microsoft. All rights reserved.
;; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;;-------------------------------------------------------------------------------------------------------
(module
  (func (export "popcount") 
    (drop (f32x4.add (f32x4.const (f32.const -0.0) (f32.const -0.0) (f32.const -0.0) (f32.const -0.0)) 
                     (f32x4.const (f32.const -1.0) (f32.const -1.0) (f32.const -1.0) (f32.const -1.0))
          )
    )
  )
) 
