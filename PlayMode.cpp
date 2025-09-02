#include "PlayMode.hpp"

// for the GL_ERRORS() macro:
#include "gl_errors.hpp"

// for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>

#include "load_save_png.hpp"
#include <set>
#include <map>


bool contains_color(PPU466::Palette const &palette, glm::u8vec4 const &color, int &index)
{
	for (int i = 0; i < 4; i++)
	{
		if (palette[i] == color)
		{
			index = i;
			return true;
		}
	}
	return false;
}

bool contains_color(PPU466::Palette const &palette, glm::u8vec4 const &color)
{
	for (int i = 0; i < 4; i++)
	{
		if (palette[i] == color)
		{

			return true;
		}
	}
	return false;
}

bool operator<(PPU466::Tile const &a, PPU466::Tile const &b)
{
	if (a.bit1 < b.bit1)
		return true;
	if (a.bit1 > b.bit1)
		return false;
	return a.bit0 < b.bit0;
}

bool is_palette_subset(PPU466::Palette const &a, PPU466::Palette const &b)
{
	return contains_color(b, a[0]) && contains_color(b, a[1]) && contains_color(b, a[2]) && contains_color(b, a[3]);
}

// Required implementation to add Palette in std::map
// Source: ChatGPT
struct PaletteLess
{
	bool operator()(PPU466::Palette const &a, PPU466::Palette const &b) const
	{
		for (int i = 0; i < 4; ++i)
		{
			glm::u8vec4 A = a[i];
			glm::u8vec4 B = b[i];
			if (A.r != B.r)
				return A.r < B.r;
			if (A.g != B.g)
				return A.g < B.g;
			if (A.b != B.b)
				return A.b < B.b;
			if (A.a != B.a)
				return A.a < B.a;
		}
		return false;
	}
};

// Required implementation to add glm::u8vec4 in std::set
// Source: ChatGPT
struct ColorLess
{
	bool operator()(glm::u8vec4 const &a, glm::u8vec4 const &b) const
	{
		if (a.r != b.r)
			return a.r < b.r;
		if (a.g != b.g)
			return a.g < b.g;
		if (a.b != b.b)
			return a.b < b.b;
		return a.a < b.a;
	}
};

// Helper to compute set union
// Source: ChatGPT
template <class T, class Comp>
std::set<T, Comp> set_union_s(std::set<T, Comp> const &a, std::set<T, Comp> const &b)
{
	std::set<T, Comp> c;
	std::set_union(a.begin(), a.end(),
				   b.begin(), b.end(),
				   std::inserter(c, c.end()),
				   Comp{});
	return c;
}

// Helper to compute set intersection
// Source: ChatGPT
template <class T, class Comp>
std::set<T, Comp> set_intersection_s(std::set<T, Comp> const &a, std::set<T, Comp> const &b)
{
	std::set<T, Comp> c;
	std::set_intersection(a.begin(), a.end(),
						  b.begin(), b.end(),
						  std::inserter(c, c.end()),
						  Comp{});
	return c;
}

bool overlaps(float x1, float y1, float x2, float y2)
{
	return !(x1 + 4 <= x2 || x2 + 4 <= x1 || y1 + 4 <= y2 || y2 + 4 <= y1);
}

bool PlayMode::in_trap(int x1, int y1)
{
	int tile_x = (x1 - ppu.background_position.x) / 8;
	int tile_y = (y1 - ppu.background_position.y) / 8;

	return background_info[tile_x + tile_y * PPU466::BackgroundWidth] == 7;
}

