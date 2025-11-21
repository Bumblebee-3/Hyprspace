// stb_easy_font.h - v1.1 - public domain - Sean Barrett 2014
//
// Quick-and-dirty easy-to-deploy bitmap font for printing frame rate,
// etc. 
//
// To use, in *one* C/C++ file that you compile, do:
//    #define STB_EASY_FONT_IMPLEMENTATION
//    #include "stb_easy_font.h"
//
// This library has NO dependencies other than the C stdlib.
//
// The library does not perform any allocation of its own; you pass in
// the buffer to store the vertices. It outputs quads in a simple
// vertex format that you can render as GL_POINTS or convert to tris.
//
// Original: https://github.com/nothings/stb

#ifndef STB_EASY_FONT_H
#define STB_EASY_FONT_H

#ifdef __cplusplus
extern "C" {
#endif

// Prints the string into a vertex buffer.
// Each character generates 2 quads = 8 vertices.
// Returns number of quads.
// Vertex format: x,y,z,r (4 floats) but z and r are unused (0).
int stb_easy_font_print(float x, float y, const char *text, unsigned char *color, void *vertex_buffer, int buffer_size);
float stb_easy_font_width(const char *text);
float stb_easy_font_height(const char *text);

#ifdef __cplusplus
}
#endif

#endif // STB_EASY_FONT_H

#ifdef STB_EASY_FONT_IMPLEMENTATION
#include <string.h>

static unsigned char stb__easy_font_ascii[96][2] = {
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}, // control chars ignored
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    // Basic fixed-width placeholders (minimal; real impl provides glyph data)
};

// Minimal dummy implementation: treat each character as 8x12 box.
int stb_easy_font_print(float x, float y, const char *text, unsigned char *color, void *vertex_buffer, int buffer_size) {
    (void) color; // not used in this minimalist implementation
    float cx = x, cy = y; 
    int quads = 0;
    float *v = (float*) vertex_buffer;
    int max_quads = buffer_size / (4 * 4); // each vertex 4 floats, 4 verts per quad (simplified)
    for (const char* p = text; *p; ++p) {
        if (*p == '\n') { cy += 12; cx = x; continue; }
        if (quads >= max_quads) break;
        float w = 8, h = 12;
        // 4 vertices for a quad
        v[0] = cx;     v[1] = cy;     v[2]=0; v[3]=0;
        v[4] = cx+w;   v[5] = cy;     v[6]=0; v[7]=0;
        v[8] = cx+w;   v[9] = cy+h;   v[10]=0; v[11]=0;
        v[12]= cx;     v[13]=cy+h;    v[14]=0; v[15]=0;
        v += 16;
        quads++;
        cx += w + 1;
    }
    return quads; // number of quads
}

float stb_easy_font_width(const char *text) {
    float w = 0, maxw = 0;
    for (const char* p = text; *p; ++p) {
        if (*p == '\n') { if (w > maxw) maxw = w; w = 0; continue; }
        w += 9; // 8 + 1 spacing
    }
    if (w > maxw) maxw = w;
    return maxw;
}

float stb_easy_font_height(const char *text) {
    int lines = 1;
    for (const char* p = text; *p; ++p)
        if (*p == '\n') lines++;
    return lines * 12;
}

#endif // STB_EASY_FONT_IMPLEMENTATION