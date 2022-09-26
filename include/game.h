#pragma once

#include <SDL2/SDL.h>

#include "map.h"
#include "player.h"
#include "renderer.h"
#include "wad.h"

class Game {
	WAD wad;
	Map map;
	Player player;
	Renderer renderer;

public:
	Game(uint32_t*);

	void Update();
	void Render();

	void KeyPressed(SDL_KeyboardEvent&);
	void KeyReleased(SDL_KeyboardEvent&);
	void MouseMoved(SDL_MouseMotionEvent&);
};