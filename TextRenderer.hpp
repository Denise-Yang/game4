#pragma once
// Class to draw text to the screen.

#include <glm/glm.hpp>

#include <map>
#include <string>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

struct TextRenderer {
    TextRenderer();

    // Add support for font selection?
    // Draw text
    void draw_string(std::string text);

    // TEMPORARY
    std::string string_to_draw;

    glm::mat4 world_to_clip;
    struct Vertex {
    Vertex(glm::vec3 const &Position_, glm::u8vec4 const &Color_) : Position(Position_), Color(Color_) { }
    glm::vec3 Position;
    glm::u8vec4 Color;
	};
	std::vector< Vertex > attribs;

    // Finish drawing (push attribs to GPU):
    ~TextRenderer();


    /// Holds all state information relevant to a character as loaded using FreeType
    struct Character {
        unsigned int TextureID; // ID handle of the glyph texture
        glm::ivec2   Size;      // Size of glyph
        glm::ivec2   Bearing;   // Offset from baseline to left/top of glyph
        unsigned int Advance;   // Horizontal offset to advance to next glyph
    };
};
