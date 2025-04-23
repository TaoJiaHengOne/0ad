/* Copyright (C) 2025 Wildfire Games.
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

#ifndef INCLUDED_FONTMANAGER
#define INCLUDED_FONTMANAGER

#include "ps/CStrIntern.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <unordered_map>

class CFont;

/**
 * Font manager: loads and caches bitmap fonts.
 */
class CFontManager
{
public:
	CFontManager();
	~CFontManager() = default;
	NONCOPYABLE(CFontManager);

	std::shared_ptr<CFont> LoadFont(CStrIntern fontName);
	void UploadTexturesAtlasToGPU();

private:
	static void ftLibraryDeleter(FT_Library library) {
		FT_Done_FreeType(library);
	}
	std::unique_ptr<FT_LibraryRec_, decltype(&ftLibraryDeleter)> m_FreeType{nullptr, &ftLibraryDeleter};

	std::shared_ptr<std::array<float,256>> m_GammaCorrectionLUT;

	using FontsMap = std::unordered_map<CStrIntern, std::shared_ptr<CFont>>;
	FontsMap m_Fonts;

	/*
	* Most monitors today use 2.2 as the standard gamma.
	* MacOS may use 2.2 or 1.8 in some cases.
	* This method assumes your OS or GPU didn’t override the gamma ramp.
	* Unless we need super-accurate gamma (e.g., for print preview or color grading), this is usually acceptable.
	*/
	static constexpr float GAMMA_CORRECTION = 2.2f;
};

#endif // INCLUDED_FONTMANAGER
