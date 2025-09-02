#include "PPU466.hpp"
#include "Mode.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <random>

struct PlayMode : Mode
{
	PlayMode();
	virtual ~PlayMode();

	// functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	// input tracking:
	struct Button
	{
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	// some weird background animation:
	float background_fade = 0.0f;

	// player position:
	glm::vec2 player_at = glm::vec2(0.0f);

	//----- drawing handled by PPU466 -----

	PPU466 ppu;

	bool game_win = false;
	bool game_win_loaded = false;
	bool game_over = false;
	bool game_over_loaded = false;
	void load_background(std::string file_name);
	void change_background(int len_x, int len_y, int pos_x, int pos_y, int tile_index, int palette_index);
	void load_sprite(std::string file_name, int tile_index, int palette_index);
	bool in_trap(int x1, int y1);

	// Generate random numbers
	// Source: ChatGPT
	std::mt19937 rng{std::random_device{}()};
	std::uniform_real_distribution<float> ang{0.0f, 2.0f * float(M_PI)};
	std::uniform_real_distribution<float> spd{10.0f, 20.0f};
	std::uniform_int_distribution<int> trap_length{8, 15};
	std::uniform_int_distribution<int> trap_pos_x{0, 31};
	std::uniform_int_distribution<int> trap_pos_y{0, 29};
	std::uniform_real_distribution<float> trap_prob{0.0f, 1.0f};

	std::array<glm::vec2, 64> enemy_pos{};
	std::array<glm::vec2, 64> velocity;
	std::array<int, PPU466::BackgroundWidth * PPU466::BackgroundHeight> background_info;

	float elasped_time = 0.0f;
	float game_end_elasped = 0.0f;
	const int trap_change_time = 5;
	bool trap_in_effect = false;

	int prev_len_x;
	int prev_len_y;
	int prev_pos_x;
	int prev_pos_y;

	const int caught_max_num = 6;
	int caught_current_num = 0;
};
