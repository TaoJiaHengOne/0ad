# Dumps lines containing the name of a font followed by a space-separated
# list of decimal codepoints (from U+0001 to U+FFFF) for which that font
# contains some glyph data.

import font_loader


def dump_font(ttf):
    (face, indexes) = font_loader.create_cairo_font_face_for_file(
        f"../../../binaries/data/tools/fontbuilder/fonts/{ttf}", 0, font_loader.FT_LOAD_DEFAULT
    )

    mappings = [(c, indexes(chr(c))) for c in range(1, 65535)]
    print(ttf, end=" ")
    print(" ".join(str(c) for (c, g) in mappings if g != 0))


dump_font("DejaVuSansMono.ttf")
dump_font("FreeSans.ttf")
dump_font("LinBiolinum_Rah.ttf")
dump_font("texgyrepagella-regular.otf")
dump_font("texgyrepagella-bold.otf")
