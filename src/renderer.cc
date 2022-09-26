#include "renderer.h"

#include <algorithm>
#include <climits>
#include <iostream>
#include <map>
#include <memory>
#include <utility>

Renderer::Renderer(WAD& wad, Map& map, Player& player, uint32_t* pixels): wad { wad }, map { map }, player { player }, pixels { pixels } {
	wad.seek(wad.GetLump("PLAYPAL")->location);
	for (auto i = 0; i < Palette::SIZE; i++) {
		palette.colors[i].r = wad.read<uint8_t>();
		palette.colors[i].g = wad.read<uint8_t>();
		palette.colors[i].b = wad.read<uint8_t>();
	}
}

void Renderer::Render() {
	horizontalOcclusion.clear();
	ceilingPlanes.clear();
	floorPlanes.clear();
	for (int x = 0; x < settings::WIDTH; x++) {
		ceilingClip[x] = 0;
		floorClip[x] = settings::HEIGHT;
	}

	for (const auto& segment : map.GetOrderedSegments(player))
		RenderSegment(*segment);
	for (const auto& plane : ceilingPlanes)
		RenderPlane(plane);
	for (const auto& plane : floorPlanes)
		RenderPlane(plane);
}

void Renderer::RenderSegment(const Segment& segment) {
	VisibleSegment vs { segment };

	vs.startAngle = NormalizeAngle(std::atan2(segment.s.y - player.y, segment.s.x - player.x) - player.angle);
	vs.endAngle = NormalizeAngle(std::atan2(segment.e.y - player.y, segment.e.x - player.x) - player.angle);
	if (std::abs(vs.endAngle - vs.startAngle) > M_PI) {
		if (vs.startAngle < -M_PI_2)
			vs.startAngle = M_PI_2;
		else if (vs.startAngle > M_PI_2)
			vs.startAngle = -M_PI_2;
		else if (vs.endAngle < -M_PI_2)
			vs.endAngle = M_PI_2;
		else if (vs.endAngle > M_PI_2)
			vs.endAngle = -M_PI_2;
	}
	if ((vs.startAngle < -M_PI_4 && vs.endAngle < -M_PI_4) || (vs.endAngle > M_PI_4 && vs.startAngle > M_PI_4))
		return;

	vs.startX = ViewX(vs.startAngle);
	vs.endX = ViewX(vs.endAngle);
	if (vs.startX > vs.endX) {
		std::swap(vs.startX, vs.endX);
		std::swap(vs.startAngle, vs.endAngle);
	}

	const auto visibleSpans = ClipHorizontal(vs.startX, vs.endX, !segment.twoSided);
	if (visibleSpans.empty())
		return;

	const auto normal = CalculateNormal(segment);
	vs.normal = std::get<0>(normal);
	vs.normalOffset = std::get<1>(normal);
	for (const auto& span : visibleSpans)
		RenderSegmentSpan(span, vs);
}

