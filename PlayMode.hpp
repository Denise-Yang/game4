#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "shader.hpp"
#include <map>
#include <string>

enum Anim_Type {
	ANIM_DEFAULT,
	ANIM_TEST
};

struct Animation {
	bool is_playing = false;
	Anim_Type type;
	void play(); // update is_playing, spawn meshes, etc
	void update(float elapsed); // move meshes around, whatever
};

enum Battle_Phase {
	// PHASE_DEFAULT, // default value
	DECIDING, // players are deciding on moves
	ANIMATING, // move animations are playing
	REPORTING, // a report of the damages dealt are displayed
	OVER // a player has won
};

struct Player;

struct Move {
	float accuracy = 1.0f;
	std::string name = "placeholder move name";
	virtual int activate(Player *user, Player *target) {
		return 0;
	};

	Move(){};
	Move(std::string name_, float acc) : accuracy(acc), name(name_) {};
	virtual ~Move() = default;
	// animation that should be played?
	// sound effect that should be played?
};

struct Player {
	// Properties shared for all players.
	static constexpr unsigned int num_moves = 4;
	static constexpr int max_health_default = 100;

	// Methods
	Player(unsigned int health_);

	// Individual player info.
	std::vector<Move>moves;
	// Move *moves[num_moves];
	bool is_deciding = true;
	int move_selected = -1;
	bool is_winner = false;
	int max_health = 100;
	int cur_health = 100;
	int damage_dealt = 0;
	Animation active_animation;
};

struct Attack : Move {
	static constexpr float dmg_variance_range = 0.05f; // +/- 5% of the damage

	unsigned int base_damage = 10;
	float crit_chance = 0.1f;
	int activate(Player *user, Player *target) override;

	Attack(std::string name_, unsigned int base_dmg, float acc, 
			float crit_chance_) : base_damage(base_dmg), 
			crit_chance(crit_chance_) {
		Move(name_, acc);
	};
};

struct Heal : Move {
	float percent_heal = 0.5f;
	int activate(Player *user, Player *target) override;
};

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	void update_deciding(float elapsed);
	void update_animating(float elapsed);
	void update_reporting(float elapsed);
	void update_over(float elapsed);
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;
	virtual void load_dialogue(std::string filename);

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} a, s, d, f, j, k, l, semi, space;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	int dialogue_index = 0;
	int windowW;
	int windowH;

	/*Could create a character struct each with their own dialogue*/
	std::vector<std::vector<std::string>> dialogue;


	//hexapod leg to wobble:
	Scene::Transform *hip = nullptr;
	Scene::Transform *upper_leg = nullptr;
	Scene::Transform *lower_leg = nullptr;
	glm::quat hip_base_rotation;
	glm::quat upper_leg_base_rotation;
	glm::quat lower_leg_base_rotation;
	float wobble = 0.0f;

	glm::vec3 get_leg_tip_position();
	//x,y,scale
	
	//music coming from the tip of the leg (as a demonstration):
	std::shared_ptr< Sound::PlayingSample > leg_tip_loop;
	
	//camera:
	Scene::Camera *camera = nullptr;

	struct NPC {
		std::vector<std::vector<std::string>> dialogue;
		int dialogue_index = 0;
		glm::vec3 postion;

	};

	struct Character {
	    unsigned int TextureID; // ID handle of the glyph texture
		glm::ivec2   Size;      // Size of glyph
		glm::ivec2   Bearing;   // Offset from baseline to left/top of glyph
		unsigned int Advance;   // Horizontal offset to advance to next glyph
	};
	std::map<char, Character> Characters;

	glm::mat4 projection = glm::ortho(0.0f, 1280.0f, 0.0f, 720.0f);

	FT_Library ft;
	FT_Face face;
	unsigned int VAO, VBO;
	Shader shader;

	void render_text(std::string text, float x, float y, float scale, glm::vec3 color);

	// Battle Stuff
	Player player1 = Player(Player::max_health_default);
	Player player2 = Player(Player::max_health_default);
	Battle_Phase cur_phase;
};
