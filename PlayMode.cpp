#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "ColorTextureProgram.hpp"
#include <istream>

#include <iostream>     // std::cin, std::cout
#include "DrawLines.hpp"
// #include "TextRenderer.hpp"
#include "shader.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <ctime>
#include <random>

#include <ft2build.h>
#include FT_FREETYPE_H

GLuint hexapod_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > hexapod_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("hexapod.pnct"));
	hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > hexapod_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("hexapod.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = hexapod_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("dusty-floor.opus"));
});

int Attack::activate(Player *user, Player *target) {
	int damage_done = base_damage;
	std::mt19937 mt(std::time(nullptr));
	// check if move hits
	float accuracy_roll = float(mt()) / float(mt.max());
	if (accuracy_roll > accuracy)
		return 0;
	// check for crit
	float crit_roll = float(mt()) / float(mt.max());
	bool was_crit = crit_roll <= crit_chance;
	if (was_crit)
		damage_done *= 2;
	// random damage variance
	int rand_dmg_range = Attack::dmg_variance_range * damage_done;
	damage_done += ((int(mt()) % (2 * rand_dmg_range)) - rand_dmg_range);

	// apply damage
	return std::max(1, damage_done);
}

int Heal::activate(Player *user, Player *target) {
	std::mt19937 mt(std::time(nullptr));
	// check if move hits
	float accuracy_roll = float(mt()) / float(mt.max());
	if (accuracy_roll > accuracy)
		return 0;
	// Heal for half of the target's max health
	return std::max(1, (int)((float)(target->max_health) * percent_heal));
}

Player::Player(unsigned int start_health) {
	max_health = start_health;
	cur_health = start_health;
}

PlayMode::PlayMode() : scene(*hexapod_scene), shader(data_path("text.vs").c_str(), data_path("text.fs").c_str()) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Hip.FL") hip = &transform;
		else if (transform.name == "UpperLeg.FL") upper_leg = &transform;
		else if (transform.name == "LowerLeg.FL") lower_leg = &transform;
	}
	if (hip == nullptr) throw std::runtime_error("Hip not found.");
	if (upper_leg == nullptr) throw std::runtime_error("Upper leg not found.");
	if (lower_leg == nullptr) throw std::runtime_error("Lower leg not found.");
	/**/
	load_dialogue(data_path("testDialogue.txt"));
	// dialogue.push_back({"Hello world", "pls end me", "live laugh love"}); 
	// dialogue.push_back({"You are now dead","yay","damn"});
	// dialogue.push_back({"You're a basic white girl","#JustGirlyTings", "#Slayyy"});


	hip_base_rotation = hip->rotation;
	upper_leg_base_rotation = upper_leg->rotation;
	lower_leg_base_rotation = lower_leg->rotation;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);

	// shader = Shader(data_path("text.vs"), data_path("text.fs"));

	// OpenGL state
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	shader.use();
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	// FreeType
	if (FT_Init_FreeType(&ft)) {
		std::cerr << "Error initializing FreeType Library" << std::endl;
		return;
	}
	std::string font_name = data_path("Roboto-Regular.ttf");
	if (FT_New_Face(ft, font_name.c_str(), 0, &face)) {
		std::cerr << "Error loading freetype font face" << std::endl;
		return;
	}
	FT_Set_Pixel_Sizes(face, 0, 48);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// TEMP: LOAD FRST 128 CHARS OF ASCII SET
	for (unsigned char c = 0; c < 128; c++) {
		// Load character glyph
		if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
			std::cerr << "Error loading glyph " << c << std::endl;
			continue;
		}
		// Generate texture
		unsigned int texture;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RED,
			face->glyph->bitmap.width,
			face->glyph->bitmap.rows,
			0,
			GL_RED,
			GL_UNSIGNED_BYTE,
			face->glyph->bitmap.buffer
		);
		// set texture options
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// now store character for later use
		Character character = {
			texture,
			glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
			glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
			static_cast<unsigned int>(face->glyph->advance.x)
		};
		Characters.insert(std::pair<char, Character>(c, character));
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	FT_Done_Face(face);
	FT_Done_FreeType(ft);
	// configure VAO/VBO for texture quds
	// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

}

