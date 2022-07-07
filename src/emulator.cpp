#include <cartridge.h>

#include <iostream>

int main(int argc, char* argv[])
{
	if (argc == 1)
	{
		return -1;
	}

	trv::snes::Cartridge cart { argv[1] };

	return 0;
}
