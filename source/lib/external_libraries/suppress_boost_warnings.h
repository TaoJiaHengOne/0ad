/* Copyright (C) 2025 Wildfire Games.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// rationale: boost is only included from the PCH, but warnings are still
// raised when templates are instantiated (e.g. vfs_lookup.cpp),
// so include this header from such source files as well.

#include "lib/sysdep/compiler.h"	// MSC_VERSION
#include "lib/sysdep/arch.h"	// ARCH_IA32

#if MSC_VERSION
# pragma warning(disable:4710) // function not inlined
# pragma warning(disable:4626) // assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted
# pragma warning(disable:4625) // copy constructor was implicitly defined as deleted
# pragma warning(disable:4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
# pragma warning(disable:5027) // 'type': move assignment operator was implicitly defined as deleted
# pragma warning(disable:4365) // signed/unsigned mismatch
# pragma warning(disable:4619) // there is no warning for 'warning'
# pragma warning(disable:5031) // #pragma warning(pop): likely mismatch, popping warning state pushed in different file
# pragma warning(disable:5026) // 'type': move constructor was implicitly defined as deleted
# pragma warning(disable:4820) // incorrect padding
# pragma warning(disable:4514) // unreferenced inlined function has been removed
# pragma warning(disable:4571) // Informational: catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught
#endif
