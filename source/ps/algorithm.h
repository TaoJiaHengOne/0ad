/* Copyright (C) 2024 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ALGORITHM_H
#define ALGORITHM_H

namespace PS
{
/**
 * Simplifed version of std::ranges::contains (C++23) as we don't support the
 * original one yet. The naming intentionally follows the STL version to make
 * the future replacement easier with less changing.
 * It supports only a subset of std::ranges::contains functionality.
 */
template<typename Range, typename T = typename Range::value_type>
bool contains(Range&& range, const T& value)
{
	return std::any_of(range.begin(), range.end(), [&](const auto& elem)
		{
			return elem == value;
		});
}
}

#endif // ALGORITHM_H