void PlayMode::load_background(std::string file_name)
{
	glm::uvec2 bg_size;
	std::vector<glm::u8vec4> bg_data;

	load_png(file_name, &bg_size, &bg_data, OriginLocation::LowerLeftOrigin);
	std::vector<glm::u8vec4> bg_data_tile(bg_data.size());

	int tiles_per_row = bg_size.x / 8;

	for (int j = 0; j < bg_size.y; j++)
	{
		int tile_y = j / 8;
		for (int i = 0; i < bg_size.x; i++)
		{
			int tile_x = i / 8;
			int index = (tile_x + tile_y * tiles_per_row) * 64 + (i % 8) + (j % 8) * 8;
			bg_data_tile[index] = bg_data[i + j * bg_size.x];
		}
	}

	int img_w = PPU466::BackgroundWidth / 2;
	int img_h = PPU466::BackgroundHeight / 2;
	std::array<uint16_t, (PPU466::BackgroundWidth / 2) * (PPU466::BackgroundHeight / 2)> img_input{};
	std::map<PPU466::Tile, int> tile_map;
	std::map<PPU466::Palette, int, PaletteLess> palette_map;
	int tile_index = 0;
	int palette_index = 0;
	for (int t = 0; t < bg_data.size() / 64; t++)
	{
		PPU466::Palette palette{};

		std::set<glm::u8vec4, ColorLess> unique_palette;
		for (int j = 0; j < 8; j++)
		{
			for (int i = 0; i < 8; i++)
			{

				glm::u8vec4 color = bg_data_tile[i + j * 8 + t * 64];
				unique_palette.insert(color);
			}
		}
		int color_idx = 0;
		for (auto c : unique_palette)
		{
			palette[color_idx++] = c;
		}
		while (color_idx < 4)
		{
			palette[color_idx++] = glm::u8vec4(0, 0, 0, 0);
		}

		// reduce the redundant palette
		for (auto p : palette_map)
		{
			if (is_palette_subset(p.first, palette))
			{
				int value = p.second;
				palette_map.erase(p.first);
				palette_map[palette] = value;
			}
			else if (is_palette_subset(palette, p.first))
			{
				palette = p.first;
			}
			else
			{
				std::set<glm::u8vec4, ColorLess> unique_palette1;
				for (int k = 0; k < 4; k++)
				{
					unique_palette1.insert(p.first[k]);
				}
				auto I = set_intersection_s<glm::u8vec4, ColorLess>(unique_palette, unique_palette1);
				if (I.size() >= 0)
				{
					auto U = set_union_s<glm::u8vec4, ColorLess>(unique_palette, unique_palette1);
					if (U.size() <= 4)
					{
						int color_idx2 = 0;
						for (auto c : U)
						{
							palette[color_idx2++] = c;
						}
						while (color_idx2 < 4)
						{
							palette[color_idx2++] = glm::u8vec4(0, 0, 0, 0);
						}
						int value = p.second;
						palette_map.erase(p.first);
						palette_map[palette] = value;
					}
				}
			}
		}

		if (palette_map.find(palette) == palette_map.end())
		{
			palette_map[palette] = palette_index++;
		}

		ppu.palette_table[palette_map[palette]] = palette;
	}

	for (int t = 0; t < bg_data.size() / 64; t++)
	{
		PPU466::Tile tile{};
		PPU466::Palette palette{};
		int color_get_index = 0;

		std::set<glm::u8vec4, ColorLess> unique_palette;
		for (int j = 0; j < 8; j++)
		{
			for (int i = 0; i < 8; i++)
			{

				glm::u8vec4 color = bg_data_tile[i + j * 8 + t * 64];
				unique_palette.insert(color);
			}
		}
		int color_idx = 0;
		for (auto c : unique_palette)
		{
			palette[color_idx++] = c;
		}
		while (color_idx < 4)
		{
			palette[color_idx++] = glm::u8vec4(0, 0, 0, 0);
		}

		for (int j = 0; j < 8; j++)
		{
			for (int i = 0; i < 8; i++)
			{

				glm::u8vec4 color = bg_data_tile[i + j * 8 + t * 64];
				for (auto p : palette_map)
				{
					if (is_palette_subset(palette, p.first) && contains_color(p.first, color, color_get_index))
					{
						palette = p.first;
						break;
					}
				}

				uint8_t bit0 = color_get_index & 1;
				uint8_t bit1 = (color_get_index >> 1) & 1;
				tile.bit0[j] |= bit0 << i;
				tile.bit1[j] |= bit1 << i;
			}
		}

		if (tile_map.find(tile) == tile_map.end())
		{
			tile_map[tile] = tile_index++;
		}

		ppu.tile_table[tile_map[tile]] = tile;

		int x = t % img_w;
		int y = t / img_w;
		img_input[x + y * img_w] = uint16_t(tile_map[tile] & 0xFF) | uint16_t((palette_map[palette] & 0x7) << 8);
	}

	for (int j = 0; j < PPU466::BackgroundHeight; j++)
	{
		for (int i = 0; i < PPU466::BackgroundWidth; i++)
		{
			int x = i % img_w;
			int y = j % img_h;
			ppu.background[i + j * PPU466::BackgroundWidth] = img_input[x + y * img_w];
		}
	}
}

