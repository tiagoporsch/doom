#include <iostream>
#include <queue>
#include <SDL2/SDL.h>

#include "game.h"
#include "map.h"
#include "player.h"
#include "renderer.h"

static constexpr auto MS_PER_UPDATE = 1000 / 60;

auto main() -> int {
	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		std::cerr << "SDL_Init: " << SDL_GetError() << std::endl;
		return 1;
	}
	auto window = SDL_CreateWindow("DOOM", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, settings::WIDTH * settings::SCALE, settings::HEIGHT * settings::SCALE, 0);
	if (!window) {
		SDL_Quit();
		std::cerr << "SDL_CreateWindow: " << SDL_GetError() << std::endl;
		return 1;
	}
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_SetRelativeMouseMode(SDL_TRUE);
	auto windowSurface = SDL_GetWindowSurface(window);
	auto windowSurfaceRect = SDL_Rect { 0, 0, settings::WIDTH * settings::SCALE, settings::HEIGHT * settings::SCALE };
	auto screen = SDL_CreateRGBSurface(0, settings::WIDTH, settings::HEIGHT, 32, 0, 0, 0, 0);
	auto screenRect = SDL_Rect { 0, 0, settings::WIDTH, settings::HEIGHT };

	// Initialize game
	auto game = Game { reinterpret_cast<uint32_t*>(screen->pixels) };

	// Main loop
	auto quit = false;
	SDL_Event event;
	while (!quit) {
		Uint64 past = SDL_GetTicks64();
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_KEYDOWN:
				game.KeyPressed(event.key);
				if (event.key.keysym.sym == SDLK_ESCAPE)
					quit = true;
				break;
			case SDL_KEYUP:
				game.KeyReleased(event.key);
				break;
			case SDL_MOUSEMOTION:
				game.MouseMoved(event.motion);
				break;
			}
		}
		game.Update();
		SDL_LockSurface(screen);
		game.Render();
		SDL_UnlockSurface(screen);
		SDL_BlitScaled(screen, &screenRect, windowSurface, &windowSurfaceRect);
		SDL_UpdateWindowSurface(window);
		Uint64 now = SDL_GetTicks64();
		if (now > past + MS_PER_UPDATE)
			printf("Running %lu ms late.\n", now - past - MS_PER_UPDATE);
		else
			SDL_Delay(past + MS_PER_UPDATE - SDL_GetTicks64());
	}

	// Clean up
	SDL_DestroyWindow(window);
	SDL_Quit();
}