#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct Texture {
	const std::string name;
	const int width;
	const int height;
	const std::unique_ptr<uint8_t[]> pixels;

	Texture(const std::string& name, int width, int height):
		name { name }, width { width }, height { height }, pixels { std::make_unique<uint8_t[]>(width * height)} {}

	inline uint8_t GetPixel(int x, int y) const { return pixels[y * width + x]; }
};

class WAD {
public:
	class Lump {
	public:
		enum class Type {
			Unknown,
			MapMarker,
			Palette,
			Texture,
			PatchNames,
			PatchesStart,
			PatchesEnd,
			FlatsStart,
			FlatsEnd,
			SpritesStart,
			SpritesEnd,
		};
	private:
		Type StringToType(const std::string&);
	public:
		const uint32_t location;
		const uint32_t size;
		const std::string name;
		const Type type;
		Lump(const uint32_t, const uint32_t, const std::string);
	};

private:
	std::ifstream file;
	std::vector<std::shared_ptr<Lump>> lumps;

	std::map<std::string, std::shared_ptr<Texture>> flats;
	std::map<std::string, std::shared_ptr<Texture>> textures;

public:
	WAD(const std::string&);
	~WAD();

	std::shared_ptr<Lump> GetLump(const std::string&);
	std::map<std::string, std::shared_ptr<Lump>> GetMapLumps(const std::string&);

	std::shared_ptr<Texture> GetFlat(const std::string&) const;
	std::shared_ptr<Texture> GetTexture(const std::string&) const;

	void seek(size_t location) {
		file.seekg(location, std::ios_base::seekdir::_S_beg);
	}

	template <typename T>
	T read() {
		T ret;
		file.read(reinterpret_cast<char*>(&ret), sizeof(T));
		return ret;
	}

	template <typename T>
	void read(T* buffer, size_t count) {
		file.read(reinterpret_cast<char*>(buffer), count * sizeof(T));
	}

	std::string readString(size_t length) {
		char buffer[length + 1];
		buffer[length] = 0;
		file.read(buffer, length);
		return std::string(buffer);
	}
};