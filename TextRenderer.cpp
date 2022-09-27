#include <ft2build.h>
#include FT_FREETYPE_H

#include <map>
#include <utility>
#include <stdio.h>
#include <math.h>

#include <hb.h>
#include <hb-ft.h>

#include <iostream>

#include "ColorProgram.hpp"
#include "TextRenderer.hpp"

static GLuint vertex_buffer = 0;
static GLuint vertex_buffer_for_color_program = 0;

static FT_Library ft_library;
static FT_Face ft_face;

// REMOVE CONST FONT SIZE

static hb_font_t *hb_font;

static Load< void > setup_buffers(LoadTagDefault, []() {
    // mostly copied from drawlines:
	
    { //set up vertex buffer:
		glGenBuffers(1, &vertex_buffer);
		//for now, buffer will be un-filled.
	}

	{ //vertex array mapping buffer for color_program:
		//ask OpenGL to fill vertex_buffer_for_color_program with the name of an unused vertex array object:
		glGenVertexArrays(1, &vertex_buffer_for_color_program);

		//set vertex_buffer_for_color_program as the current vertex array object:
		glBindVertexArray(vertex_buffer_for_color_program);

		//set vertex_buffer as the source of glVertexAttribPointer() commands:
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

		//set up the vertex array object to describe arrays of PongMode::Vertex:
		glVertexAttribPointer(
			color_program->Position_vec4, //attribute
			3, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(TextRenderer::Vertex), //stride
			(GLbyte *)0 + offsetof(TextRenderer::Vertex, Position) //offset
		);
		glEnableVertexAttribArray(color_program->Position_vec4);
		//[Note that it is okay to bind a vec3 input to a vec4 attribute -- the w component will be filled with 1.0 automatically]

		glVertexAttribPointer(
			color_program->Color_vec4, //attribute
			4, //size
			GL_UNSIGNED_BYTE, //type
			GL_TRUE, //normalized
			sizeof(TextRenderer::Vertex), //stride
			(GLbyte *)0 + offsetof(TextRenderer::Vertex, Color) //offset
		);
		glEnableVertexAttribArray(color_program->Color_vec4);

		//done referring to vertex_buffer, so unbind it:
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//done setting up vertex array object, so unbind it:
		glBindVertexArray(0);
	}
});

Load<std::map<hb_codepoint_t, TextRenderer::Character>> RobotoCharacters(
		LoadTagDefault, []() -> std::map<hb_codepoint_t, TextRenderer::Character> const * {

	std::map<hb_codepoint_t, TextRenderer::Character> *ret = new 
		std::map<hb_codepoint_t, TextRenderer::Character>;

    FT_Error ft_error;
    if ((ft_error = FT_Init_FreeType (&ft_library)))
		abort();
	if ((ft_error = FT_New_Face (ft_library, "~/Library/Fonts/Roboto/Roboto-Regular.ttf", 0, &ft_face)))
		abort();
	if ((ft_error = FT_Set_Char_Size (ft_face, 18*64, 18*64, 0, 0)))
		abort();

	// set size to load glyphs as
	FT_Set_Pixel_Sizes(ft_face, 0, 48);

	// disable byte-alignment restriction
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    hb_font = hb_ft_font_create_referenced (ft_face);

    // generate Characters
    FT_Long num_glyphs = ft_face->num_glyphs;

	for (hb_codepoint_t i = 0; i < num_glyphs; i++) {
		if (FT_Load_Glyph(ft_face, i, FT_LOAD_RENDER)) {
			std::cout << "Error: Failed to load glyph " << i << std::endl;
			continue;
		}
		// generate texture
		unsigned int texture;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RED,
			ft_face->glyph->bitmap.width,
			ft_face->glyph->bitmap.rows,
			0,
			GL_RED,
			GL_UNSIGNED_BYTE,
			ft_face->glyph->bitmap.buffer
		);
		// set texture options
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// now store character for later use
		TextRenderer::Character character = {
			texture,
			glm::ivec2(ft_face->glyph->bitmap.width, ft_face->glyph->bitmap.rows),
			glm::ivec2(ft_face->glyph->bitmap_left, ft_face->glyph->bitmap_top),
			static_cast<unsigned int>(ft_face->glyph->advance.x)
		};
		(*ret)[i] = character;
		// ret->insert(std::pair<hb_codepoint_t, TextRenderer::Character>(i, character));
	}
    glBindTexture(GL_TEXTURE_2D, 0);
	return ret;
});

TextRenderer::TextRenderer() {

}

void TextRenderer::draw_string(std::string text) {
    string_to_draw = text;
}

TextRenderer::~TextRenderer() {

    float x = 5.0f;
    float y = 5.0f;
    // glm::u8vec4 const color = glm::u8vec4(0x00, 0x00, 0x00, 0x00);
    
	hb_buffer_t *buf = hb_buffer_create();
	hb_buffer_add_utf8 (buf, string_to_draw.c_str(), -1, 0, -1);
	hb_buffer_guess_segment_properties (buf);
	
	/* Shape it! */
	hb_shape (hb_font, buf, NULL, 0);

	/* Get glyph information and positions out of the buffer. */
	unsigned int len = hb_buffer_get_length (buf);
	hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buf, NULL);
	// hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (buf, NULL);
    
    // Iterate through every character and draw
    for (unsigned int i = 0; i < len; i++) {
		hb_codepoint_t glyph_idx = info[i].codepoint;
		Character ch = RobotoCharacters->at(glyph_idx);
        float xpos = x + ch.Bearing.x;
        float ypos = y - (ch.Size.y - ch.Bearing.y);

        float w = ch.Size.x;
        float h = ch.Size.y;
        // update VBO for each character
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },            
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }           
        };
        // render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        // update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // be sure to use glBufferSubData and not glBufferData

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
        x += (ch.Advance >> 6); // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
