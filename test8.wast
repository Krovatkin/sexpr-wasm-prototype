;;-------------------------------------------------------------------------------------------------------
;; Copyright (C) Microsoft. All rights reserved.
;; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;;-------------------------------------------------------------------------------------------------------
(module
  (func $f1 (param f32x4) ;;passing f32x4
    (drop 
	(f32x4.add 
		(f32x4.const (f32.const -0.0) (f32.const -0.0) (f32.const -0.0) (f32.const -0.0)) 
		(get_local 0)
	)
    )
  )

  (func $f2 (result f32x4) 
	(f32x4.const (f32.const -0.0) (f32.const -0.0) (f32.const -0.0) (f32.const -0.0))  ;; returning f32x4
  )

  (func (export "popcount") (local f32x4)
    (set_local 0      
	(f32x4.add 
		(f32x4.const (f32.const -0.0) (f32.const -0.0) (f32.const -0.0) (f32.const -0.0)) 
		(call $f2)
    	)
    )
    (call $f1 (get_local 0))
  )
) 
