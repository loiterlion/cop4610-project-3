#include "fat32.h"

using namespace FAT_FS;

/**
 * FAT32 Constructor
 */
FAT32::FAT32( fstream & fatImage ) : fatImage( fatImage ) {

	// Read BIOS Parameter Block
	this->getPrimitive( BPB_BytsPerSec, this->bytesPerSector );
	this->getPrimitive( BPB_SecPerClus, this->sectorsPerCluster );
	this->getPrimitive( BPB_TotSec32, this->totalSectors );
	this->getPrimitive( BPB_NumFATs, this->numFats );
	this->getPrimitive( BPB_FATSz32, this->sectorsPerFAT );
	this->getPrimitive( BPB_RsvdSecCnt, this->reservedSectorCount );
	this->getPrimitive( BPB_RootClus, this->rootCluster );

	this->firstDataSector = this->reservedSectorCount + ( this->numFats * this->sectorsPerFAT );

	this->fatLocation = this->reservedSectorCount * this->bytesPerSector;

	// Find free blocks
	unsigned int entry;
	unsigned int range = ( this->sectorsPerFAT * this->bytesPerSector ) / FATEntrySize;
	for ( unsigned int i = 0; i < range; i++ )
		if ( isFreeCluster( ( entry = getFATEntry( i ) ) ) )
			this->freeClusters.push_back( i );

	// Position ourselves in root directory
	changeDirectory( this->rootCluster );
}

void FAT32::changeDirectory( unsigned int cluster ) {

	unsigned int size = 0;
	unsigned char * contents = getFileContents( cluster, size );
	deque<LongDirectoryEntry> longEntries;

	// Not concerned with current directory anymore
	currentDirectory.clear();

	for ( unsigned int i = 0; i < size; i += 32 ) {

		unsigned char ordinal = contents[i];
		unsigned char attribute =  contents[i+11];

		// Check if not free entry
		if ( ordinal != 0xE5 ) {

			// Rest of entries ahead of this are free
			if ( ordinal == 0x00 )
				break;

			if ( ( attribute & ATTR_LONG_NAME_MASK ) == ATTR_LONG_NAME ) {

				LongDirectoryEntry tempLongEntry;
				copy( tempLongEntry, contents+i );
				longEntries.push_front( tempLongEntry );

			} else {

				wstring name = L"\0";
				unsigned char attr = attribute & ( ATTR_DIRECTORY | ATTR_VOLUME_ID );

				ShortDirectoryEntry shortEntry;
				copy( shortEntry, contents+i );
				convertShortName( name, shortEntry.name );

				if ( !longEntries.empty() ) {

					name = L"\0";

					for ( unsigned int i = 0; i < longEntries.size(); i++ ) {

						appendLongName( name, longEntries[i].name1, 5 );
						appendLongName( name, longEntries[i].name2, 6 );
						appendLongName( name, longEntries[i].name3, 2 );
					}

				}

				if ( attr == 0x00 || attr == ATTR_DIRECTORY || attr == ATTR_VOLUME_ID ) {

					currentDirectory.push_back( DirectoryEntry( name, shortEntry ) );

				} else {

					// Invalid entry, ignore
				}

				longEntries.clear();
			}
		}
	}

	delete[] contents;
}

void FAT32::appendLongName( wstring & current, unsigned short * name, unsigned int size ) {

	wchar_t temp = L'\0';

	for ( unsigned int i = 0; i < size; i++ ) {

		if ( name[i] == 0xFFFF || name[i] == 0x0000 )
			break;

		else {

			memcpy( &temp, name + i, sizeof( unsigned short ) );
			current += temp;
		}
	}
}

