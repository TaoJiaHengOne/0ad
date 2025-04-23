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

#ifndef INCLUDED_FONT
#define INCLUDED_FONT

#include "graphics/Texture.h"
#include "maths/Rect.h"
#include "ps/CLogger.h"
#include "renderer/Renderer.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H
#include FT_OUTLINE_H
#include FT_GLYPH_H
#include FT_BITMAP_H
#include <optional>

/**
 * Storage for a bitmap font. Loaded by CFontManager.
 */
class CFont
{
public:
	CFont(FT_Library library, std::shared_ptr<std::array<float, 256>> gammaCorrection) : m_FreeType{library}, m_GammaCorrectionLUT{gammaCorrection} {};
	~CFont() = default;
	NONCOPYABLE(CFont);

	struct GlyphData
	{
		float u0, v0, u1, v1;
		i16 x0, y0, x1, y1;
		i16 xadvance;
		u8 defined{0};
	};

	/**
	 * Relatively efficient lookup of GlyphData from 16-bit Unicode codepoint.
	 * This is stored as a sparse 2D array, exploiting the knowledge that a font
	 * typically only supports a small number of 256-codepoint blocks, so most
	 * elements of m_Data will be NULL.
	 * This implementation expand the store the GlyphData for a given Unicode codepoint (0 ≤ cp ≤ 0x10FFFF)
	 */
	class GlyphMap
	{
	public:
		GlyphMap() = default;
		~GlyphMap() = default;
		NONCOPYABLE(GlyphMap);

		/**
		* @brief Store the GlyphData for a given Unicode codepoint
		*
		* @param codepoint The unicode codepoint (0 ≤ cp ≤ 0x10FFFF)
		* @param glyph The glyphData data to store
		*/
		void set(u16 codepoint, const GlyphData& glyph);

		const GlyphData* get(u16 codepoint) const;
	private:
		std::unique_ptr<std::array<GlyphData, 256>> m_Data[256];
	};

	bool HasRGB() const { return m_HasRGB; }
	int GetLineSpacing() const { return m_LineSpacing; }
	int GetHeight() const { return m_Height; }
	int GetCharacterWidth(wchar_t c);
	int GetStrokeWidth() const { return m_StrokeWidth; }
	void CalculateStringSize(const wchar_t* string, int& w, int& h);
	void GetGlyphBounds(float& x0, float& y0, float& x1, float& y1) const
	{
		x0 = m_Bounds.left;
		y0 = m_Bounds.top;
		x1 = m_Bounds.right;
		y1 = m_Bounds.bottom;
	}

	CTexturePtr GetTexture() const { return m_Texture; }
	void UploadTextureAtlasToGPU();
	const GlyphData* GetGlyph(u16 i);
private:
	static void ftFaceDeleter(FT_Face face) {
		FT_Done_Face(face);
	}
	static void ftStrokerDeleter(FT_Stroker stroker) {
		FT_Stroker_Done(stroker);
	}

	struct Offset
	{
		int x{0};
		int y{0};
	};

	friend class CFontManager;

	bool SetFontFromPath(const std::string& fontPath, const std::string& fontName, int size, int strokeWidth);

	void BlendGlyphBitmapToTexture(const FT_Bitmap& bitmap, int targetX, int targetY, u8 r, u8 g, u8 b);
	void BlendGlyphBitmapToTextureRGBA(const FT_Bitmap& bitmap, int targetX, int targetY, u8 r, u8 g, u8 b);
	void BlendGlyphBitmapToTextureR8(const FT_Bitmap& bitmap, int targetX, int targetY);

	std::optional<Offset> GenerateStrokeGlyphBitmap(const FT_Glyph& glyph, u16 codepoint, FT_Render_Mode renderMode, const int baselineInAtlas);
	std::optional<Offset> GenerateGlyphBitmap(FT_Glyph& glyph, u16 codepoint, FT_Render_Mode renderMode, Offset offset, const int baselineInAtlas);

	const GlyphData* ExtractAndGenerateGlyph(u16 codepoint);
	bool ConstructTextureAtlas();
	Renderer::Backend::Sampler::Desc ChooseTextureFormatAndSampler();

	CTexturePtr m_Texture;

	// True if RGBA, false if ALPHA.
	bool m_HasRGB{true};

	GlyphMap m_Glyphs;

	int m_LineSpacing;

	// Height of a capital letter, roughly.
	int m_Height;

	// Bounding box of all glyphs
	CRect m_Bounds;

	// The position for the next glyph in the texture atlas.
	int m_AtlasX;
	int m_AtlasY;

	// Size of the texture atlas.
	int m_AtlasWidth;
	int m_AtlasHeight;

	int m_AtlasPadding;
	bool m_IsDirty{false};
	bool m_IsLoading{false};
	int m_StrokeWidth{0};

	std::unique_ptr<u8[]> m_TexData;
	Renderer::Backend::Format m_TextureFormat{Renderer::Backend::Format::R8G8B8A8_UNORM};
	int m_TextureFormatStride{4};
	int m_AtlasSize{0};

	int m_FontSize{0};
	std::string m_FontName;
	std::shared_ptr<std::array<float, 256>> m_GammaCorrectionLUT{nullptr};

	FT_Library m_FreeType;
	std::unique_ptr<FT_FaceRec_, decltype(&ftFaceDeleter)> m_Font{nullptr, &ftFaceDeleter};
	std::unique_ptr<FT_StrokerRec_, decltype(&ftStrokerDeleter)> m_Stroker{nullptr, &ftStrokerDeleter};

	/**
	* FreeType represents most of its size and position values in 26.6 fixed-point format — that is,
	* 26 bits for the integer part and 6 bits for the fractional part.
	* FreeType's metrics such as: ascender, descender, height, advance, etc. are measured in 1/64th of a pixel.
	*/
	static constexpr int SUBPIXEL_SHIFT{6};

	/**
	* Some fonts are not rendered well at small sizes, so we set a minimum size.
	* Because we are using a bitmap blending mode, when a font is using small size,
	* We need to use a different render mode, one with less antialiasing.
	*/
	static constexpr int MINIMAL_FONT_SIZE_ANTIALIASING{12};
};

#endif // INCLUDED_FONT
