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
typedef uint16_t Address;

enum class Interrupt
{
	COP,
	BRK,
	ABORT,
	NMI,
	IRQ,
	RES,
	Count
};

enum class ROMType : uint8_t
{
	LoROM,
	HiROM,
	ExLoROM,
	ExHiROM,
};

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
	Header(const std::vector<unsigned char>& cartridge, bool hasSMCHeader)
	{
		if (!build(cartridge, hasSMCHeader ? (0x7FC0 + 0x200) : 0x7FC0))
		{
			if (!build(cartridge, hasSMCHeader ? (0xFFC0 + 0x200) : 0xFFC0))
			{
				throw std::runtime_error(
				    "TRV::SNES::CARTRIDGE Unable to determine ROM format cartridge may be "
				    "corrupt.");
			}
			else
			{
				romType = ROMType::HiROM;
			}
		}
		else
		{
			romType = ROMType::LoROM;
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

		// copyInto(country, cartridge, offset);
		copyInto(licensee, cartridge, offset);
		offset += 1;
		copyInto(version, cartridge, offset);
		copyInto(nchecksum, cartridge, offset);
		copyInto(checksum, cartridge, offset);

		if ((nchecksum ^ checksum) != 0xFFFF)
		{
			return false;
		}

		return true;
	};

	friend std::ostream& operator<<(std::ostream& o, const Header& header)
	{
		o << "Title: " << header.title.data()
		  << "\nFastROM: " << (header.fastROM ? "True" : "False")
		  << "\nMapping Mode: " << header.mappingMode
		  << "\nCartridge Type: " << static_cast<int>(header.cartridgeType)
		  << "\nROM Size: " << header.cartridgeROMSize << "\nRAM Size: " << header.cartridgeRAMSize
		  << "\nCountry: " << header.country << "\nLicensee" << header.licensee
		  << "\nVersion: " << header.version << "\nROM Type: " << static_cast<int>(header.romType);
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
	std::uint16_t nchecksum;
	std::uint16_t checksum;
	ROMType romType;
};

inline uint16_t get_address(const std::vector<unsigned char>& cartridge, std::size_t address)
{
	return (cartridge[address + 1] << 8) + cartridge[address];
};
struct NativeInterruptVector
{
	NativeInterruptVector() = default;
	NativeInterruptVector(const std::vector<unsigned char>& cartridge, std::size_t offset) :
	    addresses {
		    get_address(cartridge, 0xFE4 + offset),  // COP
		    get_address(cartridge, 0xFE6 + offset),  // BRK
		    get_address(cartridge, 0xFE8 + offset),  // ABORT
		    get_address(cartridge, 0xFEA + offset),  // NMI
		    get_address(cartridge, 0xFEF + offset)   // IRQ
	    } {};

	Address dispatchInterrupt(Interrupt interrupt)
	{
		if (interrupt == Interrupt::RES || interrupt == Interrupt::Count)
		{
			throw std::runtime_error(
			    "TRV::SNES::CARTRIDGE Unexpected interrupt recieved in native mode.");
		}

		return addresses[static_cast<std::size_t>(interrupt)];
	};

   private:
	std::array<Address, static_cast<std::size_t>(Interrupt::Count)> addresses;
};

struct EmulatorInterruptVector
{
	EmulatorInterruptVector() = default;
	EmulatorInterruptVector(const std::vector<unsigned char>& cartridge, std::size_t offset) :
	    addresses {
		    get_address(cartridge, 0xFF4 + offset),  // COP
		    get_address(cartridge, 0xFFE + offset),  // BRK
		    get_address(cartridge, 0xFF8 + offset),  // ABORT
		    get_address(cartridge, 0xFEA + offset),  // NMI
		    get_address(cartridge, 0xFFE + offset),  // IRQ
		    get_address(cartridge, 0xFFC + offset),  // RES
	    } {};

	Address dispatchInterrupt(Interrupt interrupt)
	{
		if (interrupt == Interrupt::Count)
		{
			throw std::runtime_error(
			    "TRV::SNES::CARTRIDGE Unexpected interrupt recieved in native mode.");
		}

		return addresses[static_cast<std::size_t>(interrupt)];
	};

   private:
	std::array<Address, static_cast<std::size_t>(Interrupt::Count)> addresses;
};

struct Cartridge
{
	static constexpr std::size_t MAX_CART_SIZE = 0x6'000'000;
	static constexpr std::size_t MIN_CART_SIZE = 0x8'000;

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

		infile.clear();
		infile.seekg(0, std::ios_base::beg);

		std::cout << "Size: " << length << "\n";

		if (length < MIN_CART_SIZE)
		{
			throw std::runtime_error("TRV::SNES::CARTRIDGE File size smaller than expected.");
		}

		if (length > MAX_CART_SIZE)
		{
			throw std::runtime_error("TRV::SNES::CARTRIDGE File size larger than expected.");
		}

		std::size_t remainder = length & 0x7FFF;

		bool hasSMCHeader = false;

		if (remainder == 0x200)
		{
			hasSMCHeader = true;
		}
		else if (remainder != 0)
		{
			throw std::runtime_error("TRV::SNES::CARTRIDGE Unexpected ROM size.");
		}

		data.reserve(length);
		std::copy(std::istreambuf_iterator<char>(infile), {}, std::back_inserter(data));

		header = Header(data, hasSMCHeader);
		std::cout << header;

		std::size_t offset;

		switch (header.romType)
		{
			case ROMType::LoROM:
				offset = 0x7000;
				break;
			case ROMType::HiROM:
				offset = 0xF000;
				break;
		}

		nativeInterrupts   = NativeInterruptVector(data, offset);
		emulatorInterrupts = EmulatorInterruptVector(data, offset);
	};

	Header header;
	NativeInterruptVector nativeInterrupts;
	EmulatorInterruptVector emulatorInterrupts;
	std::vector<unsigned char> data;
};
}
#endif  // TRV_CARTRIDGE_H
