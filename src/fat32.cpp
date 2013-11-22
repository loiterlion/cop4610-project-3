#include "fat32.h"

using namespace FAT_FS;

/**
 * FAT32 Constructor
 */
FAT32::FAT32( fstream & fatImage ) : fatImage( fatImage ) {

	// Read BIOS Parameter Block
	this->fatImage.seekg( 0 );
	this->fatImage.read( reinterpret_cast<char *>( &this->bpb ), sizeof( this->bpb ) );

	this->firstDataSector = this->bpb.reservedSectorCount + ( this->bpb.numFATs * this->bpb.FATSz32 );

	this->fatLocation = this->bpb.reservedSectorCount * this->bpb.bytesPerSector;

	// Read in fat
	uint32_t fatEntries = ( this->bpb.FATSz32 * this->bpb.bytesPerSector ) / FATEntrySize;
	this->fat = new uint32_t[fatEntries];
	this->fatImage.seekg( this->fatLocation );
	this->fatImage.read( reinterpret_cast<char *>( this->fat ), this->bpb.FATSz32 * this->bpb.bytesPerSector );

	// Find free blocks
	uint32_t entry;
	for ( uint32_t i = 0; i < fatEntries; i++ )
		if ( isFreeCluster( ( entry = getFATEntry( i ) ) ) )
			this->freeClusters.push_back( i );

	// Position ourselves in root directory
	this->currentDirectoryListing = getDirectoryListing( this->bpb.rootCluster );
}

FAT32::~FAT32() {

	// Cleanup
	delete[] this->fat;
}

const vector<string> & FAT32::getCurrentPath() const {

	return this->currentPath;
}

vector<DirectoryEntry> FAT32::getDirectoryListing( uint32_t cluster ) const {

	uint32_t size = 0;
	uint8_t * contents = getFileContents( cluster, size );
	deque<LongDirectoryEntry> longEntries;
	vector<DirectoryEntry> result;

	for ( uint32_t i = 0; i < size; i += 32 ) {

		uint8_t ordinal = contents[i];
		uint8_t attribute =  contents[i+11];

		// Check if not free entry
		if ( ordinal != 0xE5 ) {

			// Rest of entries ahead of this are free
			if ( ordinal == 0x00 )
				break;

			if ( ( attribute & ATTR_LONG_NAME_MASK ) == ATTR_LONG_NAME ) {

				LongDirectoryEntry tempLongEntry;
				memcpy( &tempLongEntry, contents+i, sizeof( LongDirectoryEntry ) );
				longEntries.push_front( tempLongEntry );

			} else {

				string name = "";
				uint8_t attr = attribute & ( ATTR_DIRECTORY | ATTR_VOLUME_ID );

				ShortDirectoryEntry tempShortEntry;
				memcpy( &tempShortEntry, contents+i, sizeof( ShortDirectoryEntry ) );
				convertShortName( name, tempShortEntry.name );

				if ( !longEntries.empty() ) {

					name = "";

					for ( uint32_t i = 0; i < longEntries.size(); i++ ) {

						appendLongName( name, longEntries[i].name1, sizeof( longEntries[i].name1 )/2 );
						appendLongName( name, longEntries[i].name2, sizeof( longEntries[i].name2 )/2 );
						appendLongName( name, longEntries[i].name3, sizeof( longEntries[i].name3 )/2 );
					}
				}

				if ( attr == 0x00 || attr == ATTR_DIRECTORY || attr == ATTR_VOLUME_ID ) {

					// Add new DirectoryEntry
					DirectoryEntry tempDirectoryEntry;
					tempDirectoryEntry.name = name;
					tempDirectoryEntry.shortEntry = tempShortEntry;
					result.push_back( tempDirectoryEntry );

				} else {

					// Invalid entry, ignore
				}

				longEntries.clear();
			}
		}
	}

	delete[] contents;

	return result;
}

void FAT32::appendLongName( string & current, uint16_t * name, uint32_t size ) const {

	char temp;

	for ( uint32_t i = 0; i < size; i++ ) {

		if ( name[i] == 0xFFFF || name[i] == 0x0000 )
			break;

		else {

			temp = *(name + i);
			current += temp;
		}
	}
}

void FAT32::convertShortName( string & current, uint8_t * name ) const {

	char temp;
	bool trailFound = false;
	bool connectionComplete = false;

	for ( uint32_t i = 0; i < 11; i++ ) {

		if ( name[i] == 0x20 ) {

			trailFound = true;
			continue;
		}

		else {

			if ( !connectionComplete && trailFound ) {

				connectionComplete = true;
				current += '.';
			}

			temp = *(name + i);
			current += temp;
		}
	}
}