void Renderer::RenderSegmentSpan(const Span& span, const VisibleSegment& vs) {
	const auto& segment = vs.segment;
	const auto& frontSector = segment.frontSide->sector;

	for (auto x = span.s; x < span.e; x++) {
		if (ceilingClip[x] >= floorClip[x])
			continue;

		const auto angle = ViewAngle(x);
		const auto viewRelativeAngle = NormalizeAngle(vs.normal.angle - M_PI - (player.angle + angle));
		const auto interceptDistance = vs.normal.distance / std::cos(viewRelativeAngle);
		const auto projectionDistance = std::abs(std::cos(angle) * interceptDistance);
		if (projectionDistance < 1.0)
			continue;

		const auto ceilingHeight = frontSector->isSky ? NAN : (frontSector->ceilingHeight - player.z);
		const auto floorHeight = frontSector->floorHeight - player.z;

		const auto outerTopY = ViewY(projectionDistance, frontSector->ceilingHeight - player.z);
		const auto outerBotY = ViewY(projectionDistance, frontSector->floorHeight - player.z);
		const auto outerSpan = ClipVertical(x, outerTopY, outerBotY, !segment.twoSided, *frontSector, &ceilingHeight, &floorHeight);

		const auto offset = std::abs(interceptDistance * std::sin(viewRelativeAngle));
		WallSlice middleSlice = {
			x,
			outerSpan,
			vs.normalOffset + (viewRelativeAngle > 0 ? -offset : offset) + segment.xOffset + segment.frontSide->xOffset,
			projectionDistance / (settings::HEIGHT * 1.30434782),
			segment.lowerUnpegged ? outerBotY : outerTopY,
			segment.frontSide->yOffset,
			segment.frontSide->middleTexture,
			Lightness(projectionDistance, frontSector->lightLevel, &segment),
		};

		if (segment.twoSided) {
			const auto& backSector = segment.backSide->sector;
			const auto isSky = frontSector->isSky && backSector->isSky;

            const auto innerTopY = ViewY(projectionDistance, backSector->ceilingHeight - player.z);
            const auto innerBotY = ViewY(projectionDistance, backSector->floorHeight - player.z);
			const auto innerSpan = ClipVertical(x, innerTopY, innerBotY, false, *frontSector, isSky ? &ceilingHeight : nullptr);

			if (!isSky) {
				WallSlice upperSlice = { middleSlice };
				upperSlice.span = Span { outerSpan.s, innerSpan.s };
				upperSlice.texture = segment.frontSide->upperTexture;
				upperSlice.yPegging = segment.upperUnpegged ? outerTopY : innerTopY;
				RenderWallSlice(upperSlice);
			}

			WallSlice lowerSlice = { middleSlice };
			lowerSlice.span = Span { innerSpan.e, outerSpan.e };
			lowerSlice.texture = segment.frontSide->lowerTexture;
			lowerSlice.yPegging = segment.lowerUnpegged ? outerTopY : innerBotY;
			RenderWallSlice(lowerSlice);
		} else {
			RenderWallSlice(middleSlice);
		}
	}
}

void Renderer::RenderWallSlice(const WallSlice& ws) {
	if (ws.texture == nullptr)
		return;
	const auto tx = Clip(static_cast<int>(ws.xTexel), ws.texture->width);
	double yTexel = (ws.span.s - ws.yPegging) * ws.yScale;
	auto offset = settings::WIDTH * ws.span.s + (settings::WIDTH - 1 - ws.x);
	for (auto y = ws.span.s; y < ws.span.e; y++) {
		const auto ty = Clip(static_cast<int>(yTexel) + ws.yOffset, ws.texture->height);
		pixels[offset] = lightmap.Apply(ws.light, palette.GetColor(ws.texture->GetPixel(tx, ty))).GetRGB();
		yTexel += ws.yScale;
		offset += settings::WIDTH;
	}
}

void Renderer::RenderPlane(const Plane& plane) {
	std::shared_ptr<Texture> texture = plane.isSky ? wad.GetTexture("SKY1") : plane.texture;

	const auto angle = NormalizeAngle(player.angle);
	const auto angleStep = M_PI_4 / (settings::WIDTH / 2);
	const auto cosA = std::cos(angle);
	const auto sinA = std::sin(angle);

	for (auto y = 0; y < static_cast<int>(plane.spans.size()); y++) {
		const auto& spans = plane.spans[y];
		if (spans.empty())
			continue;

		std::list<Span> mergedSpans(spans.begin(), spans.end());
		mergedSpans.sort();
		auto es = mergedSpans.begin();
		do {
			auto ps = es++;
			if (es != mergedSpans.end() && (ps->e == es->s || ps->e == es->s - 1)) {
				es->s = ps->s;
				mergedSpans.erase(ps);
			}
		} while (es != mergedSpans.end());

		if (plane.isSky) {
			const auto ty = Clip(y * texture->height / settings::HEIGHT, texture->height);
			const auto xScale = texture->width / M_PI_4;
			for (const auto& span : mergedSpans) {
				auto offset = settings::WIDTH * y + (settings::WIDTH - 1 - span.s);
				for (auto x = span.s; x < span.e; x++) {
					const auto tx = Clip(static_cast<int>((angle + ViewAngle(x)) * xScale), texture->width);
					pixels[offset] = palette.GetColor(texture->GetPixel(tx, ty)).GetRGB();
					offset--;
				}
			}
		} else {
			const auto centerDistance = (settings::HEIGHT * 30.0) * std::abs(plane.height / 23.0) / static_cast<double>(std::abs(y - settings::HEIGHT / 2));
			if (!std::isfinite(centerDistance) || centerDistance < 1.0)
				continue;

			const auto light = Lightness(centerDistance, plane.light);
			const auto ccosA = centerDistance * cosA;
			const auto csinA = centerDistance * sinA;

			const auto stepX = -csinA * angleStep;
			const auto stepY = ccosA * angleStep;

			for (const auto& span : mergedSpans) {
				const auto angleTan = angleStep * (span.s - settings::WIDTH / 2);

				auto texelX = Clip(player.x + ccosA - csinA * angleTan, static_cast<double>(texture->width));
				auto texelY = Clip(player.y + csinA + ccosA * angleTan, static_cast<double>(texture->height));

				auto offset = settings::WIDTH * y + (settings::WIDTH - 1 - span.s);
				for (auto x = span.s; x < span.e; x++) {
					const auto tx = Clip(static_cast<int>(texelX), texture->width);
					const auto ty = Clip(static_cast<int>(texelY), texture->height);
					pixels[offset] = lightmap.Apply(light, palette.GetColor(texture->GetPixel(tx, ty))).GetRGB();
					texelX += stepX;
					texelY += stepY;
					offset--;
				}
			}
		}
	}
}

