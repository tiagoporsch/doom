#pragma once

#include <deque>
#include <list>
#include <memory>
#include <SDL2/SDL.h>
#include <vector>

#include "map.h"
#include "player.h"
#include "settings.h"

/*
 * Generic
 */
struct Vector {
	double angle;
	double distance;

	constexpr Vector(): Vector {0, 0} {}
	constexpr Vector(double angle, double distance): angle {angle}, distance {distance} {}
};

struct Span {
	int s, e;

	constexpr Span(): Span {0, 0} {}
	constexpr Span(int s, int e): s {s}, e {e} {}

	bool operator<(const Span& r) const { return s == r.s ? e < r.e : s < r.s; }
};

/*
 * Colors
 */
struct Color {
	uint8_t r, g, b;

	inline uint32_t GetRGB() const { return (r << 16) | (g << 8) | b; }
};

struct Palette {
	static constexpr auto SIZE = 256;

	std::array<Color, SIZE> colors;

	Palette() = default;
	Palette(std::array<Color, SIZE> colors): colors {colors} {}

	inline Color GetColor(uint8_t pixel) const { return colors[pixel]; }
};

struct Lightmap {
	static constexpr auto SIZE = 256;

	uint8_t map[SIZE][SIZE];

	Lightmap() {
		for (auto l = 0; l < SIZE; l++) {
			for (auto v = 0; v < SIZE; v++) {
				map[l][v] = static_cast<uint8_t>(static_cast<double>(v) * static_cast<double>(l) / static_cast<double>(SIZE));
			}
    	}
	};

	inline Color Apply(uint8_t light, const Color& color) const {
		return { map[light][color.r], map[light][color.g], map[light][color.b] };
	};
};

/*
 * Structure
 */
struct VisibleSegment {
	const Segment& segment;
	Vector normal;
	double normalOffset;
	int startX;
	int endX;
	double startAngle;
	double endAngle;

	VisibleSegment(const Segment& segment): segment {segment} {}
};

struct Plane {
	double height;
	double light;
	std::shared_ptr<Texture> texture;
	std::vector<std::vector<Span>> spans;

	bool isSky = std::isnan(height);

	Plane(double height, double light, std::shared_ptr<Texture> texture, size_t size):
	height {height}, light {light}, texture {texture}, spans {size} {}
};

struct WallSlice {
	int x;
	Span span;

	double xTexel;
	double yScale;
	int yPegging;
	int yOffset;

	std::shared_ptr<Texture> texture;
	int light;
};

/*
 * Renderer
 */
class Renderer {
	WAD& wad;
	Map& map;
	Player& player;
	uint32_t* pixels;

	std::list<Span> horizontalOcclusion;
	std::deque<Plane> ceilingPlanes;
	std::deque<Plane> floorPlanes;
	int ceilingClip[settings::WIDTH];
	int floorClip[settings::WIDTH];

	Palette palette;
	Lightmap lightmap;

public:
	Renderer(WAD&, Map&, Player&, uint32_t*);

	void Render();

private:
	// Rendering
	void RenderSegment(const Segment&);
	void RenderSegmentSpan(const Span&, const VisibleSegment&);
	void RenderWallSlice(const WallSlice&);
	void RenderPlane(const Plane&);

	// Clipping
	std::vector<Span> ClipHorizontal(int, int, bool);
	Span ClipVertical(int, int, int, bool, const Sector&, const double* = nullptr, const double* = nullptr);
	void ClipPlane(std::deque<Plane>&, int, int, int, double, double, std::shared_ptr<Texture>);

	// Helpers
	std::tuple<Vector, double> CalculateNormal(const Line&);
	double NormalizeAngle(double);
	int ViewX(double);
	int ViewY(double, double);
	double ViewAngle(int);
	int Lightness(double, double, const Segment* = nullptr);
	int Clip(int, int);
	double Clip(double, double);
};