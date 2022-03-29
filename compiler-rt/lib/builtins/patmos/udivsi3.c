/* ===-- udivsi3.c - Implement __udivsi3 -----------------------------------===
 *
 *                     The LLVM Compiler Infrastructure
 *
 * This file is dual licensed under the MIT and the University of Illinois Open
 * Source Licenses. See LICENSE.TXT for details.
 *
 * ===----------------------------------------------------------------------===
 *
 * This file implements __udivsi3 for the compiler_rt library,
 * as single-path and optimized for Patmos.
 *
 * ===----------------------------------------------------------------------===
 */

#include "../int_lib.h"

/* Returns: a / b */
COMPILER_RT_ABI su_int
__udivsi3(su_int n, su_int d)
{
    /* This has ~430 cycles, bundling compiler should be able to bring this down to ~230 cycles 
     * Should still be better than original C without branches due to lower data dependencies 
     * TODO The compiler should be able to lower this to the code below in the future. */
    unsigned r = 0;
    unsigned q = 0;
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
    return q;
}