void PlayMode::change_background(int len_x, int len_y, int pos_x, int pos_y, int tile_index, int palette_index)
{
	for (int i = 0; i < len_x; i++)
	{
		for (int j = 0; j < len_y; j++)
		{
			int index = pos_x + pos_y * PPU466::BackgroundWidth + i + j * PPU466::BackgroundWidth;

			uint16_t tile = tile_index;
			uint16_t new_palette = palette_index;
			ppu.background[index] = tile | (new_palette << 8);
			background_info[index] = palette_index;
		}
	}
}

void PlayMode::load_sprite(std::string file_name, int tile_index, int palette_index)
{
	glm::uvec2 bg_size;
	std::vector<glm::u8vec4> bg_data;

	load_png(file_name, &bg_size, &bg_data, OriginLocation::LowerLeftOrigin);

	PPU466::Tile tile{};
	PPU466::Palette palette{};
	int color_get_index = 0;

	std::set<glm::u8vec4, ColorLess> unique_palette;
	for (int j = 0; j < 8; j++)
	{
		for (int i = 0; i < 8; i++)
		{

			glm::u8vec4 color = bg_data[i + j * 8];
			unique_palette.insert(color);
		}
	}
	int color_idx = 0;
	for (auto c : unique_palette)
	{
		palette[color_idx++] = c;
	}
	while (color_idx < 4)
	{
		palette[color_idx++] = glm::u8vec4(0, 0, 0, 0);
	}

	for (int j = 0; j < 8; j++)
	{
		for (int i = 0; i < 8; i++)
		{

			glm::u8vec4 color = bg_data[i + j * 8];

			contains_color(palette, color, color_get_index);

			uint8_t bit0 = color_get_index & 1;
			uint8_t bit1 = (color_get_index >> 1) & 1;
			tile.bit0[j] |= bit0 << i;
			tile.bit1[j] |= bit1 << i;
		}
	}

	ppu.palette_table[palette_index] = palette;
	ppu.tile_table[tile_index] = tile;
}

PlayMode::PlayMode()
{
	// TODO:
	//  you *must* use an asset pipeline of some sort to generate tiles.
	//  don't hardcode them like this!
	//  or, at least, if you do hardcode them like this,
	//   make yourself a script that spits out the code that you paste in here
	//    and check that script into your repository.
	load_background("assets/background.png");
	load_sprite("assets/player.png", 255, 5);
	load_sprite("assets/enemy.png", 254, 4);
	load_sprite("assets/target.png", 253, 4);
	load_sprite("assets/trap1.png", 252, 6);
	load_sprite("assets/trap2.png", 252, 7);

	for (int i = 1; i < 13; i++)
	{
		ppu.sprites[i].x = 96 + 2 * i;
		ppu.sprites[i].y = 96 + 2 * i;
		ppu.sprites[i].index = 254;
		ppu.sprites[i].attributes = 4;
	}

	for (int i = 13; i < 19; i++)
	{
		ppu.sprites[i].x = 96 + 2 * i;
		ppu.sprites[i].y = 96 + 2 * i;
		ppu.sprites[i].index = 253;
		ppu.sprites[i].attributes = 4;
	}

	for (int i = 19; i < 64; i++)
	{
		ppu.sprites[i].x = 96 + 2 * i;
		ppu.sprites[i].y = 96 + 2 * i;
		ppu.sprites[i].index = 0;
		ppu.sprites[i].attributes = 0;

		ppu.sprites[i].attributes |= 0x80; //'behind' bit
	}

	for (int i = 1; i < 64; i++)
	{
		float a = ang(rng);
		float s = spd(rng);
		glm::vec2 vel = {std::cos(a) * s, std::sin(a) * s};

		velocity[i] = vel;

		enemy_pos[i] = glm::vec2(
			96 + 2 * i,
			96 + 2 * i);
	}
}

