/* ===-- ctzsi2.c - Implement __ctzsi2 -------------------------------------===
 *
 *                     The LLVM Compiler Infrastructure
 *
 * This file is dual licensed under the MIT and the University of Illinois Open
 * Source Licenses. See LICENSE.TXT for details.
 *
 * ===----------------------------------------------------------------------===
 *
 * This file implements __ctzsi2 for the compiler_rt library for Patmos.
 *
 * TODO this is untested (and unused) as of now.
 *
 * ===----------------------------------------------------------------------===
 */

#include "../int_lib.h"

/* Returns: the number of trailing 0-bits */

COMPILER_RT_ABI si_int
__ctzsi2(si_int a)
{
    unsigned t = 32;
    // TODO This gets lowered to ~31 cycles including ret w/o bundles.
    //      We can get rid of 5 cycles if we lower select in bitcode to predicated sub instead of
    //      sub + predicated mov, with bundling this should be lowered to the code below.
    if (a) t--;
    a = a & -a;
    if (a & 0x0000FFFF) t -= 16;
    if (a & 0x00FF00FF) t -= 8;
    if (a & 0x0F0F0F0F) t -= 4;
    if (a & 0x33333333) t -= 2;
    if (a & 0x55555555) t -= 1;
    return t;
}
