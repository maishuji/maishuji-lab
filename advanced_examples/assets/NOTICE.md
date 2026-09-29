# Multilingual glyph atlas notice

`multilingual_font.hpp` contains a small ARGB4444 atlas generated from the
regular Noto Sans CJK and Noto Sans faces installed on the development host.
The atlas contains only the glyphs needed by the fixed lesson strings; the
original font files are not redistributed here.

Noto Sans CJK supports Japanese and Traditional Chinese and is released under
the SIL Open Font License, version 1.1. Noto Sans uses the same license family.
See the upstream source and license:

- <https://github.com/notofonts/noto-cjk>
- <https://github.com/notofonts/noto-cjk/blob/main/Sans/LICENSE>
- <https://openfontlicense.org/>

The atlas generator records the selected font family names and can recreate the
tracked header with ImageMagick. Keep this notice with the generated asset.
