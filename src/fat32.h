#pragma once

#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <cwchar>

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

const unsigned char ATTR_READ_ONLY = 0x01,
					ATTR_HIDDEN = 0x02,
					ATTR_SYSTEM = 0x04,
					ATTR_VOLUME_ID = 0x08,
					ATTR_DIRECTORY = 0x10,
					ATTR_ARCHIVE = 0x20,
					ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID,
					ATTR_LONG_NAME_MASK = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID | ATTR_DIRECTORY | ATTR_ARCHIVE,
					LAST_LONG_ENTRY = 0x40;

const unsigned int FATEntrySize = 0x4,
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

typedef struct ShortDirectoryEntry {

	unsigned char name[11];			
	unsigned char attributes;		
	unsigned char NTReserved;		
	unsigned char createdTimeTenth;	
	unsigned short createdTime;		
	unsigned short createdDate;		
	unsigned short lastAccessDate;	
	unsigned short firstClusterHI;	
	unsigned short writeTime;		
	unsigned short writeDate;		
	unsigned short firstClusterLO;	
	unsigned int fileSize;			
} ShortDirectoryEntry;

typedef struct LongDirectoryEntry {

	unsigned char ordinal;  		
	unsigned short name1[5];		
	unsigned char attributes;		
	unsigned char type;				
	unsigned char checksum;			
	unsigned short name2[6];		
	unsigned short firstClusterLO;	
	unsigned short name3[2];		

} LongDirectoryEntry;

typedef struct DirectoryEntry {

	DirectoryEntry( const wstring & name, const ShortDirectoryEntry & shortEntry ) {

		this->name = name;
		this->attributes = shortEntry.attributes;		
		this->NTReserved = shortEntry.NTReserved;			
		this->createdTimeTenth = shortEntry.createdTimeTenth;		
		this->createdTime = shortEntry.createdTime;			
		this->createdDate = shortEntry.createdDate;			
		this->lastAccessDate = shortEntry.lastAccessDate;		
		this->firstClusterHI = shortEntry.firstClusterHI;		
		this->writeTime = shortEntry.writeTime;			
		this->writeDate = shortEntry.writeDate;			
		this->firstClusterLO = shortEntry.firstClusterLO;		
		this->fileSize = shortEntry.fileSize;	
	}

	wstring name;
	unsigned char attributes;		
	unsigned char NTReserved;		
	unsigned char createdTimeTenth;	
	unsigned short createdTime;		
	unsigned short createdDate;		
	unsigned short lastAccessDate;	
	unsigned short firstClusterHI;	
	unsigned short writeTime;		
	unsigned short writeDate;		
	unsigned short firstClusterLO;	
	unsigned int fileSize;
} DirectoryEntry;

/**
 * FAT File System
 * Description: Representation of a FAT File System that can be operated on.
 */
class FAT32 {

private:

	// BIOS Parameter Block Info
	unsigned short bytesPerSector,
				   reservedSectorCount;

	unsigned char sectorsPerCluster,
				  numFats;

	unsigned int totalSectors, 
				 sectorsPerFAT, 
				 rootCluster,
				 firstDataSector,
				 fatLocation;

	fstream & fatImage;

	vector<unsigned int> freeClusters;
	vector<DirectoryEntry> currentDirectory;

	template<class T>
	void getPrimitive( const streampos & pos, T & out );
	unsigned int getFirstSectorOfCluster( unsigned int n );
	bool isFreeCluster( unsigned int entry );
	unsigned int getFATEntry( unsigned int n );
	void changeDirectory( unsigned int cluster ); 
	unsigned char * getFileContents( unsigned int initialCluster, unsigned int & size );
	void copy( LongDirectoryEntry & longEntry, unsigned char * buffer );
	void copy( ShortDirectoryEntry & shortEntry, unsigned char * buffer ); 
	void appendLongName( wstring & current, unsigned short * name, unsigned int size );
	void convertShortName( wstring & current, unsigned char * name );

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