PlayMode::~PlayMode() {
}


void PlayMode::load_dialogue(std::string filename){
	std::ifstream file;
	file.open("dist/testDialogue.txt");

	if (file.is_open()) {
		std::string line;
		while (getline( file, line)) {
			// using printf() in all tests for consistency

			size_t pos = 0;
			// size_t prevpos = 0;
			std::string token;
			std::string delimiter = ",";
			std::vector<std::string> text_choice;
			pos = line.find(delimiter);
			while (pos != std::string::npos) {
				token = line.substr(0, pos);
				line = line.substr(pos+1);
				pos = line.find(delimiter);
				text_choice.push_back(token);
			}
			text_choice.push_back(line);
			dialogue.push_back(text_choice);
		}
    	file.close();
	}
}
	
	

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	windowW = window_size.x;
	windowH = window_size.y;
	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			a.downs += 1;
			a.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			s.downs += 1;
			s.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			d.downs += 1;
			d.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_f) {
			f.downs += 1;
			f.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_j) {
			j.downs += 1;
			j.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_k) {
			k.downs += 1;
			k.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_l) {
			l.downs += 1;
			l.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SEMICOLON) {
			semi.downs += 1;
			semi.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			a.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			s.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			d.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_f) {
			f.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_j) {
			j.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_k) {
			k.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_l) {
			l.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SEMICOLON) {
			semi.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} 
	// Lock camera for now
	// else if (evt.type == SDL_MOUSEMOTION) {
	// 	if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
	// 		glm::vec2 motion = glm::vec2(
	// 			evt.motion.xrel / float(window_size.y),
	// 			-evt.motion.yrel / float(window_size.y)
	// 		);
	// 		camera->transform->rotation = glm::normalize(
	// 			camera->transform->rotation
	// 			* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
	// 			* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
	// 		);
	// 		return true;
	// 	}
	// }

	return false;
}

void PlayMode::update(float elapsed) {

	//slowly rotates through [0,1):
	wobble += elapsed / 10.0f;
	wobble -= std::floor(wobble);

	hip->rotation = hip_base_rotation * glm::angleAxis(
		glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
		glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
		glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	//move sound to follow leg tip position:
	leg_tip_loop->set_position(get_leg_tip_position(), 1.0f / 60.0f);

	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	//reset button press counters:
	a.downs = 0;
	s.downs = 0;
	d.downs = 0;
	f.downs = 0;
	j.downs = 0;
	k.downs = 0;
	l.downs = 0;
	semi.downs = 0;
}

void PlayMode::render_text(std::string text, float x, float y, float scale, glm::vec3 color) {
    // activate corresponding render state	
    shader.use();
    glUniform3f(glGetUniformLocation(shader.ID, "textColor"), color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);

    // iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) 
    {
        Character ch = Characters[*c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;
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
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // be sure to use glBufferSubData and not glBufferData

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
        x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);
	if (dialogue_index < dialogue.size()){
		render_text(dialogue[dialogue_index][0], (windowW-300.f)/2,150.f, .5f, glm::vec3(0, 0.8f, 0.2f));
		render_text(dialogue[dialogue_index][2] + "(x)", windowW-300.f,100.f, .5f, glm::vec3(0.f, 0.8f, 0.2f));
		render_text(dialogue[dialogue_index][1] + "(z)", 50.f, 100.f, .5f, glm::vec3(0.0, 0.8f, 0.2f));

	}
	
	GL_ERRORS();
}

glm::vec3 PlayMode::get_leg_tip_position() {
	//the vertex position here was read from the model in blender:
	return lower_leg->make_local_to_world() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
}
