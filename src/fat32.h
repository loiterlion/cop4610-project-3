#pragma once

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <stdint.h>
#include <string>
#include <sys/time.h>
#include <vector>

#include <iomanip>

using namespace std;

namespace FAT_FS {

// FAT Markers/Constants
const uint8_t ATTR_READ_ONLY = 0x01,
			  ATTR_HIDDEN = 0x02,
			  ATTR_SYSTEM = 0x04,
			  ATTR_VOLUME_ID = 0x08,
			  ATTR_DIRECTORY = 0x10,
			  ATTR_ARCHIVE = 0x20,
			  ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID,
			  ATTR_LONG_NAME_MASK = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID | ATTR_DIRECTORY | ATTR_ARCHIVE,
			  LAST_LONG_ENTRY = 0x40,
			  SHORT_NAME_SPACE_PAD = 0x20,
			  DIR_FREE_ENTRY = 0xE5,
			  DIR_LAST_FREE_ENTRY = 0x00,
			  LONG_NAME_LENGTH = 0x0D;

const uint16_t LONG_NAME_TRAIL = 0xFFFF,
			   LONG_NAME_NULL = 0x0000;

const uint32_t FAT_ENTRY_SIZE = 0x04,
			   FAT_ENTRY_MASK = 0x0FFFFFFF,
			   FREE_CLUSTER = 0x00000000,
			   EOC = 0x0FFFFFF8,
			   DIR_ENTRY_SIZE = 0x20,
			   DIR_Attr = 0x0B,
			   DIR_Name_LENGTH = 0x0B,
			   DIR_MAX_SIZE = 0x200000,
			   FILE_MAX_SIZE = 0xFFFFFFFF;  	

// Open Mode Constants
const uint8_t READ = 0x01,
			  WRITE = 0x02,
			  READWRITE = READ|WRITE;

/**
 * FAT Data Structures
 */

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

typedef struct FSInfo {

	uint32_t leadSignature;
	uint32_t reserved1[120];
	uint32_t structSignature;
	uint32_t freeCount;
	uint32_t nextFree;
	uint32_t reserved2[3];
	uint32_t trailingSignature;

} __attribute__((packed)) FSInfo;

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
	uint32_t location;

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
	uint32_t location;

} __attribute__((packed)) LongDirectoryEntry;

typedef struct DirectoryEntry {

	string name;
	string fullPath;
	ShortDirectoryEntry shortEntry;
	deque<LongDirectoryEntry> longEntries;

} DirectoryEntry;

bool operator< ( const DirectoryEntry & left, const DirectoryEntry & right );

/**
 * FAT File System
 * Description: Representation of a FAT File System that can be operated on.
 */
class FAT32 {

private:

	BIOSParameterBlock bpb;
	FSInfo fsInfo;
	uint32_t bytesPerCluster,
			 firstDataSector,
			 fatLocation,
			 countOfClusters,
			 * fat,
			 currentDirectoryFirstCluster;

	fstream & fatImage;
	vector<string> currentPath;
	vector<uint32_t> freeClusters;
	vector<DirectoryEntry> currentDirectoryListing;
	map<DirectoryEntry, uint8_t> openFiles;
	
	void addFile( DirectoryEntry & entry );
	void appendLongName( string & current, uint16_t * name, uint32_t size ) const;
	inline uint32_t calculateDirectoryEntryLocation( uint32_t byte, vector<uint32_t> clusterChain ) const;
	inline uint8_t calculateChecksum( const uint8_t * shortName ) const;
	void convertLongNameSegment( uint16_t * nameInStruct, uint8_t length, uint8_t & charLeft, bool & nullStored, const string & name ) const;
	const string convertShortName( uint8_t * name ) const;
	bool directoryExists( const string & directoryName ) const;
	bool fileExists( const string & fileName ) const;
	bool findDirectory( const string & directoryName, uint32_t & index ) const;
	bool findEntry( const string & entryName, uint32_t & index ) const;
	bool findFile( const string & fileName, uint32_t & index ) const;
	inline uint32_t formCluster( const ShortDirectoryEntry & entry ) const;
	const string generateBasisName( const string & longName, bool & lossyConversion ) const;
	string generateNumericTail( string basisName ) const;
	vector<DirectoryEntry> getDirectoryListing( uint32_t cluster ) const;
	inline uint32_t getFATEntry( uint32_t n ) const;
	uint8_t * getFileContents( uint32_t initialCluster, vector<uint32_t> & clusterChain ) const;
	inline uint32_t getFirstDataSectorOfCluster( uint32_t n ) const;
	inline bool isDirectory( const DirectoryEntry & entry ) const;
	inline bool isFile( const DirectoryEntry & entry ) const;
	inline bool isFreeCluster( uint32_t value ) const;
	inline bool isValidEntryName( const string & entryName ) const;
	inline uint8_t isValidOpenMode( const string & openMode ) const;
	bool makeFile( const string & fileName, DirectoryEntry & entry, bool directory ) const;
	inline const string modeToString( const uint8_t & mode ) const;
	void removeEntry( DirectoryEntry & entry, uint32_t index, bool safe );
	uint8_t * resize( uint32_t amount, vector<uint32_t> & clusterChain );
	inline void setClusterValue( uint32_t n, uint32_t newValue );
	inline bool shortNameExists( string name ) const;
	void writeFileContents( const uint8_t * contents, const vector<uint32_t> & clusterChain );
	void zeroOutFileContents( uint32_t initialCluster ) const;

public:

	FAT32( fstream & fatImage );
	~FAT32();

	const string getCurrentPath() const;

	/**
	 * Commands
	 */
	 
	void fsinfo() const;
	void open( const string & fileName, const string & openMode  );
	void close( const string & fileName );
	void create( const string & fileName );
	void read( const string & fileName, uint32_t startPos, uint32_t numBytes );
	void write( const string & fileName, uint32_t startPos, const string & quotedData );
	void rm( const string & fileName, bool safe = false );
	void cd( const string & directory );
	void ls( const string & directory ) const;
	void mkdir( const string & directoryName );
	void rmdir( const string & directoryName );
	void size( const string & fileName ) const;

};

}
