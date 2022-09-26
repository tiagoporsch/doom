#include "wad.h"

#include <algorithm>
#include <fstream>
#include <iostream>

/*
 * Lump
 */
WAD::Lump::Type WAD::Lump::StringToType(const std::string& name) {
	if (name[0] == 'E' && name[2] == 'M')
		return Type::MapMarker;
	if (name[0] == 'P' && isdigit(name[1]) && name[2] == '_')
		return name[3] == 'S' ? Type::PatchesStart : Type::PatchesEnd;
	if (name[0] == 'F' && isdigit(name[1]) && name[2] == '_')
		return name[3] == 'S' ? Type::FlatsStart : Type::FlatsEnd;
	if (name[0] == 'S' && name[1] == '_')
		return name[3] == 'S' ? Type::SpritesStart : Type::SpritesEnd;
	if (name == "PLAYPAL")
		return Type::Palette;
	if (name == "PNAMES")
		return Type::PatchNames;
	if (name.substr(0, 7) == "TEXTURE")
		return Type::Texture;
	return Type::Unknown;
}

WAD::Lump::Lump(uint32_t location, uint32_t size, std::string name): location {location}, size { size }, name { name }, type { StringToType(name) } {
}

/*
 * WAD
 */
WAD::WAD(const std::string& filename) {
	file = std::ifstream(filename, std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "Error: could not open WAD '" << filename << "'" << std::endl;
		exit(1);
	}

	auto type = readString(4);
	if (type != "IWAD") {
		std::cerr << "Error: unsupported WAD type '" << type << "'" << std::endl;
		exit(1);
	}
	auto lumpCount = read<uint32_t>();
	auto directoryLocation = read<uint32_t>();
	seek(directoryLocation);
	for (uint32_t i = 0; i < lumpCount; i++) {
		auto location = read<uint32_t>();
		auto size = read<uint32_t>();
		auto name = readString(8);
		lumps.push_back(std::make_shared<Lump>(location, size, name));
	}

	struct Patch {
		int16_t width;
		int16_t height;
		int16_t left;
		int16_t top;
		std::unique_ptr<uint8_t[]> pixels;
	};
	std::map<std::string, std::shared_ptr<Patch>> patches;
	std::vector<std::string> patchNames;

	for (auto it = lumps.begin(); it != lumps.end(); it++) {
		auto lump = *it;
		switch (lump->type) {
		case Lump::Type::PatchNames: {
			seek(lump->location);
			auto patchCount = read<int32_t>();
			for (auto i = 0; i < patchCount; i++) {
				patchNames.push_back(readString(8));
			}
			break;
		}
		case Lump::Type::FlatsStart: {
			while ((*++it)->type != Lump::Type::FlatsEnd) {
				seek((*it)->location);
				auto t = std::make_shared<Texture>((*it)->name, 64, 64);
				read(t->pixels.get(), 64 * 64);
				flats.insert(make_pair((*it)->name, t));
			}
			break;
		}
		case Lump::Type::PatchesStart: {
			while ((*++it)->type != Lump::Type::PatchesEnd) {
				seek((*it)->location);

				auto p = std::make_shared<Patch>();
				p->width = read<int16_t>();
				p->height = read<int16_t>();
				p->left = read<int16_t>();
				p->top = read<int16_t>();
				p->pixels = std::make_unique<uint8_t[]>(p->width * p->height);

				int32_t columnArray[p->width];
				for (auto i = 0; i < p->width; i++)
					columnArray[i] = read<int32_t>();

				for (auto i = 0; i < p->width; i++) {
					seek((*it)->location + columnArray[i]);
					uint8_t rowStart = 0;
					while (rowStart != 255) {
						rowStart = read<uint8_t>();
						if (rowStart == 255)
							break;
						auto pixelCount = read<uint8_t>();
						read<uint8_t>(); // dummy value
						for (uint8_t j = 0; j < pixelCount; j++)
							p->pixels[(j + rowStart) * p->width + i] = read<uint8_t>();
						read<uint8_t>(); // dummy value
					}
				}

				patches.insert(make_pair((*it)->name, p));
			}
			break;
		}
		default:
			break;
		}
	}

	for (const auto& lump : lumps) {
		switch (lump->type) {
		case Lump::Type::Texture: {
			seek(lump->location);

			auto textureCount = read<uint32_t>();
			std::vector<int32_t> textureOffsets(textureCount);
			read(textureOffsets.data(), textureOffsets.size());

			for (unsigned i = 0; i < textureCount; i++) {
				seek(lump->location + textureOffsets[i]);

				auto name = readString(8);
				read<uint32_t>(); // masked
				auto width = read<int16_t>();
				auto height = read<int16_t>();
				read<uint32_t>(); // column directory
				auto patchCount = read<int16_t>();

				auto t = std::make_shared<Texture>(name, width, height);
				for (auto j = 0; j < patchCount; j++) {
					auto px = read<int16_t>();
					auto py = read<int16_t>();
					auto patchNumber = read<uint16_t>();
					read<int16_t>(); // step direction
					read<int16_t>(); // color map

					if (patchNumber >= patchNames.size())
						continue;
					auto patchIndex = patchNames[patchNumber];
					if (patches.find(patchIndex) == patches.end())
						continue;
					auto patch = patches[patchIndex].get();

					for (auto x = 0; x < patch->width; x++) {
						for (auto y = 0; y < patch->height; y++) {
							const auto pixel = patch->pixels[y * patch->width + x];
							if (pixel == 247)
								continue;
							const auto dx = px + x;
							const auto dy = py + y;
							if (dx < 0 || dx >= t->width || dy < 0 || dy >= t->height)
								continue;
							t->pixels[dy * t->width + dx] = pixel;
						}
					}
				}

				textures.insert(make_pair(name, t));
			}
			break;
		}
		default:
			break;
		}
	}
}

WAD::~WAD() {
	if (file.is_open())
		file.close();
}

std::shared_ptr<WAD::Lump> WAD::GetLump(const std::string& name) {
	for (auto it = lumps.begin(); it != lumps.end(); it++) {
		if ((*it)->name == name) {
			return *it;
		}
	}
	std::cerr << "[WARN]: Could not find lump '" << name << '\'' << std::endl;
	return nullptr;
}

std::map<std::string, std::shared_ptr<WAD::Lump>> WAD::GetMapLumps(const std::string& name) {
	std::map<std::string, std::shared_ptr<Lump>> mapLumps;
	for (auto it = lumps.begin(); it != lumps.end(); it++) {
		if ((*it)->type == Lump::Type::MapMarker && (*it)->name == name) {
			for (auto i = 0; i < 10; i++, it++) {
				mapLumps.insert(make_pair((*it)->name, *it));
			}
			break;
		}
	}
	return mapLumps;
}

std::shared_ptr<Texture> WAD::GetFlat(const std::string& name) const {
	if (name == "-")
		return nullptr;
	std::string upper(name);
	std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
	const auto& flat = flats.find(upper);
	if (flat != flats.end())
		return flat->second;
	std::cerr << "[WARN]: Could not find flat '" << name << "'" << std::endl;
	return nullptr;
}

std::shared_ptr<Texture> WAD::GetTexture(const std::string& name) const {
	if (name == "-")
		return nullptr;
	std::string upper(name);
	std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
	const auto& texture = textures.find(upper);
	if (texture != textures.end())
		return texture->second;
	std::cerr << "[WARN]: Could not find texture '" << name << "'" << std::endl;
	return nullptr;
}