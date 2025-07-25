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

#ifndef INCLUDED_FILE_LOADER
#define INCLUDED_FILE_LOADER

#include "lib/os_path.h"
#include "lib/status.h"
#include "lib/types.h"

#include <cstddef>
#include <memory>

struct IFileLoader
{
	virtual ~IFileLoader();

	virtual size_t Precedence() const = 0;
	virtual wchar_t LocationCode() const = 0;
	virtual OsPath Path() const = 0;

	virtual Status Load(const OsPath& name, const std::shared_ptr<u8>& buf, size_t size) const = 0;
};

typedef std::shared_ptr<IFileLoader> PIFileLoader;

#endif	// #ifndef INCLUDED_FILE_LOADER
