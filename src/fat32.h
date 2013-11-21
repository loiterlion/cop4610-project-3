#pragma once

#include <cstdint>
#include <cwchar>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace std;

namespace FAT_FS {

// FAT Markers
const streampos BPB_BytsPerSec = 0xB,
				BPB_SecPerClus = 0xD,
				BPB_RsvdSecCnt = 0xE,
				BPB_NumFATs = 0x10,
				BPB_TotSec32 = 0x20,
				BPB_FATSz32 = 0x24,
				BPB_RootClus = 0x2C;

const uint8_t ATTR_READ_ONLY = 0x01,
					ATTR_HIDDEN = 0x02,
					ATTR_SYSTEM = 0x04,
					ATTR_VOLUME_ID = 0x08,
					ATTR_DIRECTORY = 0x10,
					ATTR_ARCHIVE = 0x20,
					ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID,
					ATTR_LONG_NAME_MASK = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID | ATTR_DIRECTORY | ATTR_ARCHIVE,
					LAST_LONG_ENTRY = 0x40;

const uint32_t FATEntrySize = 0x4,
				   FATEntryMask = 0x0FFFFFFF,
				   FreeCluster = 0x00000000,
				   EOCMarker = 0x0FFFFFF8,
				   DIR_Name = 0x00,
				   DIR_Attr = 0x0B,
				   DIR_NTRes = 0x0C,
				   DIR_CrtTimeTenth = 0x0D,
				   DIR_CrtTime = 0x0E,
				   DIR_CrtDate = 0x10,
				   DIR_LstAccDate = 0x12,
				   DIR_FstClusHI = 0x14,
				   DIR_WrtTime = 0x16,
				   DIR_WrtDate = 0x18,
				   DIR_FstClusLO = 0x1A,
				   DIR_FileSize = 0x1C,
				   LDIR_Ord = 0x00,
				   LDIR_Name1 = 0x01,
				   LDIR_Attr = 0x0B,
				   LDIR_Type = 0x0C,
				   LDIR_Chksum = 0x0D,
				   LDIR_Name2 = 0x0E,
				   LDIR_FstClusLO = 0x1A,
				   LDIR_Name3 = 0x1C;

typedef struct BIOSParameterBlock {

	uint8_t jmpBoot[3];
	uint8_t OEMName[8];
	uint16_t bytesPerSector;
	uint8_t sectorsPerCluster;
	uint16_t reservedSectorCount;
	uint8_t numFATs;
	uint16_t rootEntryCount;
	uint16_t totalSectors16;
	uint8_t media;
	uint16_t FATSz16;
	uint16_t sectorsPerTrack;
	uint16_t numHeads;
	uint32_t hiddenSectors;
	uint32_t totalSectors32;
	uint32_t FATSz32;
	uint16_t extraFlags;
	uint16_t FSVersion;
	uint32_t rootCluster;
	uint16_t FSInfo;
	uint16_t backupBootSector;
	uint8_t reserved[12];
	uint8_t driveNumber;
	uint8_t reserved1;
	uint8_t bootSignature;
	uint32_t volumeID;
	uint8_t volumeLabel[11];
	uint8_t fileSystemType[8];

} __attribute__((packed)) BIOSParameterBlock;

typedef struct ShortDirectoryEntry {

	uint8_t name[11];			
	uint8_t attributes;		
	uint8_t NTReserved;		
	uint8_t createdTimeTenth;	
	uint16_t createdTime;		
	uint16_t createdDate;		
	uint16_t lastAccessDate;	
	uint16_t firstClusterHI;	
	uint16_t writeTime;		
	uint16_t writeDate;		
	uint16_t firstClusterLO;	
	uint32_t fileSize;	

} __attribute__((packed)) ShortDirectoryEntry;

typedef struct LongDirectoryEntry {

	uint8_t ordinal;  		
	uint16_t name1[5];		
	uint8_t attributes;		
	uint8_t type;				
	uint8_t checksum;			
	uint16_t name2[6];		
	uint16_t firstClusterLO;	
	uint16_t name3[2];		

} __attribute__((packed)) LongDirectoryEntry;

typedef struct DirectoryEntry {

	string name;
	uint8_t  attributes;		
	uint8_t  NTReserved;		
	uint8_t  createdTimeTenth;	
	uint16_t createdTime;		
	uint16_t createdDate;		
	uint16_t lastAccessDate;	
	uint16_t firstClusterHI;	
	uint16_t writeTime;		
	uint16_t writeDate;		
	uint16_t firstClusterLO;	
	uint32_t  fileSize;

} __attribute__((packed)) DirectoryEntry;

/**
 * FAT File System
 * Description: Representation of a FAT File System that can be operated on.
 */
class FAT32 {

private:

	// BIOS Parameter Block Info
	BIOSParameterBlock bpb;

	uint32_t firstDataSector, fatLocation;

	fstream & fatImage;

	string currentDirectory;

	vector<uint32_t> freeClusters;
	vector<DirectoryEntry> currentDirectoryListing;
	
	uint32_t getFirstSectorOfCluster( uint32_t n );
	bool isFreeCluster( uint32_t entry );
	uint32_t getFATEntry( uint32_t n );
	void changeDirectory( uint32_t cluster ); 
	uint8_t * getFileContents( uint32_t initialCluster, uint32_t & size );
	void appendLongName( string & current, uint16_t * name, uint32_t size );
	void convertShortName( string & current, uint8_t * name );

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
	void ls( const string & directory ) const;
	void mkdir();
	void rmdir();
	void size();
	void srm();

};

}