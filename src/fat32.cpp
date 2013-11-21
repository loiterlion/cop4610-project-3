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

	// Find free blocks
	uint32_t entry;
	uint32_t range = ( this->bpb.FATSz32 * this->bpb.bytesPerSector ) / FATEntrySize;
	for ( uint32_t i = 0; i < range; i++ )
		if ( isFreeCluster( ( entry = getFATEntry( i ) ) ) )
			this->freeClusters.push_back( i );

	// Position ourselves in root directory
	changeDirectory( this->bpb.rootCluster );
	this->currentDirectory = L"/";
}

void FAT32::changeDirectory( uint32_t cluster ) {

	uint32_t size = 0;
	uint8_t * contents = getFileContents( cluster, size );
	deque<LongDirectoryEntry> longEntries;

	// Not concerned with current directory anymore
	currentDirectoryListing.clear();

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

				wstring name = L"\0";
				uint8_t attr = attribute & ( ATTR_DIRECTORY | ATTR_VOLUME_ID );

				ShortDirectoryEntry tempShortEntry;
				memcpy( &tempShortEntry, contents+i, sizeof( ShortDirectoryEntry ) );
				convertShortName( name, tempShortEntry.name );

				if ( !longEntries.empty() ) {

					name = L"\0";

					for ( uint32_t i = 0; i < longEntries.size(); i++ ) {

						appendLongName( name, longEntries[i].name1, sizeof( longEntries[i].name1 ) );
						appendLongName( name, longEntries[i].name2, sizeof( longEntries[i].name2 ) );
						appendLongName( name, longEntries[i].name3, sizeof( longEntries[i].name3 ) );
					}

				}

				if ( attr == 0x00 || attr == ATTR_DIRECTORY || attr == ATTR_VOLUME_ID ) {

					// Add new DirectoryEntry
					DirectoryEntry tempDirectoryEntry;
					tempDirectoryEntry.name = name;
					memcpy( &tempDirectoryEntry + sizeof( tempDirectoryEntry.name ),
						&tempShortEntry + sizeof( tempShortEntry.name ), sizeof( ShortDirectoryEntry ) );
					currentDirectoryListing.push_back( tempDirectoryEntry );

				} else {

					// Invalid entry, ignore
				}

				longEntries.clear();
			}
		}
	}

	delete[] contents;
}

void FAT32::appendLongName( wstring & current, uint16_t * name, uint32_t size ) {

	wchar_t temp = L'\0';

	for ( uint32_t i = 0; i < size; i++ ) {

		if ( name[i] == 0xFFFF || name[i] == 0x0000 )
			break;

		else {

			memcpy( &temp, name + i, sizeof( uint16_t ) );
			current += temp;
		}
	}
}

void FAT32::convertShortName( wstring & current, uint8_t * name ) {

	wchar_t temp = L'\0';
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
				current += L'.';
			}

			memcpy( &temp, name + i, 1 );
			current += temp;
		}
	}
}

uint8_t * FAT32::getFileContents( uint32_t initialCluster, uint32_t & size ) {

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

uint32_t FAT32::getFirstSectorOfCluster( uint32_t n ) {

	return ( ( n - 2 ) * this->bpb.sectorsPerCluster ) + this->
	firstDataSector;
}

bool FAT32::isFreeCluster( uint32_t entry ) {

	return ( entry == FreeCluster );
}

uint32_t FAT32::getFATEntry( uint32_t n ) {

	uint32_t result = 0;

	this->fatImage.seekg( this->fatLocation + ( n * FATEntrySize ) );
	this->fatImage.read( reinterpret_cast<char *>( &result ), FATEntrySize );

	return result & FATEntryMask;
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

void FAT32::cd() {

	cout << "error: unimplmented" << endl;
}

void FAT32::ls( const string & directory ) const {

	if ( !wcout )
		cout << "WCOUT WENT BAD!" << endl;

	vector<DirectoryEntry> listing;

	if ( !directory.empty() ) {

	} else {

		listing = currentDirectoryListing;
	}

	for ( uint32_t i = 0; i < listing.size(); i++ ) {

		wcout << listing[i].name << " ";
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
