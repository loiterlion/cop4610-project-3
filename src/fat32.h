#pragma once

#include <fstream>
#include <iostream>

using namespace std;

namespace FAT_FS {

// FAT Markers
const streampos BPB_BytsPerSec = 0xB,
				BPB_SecPerClus = 0xD,
				BPB_RsvdSecCnt = 0xE,
				BPB_NumFATs = 0x10,
				BPB_TotSec32 = 0x20,
				BPB_FATSz32 = 0x24;

/**
 * FAT File System
 * Description: Representation of a FAT File System that can be operated on.
 */
class FAT32 {

private:

	// BIOS Parameter Block Info
	unsigned short bytesPerSector, reservedSectorCount;
	unsigned char sectorsPerCluster, numFats;
	unsigned int totalSectors, sectorsPerFAT;

	fstream & fatImage;

	template<class T>
	void getPrimitive( const streampos & pos, T & out );

public:

	FAT32( fstream & fatImage );

	/**
	 * Commands
	 */
	 
	void fsinfo() const;
	void open();
	void close();
	void create();
	void read();
	void write();
	void rm();
	void cd();
	void ls();
	void mkdir();
	void rmdir();
	void size();
	void srm();

};

}