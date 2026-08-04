/* Generates the compact alpha atlas used by the native MUI text renderer. */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIRST_GLYPH 32
#define GLYPH_COUNT 95
#define CELL_WIDTH 16
#define CELL_HEIGHT 18
#define BASELINE 14
#define PIXEL_SIZE 14
#define FONT_WEIGHT 450

static uint8_t atlas[GLYPH_COUNT][CELL_WIDTH * CELL_HEIGHT];
static uint8_t advances[GLYPH_COUNT];

static void emit_values(const uint8_t* values, size_t count, int columns) {
    for (size_t i = 0; i < count; i++) {
        if (i % (size_t)columns == 0) {
            fputs("    ", stdout);
        }
        printf("%3u", values[i]);
        if (i + 1 != count) {
            fputc(',', stdout);
        }
        if (i % (size_t)columns == (size_t)columns - 1 || i + 1 == count) {
            fputc('\n', stdout);
        } else {
            fputc(' ', stdout);
        }
    }
}

int main(int argc, char** argv) {
    FT_Library library;
    FT_Face face;
    FT_MM_Var* variations = NULL;

    if (argc != 2) {
        fprintf(stderr, "usage: %s path/to/Outfit.ttf\n", argv[0]);
        return 2;
    }
    if (FT_Init_FreeType(&library) != 0) {
        fputs("fontgen: FreeType initialization failed\n", stderr);
        return 1;
    }
    if (FT_New_Face(library, argv[1], 0, &face) != 0) {
        fprintf(stderr, "fontgen: cannot load %s\n", argv[1]);
        FT_Done_FreeType(library);
        return 1;
    }
    if (FT_Get_MM_Var(face, &variations) == 0 && variations) {
        FT_Fixed* coordinates =
            (FT_Fixed*)calloc(variations->num_axis, sizeof(*coordinates));
        if (!coordinates) {
            fputs("fontgen: out of memory\n", stderr);
            FT_Done_MM_Var(library, variations);
            FT_Done_Face(face);
            FT_Done_FreeType(library);
            return 1;
        }
        for (FT_UInt axis = 0; axis < variations->num_axis; axis++) {
            coordinates[axis] = variations->axis[axis].def;
            if (variations->axis[axis].tag == FT_MAKE_TAG('w', 'g', 'h', 't')) {
                coordinates[axis] = (FT_Fixed)FONT_WEIGHT << 16;
            }
        }
        if (FT_Set_Var_Design_Coordinates(face, variations->num_axis,
                                          coordinates) != 0) {
            fputs("fontgen: cannot select font weight\n", stderr);
            free(coordinates);
            FT_Done_MM_Var(library, variations);
            FT_Done_Face(face);
            FT_Done_FreeType(library);
            return 1;
        }
        free(coordinates);
        FT_Done_MM_Var(library, variations);
    }
    if (FT_Set_Pixel_Sizes(face, 0, PIXEL_SIZE) != 0) {
        fputs("fontgen: cannot select font size\n", stderr);
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    for (int codepoint = FIRST_GLYPH; codepoint < FIRST_GLYPH + GLYPH_COUNT;
         codepoint++) {
        int index = codepoint - FIRST_GLYPH;
        FT_GlyphSlot glyph;
        int x0;
        int y0;

        if (FT_Load_Char(face, (FT_ULong)codepoint,
                         FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL) != 0) {
            fprintf(stderr, "fontgen: cannot rasterize U+%04x\n", codepoint);
            FT_Done_Face(face);
            FT_Done_FreeType(library);
            return 1;
        }
        glyph = face->glyph;
        advances[index] = (uint8_t)(glyph->advance.x >> 6);
        if (advances[index] == 0) advances[index] = 1;
        x0 = glyph->bitmap_left;
        y0 = BASELINE - glyph->bitmap_top;

        for (unsigned int row = 0; row < glyph->bitmap.rows; row++) {
            for (unsigned int column = 0; column < glyph->bitmap.width; column++) {
                int x = x0 + (int)column;
                int y = y0 + (int)row;
                if (x < 0 || y < 0 || x >= CELL_WIDTH || y >= CELL_HEIGHT) {
                    continue;
                }
                atlas[index][y * CELL_WIDTH + x] =
                    glyph->bitmap.buffer[row * glyph->bitmap.pitch + column];
            }
        }
    }

    puts("/* Generated from Outfit weight 450 at 14 px. See mui_font_outfit.LICENSE.txt. */");
    puts("#ifndef AOS_MUI_FONT_OUTFIT_H");
    puts("#define AOS_MUI_FONT_OUTFIT_H");
    puts("");
    puts("#include <stdint.h>");
    puts("");
    printf("#define MUI_FONT_FIRST %d\n", FIRST_GLYPH);
    printf("#define MUI_FONT_COUNT %d\n", GLYPH_COUNT);
    printf("#define MUI_FONT_CELL_WIDTH %d\n", CELL_WIDTH);
    printf("#define MUI_FONT_CELL_HEIGHT %d\n", CELL_HEIGHT);
    puts("");
    puts("static const uint8_t mui_font_advance[MUI_FONT_COUNT] = {");
    emit_values(advances, GLYPH_COUNT, 16);
    puts("};");
    puts("");
    puts("static const uint8_t");
    puts("mui_font_alpha[MUI_FONT_COUNT][MUI_FONT_CELL_WIDTH * MUI_FONT_CELL_HEIGHT] = {");
    for (int glyph = 0; glyph < GLYPH_COUNT; glyph++) {
        printf("  { /* 0x%02x */\n", glyph + FIRST_GLYPH);
        emit_values(atlas[glyph], CELL_WIDTH * CELL_HEIGHT, CELL_WIDTH);
        printf("  }%s\n", glyph + 1 == GLYPH_COUNT ? "" : ",");
    }
    puts("};");
    puts("");
    puts("#endif");

    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return 0;
}