void FAT32::convertShortName( wstring & current, unsigned char * name ) {

	wchar_t temp = L'\0';
	bool trailFound = false;
	bool connectionComplete = false;

	for ( unsigned int i = 0; i < 11; i++ ) {

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

void FAT32::copy( LongDirectoryEntry & longEntry, unsigned char * buffer ) {

	memcpy( &longEntry.ordinal, buffer + LDIR_Ord, sizeof( longEntry.ordinal ) );  		
	memcpy( &longEntry.name1, buffer + LDIR_Name1, sizeof( longEntry.name1 ) );		
	memcpy( &longEntry.attributes, buffer + LDIR_Attr, sizeof( longEntry.attributes ) );		
	memcpy( &longEntry.type, buffer + LDIR_Type, sizeof( longEntry.type ) );				
	memcpy( &longEntry.checksum, buffer + LDIR_Chksum, sizeof( longEntry.checksum ) );			
	memcpy( &longEntry.name2, buffer + LDIR_Name2, sizeof( longEntry.name2 ) );		
	memcpy( &longEntry.firstClusterLO, buffer + LDIR_FstClusLO, sizeof( longEntry.firstClusterLO ) );	
	memcpy( &longEntry.name3, buffer + LDIR_Name3, sizeof( longEntry.name3 ) );		
}

void FAT32::copy( ShortDirectoryEntry & shortEntry, unsigned char * buffer ) {

	memcpy( &shortEntry.name, buffer + DIR_Name, sizeof( shortEntry.name ) );			
	memcpy( &shortEntry.attributes, buffer + DIR_Attr, sizeof( shortEntry.attributes ) );	
	memcpy( &shortEntry.NTReserved, buffer + DIR_NTRes, sizeof( shortEntry.NTReserved ) );	
	memcpy( &shortEntry.createdTimeTenth, buffer + DIR_CrtTimeTenth, sizeof( shortEntry.createdTimeTenth ) );
	memcpy( &shortEntry.createdTime, buffer + DIR_CrtTime, sizeof( shortEntry.createdTime ) );	
	memcpy( &shortEntry.createdDate, buffer + DIR_CrtDate, sizeof( shortEntry.createdDate ) );	
	memcpy( &shortEntry.lastAccessDate, buffer + DIR_LstAccDate, sizeof( shortEntry.lastAccessDate ) );
	memcpy( &shortEntry.firstClusterHI, buffer + DIR_FstClusHI, sizeof( shortEntry.firstClusterHI ) );
	memcpy( &shortEntry.writeTime, buffer + DIR_WrtTime, sizeof( shortEntry.writeTime ) );	
	memcpy( &shortEntry.writeDate, buffer + DIR_WrtDate, sizeof( shortEntry.writeDate ) );	
	memcpy( &shortEntry.firstClusterLO, buffer + DIR_FstClusLO, sizeof( shortEntry.firstClusterLO ) );
	memcpy( &shortEntry.fileSize, buffer + DIR_FileSize, sizeof( shortEntry.fileSize ) );			
}

unsigned char * FAT32::getFileContents( unsigned int initialCluster, unsigned int & size ) {

	vector<unsigned int> clusterChain;

	unsigned int nextCluster = initialCluster;

	// Build list of clusters for this file
	do {

		clusterChain.push_back( nextCluster );

	} while ( ( nextCluster = getFATEntry( nextCluster ) ) < EOCMarker  );

	size = clusterChain.size() * ( this->sectorsPerCluster * this->bytesPerSector );
	unsigned char * data = new unsigned char[ size ];
	unsigned char * temp = data;

	for ( unsigned int i = 0; i < clusterChain.size(); i++ ) {

		this->fatImage.seekg( this->getFirstSectorOfCluster( clusterChain[i] ) * this->bytesPerSector );

		for ( unsigned int j = 0; j < this->sectorsPerCluster; j++ ) {

			this->fatImage.read( reinterpret_cast<char *>( temp ), this->bytesPerSector );
			temp += this->bytesPerSector;
		}
	}

	return data;
}

/**
 * Get Primitive
 * Description: Gets a primitive from the image in its Big Endian form.
 *				Little Endian to Big Endian conversion is implicity done
 *				by reading forward in the file reversing the bytes.
 */
template<class T>
void FAT32::getPrimitive( const streampos & pos, T & out ) {

	this->fatImage.seekg( pos );
	this->fatImage.read( reinterpret_cast<char *>( &out ), sizeof( T ) );
}

unsigned int FAT32::getFirstSectorOfCluster( unsigned int n ) {

	return ( ( n - 2 ) * this->sectorsPerCluster ) + this->
	firstDataSector;
}

bool FAT32::isFreeCluster( unsigned int entry ) {

	return ( entry == FreeCluster );
}

unsigned int FAT32::getFATEntry( unsigned int n ) {

	unsigned int result = 0;

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
	cout << "Bytes per sector: " << this->bytesPerSector
		 << "\nSectors per cluster: " << +this->sectorsPerCluster
		 << "\nTotal sectors: " << this->totalSectors
		 << "\nNumber of FATs: " << +this->numFats
		 << "\nSectors per FAT: " << this->sectorsPerFAT
		 << "\nNumber of free sectors: " << this->freeClusters.size() * this->sectorsPerCluster
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

	vector<DirectoryEntry> listing;

	if ( !directory.empty() ) {

	} else {

		listing = currentDirectory;
	}

	for ( unsigned int i = 0; i < listing.size(); i++ ) {

		unsigned char attr = listing[i].attributes & ( ATTR_DIRECTORY | ATTR_VOLUME_ID );

		if ( attr == 0x00 ) {

			wcout << "Found file: " << listing[i].name << endl;
		}
		else if ( attr == ATTR_DIRECTORY ) {

			wcout << "Found directory: " << listing[i].name << endl;
		}
		else if ( attr == ATTR_VOLUME_ID ) {

			wcout << "Found volume label: " << listing[i].name << endl;
		}
	}
	
	cout << "error: unimplmented" << endl;
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
