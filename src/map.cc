#include "map.h"

#include <algorithm>
#include <climits>
#include <iostream>
#include <memory>
#include <stack>

#include "player.h"

/*
 * Sector
 */
Sector::Sector(double floorHeight, double ceilingHeight, std::shared_ptr<Texture> floorTexture, std::shared_ptr<Texture> ceilingTexture, double lightLevel):
	ceilingHeight {ceilingHeight},
	floorHeight {floorHeight},
	floorTexture {floorTexture},
	ceilingTexture {ceilingTexture},
	lightLevel {lightLevel},
	timer {0},
	open {false},
	close {false},
	isSky {ceilingTexture->name == "F_SKY1"}
{
}

void Sector::Update() {
	if (timer > 0) {
		timer--;
		return;
	}
	if (open && ceilingHeight < floorHeight + 64) {
		if (++ceilingHeight == floorHeight + 64) {
			timer = 4 * 60;
			open = false;
			close = true;
		}
	}
	if (close && ceilingHeight > floorHeight) {
		if (--ceilingHeight == floorHeight) {
			close = false;
		}
	}
}

void Sector::Trigger() {
	if (!open) {
		open = true;
		close = false;
	}
}

/*
 * Map
 */
Map::Map(WAD& wad, const std::string& name) {
	auto lumps = wad.GetMapLumps(name);

	// Structural data
	std::vector<Vertex> vertices;
	auto verticesIterator = lumps.find("VERTEXES");
	if (verticesIterator == lumps.end()) {
		std::cerr << "Error: no VERTEXES lump" << std::endl;
		exit(1);
	}
	auto verticesLump = verticesIterator->second.get();
	wad.seek(verticesLump->location);
	for (uint32_t i = 0; i < verticesLump->size; i += 4) {
		const auto x = wad.read<int16_t>();
		const auto y = wad.read<int16_t>();

		vertices.push_back(Vertex {
			(double) x,
			(double) y
		});
	}

	auto sectorsIterator = lumps.find("SECTORS");
	if (sectorsIterator == lumps.end()) {
		std::cerr << "Error: no SECTORS lump" << std::endl;
		exit(1);
	}
	auto sectorsLump = sectorsIterator->second.get();
	wad.seek(sectorsLump->location);
	for (uint32_t i = 0; i < sectorsLump->size; i += 26) {
		const auto floorHeight = wad.read<int16_t>();
		const auto ceilingHeight = wad.read<int16_t>();
		const auto floorTexture = wad.readString(8);
		const auto ceilingTexture = wad.readString(8);
		const auto lightLevel = wad.read<int16_t>();
		const auto type = wad.read<int16_t>();
		const auto tag = wad.read<int16_t>();

		std::ignore = type;
		std::ignore = tag;

		sectors.push_back(Sector {
			(double) floorHeight,
			(double) ceilingHeight,
			wad.GetFlat(floorTexture),
			wad.GetFlat(ceilingTexture),
			(double) lightLevel / 255.0,
		});
	}

	auto sidedefsIterator = lumps.find("SIDEDEFS");
	if (sidedefsIterator == lumps.end()) {
		std::cerr << "Error: no SIDEDEFS lump" << std::endl;
		exit(1);
	}
	auto sidedefsLump = sidedefsIterator->second.get();
	wad.seek(sidedefsLump->location);
	for (uint32_t i = 0; i < sidedefsLump->size; i += 30) {
		const auto xOffset = wad.read<int16_t>();
		const auto yOffset = wad.read<int16_t>();
		const auto upperTexture = wad.readString(8);
		const auto lowerTexture = wad.readString(8);
		const auto middleTexture = wad.readString(8);
		const auto sector = wad.read<int16_t>();

		sides.push_back({
			xOffset,
			yOffset,
			wad.GetTexture(upperTexture),
			wad.GetTexture(lowerTexture),
			wad.GetTexture(middleTexture),
			&sectors[sector]
		});
	}

	auto linedefsIterator = lumps.find("LINEDEFS");
	if (linedefsIterator == lumps.end()) {
		std::cerr << "Error: no LINEDEFS lump" << std::endl;
		exit(1);
	}
	auto linedefsLump = linedefsIterator->second.get();
	wad.seek(linedefsLump->location);
	for (uint32_t i = 0; i < linedefsLump->size; i += 14) {
		const auto startVertex = wad.read<int16_t>();
		const auto endVertex = wad.read<int16_t>();
		const auto flags = wad.read<int16_t>();
		const auto specialType = wad.read<int16_t>();
		const auto sectorTag = wad.read<int16_t>();
		const auto rightSidedef = wad.read<int16_t>();
		const auto leftSidedef = wad.read<int16_t>();

		std::ignore = sectorTag;

		walls.push_back({
			vertices[startVertex],
			vertices[endVertex],
			flags,
			specialType,
			rightSidedef == -1 ? nullptr : &sides[rightSidedef],
			leftSidedef == -1 ? nullptr : &sides[leftSidedef],
		});
	}

	// Binary-space partition data
	auto segsIterator = lumps.find("SEGS");
	if (segsIterator == lumps.end()) {
		std::cerr << "Error: no SEGS lump" << std::endl;
		exit(1);
	}
	auto segsLump = segsIterator->second.get();
	wad.seek(segsLump->location);
	for (uint32_t i = 0; i < segsLump->size; i += 12) {
		const auto startVertex = wad.read<int16_t>();
		const auto endVertex = wad.read<int16_t>();
		const auto angle = wad.read<int16_t>();
		const auto linedef = wad.read<int16_t>();
		const auto direction = wad.read<int16_t>();
		const auto xOffset = wad.read<int16_t>();

		std::ignore = angle;

		segs.push_back(std::make_shared<Segment>(Segment {
			vertices[startVertex],
			vertices[endVertex],
			walls[linedef],
			direction == 1,
			xOffset,
		}));
	}

	auto subsectorsIterator = lumps.find("SSECTORS");
	if (subsectorsIterator == lumps.end()) {
		std::cerr << "Error: no SSECTORS lump" << std::endl;
		exit(1);
	}
	auto subsectorsLump = subsectorsIterator->second.get();
	wad.seek(subsectorsLump->location);
	for (uint32_t i = 0; i < subsectorsLump->size; i += 4) {
		const auto segCount = wad.read<int16_t>();
		const auto firstSeg = wad.read<int16_t>();

		std::vector<std::shared_ptr<Segment>> segments;
		for (int16_t i = 0; i < segCount; i++)
			segments.push_back(segs[firstSeg + i]);
		subsectors.push_back(std::make_shared<SubSector>(SubSector {
			segments
		}));
	}

	auto nodesIterator = lumps.find("NODES");
	if (nodesIterator == lumps.end()) {
		std::cerr << "Error: no NODES lump" << std::endl;
		exit(1);
	}
	auto nodesLump = nodesIterator->second.get();
	wad.seek(nodesLump->location);
	for (uint32_t i = 0; i < nodesLump->size; i += 28) {
		int16_t x = wad.read<int16_t>();
		int16_t y = wad.read<int16_t>();
		int16_t dx = wad.read<int16_t>();
		int16_t dy = wad.read<int16_t>();
		for (int i = 0; i < 8; i++)
			wad.read<int16_t>(); // bounding box
		int16_t rightChild = wad.read<int16_t>();
		int16_t leftChild = wad.read<int16_t>();
		nodes.push_back({
			(double) x, (double) y,
			(double) dx, (double) dy,
			rightChild,
			leftChild,
		});
	}

	// Entity data
	auto thingsIterator = lumps.find("THINGS");
	if (thingsIterator == lumps.end()) {
		std::cerr << "Error: no THINGS lump" << std::endl;
		exit(1);
	}
	auto thingsLump = thingsIterator->second.get();
	wad.seek(thingsLump->location);
	for (uint32_t i = 0; i < thingsLump->size; i += 10) {
		int16_t x = wad.read<int16_t>();
		int16_t y = wad.read<int16_t>();
		int16_t angle = wad.read<int16_t>();
		int16_t type = wad.read<int16_t>();
		wad.read<int16_t>(); // flags
		things.push_back({
			(double) x, (double) y,
			M_PI * (double) (angle + 0x4000) / (double) 0x8000,
			(Thing::Type) type,
		});
	}
}

void Map::Update() {
	for (auto& sector : sectors) {
		sector.Update();
	}
}

bool Map::IsInFrontOf(const Player& player, const Node& node) {
	const auto dx = player.x - node.x;
	const auto dy = player.y - node.y;
	return ((dx * node.dy) - (dy * node.dx)) >= 0;
}

std::vector<std::shared_ptr<Segment>> Map::GetOrderedSegments(const Player& player) const {
	std::vector<std::shared_ptr<Segment>> segments;
	std::stack<int> queue;
	queue.push(nodes.size() - 1);
	while (!queue.empty()) {
		int node = queue.top();
		queue.pop();
		if (node & 0x8000) {
			for (const auto& segment : subsectors[node & 0x7fff]->segments) {
				segments.push_back(segment);
			}
		} else if (IsInFrontOf(player, nodes[node])) {
			queue.push(nodes[node].leftChild);
			queue.push(nodes[node].rightChild);
		} else {
			queue.push(nodes[node].rightChild);
			queue.push(nodes[node].leftChild);
		}
	}
	return segments;
}