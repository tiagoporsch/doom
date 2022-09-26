#include "player.h"

#include <cmath>
#include <iostream>

Player::Player(Map& map): Location {0, 0, 0}, map {map} {
	for (const auto& t : map.things) {
		if (t.type == Thing::Type::PLAYER_1_START) {
			x = t.x;
			y = t.y;
			angle = t.angle;
			break;
		}
	}
}

double Dot(double ax, double ay, double bx, double by) {
	return ax * bx + ay * by;
}

double DistToLinedef(double cx, double cy, const Segment& linedef) {
	const auto a = linedef.s;
	const auto b = linedef.e;
	const auto acx = cx - a.x;
	const auto acy = cy - a.y;
	const auto abx = b.x - a.x;
	const auto aby = b.y - a.y;
	const auto dx = a.x + (abx * Dot(acx, acy, abx, aby) / Dot(abx, aby, abx, aby));
	const auto dy = a.y + (aby * Dot(acx, acy, abx, aby) / Dot(abx, aby, abx, aby));
	const auto adx = dx - a.x;
	const auto ady = dy - a.y;
	const auto k = abs(abx) > abs(aby) ? adx / abx : ady / aby;
	if (k <= 0.0)
		return Dot(cx - a.x, cy - a.y, cx - a.x, cy - a.y);
	else if (k >= 1.0)
		return Dot(cx - b.x, cy - b.y, cx - b.x, cy - b.y);
	else
		return Dot(cx - dx, cy - dy, cx - dx, cy - dy);
}

void Player::Update() {
	auto currentSubsector = GetCurrentSubsector();
	auto currentSector = currentSubsector->segments.front()->frontSide->sector;

	if (turnLeft) {
		if ((angle += turnSpeed * M_PI) > M_PI)
			angle -= 2 * M_PI;
	}
	if (turnRight) {
		if ((angle -= turnSpeed * M_PI) < M_PI)
			angle += 2 * M_PI;
	}

	double vx = 0, vy = 0;
	double s = std::sin(angle), c = std::cos(angle);
	if (forward) {
		vx += walkSpeed * c;
		vy += walkSpeed * s;
	}
	if (backward) {
		vx -= walkSpeed * c;
		vy -= walkSpeed * s;
	}
	if (strafeLeft) {
		vx -= walkSpeed * s;
		vy += walkSpeed * c;
	}
	if (strafeRight) {
		vx += walkSpeed * s;
		vy -= walkSpeed * c;
	}

	for (const auto& linedef : map.GetOrderedSegments(*this)) {
		if (DistToLinedef(x + vx, y + vy, *linedef) < 8 * 8) {
			double heightChange = 0;
			double targetHeight = 0;
			if (linedef->twoSided) {
				if (linedef->frontSide->sector == currentSector) {
					heightChange = linedef->backSide->sector->floorHeight - currentSector->floorHeight;
					targetHeight = linedef->backSide->sector->ceilingHeight - linedef->backSide->sector->floorHeight;
				} else {
					heightChange = linedef->frontSide->sector->floorHeight - currentSector->floorHeight;
					targetHeight = linedef->frontSide->sector->ceilingHeight - linedef->frontSide->sector->floorHeight;
				}
			}
			if (!linedef->twoSided || heightChange > 24 || targetHeight < 56) {
				if (linedef->type >= 1 && linedef->type <= 4 && linedef->twoSided)
					linedef->backSide->sector->Trigger();
				double dx = linedef->e.x - linedef->s.x;
				double dy = linedef->e.y - linedef->s.y;
				double det = (vx * dx + dy * vy) / (dx * dx + dy * dy);
				vx = dx * det;
				vy = dy * det;
			}
		}
	}

	x += vx;
	y += vy;
	z = currentSector->floorHeight + 41;
}

std::shared_ptr<SubSector> Player::GetCurrentSubsector() const {
	int node = map.nodes.size() - 1;
	while ((node & 0x8000) == 0)
		node = Map::IsInFrontOf(*this, map.nodes[node]) ? map.nodes[node].rightChild : map.nodes[node].leftChild;
	return map.subsectors[node & 0x7fff];
};