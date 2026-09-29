#include "maishuji/pvr.hpp"

#include "assets/multilingual_font.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <kos.h>

namespace {

constexpr float output_scale = 2.0f;
constexpr float screen_width = 640.0f;
constexpr int frames = 600;
// Keep the final frame visible long enough for the Flycast checker.
constexpr int capture_hold_frames = 900;
constexpr int expected_glyph_quads = 28;

constexpr std::string_view japanese_text = "ケーキは嘘だ。";
constexpr std::string_view traditional_chinese_text = "蛋糕是個謊言。";
constexpr std::string_view english_text = "The cake is a lie.";

struct PositionedGlyph {
    maishuji::TexturedQuad quad{};
};

bool continuation_byte(unsigned char byte) noexcept {
    return (byte & 0xc0) == 0x80;
}

bool decode_utf8(std::string_view text, std::size_t &offset,
                 std::uint32_t &codepoint) noexcept {
    if(offset >= text.size())
        return false;

    const unsigned char first = static_cast<unsigned char>(text[offset++]);
    if(first < 0x80) {
        codepoint = first;
        return true;
    }

    if(first >= 0xc2 && first <= 0xdf) {
        if(offset >= text.size())
            return false;
        const unsigned char second = static_cast<unsigned char>(text[offset++]);
        if(!continuation_byte(second))
            return false;
        codepoint = ((first & 0x1f) << 6) | (second & 0x3f);
        return true;
    }

    if(first >= 0xe0 && first <= 0xef) {
        if(offset + 1 >= text.size())
            return false;
        const unsigned char second = static_cast<unsigned char>(text[offset++]);
        const unsigned char third = static_cast<unsigned char>(text[offset++]);
        if(!continuation_byte(second) || !continuation_byte(third) ||
           (first == 0xe0 && second < 0xa0) ||
           (first == 0xed && second >= 0xa0))
            return false;
        codepoint = ((first & 0x0f) << 12) |
                    ((second & 0x3f) << 6) | (third & 0x3f);
        return codepoint < 0xd800 || codepoint > 0xdfff;
    }

    if(first >= 0xf0 && first <= 0xf4) {
        if(offset + 2 >= text.size())
            return false;
        const unsigned char second = static_cast<unsigned char>(text[offset++]);
        const unsigned char third = static_cast<unsigned char>(text[offset++]);
        const unsigned char fourth = static_cast<unsigned char>(text[offset++]);
        if(!continuation_byte(second) || !continuation_byte(third) ||
           !continuation_byte(fourth) ||
           (first == 0xf0 && second < 0x90) ||
           (first == 0xf4 && second >= 0x90))
            return false;
        codepoint = ((first & 0x07) << 18) |
                    ((second & 0x3f) << 12) |
                    ((third & 0x3f) << 6) | (fourth & 0x3f);
        return codepoint <= 0x10ffff;
    }

    return false;
}

const maishuji::advanced_text_asset::Glyph *find_glyph(
    std::uint32_t codepoint) noexcept {
    for(const auto &glyph : maishuji::advanced_text_asset::glyphs) {
        if(glyph.codepoint == codepoint)
            return &glyph;
    }
    return nullptr;
}

bool measure_text(std::string_view text, float &width) noexcept {
    width = 0.0f;
    std::size_t offset = 0;
    while(offset < text.size()) {
        std::uint32_t codepoint = 0;
        if(!decode_utf8(text, offset, codepoint))
            return false;

        if(codepoint == ' ') {
            width += 12.0f * output_scale;
            continue;
        }

        const auto *glyph = find_glyph(codepoint);
        if(glyph == nullptr)
            return false;
        width += static_cast<float>(glyph->advance) * output_scale;
    }
    return true;
}

maishuji::TexturedQuad make_quad(
    const maishuji::advanced_text_asset::Glyph &glyph, float x, float y,
    const maishuji::Color &color) noexcept {
    const float u0 = static_cast<float>(glyph.cell_x) /
                     maishuji::advanced_text_asset::atlas_width;
    const float v0 = static_cast<float>(glyph.cell_y) /
                     maishuji::advanced_text_asset::atlas_height;
    const float u1 = static_cast<float>(glyph.cell_x +
                                        maishuji::advanced_text_asset::cell_size) /
                     maishuji::advanced_text_asset::atlas_width;
    const float v1 = static_cast<float>(glyph.cell_y +
                                        maishuji::advanced_text_asset::cell_size) /
                     maishuji::advanced_text_asset::atlas_height;
    const float width = static_cast<float>(glyph.advance) * output_scale;
    const float height =
        maishuji::advanced_text_asset::cell_size * output_scale;

    return {
        {x, y, 1.0f, u0, v0, color},
        {x, y + height, 1.0f, u0, v1, color},
        {x + width, y, 1.0f, u1, v0, color},
        {x + width, y + height, 1.0f, u1, v1, color},
    };
}

template <std::size_t Capacity>
bool build_line(std::array<PositionedGlyph, Capacity> &output,
                std::size_t &output_count, std::string_view text, float y,
                const maishuji::Color &color) noexcept {
    float width = 0.0f;
    if(!measure_text(text, width))
        return false;

    output_count = 0;
    float x = (screen_width - width) * 0.5f;
    std::size_t offset = 0;
    while(offset < text.size()) {
        std::uint32_t codepoint = 0;
        if(!decode_utf8(text, offset, codepoint))
            return false;

        if(codepoint == ' ') {
            x += 12.0f * output_scale;
            continue;
        }

        const auto *glyph = find_glyph(codepoint);
        if(glyph == nullptr || output_count >= output.size())
            return false;

        output[output_count++].quad = make_quad(*glyph, x, y, color);
        x += static_cast<float>(glyph->advance) * output_scale;
    }
    return true;
}

maishuji::Status submit_line(
    maishuji::RenderList &list, const maishuji::Texture &texture,
    std::span<const PositionedGlyph> glyphs) noexcept {
    for(const PositionedGlyph &glyph : glyphs) {
        const maishuji::Status status = list.submit(texture, glyph.quad);
        if(maishuji::failed(status))
            return status;
    }
    return maishuji::Status::Success;
}

maishuji::Status run_frame(
    maishuji::Pvr &pvr, const maishuji::Texture &texture,
    std::span<const PositionedGlyph> japanese,
    std::span<const PositionedGlyph> traditional_chinese,
    std::span<const PositionedGlyph> english) noexcept {
    maishuji::Frame frame;
    maishuji::Status status = pvr.begin_frame(frame);
    if(maishuji::failed(status))
        return status;

    maishuji::RenderList text;
    status = frame.begin_list(text, maishuji::List::PunchThrough);
    if(maishuji::failed(status))
        return status;

    status = submit_line(text, texture, japanese);
    if(maishuji::failed(status))
        return status;
    status = submit_line(text, texture, traditional_chinese);
    if(maishuji::failed(status))
        return status;
    status = submit_line(text, texture, english);
    if(maishuji::failed(status))
        return status;

    status = text.finish();
    if(maishuji::failed(status))
        return status;

    return frame.finish();
}

} // namespace

