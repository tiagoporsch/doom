#pragma once

#include <SDL2/SDL.h>
#include <iostream>

#include "wad.h"

struct Player;

// Generic
struct Point {
	double x;
	double y;

	constexpr Point(double x, double y): x {x}, y {y} {}
};

struct Line {
	Point s;
	Point e;

	constexpr Line(Point s, Point e): s {s}, e {e} {}
};

struct Location : Point {
	double z;

	constexpr Location(double x, double y, double z): Point {x, y}, z {z} {}
};

// Map structures
struct Vertex : Point {
	constexpr Vertex(double x, double y): Point {x, y} {}
};

struct Sector {
	double ceilingHeight;
	double floorHeight;
	std::shared_ptr<Texture> floorTexture;
	std::shared_ptr<Texture> ceilingTexture;
	double lightLevel;

	int timer;
	bool open;
	bool close;
	const bool isSky;

	Sector(double, double, std::shared_ptr<Texture>, std::shared_ptr<Texture>, double);
	void Update();
	void Trigger();
};

struct Side {
	int xOffset;
	int yOffset;
	std::shared_ptr<Texture> upperTexture;
	std::shared_ptr<Texture> lowerTexture;
	std::shared_ptr<Texture> middleTexture;
	Sector* sector;

	Side(int xOffset, int yOffset, std::shared_ptr<Texture> upperTexture, std::shared_ptr<Texture> lowerTexture, std::shared_ptr<Texture> middleTexture, Sector* sector):
	xOffset {xOffset}, yOffset {yOffset}, upperTexture {upperTexture}, lowerTexture {lowerTexture}, middleTexture {middleTexture}, sector {sector} {}
};

struct Wall : Line {
	int type;
	int flags;
	Side* frontSide;
	Side* backSide;

	bool twoSided = backSide != nullptr;

	Wall(Vertex s, Vertex e, int type, int flags, Side* frontSide, Side* backSide):
	Line {s, e}, type {type}, flags {flags}, frontSide {frontSide}, backSide {backSide} {}
};

struct Segment : Wall {
	int xOffset;

	bool upperUnpegged = (flags & 0x0008) != 0;
	bool lowerUnpegged = (flags & 0x0010) != 0;
	bool isHorizontal = s.y == e.y;
	bool isVertical = s.x == e.x;

	Segment(Vertex s, Vertex e, Wall wall, bool opposite, int xOffset):
	Wall {s, e, wall.type, wall.flags, opposite ? wall.backSide : wall.frontSide, opposite ? wall.frontSide : wall.backSide}, xOffset {xOffset} {}
};

struct SubSector {
	const std::vector<std::shared_ptr<Segment>> segments;
};

struct Node {
	const double x, y;
	const double dx, dy;
	const int rightChild;
	const int leftChild;
};

struct Thing {
	enum class Type {
		PLAYER_1_START = 1,
	};
	const double x, y;
	const double angle;
	const Type type;
};

// Map
struct Map {
	// Structural data
	std::vector<Wall> walls;
	std::vector<Side> sides;
	std::vector<Sector> sectors;

	// Binary-space partition data
	std::vector<std::shared_ptr<Segment>> segs;
	std::vector<std::shared_ptr<SubSector>> subsectors;
	std::vector<Node> nodes;

	// Entity data
	std::vector<Thing> things;

	Map(WAD&, const std::string&);
	void Update();

	static bool IsInFrontOf(const Player&, const Node&);
	std::vector<std::shared_ptr<Segment>> GetOrderedSegments(const Player&) const;
};