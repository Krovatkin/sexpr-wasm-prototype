
;;-------------------------------------------------------------------------------------------------------
;; Copyright (C) Microsoft. All rights reserved.
;; Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
;;-------------------------------------------------------------------------------------------------------
(module
	(func (export "popcount") 

		(drop 
			(i32x4.build 
				(i32.const 1)
				(i32.const 2)
				(i32.const 3)
				(i32.const 4)
			)
		)
		(drop 
			(i16x8.build 
				(i32.const 1)
				(i32.const 2)
				(i32.const 3)
				(i32.const 4)
				(i32.const 5)
				(i32.const 6)
				(i32.const 7)
				(i32.const 8)
			)
		)
		(drop 
			(i8x16.build 
				(i32.const 1)
				(i32.const 2)
				(i32.const 3)
				(i32.const 4)
				(i32.const 5)
				(i32.const 6)
				(i32.const 7)
				(i32.const 8)
				(i32.const 9)
				(i32.const 10)
				(i32.const 11)
				(i32.const 12)
				(i32.const 13)
				(i32.const 14)
				(i32.const 15)
				(i32.const 16)
			)
		)
		(drop 
			(b32x4.build 
				(i32.const 1)
				(i32.const 2)
				(i32.const 3)
				(i32.const 4)
			)
		)
		(drop 
			(b16x8.build 
				(i32.const 1)
				(i32.const 2)
				(i32.const 3)
				(i32.const 4)
				(i32.const 5)
				(i32.const 6)
				(i32.const 7)
				(i32.const 8)
			)
		)
		(drop 
			(b8x16.build 
				(i32.const 1)
				(i32.const 2)
				(i32.const 3)
				(i32.const 4)
				(i32.const 5)
				(i32.const 6)
				(i32.const 7)
				(i32.const 8)
				(i32.const 9)
				(i32.const 10)
				(i32.const 11)
				(i32.const 12)
				(i32.const 13)
				(i32.const 14)
				(i32.const 15)
				(i32.const 16)
			)
		)
		(drop 
			(u32x4.build 
				(i32.const 1)
				(i32.const 2)
				(i32.const 3)
				(i32.const 4)
			)
		)
		(drop 
			(u16x8.build 
				(i32.const 1)
				(i32.const 2)
				(i32.const 3)
				(i32.const 4)
				(i32.const 5)
				(i32.const 6)
				(i32.const 7)
				(i32.const 8)
			)
		)
		(drop 
			(u8x16.build 
				(i32.const 1)
				(i32.const 2)
				(i32.const 3)
				(i32.const 4)
				(i32.const 5)
				(i32.const 6)
				(i32.const 7)
				(i32.const 8)
				(i32.const 9)
				(i32.const 10)
				(i32.const 11)
				(i32.const 12)
				(i32.const 13)
				(i32.const 14)
				(i32.const 15)
				(i32.const 16)
			)
		)

	)
)

