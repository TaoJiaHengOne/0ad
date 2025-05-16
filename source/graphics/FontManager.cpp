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

#include "precompiled.h"

#include "FontManager.h"

#include "graphics/Font.h"
#include "graphics/TextureManager.h"
#include "i18n/L10n.h"
#include "ps/CLogger.h"
#include "ps/ConfigDB.h"
#include "ps/CStr.h"
#include "ps/CStrInternStatic.h"
#include "ps/Filesystem.h"
#include "renderer/Renderer.h"

#include <string>
#include <regex>
#include <limits>

namespace {
struct FontSpec {
	std::string type;
	bool bold{false};
	bool italic{false};
	bool stroke{false};
	int size{0};
};

FontSpec ParseFontSpec(const std::string& spec)
{
	// Regex breakdown:
	//   ^([^\\-]+)           → capture fontType (one or more non-'-')
	//   (?:-(bold|italic))?  → optional "-bold" or "-italic"
	//   (?:-(stroke))?       → optional "-stroke"
	//   -([0-9]+)$           → "-" then fontSize digits at end
	// examples:
	// "Roboto-italic-stroke-24",
	// "OpenSans-bold-32",
	// "Arial-stroke-16",
	// "Lato-14"
	static const std::regex pattern{R"(^([^\-]+)(?:-(bold|italic))?(?:-(stroke))?-([0-9]+)$)",
		std::regex::icase};

	std::smatch m;
	if (!std::regex_match(spec, m, pattern))
	{
		LOGERROR("Invalid font specification: %s", spec.c_str());
		return {};
	}

	FontSpec fs;
	fs.type = m[1].str();

	if (m[2].matched)
	{
		std::string style = m[2].str();
		if (strcasecmp(style.c_str(), "bold") == 0)
			fs.bold = true;
		else if (strcasecmp(style.c_str(), "italic") == 0)
			fs.italic = true;
	}

	if (m[3].matched)
		fs.stroke = true;

	fs.size = std::stoi(m[4].str());

	return fs;
}
} // namespace

CFontManager::CFontManager()
{
	FT_Library lib;
	FT_Error error{FT_Init_FreeType(&lib)};
	if (error)
		throw std::runtime_error{"Failed to initialize FreeType " + std::to_string(error)};
	m_FreeType.reset(lib);

	m_GammaCorrectionLUT = std::make_shared<std::array<float, 256>>();

	std::generate(m_GammaCorrectionLUT->begin(), m_GammaCorrectionLUT->end(), [i = 0]() mutable {
		return std::pow((i++) / 255.0f, 1.0f / GAMMA_CORRECTION);
	});
}

std::shared_ptr<CFont> CFontManager::LoadFont(CStrIntern fontName)
{
	const std::string locale{g_L10n.GetCurrentLocale() != icu::Locale::getUS() ? g_L10n.GetCurrentLocaleString() : ""};
	const float guiScale{g_ConfigDB.Get("gui.scale", 1.0f)};
	CStrIntern localeFontName{fmt::format("{}{}-{}", locale ,fontName.string(), guiScale)};

	FontsMap::iterator it{m_Fonts.find(localeFontName)};
	if (it != m_Fonts.end())
		return it->second;

	// TODO: use hooks or something to hotrealoding default font.
	const std::string defaultFont{g_ConfigDB.Get("fonts.default", std::string{})};

	if (defaultFont.empty())
	{
		LOGERROR("Default font not set in config");
		return nullptr;
	}

	// FontName contain the format fontType(-fontBold|fontItalic)(-fontStroke)-fontSize.
	// We are going to split it to get the fontType and fontSize.
	FontSpec fontSpec{ParseFontSpec(fontName.string())};

	if (fontSpec.type.empty())
	{
		LOGERROR("Failed to parse font specification: %s, using default font", fontName.string().c_str());
		fontSpec = ParseFontSpec(str_sans_10.string());
	}

	// Check for font configuration or fallback.
	const std::string fontToSearch{[&]
		{
			std::vector<std::string> candidateFonts;
			// 3 types * 2 (bold, italic).
			candidateFonts.reserve(6);

			// TODO: explicit Locale like RTL or Arabic fonts.
			// 1. Locale-specific fonts first
			if (!locale.empty())
			{
				if (fontSpec.bold)
					candidateFonts.push_back(fmt::format("fonts.{}.{}.bold", locale, fontSpec.type));
				if (fontSpec.italic)
					candidateFonts.push_back(fmt::format("fonts.{}.{}.italic", locale, fontSpec.type));
				candidateFonts.push_back(fmt::format("fonts.{}.{}.regular", locale, fontSpec.type));
			}

			// 2. Then global fonts
			if (fontSpec.bold)
				candidateFonts.push_back(fmt::format("fonts.{}.bold", fontSpec.type));
			if (fontSpec.italic)
				candidateFonts.push_back(fmt::format("fonts.{}.italic", fontSpec.type));
			candidateFonts.push_back(fmt::format("fonts.{}.regular", fontSpec.type));

			for (const std::string& key : candidateFonts)
			{
				std::string value = g_ConfigDB.Get(key, std::string{});
				if (!value.empty())
					return value;
			}

			// Fallback to default.
			return defaultFont;
		}()
	};

	std::shared_ptr<CFont> font{std::make_shared<CFont>(this->m_FreeType.get(), m_GammaCorrectionLUT)};

	const VfsPath path(L"fonts/");
	const VfsPath fntName(fontToSearch);
	OsPath realPath;

	if (!VfsFileExists(path / fntName))
	{
		LOGERROR("Font file %s not found", fontToSearch.c_str());
		return nullptr;
	}

	g_VFS->GetOriginalPath(path / fntName, realPath);
	if (realPath.empty())
	{
		LOGERROR("Font file %s not found", fontToSearch.c_str());
		return nullptr;
	}

	// TODO: For now set strokeWith = 1, later we can expose it to the GUI.
	if (!font->SetFontFromPath(realPath.string8(), localeFontName.string(), fontSpec.size, fontSpec.stroke ? 1.0f : 0.0f, guiScale))
	{
		return nullptr;
	}

	m_Fonts[localeFontName] = font;
	return font;
}

void CFontManager::UploadTexturesAtlasToGPU()
{
	for (auto& [fontName, fontPtr] : m_Fonts)
	{
		if (!fontPtr)
			continue;

		fontPtr->UploadTextureAtlasToGPU();
	}
}
