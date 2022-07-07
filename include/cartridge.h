#ifndef TRV_CARTRIDGE_H
#define TRV_CARTRIDGE_H

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace trv::snes
{
enum class CartridgeType : uint8_t
{
	ROM                 = 0x00,
	ROM_RAM             = 0x01,
	ROM_RAM_Battery     = 0x02,
	ROM_SA1             = 0x33,
	ROM_SA1_RAM         = 0X34,
	ROM_SA1_RAM_Battery = 0x35
};

template <typename T>
concept Pointer = requires(T t)
{
	std::is_pointer_v<T>;
};
template <typename T>
concept NotPointer = requires(T t)
{
	!std::is_pointer_v<T>;
};

struct Header
{
   public:
	Header() = default;
	Header(const std::vector<unsigned char>& cartridge)
	{
		if (!build(cartridge, 0x7FC0))
		{
			if (!build(cartridge, 0xFFC0))
			{
				throw std::runtime_error(
				    "TRV::SNES::CARTRIDGE Unable to determine ROM format cartridge may be "
				    "corrupt.");
			}
		}
	};

	bool build(const std::vector<unsigned char>& cartridge, std::size_t offset)
	{
		copyInto(title, cartridge, offset);
		uint8_t mapMode;
		copyInto(&mapMode, cartridge, offset);

		if (mapMode >> 5 != 1)
		{
			return false;
		}

		fastROM     = mapMode & 0b00010000;
		mappingMode = mapMode & 0b00001111;
		copyInto(cartridgeType, cartridge, offset);

		uint8_t logKBROMSize, logKBRAMSize;
		copyInto(logKBROMSize, cartridge, offset);
		copyInto(logKBRAMSize, cartridge, offset);
		cartridgeROMSize = 0x400 << logKBROMSize;
		cartridgeRAMSize = 0X400 << logKBRAMSize;

		copyInto(country, cartridge, offset);
		copyInto(licensee, cartridge, offset);
		copyInto(version, cartridge, offset);
		return true;
	};

	friend std::ostream& operator<<(std::ostream& o, const Header& header)
	{
		o << "Title: " << header.title.data()
		  << "\nFastROM: " << (header.fastROM ? "True" : "False")
		  << "\nMapping Mode: " << header.mappingMode
		  << "\nCartridge Type: " << static_cast<uint8_t>(header.cartridgeType)
		  << "\nROM Size: " << header.cartridgeROMSize << "\nRAM Size: " << header.cartridgeRAMSize
		  << "\nCountry: " << header.country << "\nLicensee" << header.licensee
		  << "\nVersion: " << header.version;
		return o;
	};

   private:
	template <typename T>
	void copyInto(T& t, const std::vector<unsigned char>& cartridge, std::size_t& offset)
	{
		std::memcpy(&t, cartridge.data() + offset, sizeof(T));
		offset += sizeof(T);
	};

	template <typename T>
	void copyInto(T* t, const std::vector<unsigned char>& cartridge, std::size_t& offset)
	{
		std::memcpy(t, cartridge.data() + offset, sizeof(T));
		offset += sizeof(T);
	};

	template <typename T, std::size_t n>
	void copyInto(std::array<T, n>& arr,
	              const std::vector<unsigned char>& cartridge,
	              std::size_t& offset)
	{
		std::memcpy(arr.data(), cartridge.data() + offset, sizeof(T) * n);
		offset += sizeof(T) * n;
	};

   public:
	std::array<unsigned char, 21> title;
	bool fastROM;
	std::uint8_t mappingMode;
	CartridgeType cartridgeType;
	std::size_t cartridgeROMSize;
	std::size_t cartridgeRAMSize;
	std::uint8_t country;
	std::uint8_t licensee;
	std::uint8_t version;
};

struct Cartridge
{
	static constexpr std::size_t MAX_CART_SIZE = 0x6'000'000;

	Cartridge() = default;

	Cartridge(const std::string& filepath)
	{
		std::cout << "Opening: " << filepath << "\n";
		std::ifstream infile(filepath, std::ios_base::binary | std::ios_base::in);

		if (infile.rdstate() & std::ios_base::failbit)
		{
			throw std::runtime_error("TRV::SNES::CARTRIDGE Unable to open file.");
		}

		infile.ignore(std::numeric_limits<std::streamsize>::max());
		std::streamsize length = infile.gcount();

		std::cout << "Size: " << length << "\n";

		if (length > MAX_CART_SIZE)
		{
			throw std::runtime_error("TRV::SNES::CARTRIDGE File size larger than expected.");
		}

		infile.clear();
		infile.seekg(0, std::ios_base::beg);

		data.reserve(length);
		std::copy(std::istreambuf_iterator<char>(infile), {}, std::back_inserter(data));

		header = Header(data);
		std::cout << header;
	};

	Header header;
	std::vector<unsigned char> data;
};
}
#endif  // TRV_CARTRIDGE_H