uint8_t * FAT32::getFileContents( uint32_t initialCluster, uint32_t & size ) const {

	vector<uint32_t> clusterChain;

	uint32_t nextCluster = initialCluster;

	// Build list of clusters for this file
	do {

		clusterChain.push_back( nextCluster );

	} while ( ( nextCluster = getFATEntry( nextCluster ) ) < EOCMarker  );

	size = clusterChain.size() * ( this->bpb.sectorsPerCluster * this->bpb.bytesPerSector );
	uint8_t * data = new uint8_t[ size ];
	uint8_t * temp = data;

	for ( uint32_t i = 0; i < clusterChain.size(); i++ ) {

		this->fatImage.seekg( this->getFirstSectorOfCluster( clusterChain[i] ) * this->bpb.bytesPerSector  );

		for ( uint32_t j = 0; j < this->bpb.sectorsPerCluster ; j++ ) {

			this->fatImage.read( reinterpret_cast<char *>( temp ), this->bpb.bytesPerSector  );
			temp += this->bpb.bytesPerSector ;
		}
	}

	return data;
}

uint32_t FAT32::getFirstSectorOfCluster( uint32_t n ) const {

	return ( ( n - 2 ) * this->bpb.sectorsPerCluster ) + this->
	firstDataSector;
}

bool FAT32::isFreeCluster( uint32_t entry ) const {

	return ( entry == FreeCluster );
}

inline uint32_t FAT32::getFATEntry( uint32_t n ) const {

	return this->fat[n] & FATEntryMask;
}

inline bool FAT32::isDirectory( const DirectoryEntry & entry ) const {

	return ( entry.shortEntry.attributes & ( ATTR_DIRECTORY | ATTR_VOLUME_ID ) ) == ATTR_DIRECTORY;
}

inline uint32_t FAT32::formCluster( const ShortDirectoryEntry & entry ) const {

	uint32_t result = 0;
	result |= entry.firstClusterLO;
	result |= entry.firstClusterHI << 16;

	return result;
}

int32_t FAT32::findDirectory( const string & directory, uint32_t & index ) const {

	// Check if directory user want is in current directory
	for ( uint32_t i = 0; i < currentDirectoryListing.size(); i++ )
		if ( currentDirectoryListing[i].name.compare( directory ) == 0 ) {

			if ( isDirectory( currentDirectoryListing[i] ) )  {

				index = i;
				return 0;
			}

			else 
				// File not directory
				return -2;
			
		}

	// Directory not found
	return -1;
}

/**
 * Commands
 */

/**
 * FS Info
 * Description: Prints out info for the loaded FAT32 FS.
 */
void FAT32::fsinfo() const {

	// + used to promote type to a printable number
	cout << "Bytes per sector: " << this->bpb.bytesPerSector
		 << "\nSectors per cluster: " << +this->bpb.sectorsPerCluster
		 << "\nTotal sectors: " << this->bpb.totalSectors32
		 << "\nNumber of FATs: " << +this->bpb.numFATs
		 << "\nSectors per FAT: " << this->bpb.FATSz32
		 << "\nNumber of free sectors: " << this->freeClusters.size() * this->bpb.sectorsPerCluster
		 << endl;
}

void FAT32::open() {

	cout << "error: unimplmented" << endl;
}

void FAT32::close() {

	cout << "error: unimplmented" << endl;
}

void FAT32::create() {

	cout << "error: unimplmented" << endl;
}

void FAT32::read() {

	cout << "error: unimplmented" << endl;
}

void FAT32::write() {

	cout << "error: unimplmented" << endl;
}

void FAT32::rm() {

	cout << "error: unimplmented" << endl;
}

void FAT32::cd( const string & directory ) {

	uint32_t index;
	int32_t result;
	result = findDirectory( directory, index );

	if ( result == 0 ) {

		if ( directory.compare( ".." ) == 0 && formCluster( currentDirectoryListing[index].shortEntry ) == 0 ) {

			this->currentPath.clear();
			this->currentDirectoryListing = getDirectoryListing( this->bpb.rootCluster );

		} else {

			if ( directory.compare( ".." ) == 0 )
				this->currentPath.pop_back();

			else if ( directory.compare( "." ) != 0 )
				this->currentPath.push_back( directory );

			this->currentDirectoryListing = getDirectoryListing( formCluster( currentDirectoryListing[index].shortEntry ) );
		}		
	}

	else if ( result == -1 )
		cout << "error: cd: " << directory << " not found" << endl;

	else if ( result == -2 )
		cout << "error: cd: " << directory << " is not a directory" << endl;
}

void FAT32::ls( const string & directory ) const {

	vector<DirectoryEntry> listing = currentDirectoryListing;

	if ( !directory.empty() ) {

		uint32_t index;
		int32_t result;
		result = findDirectory( directory, index );

		if ( result == 0 )
			listing = getDirectoryListing( formCluster( currentDirectoryListing[index].shortEntry ) );

		else if ( result == -1 ) {

			cout << "error: ls: " << directory << " not found" << endl;
			return;
		}

		else if ( result == -2 ) {

			cout << "error: ls: " << directory << " is not a directory" << endl;
			return;
		}
	}

	for ( uint32_t i = 0; i < listing.size(); i++ ) {

		cout << listing[i].name << " ";
	}

	cout << endl;
}

void FAT32::mkdir() {

	cout << "error: unimplmented" << endl;
}

void FAT32::rmdir() {

	cout << "error: unimplmented" << endl;
}

void FAT32::size() {

	cout << "error: unimplmented" << endl;
}

void FAT32::srm() {

	cout << "error: unimplmented" << endl;
}
