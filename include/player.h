#pragma once

#include "wad.h"
#include "map.h"

struct Player : Location {
	double angle;

	const double turnSpeed = 0.02;
	const double walkSpeed = 8.0;
	bool forward = false;
	bool backward = false;
	bool strafeLeft = false;
	bool strafeRight = false;
	bool turnLeft = false;
	bool turnRight = false;

	Map& map;

	Player(Map&);

	void Respawn();
	void Update();

private:
	std::shared_ptr<SubSector> GetCurrentSubsector() const;
};