PlayMode::~PlayMode()
{
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size)
{

	if (evt.type == SDL_EVENT_KEY_DOWN)
	{
		if (evt.key.key == SDLK_LEFT)
		{
			left.downs += 1;
			left.pressed = true;
			return true;
		}
		else if (evt.key.key == SDLK_RIGHT)
		{
			right.downs += 1;
			right.pressed = true;
			return true;
		}
		else if (evt.key.key == SDLK_UP)
		{
			up.downs += 1;
			up.pressed = true;
			return true;
		}
		else if (evt.key.key == SDLK_DOWN)
		{
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	}
	else if (evt.type == SDL_EVENT_KEY_UP)
	{
		if (evt.key.key == SDLK_LEFT)
		{
			left.pressed = false;
			return true;
		}
		else if (evt.key.key == SDLK_RIGHT)
		{
			right.pressed = false;
			return true;
		}
		else if (evt.key.key == SDLK_UP)
		{
			up.pressed = false;
			return true;
		}
		else if (evt.key.key == SDLK_DOWN)
		{
			down.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed)
{

	// slowly rotates through [0,1):
	//  (will be used to set background color)
	background_fade += elapsed / 10.0f;
	background_fade -= std::floor(background_fade);

	constexpr float PlayerSpeed = 30.0f;

	if (game_over && !game_over_loaded)
	{
		load_background("assets/lose.png");
		game_over_loaded = true;
		return;
	}
	if (game_over)
		return;

	if (game_win && !game_win_loaded)
	{
		load_background("assets/win.png");
		game_win_loaded = true;
		return;
	}
	if (game_win)
		return;

	if (left.pressed)
		player_at.x -= PlayerSpeed * elapsed;
	if (right.pressed)
		player_at.x += PlayerSpeed * elapsed;
	if (down.pressed)
		player_at.y -= PlayerSpeed * elapsed;
	if (up.pressed)
		player_at.y += PlayerSpeed * elapsed;

	// reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;

	for (int i = 1; i < 63; i++)
	{
		enemy_pos[i] += velocity[i] * elapsed;
	}
	float prob = trap_prob(rng);
	elasped_time += elapsed;
	if (elasped_time > trap_change_time && trap_in_effect)
	{
		change_background(prev_len_x, prev_len_y, prev_pos_x, prev_pos_y, 252, 7);

		trap_in_effect = false;
	}
	if (prob > 0.9f && !trap_in_effect)
	{
		int len_x = trap_length(rng);
		int len_y = trap_length(rng);
		int pos_x = trap_pos_x(rng);
		int pos_y = trap_pos_y(rng);
		change_background(len_x, len_y, pos_x, pos_y, 252, 6);

		prev_len_x = len_x;
		prev_len_y = len_y;
		prev_pos_x = pos_x;
		prev_pos_y = pos_y;
		elasped_time = 0.0f;
		trap_in_effect = true;
	}

	for (int i = 1; i < 64; i++)
	{
		if (((ppu.sprites[i].attributes >> 7) == 0) && overlaps(ppu.sprites[0].x, ppu.sprites[0].y, ppu.sprites[i].x, ppu.sprites[i].y))
		{
			if (ppu.sprites[i].index == 254)
				game_over = true;
			else if (ppu.sprites[i].index == 253)
			{
				ppu.sprites[i].attributes |= 128;
				caught_current_num++;
			}
		}
	}

	if (in_trap(ppu.sprites[0].x, ppu.sprites[0].y))
		game_over = true;

	if (caught_current_num == caught_max_num)
		game_win = true;
}

void PlayMode::draw(glm::uvec2 const &drawable_size)
{
	//--- set ppu state based on game state ---

	// background color will be some hsv-like fade:
	ppu.background_color = glm::u8vec4(
		std::min(255, std::max(0, int32_t(255 * 0.5f * (0.5f + std::sin(2.0f * M_PI * (background_fade + 0.0f / 3.0f)))))),
		std::min(255, std::max(0, int32_t(255 * 0.5f * (0.5f + std::sin(2.0f * M_PI * (background_fade + 1.0f / 3.0f)))))),
		std::min(255, std::max(0, int32_t(255 * 0.5f * (0.5f + std::sin(2.0f * M_PI * (background_fade + 2.0f / 3.0f)))))),
		0xff);

	// background scroll:
	//  ppu.background_position.x = int32_t(-0.5f * player_at.x);
	//  ppu.background_position.y = int32_t(-0.5f * player_at.y);

	// player sprite:
	ppu.sprites[0].x = int8_t(player_at.x);
	ppu.sprites[0].y = int8_t(player_at.y);
	ppu.sprites[0].index = 255;
	ppu.sprites[0].attributes = 5;

	for (int i = 1; i < 19; i++)
	{
		ppu.sprites[i].x = std::ceil(enemy_pos[i].x);
		ppu.sprites[i].y = std::ceil(enemy_pos[i].y);
	}

	//--- actually draw ---
	ppu.draw(drawable_size);
}