int main() {
    std::array<PositionedGlyph, 7> japanese{};
    std::array<PositionedGlyph, 7> traditional_chinese{};
    std::array<PositionedGlyph, 18> english{};
    std::size_t japanese_count = 0;
    std::size_t traditional_chinese_count = 0;
    std::size_t english_count = 0;

    const bool lines_ready =
        build_line(japanese, japanese_count, japanese_text, 64.0f,
                   {96, 220, 255, 255}) &&
        build_line(traditional_chinese, traditional_chinese_count,
                   traditional_chinese_text, 200.0f, {255, 220, 96, 255}) &&
        build_line(english, english_count, english_text, 336.0f,
                   {160, 255, 160, 255});
    if(!lines_ready || japanese_count + traditional_chinese_count +
                           english_count != expected_glyph_quads) {
        dbglog(DBG_ERROR,
               "maishuji: multilingual text atlas could not resolve all glyphs\n");
        return 1;
    }

    maishuji::Pvr pvr;
    maishuji::Status status = pvr.initialize();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR initialization failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    maishuji::Texture texture;
    status = texture.allocate(pvr, maishuji::advanced_text_asset::atlas_width,
                              maishuji::advanced_text_asset::atlas_height);
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR,
               "maishuji: multilingual atlas allocation failed: %s\n",
               maishuji::status_name(status));
        (void)pvr.shutdown();
        return 1;
    }

    status = texture.upload(std::span<const std::uint16_t>{
        maishuji::advanced_text_asset::pixels.data(),
        maishuji::advanced_text_asset::pixels.size()});
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: multilingual atlas upload failed: %s\n",
               maishuji::status_name(status));
        (void)texture.release();
        (void)pvr.shutdown();
        return 1;
    }

    const auto japanese_view = std::span<const PositionedGlyph>{
        japanese.data(), japanese_count};
    const auto traditional_chinese_view = std::span<const PositionedGlyph>{
        traditional_chinese.data(), traditional_chinese_count};
    const auto english_view =
        std::span<const PositionedGlyph>{english.data(), english_count};

    for(int frame = 0; frame < frames; ++frame) {
        status = run_frame(pvr, texture, japanese_view,
                           traditional_chinese_view, english_view);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: multilingual text frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    dbglog(DBG_NOTICE,
           "maishuji: advanced multilingual text passed (%d glyph quads; "
           "one ARGB4444 atlas)\n",
           expected_glyph_quads);

    for(int frame = 0; frame < capture_hold_frames; ++frame) {
        status = run_frame(pvr, texture, japanese_view,
                           traditional_chinese_view, english_view);
        if(maishuji::failed(status)) {
            dbglog(DBG_ERROR,
                   "maishuji: multilingual capture hold frame %d failed: %s\n",
                   frame, maishuji::status_name(status));
            (void)texture.release();
            (void)pvr.shutdown();
            return 1;
        }
    }

    status = texture.release();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: multilingual atlas release failed: %s\n",
               maishuji::status_name(status));
        (void)pvr.shutdown();
        return 1;
    }

    status = pvr.shutdown();
    if(maishuji::failed(status)) {
        dbglog(DBG_ERROR, "maishuji: PVR shutdown failed: %s\n",
               maishuji::status_name(status));
        return 1;
    }

    return 0;
}