std::vector<Span> Renderer::ClipHorizontal(int startX, int endX, bool solid) {
	std::vector<Span> visible;

	startX = std::clamp(startX, 0, settings::WIDTH);
	endX = std::clamp(endX, 0, settings::WIDTH);
	if (startX >= endX)
		return visible;
	
	if (horizontalOcclusion.empty()) {
		visible.push_back({ startX, endX });
		if (solid)
			horizontalOcclusion.push_front({ startX, endX });
		return visible;
	}

	auto eo = horizontalOcclusion.begin();
	do {
		if (eo->s >= endX) {
			visible.push_back({ startX, endX });
			if (solid)
				horizontalOcclusion.insert(eo, { startX, endX });
			startX = endX;
			break;
		}
		if (eo->e < startX)
			continue;
		if (eo->s <= startX && eo->e >= endX) {
			startX = endX;
			break;
		}
		if (eo->s < startX) {
			startX = std::min(endX, eo->e);
		} else if (eo->s > startX) {
			visible.push_back({ startX, eo->s });
			if (solid)
				eo->s = startX;
			startX = std::min(endX, eo->e);
		}
		if (eo->e >= endX) {
			startX = endX;
			break;
		} else if (eo->e < endX) {
			startX = eo->e;
		}
	} while (++eo != horizontalOcclusion.end() && startX != endX);

	if (endX > startX) {
		visible.push_back({ startX, endX });
		if (solid)
			horizontalOcclusion.push_back({ startX, endX });
	}

	if (solid && horizontalOcclusion.size()) {
		eo = horizontalOcclusion.begin();
		do {
			auto pe = eo++;
			if (eo != horizontalOcclusion.end() && pe->e == eo->s) {
				eo->s = pe->s;
				horizontalOcclusion.erase(pe);
			}
		} while (eo != horizontalOcclusion.end());
	}

	return visible;
}

Span Renderer::ClipVertical(int x, int sy, int ey, bool solid, const Sector& sector, const double* ceilingHeight, const double* floorHeight) {
	Span span;
	if (floorClip[x] <= ceilingClip[x])
		return span;

	if (sy > ceilingClip[x]) {
		span.s = std::min(sy, floorClip[x]);
		if (ceilingHeight != nullptr)
			ClipPlane(ceilingPlanes, x, ceilingClip[x], span.s, *ceilingHeight, sector.lightLevel, sector.ceilingTexture);
	} else {
		span.s = ceilingClip[x];
	}
	if (ey < floorClip[x]) {
		span.e = std::max(ey, ceilingClip[x]);
		if (floorHeight != nullptr)
			ClipPlane(floorPlanes, x, span.e, floorClip[x], *floorHeight, sector.lightLevel, sector.floorTexture);
	} else {
		span.e = floorClip[x];
	}

	if (solid) {
		ceilingClip[x] = floorClip[x] = 0;
	} else {
		if (sy > ceilingClip[x])
			ceilingClip[x] = sy;
		if (ey < floorClip[x])
			floorClip[x] = ey;
	}

	return span;
}

