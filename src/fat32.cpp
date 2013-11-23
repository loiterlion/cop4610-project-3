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

const string FAT32::getCurrentPath() const {

	string path = "/";

	for ( uint32_t i = 0; i < this->currentPath.size(); i++ )
		path += this->currentPath[i] + "/";

	return path;
}

vector<DirectoryEntry> FAT32::getDirectoryListing( uint32_t cluster ) const {

	uint32_t size = 0;
	uint8_t * contents = getFileContents( cluster, size );
	deque<LongDirectoryEntry> longEntries;
	vector<DirectoryEntry> result;

	for ( uint32_t i = 0; i < size; i += FATEntrySize ) {

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
					tempDirectoryEntry.fullPath = this->getCurrentPath() + name;
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

inline bool FAT32::isFile( const DirectoryEntry & entry ) const {

	return ( entry.shortEntry.attributes & ( ATTR_DIRECTORY | ATTR_VOLUME_ID ) ) == 0x00;
}

inline uint32_t FAT32::formCluster( const ShortDirectoryEntry & entry ) const {

	uint32_t result = 0;
	result |= entry.firstClusterLO;
	result |= entry.firstClusterHI << 16;

	return result;
}

bool FAT32::findDirectory( const string & directoryName, uint32_t & index ) const {

	if ( !isValidEntryName( directoryName ) ) {

		cout << "error: directory name may not contain /." << endl;
		return false;
	}

	// Check if directory user want is in current directory
	for ( uint32_t i = 0; i < currentDirectoryListing.size(); i++ )
		if ( currentDirectoryListing[i].name.compare( directoryName ) == 0 ) {

			if ( isDirectory( currentDirectoryListing[i] ) )  {

				index = i;
				return true;
			}

			else {

				cout << "error: " << directoryName << " is not a directory." << endl;
				return false;
			}
			
		}

	cout << "error: " << directoryName << " not found." << endl;
	return false;
}

bool FAT32::findFile( const string & fileName, uint32_t & index ) const {

	if ( !isValidEntryName( fileName ) ) {

		cout << "error: file name may not contain /." << endl;
		return false;
	}

	// Check if directory user want is in current directory
	for ( uint32_t i = 0; i < currentDirectoryListing.size(); i++ )
		if ( currentDirectoryListing[i].name.compare( fileName ) == 0 ) {

			if ( isFile( currentDirectoryListing[i] ) )  {

				index = i;
				return true;
			}

			else {

				cout << "error: " << fileName << " is not a file." << endl;
				return false;
			}
			
		}

	cout << "error: " << fileName << " not found." << endl;
	return false;
}

bool FAT32::findEntry( const string & entryName, uint32_t & index ) const {

	if ( !isValidEntryName( entryName ) ) {

		cout << "error: entry name may not contain /." << endl;
		return false;
	}

	// Check if directory user want is in current directory
	for ( uint32_t i = 0; i < currentDirectoryListing.size(); i++ )
		if ( currentDirectoryListing[i].name.compare( entryName ) == 0 ) {

			index = i;
			return true;
		}

	cout << "error: " << entryName << " not found." << endl;
	return false;
}

inline bool FAT32::isValidEntryName( const string & entryName ) const {

	return entryName.find( "/" ) == string::npos;	
}

inline uint8_t FAT32::isValidOpenMode( const string & openMode ) const {

	uint8_t result = 0;

	if ( openMode.compare( "r" ) == 0 )
		result = READ;

	else if ( openMode.compare( "w" ) == 0 )
		result = WRITE;

	else if ( openMode.compare( "rw" ) == 0 )
		result = READWRITE;

	return result;
}

inline const string FAT32::modeToString( const uint8_t & mode ) const {

	switch ( mode ) {

		case READ: return "reading";
		case WRITE: return "writing";
		case READWRITE: return "reading and writing";
	}

	return "invalid mode";
}

bool FAT_FS::operator< ( const DirectoryEntry & left, const DirectoryEntry & right ) {

	return operator<( left.fullPath, right.fullPath );
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

void FAT32::open( const string & fileName, const string & openMode ) {

	uint8_t mode;

	if ( ( mode = isValidOpenMode( openMode ) ) == 0 ) {

		cout << "error: mode must be either r, w, rw." << endl;
		return;
	}

	uint32_t index;

	if ( findFile( fileName, index ) ) {

		if ( this->openFiles.find( this->currentDirectoryListing[index] ) == this->openFiles.end() ) {

			this->openFiles[ this->currentDirectoryListing[index] ] = mode;
			cout << fileName << " has been opened with " << this->modeToString( mode ) << " permission." << endl;
		}

		else {

			cout << "error: " << fileName << " already open." << endl;
			return;
		}
	}
}

void FAT32::close( const string & fileName ) {

	uint32_t index;

	if ( findFile( fileName, index ) ) {

		if ( this->openFiles.find( this->currentDirectoryListing[index] ) != this->openFiles.end() ) {

			this->openFiles.erase( this->currentDirectoryListing[index] );
			cout << fileName << " is now closed." << endl;
		}

		else {

			cout << "error: " << fileName << " not found in the open file table." << endl;
			return;
		}
	}
}

void FAT32::create() {

	cout << "error: unimplmented." << endl;
}

void FAT32::read( const string & fileName, uint32_t startPos, uint32_t numBytes ) {

	uint32_t index;

	if ( findFile( fileName, index ) ) {

		DirectoryEntry file = this->currentDirectoryListing[index];

		if ( this->openFiles.find( file ) != this->openFiles.end() ) {

			if ( this->openFiles[file] == READ
				|| this->openFiles[file] == READWRITE ) {

				cout << "going to read the file that is of size " << file.shortEntry.fileSize << endl;

				// uint32_t size = 0;
				// uint8_t * contents = getFileContents( cluster, size );
			
			} else {

				cout << "error: " << fileName << " not open for reading." << endl;
				return;
			} 
		}

		else {

			cout << "error: " << fileName << " not found in the open file table." << endl;
			return;
		}
	}

	cout << "error: unimplmented." << endl;
}

void FAT32::write() {

	cout << "error: unimplmented." << endl;
}

void FAT32::rm() {

	cout << "error: unimplmented." << endl;
}

void FAT32::cd( const string & directoryName ) {

	uint32_t index;

	if ( findDirectory( directoryName, index ) ) {

		if ( directoryName.compare( ".." ) == 0 && formCluster( currentDirectoryListing[index].shortEntry ) == 0 ) {

			this->currentPath.clear();
			this->currentDirectoryListing = getDirectoryListing( this->bpb.rootCluster );

		} else {

			if ( directoryName.compare( ".." ) == 0 )
				this->currentPath.pop_back();

			else if ( directoryName.compare( "." ) != 0 )
				this->currentPath.push_back( directoryName );

			this->currentDirectoryListing = getDirectoryListing( formCluster( currentDirectoryListing[index].shortEntry ) );
		}		
	}
}

void FAT32::ls( const string & directoryName ) const {

	vector<DirectoryEntry> listing = currentDirectoryListing;

	if ( !directoryName.empty() ) {

		uint32_t index;

		if ( findDirectory( directoryName, index ) )
			listing = getDirectoryListing( formCluster( currentDirectoryListing[index].shortEntry ) );

		else
			return;
	}

	for ( uint32_t i = 0; i < listing.size(); i++ )
		cout << listing[i].name << " ";

	cout << endl;
}

void FAT32::mkdir() {

	cout << "error: unimplmented." << endl;
}

void FAT32::rmdir() {

	cout << "error: unimplmented." << endl;
}

void FAT32::size( const string & entryName ) const {

	uint32_t index;

	if ( findEntry( entryName, index ) ) {

		uint32_t totalSize = 0;
		DirectoryEntry entry = this->currentDirectoryListing[index];

		if ( isFile( entry ) )
			totalSize = entry.shortEntry.fileSize;

		else if ( isDirectory( entry ) ) {

			uint32_t size = 0;
			uint8_t * contents = getFileContents( formCluster( entry.shortEntry ), size );

			for ( uint32_t i = 0; i < size; i += FATEntrySize ) {

				uint8_t ordinal = contents[i];

				// Check if not free entry
				if ( ordinal != 0xE5 ) {

					// Rest of entries ahead of this are free
					if ( ordinal == 0x00 )
						break;

					totalSize += FATEntrySize;
				}
			}

			delete[] contents;
		}

		// Volume ID
		else
			totalSize = 0;

		cout << totalSize << " bytes." << endl;
	}

	cout << "error: waiting on comfirmation from daniel about sizing directory." << endl;
}

void FAT32::srm() {

	cout << "error: unimplmented." << endl;
}
