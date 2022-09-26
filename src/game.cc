#include "game.h"

Game::Game(uint32_t* screen):
wad { "DOOM.WAD" },
map { wad, "E1M1" },
player { map },
renderer { wad, map, player, screen } {
}

void Game::Update() {
	map.Update();
	player.Update();
}

void Game::Render() {
	renderer.Render();
}

void Game::KeyPressed(SDL_KeyboardEvent& e) {
	switch (e.keysym.sym) {
		case SDLK_w: player.forward = true; break;
		case SDLK_a: player.strafeLeft = true; break;
		case SDLK_s: player.backward = true; break;
		case SDLK_d: player.strafeRight = true; break;
		case SDLK_LEFT: player.turnLeft = true; break;
		case SDLK_RIGHT: player.turnRight = true; break;
		default:
			break;
	}
}

void Game::KeyReleased(SDL_KeyboardEvent& e) {
	switch (e.keysym.sym) {
		case SDLK_w: player.forward = false; break;
		case SDLK_a: player.strafeLeft = false; break;
		case SDLK_s: player.backward = false; break;
		case SDLK_d: player.strafeRight = false; break;
		case SDLK_LEFT: player.turnLeft = false; break;
		case SDLK_RIGHT: player.turnRight = false; break;
		default:
			break;
	}
}

void Game::MouseMoved(SDL_MouseMotionEvent& e) {
	player.angle -= e.xrel * 0.003;
	if (player.angle < 0)
		player.angle += 2 * M_PI;
	if (player.angle >= 2 * M_PI)
		player.angle -= 2 * M_PI;
}