void Renderer::ClipPlane(std::deque<Plane>& planes, int x, int sy, int ey, double height, double light, std::shared_ptr<Texture> texture) {
	auto plane = std::find_if(planes.begin(), planes.end(), [&](const Plane& plane) {
		return (plane.height == height || (!std::isfinite(plane.height) && !std::isfinite(height))) && plane.light == light && plane.texture == texture;
	});
	if (plane == planes.end()) {
		planes.push_front(Plane {height, light, texture, settings::HEIGHT});
		plane = planes.begin();
	}
	for (auto y = sy; y < ey; y++) {
		auto& span = plane->spans[y];
		if (span.empty()) {
			span.push_back(Span {x, x});
		} else {
			auto& lastSpan = span.back();
			if (lastSpan.e == x - 1) {
				lastSpan.e = x;
			} else {
				span.push_back(Span {x, x});
			}
		}
	}
}

/*
 * Helpers
 */
std::tuple<Vector, double> Renderer::CalculateNormal(const Line& l) {
	auto lineAngle = std::atan2(l.e.y - l.s.y, l.e.x - l.s.x);
	auto normalAngle = lineAngle + M_PI_2;
	auto sx = l.s.x - player.x;
	auto sy = l.s.y - player.y;
	auto dx = player.x - l.s.x;
	auto dy = player.y - l.s.y;
	auto startDistance = std::sqrt(dx * dx + dy * dy);
	auto startAngle = atan2(sy, sx);
	auto normalDistance = std::abs(startDistance * std::cos(normalAngle - startAngle));
	auto normalOffset = std::abs(startDistance * std::sin(normalAngle - startAngle));
	if (NormalizeAngle(normalAngle - startAngle) > 0)
		normalOffset *= -1;
	return std::tuple<Vector, double> { Vector {normalAngle, normalDistance}, normalOffset };
}

double Renderer::NormalizeAngle(double angle) {
	while (angle < -M_PI)
		angle += 2 * M_PI;
	while (angle > M_PI)
		angle -= 2 * M_PI;
	return angle;
}

int Renderer::ViewX(double angle) {
	angle = NormalizeAngle(angle);
	if (angle < -M_PI_4)
		return -1;
	if (angle >= M_PI_4)
		return settings::WIDTH;
	auto midDistance = std::tan(angle) / M_PI_4;
	if (midDistance <= -1)
		return -1;
	if (midDistance >= 1)
		return settings::WIDTH;
	return static_cast<int>((settings::WIDTH / 2) * (1 + midDistance));
}

int Renderer::ViewY(double distance, double height) {
	const auto dy = static_cast<int>(std::abs(height / 23.0) * (settings::HEIGHT * 30.0) / distance);
	return settings::HEIGHT / 2 + (height > 0 ? -dy : dy);
}

double Renderer::ViewAngle(int x) {
	return std::atan(static_cast<double>(x - settings::WIDTH / 2) / (settings::WIDTH / 2) * M_PI_4);
}

int Renderer::Lightness(double distance, double lightLevel, const Segment* segment) {
	auto lightness = (0.9 - (distance / 1500.0)) * lightLevel;
	if (segment != nullptr) {
		if (segment->isVertical)
			lightness *= 1.1;
		if (segment->isHorizontal)
			lightness *= 0.9;
	}
	if (lightness >= 1.0)
		return 255;
	if (lightness <= 0.0)
		return 38;
	return static_cast<int>(255.0 * (0.2 + lightness * 0.8));
}

int Renderer::Clip(int value, int max) {
	value %= max;
	return value < 0 ? (max + value) : value;
}

double Renderer::Clip(double value, double max) {
	if (!std::isfinite(value))
		return value;
	value = std::fmod(value, max);
	return value < 0.0 ? (max + value) : value;
}