/*===-- udivmodsi4.c - Implement __udivmodsi4 ------------------------------===
 *
 *                    The LLVM Compiler Infrastructure
 *
 * This file is dual licensed under the MIT and the University of Illinois Open
 * Source Licenses. See LICENSE.TXT for details.
 *
 * ===----------------------------------------------------------------------===
 *
 * This file implements __udivmodsi4 for the compiler_rt library.
 *
 * ===----------------------------------------------------------------------===
 */

#include "../int_lib.h"

/* Returns: n / d, *rem = n % d  */

COMPILER_RT_ABI su_int
__udivmodsi4(su_int n, su_int d, su_int* rem)
{
    unsigned r;
    unsigned q = 0;
    /* This has ~430 cycles, bundling compiler should be able to bring this down to ~230 cycles 
     * Should still be better than original C without branches due to lower data dependencies 
     * TODO The compiler should be able to lower this to the code below in the future. */
    r = 0;
    #pragma loopbound min 32 max 32
    for (int i = 31; i >= 0; i--) {
		r <<= 1;
		// r[0] = n[i] -> should be p1 = btest n, i; (p1) or r = r, 1
		r |= (n >> i) & 1;
		if (r >= d) {
			r -= d;
			// should be bset!
			q |= (1 << i);
		}
    }

    *rem = r;
    return q;
}

