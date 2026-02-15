#include <bootleg/game.hpp>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <numbers>
namespace boot {
Vector2 measure_codepoint_3d(int codepoint, Font font, float font_size)
{
    if (font.texture.id <= 0) {
        TraceLog(LOG_ERROR, "draw_codepoint_3d: font texture was null");
        return Vector2Zero();
    }
    const auto glyph_rec = GetGlyphAtlasRec(font, codepoint);
    const float scale = font_size / static_cast<float>(font.baseSize);

    const float width = (glyph_rec.width + 2 * font.glyphPadding) * scale;
    const float height = (glyph_rec.height + 2 * font.glyphPadding) * scale;
    return { width, height };
}
Vector2 measure_text_3d(const char* txt, Font font, float font_size, int spacing)
{
    Vector2 ret = {};
    if (font.texture.id <= 0) {
        TraceLog(LOG_ERROR, "draw_text_3d: font texture was null");
        return ret;
    }
    const auto view = std::string_view(txt);
    for (size_t c = 0; c < view.size();) {
        int csz = 1;
        const int codepoint = GetCodepoint(view.data() + c, &csz);
        const auto sz = measure_codepoint_3d(codepoint, font, font_size);
        c += csz;
        ret.y = sz.y;
        ret.x += sz.x + spacing;
    }
    return ret;
}
void draw_text_3d(const char* txt, Font font, Vector3 pos, float font_size, int spacing, const Color& color, bool backface, const Rotation& rotation)
{
    if (font.texture.id <= 0) {
        TraceLog(LOG_ERROR, "draw_text_3d: font texture was null");
        return;
    }
    const auto dir = Vector3Normalize(Vector3RotateByAxisAngle({ 1, 0, 0 },
                {rotation.x, rotation.y, rotation.z}, rotation.angle * (std::numbers::pi / 180.)));
    const auto view = std::string_view(txt);
    for (size_t c = 0; c < view.size();) {
        int csz = 1;
        const int codepoint = GetCodepoint(view.data() + c, &csz);
        const auto sz = draw_codepoint_3d(codepoint, font, pos, font_size, color, backface, rotation);
        c += csz;
        const auto tmp = Vector3Scale(dir, sz.x + spacing);
        pos = Vector3Add(pos, tmp);
    }
}
Vector2 draw_codepoint_3d(int codepoint, Font font, const Vector3& pos,
    float font_size, const Color& color, bool backface, const Rotation& rotation)
{
    if (font.texture.id <= 0) {
        TraceLog(LOG_ERROR, "draw_codepoint_3d: font texture was null");
        return Vector2Zero();
    }
    const auto glyph_rec = GetGlyphAtlasRec(font, codepoint);
    const float scale = font_size / static_cast<float>(font.baseSize);

    const float width = (glyph_rec.width + 2 * font.glyphPadding) * scale;
    const float height = (glyph_rec.height + 2 * font.glyphPadding) * scale;

    const float x = 0.0f;
    const float y = 0.0f;
    const float z = 0.0f;

    const float tx = glyph_rec.x / font.texture.width;
    const float ty = glyph_rec.y / font.texture.height;
    const float tw = (glyph_rec.x + glyph_rec.width) / font.texture.width;
    const float th = (glyph_rec.y + glyph_rec.height) / font.texture.height;

    rlSetTexture(font.texture.id);
    rlPushMatrix();
    {
        rlTranslatef(pos.x, pos.y, pos.z);
        rlRotatef(rotation.angle, rotation.x, rotation.y, rotation.z);
        rlBegin(RL_QUADS);
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            const auto dir = Vector3Normalize(Vector3RotateByAxisAngle({ 0, 0, 1 },
                        {rotation.x, rotation.y, rotation.z}, rotation.angle * (std::numbers::pi / 180.)));
            rlNormal3f(dir.x, dir.y, dir.z);
            rlTexCoord2f(tx, ty);
            rlVertex3f(x, y, z);
            rlTexCoord2f(tx, th);
            rlVertex3f(x, y - height, z);
            rlTexCoord2f(tw, th);
            rlVertex3f(x + width, y - height, z);
            rlTexCoord2f(tw, ty);
            rlVertex3f(x + width, y, z);
            if (backface) {
                rlNormal3f(0.0f, 0.0f, -1.0f);
                rlTexCoord2f(tx, ty);
                rlVertex3f(x, y, z);
                rlTexCoord2f(tw, ty);
                rlVertex3f(x + width, y, z);
                rlTexCoord2f(tw, th);
                rlVertex3f(x + width, y - height, z);
                rlTexCoord2f(tx, th);
                rlVertex3f(x, y - height, z);
            }
        }
        rlEnd();
    }
    rlPopMatrix();
    rlSetTexture(0);
    return { width, height };
}
}
