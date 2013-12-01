#include "fat32.h"

using namespace FAT_FS;

/**
 * FAT32 Public Methods
 */

/**
 * FAT32 Constructor
 * Description: Initializes a FAT32 object reading in file system info as
 *				well as finding currently free clusters.
 */
FAT32::FAT32( fstream & fatImage ) : fatImage( fatImage ) {

	// Read BIOS Parameter Block
	this->fatImage.seekg( 0 );
	this->fatImage.read( reinterpret_cast<char *>( &this->bpb ), sizeof( this->bpb ) );

	// Read FSInfo
	this->fatImage.seekg( this->bpb.FSInfo * this->bpb.bytesPerSector );
	this->fatImage.read( reinterpret_cast<char *>( &this->fsInfo ), sizeof( this->fsInfo ) );

	this->firstDataSector = this->bpb.reservedSectorCount + ( this->bpb.numFATs * this->bpb.FATSz32 );
	this->fatLocation = this->bpb.reservedSectorCount * this->bpb.bytesPerSector;
	this->bytesPerCluster = this->bpb.sectorsPerCluster * this->bpb.bytesPerSector;

	// Read in fat
	// Note: The extra 2 entries allocated are for the reserved clusters which the countOfClusters formula 
	// 		 doesn't account for
	this->countOfClusters = ( ( this->bpb.totalSectors32 - this->firstDataSector ) / this->bpb.sectorsPerCluster );
	this->fat = new uint32_t[this->countOfClusters + 2];
	this->fatImage.seekg( this->fatLocation );
	this->fatImage.read( reinterpret_cast<char *>( this->fat ), ( this->countOfClusters + 2 ) *  FAT_ENTRY_SIZE );

	// Find free clusters
	// Note: We ignore the 2 reserved clusters and therefore also check the last 2
	uint32_t entry;
	uint32_t range = this->countOfClusters + 2;
	for ( uint32_t i = 2; i < range; i++ )
		if ( isFreeCluster( ( entry = getFATEntry( i ) ) ) )
			this->freeClusters.push_back( i );

	// Position ourselves in root directory
	this->currentDirectoryFirstCluster = this->bpb.rootCluster;
	this->currentDirectoryListing = getDirectoryListing( this->currentDirectoryFirstCluster );
}

/**
 * FAT32 Destructor
 */
FAT32::~FAT32() {

	// Cleanup
	delete[] this->fat;
}

/**
 * Get Current Path
 * Description: Builds and returns a / separated path to the
 *				current directory.
 */
const string FAT32::getCurrentPath() const {

	string path = "/";

	for ( uint32_t i = 0; i < this->currentPath.size(); i++ )
		path += this->currentPath[i] + "/";

	return path;
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
		 << "\n";
}

/**
 * Open File
 * Description: Attempts to open a file in the currrent directory 
 *				with r, w, or rw permissions and places it in
 *				the open file table.
 */
void FAT32::open( const string & fileName, const string & openMode ) {

	uint8_t mode;

	// Validate openMode
	if ( ( mode = isValidOpenMode( openMode ) ) == 0 ) {

		cout << "error: mode must be either r, w, rw.\n";
		return;
	}

	uint32_t index;

	// Try and find file
	if ( findFile( fileName, index ) ) {

		// Attempt to add file to open file table
		if ( this->openFiles.find( this->currentDirectoryListing[index] ) == this->openFiles.end() ) {

			this->openFiles[ this->currentDirectoryListing[index] ] = mode;
			cout << fileName << " has been opened with " << this->modeToString( mode ) << " permission.\n";
		}

		// File is already open
		else {

			cout << "error: " << fileName << " already open.\n";
			return;
		}
	}
}

/**
 * Close File
 * Description: Attempts to close a file that's in the current directory
 *				and open file table.
 */
void FAT32::close( const string & fileName ) {

	uint32_t index;

	// Try and find file
	if ( findFile( fileName, index ) ) {

		// Attempt to remove file from open file table
		if ( this->openFiles.find( this->currentDirectoryListing[index] ) != this->openFiles.end() ) {

			this->openFiles.erase( this->currentDirectoryListing[index] );
			cout << fileName << " is now closed.\n";
		}

		// File isn't in the table
		else {

			cout << "error: " << fileName << " not found in the open file table.\n";
			return;
		}
	}
}

/**
 * Create File
 * Description: Attempts to create a file in the current directory.
 */
void FAT32::create( const string & fileName ) {

	if ( !fileExists( fileName ) ) {

		DirectoryEntry entry;

		if ( makeFile( fileName, entry, false  ) ) {

			addFile( entry );

			this->currentDirectoryListing = getDirectoryListing( this->currentDirectoryFirstCluster );
		}
	}
}

/**
 * Read File
 * Description: Attempts to read a file if it's in the open file table. Reads
 *				the file starting at startPos and reads up to numBytes.
 */
void FAT32::read( const string & fileName, uint32_t startPos, uint32_t numBytes ) {

	uint32_t index;

	// Try and find file
	if ( findFile( fileName, index ) ) {

		DirectoryEntry file = this->currentDirectoryListing[index];

		// Check if file is already open
		if ( this->openFiles.find( file ) != this->openFiles.end() ) {

			// Check permissions
			if ( this->openFiles[file] == READ || this->openFiles[file] == READWRITE ) {

				// Read file contents
				vector<uint32_t> clusterChain;
				uint8_t * contents = getFileContents( formCluster( file.shortEntry ), clusterChain );

				// Validate startPos against size
				if ( startPos >= file.shortEntry.fileSize )
					cout << "error: start_pos (" << startPos << ") greater than or equal to file size (" 
						<< file.shortEntry.fileSize << "). Note: start_pos is zero-based.\n";

				// Otherwise print file contents up to numBytes
				else
					for ( uint32_t i = 0; i < numBytes && startPos + i < file.shortEntry.fileSize; i++ )
						cout << contents[ startPos + i ];

				delete[] contents;
			
			} else {

				cout << "error: " << fileName << " not open for reading.\n";
				return;
			} 
		}

		else {

			cout << "error: " << fileName << " not found in the open file table.\n";
			return;
		}
	}
}

/**
 * Write to File
 * Description: Attempts to write quotedData to a given file name at a certain
 *				starting position. Resizes file if necessary.
 */
void FAT32::write( const string & fileName, uint32_t startPos, const string & quotedData ) {

	uint32_t index;

	// Try and find file
	if ( findFile( fileName, index ) ) {

		DirectoryEntry file = this->currentDirectoryListing[index];

		// Check if file is already open
		if ( this->openFiles.find( file ) != this->openFiles.end() ) {

			// Check permissions
			if ( this->openFiles[file] == WRITE || this->openFiles[file] == READWRITE ) {

				// Get current file contents
				vector<uint32_t> clusterChain;
				uint8_t * contents = getFileContents( formCluster( file.shortEntry ), clusterChain );

				uint32_t requiredSize = startPos + quotedData.length(),
					     currentSize = file.shortEntry.fileSize == 0 ? 0 : ( clusterChain.size() * this->bytesPerCluster );

			    // Check if we need to resize the file
				if ( requiredSize > currentSize ) {

					uint32_t clustersNeeded = ceil( static_cast<double>( requiredSize - currentSize ) / this->bytesPerCluster );

					// Check if we have enough free space left or if the file has reached its max size
					if ( this->freeClusters.size() < clustersNeeded 
							|| ( static_cast<uint64_t>( currentSize ) + ( clustersNeeded * this->bytesPerCluster ) ) > FILE_MAX_SIZE ) {

						cout << "Not enough space left to write to file.\n";
						return;
					}

					else 
						contents = resize( clustersNeeded, clusterChain );
				}

				// Need to update file info in case of crash
				file.shortEntry.firstClusterHI = ( clusterChain[0] >> 16 );
				file.shortEntry.firstClusterLO = ( clusterChain[0] & 0x0000FFFF );
				file.shortEntry.fileSize = requiredSize;
				file.shortEntry.attributes |= ATTR_ARCHIVE;
				this->fatImage.seekp( file.shortEntry.location );
				this->fatImage.write( reinterpret_cast<char *>( &file.shortEntry ), DIR_ENTRY_SIZE );
				this->fatImage.flush();

				// Also update our temporary listing
				currentDirectoryListing[index].shortEntry.fileSize = requiredSize;
				currentDirectoryListing[index].shortEntry.firstClusterHI = ( clusterChain[0] >> 16 );
				currentDirectoryListing[index].shortEntry.firstClusterLO = ( clusterChain[0] & 0x0000FFFF );

				// Write Data
				for ( uint32_t i = 0; i < quotedData.length(); i++ )
				 		contents[ startPos + i ] = quotedData[i];

				// Flush to disk
				writeFileContents( contents, clusterChain );
				this->fatImage.flush();

				delete[] contents;
			
			} else {

				cout << "error: " << fileName << " not open for writing.\n";
				return;
			} 
		}

		else {

			cout << "error: " << fileName << " not found in the open file table.\n";
			return;
		}
	}
}

/**
 * Remove File
 * Description: Attempts to remove a file (User Facing Function).
 */
void FAT32::rm( const string & fileName, bool safe ) {

	uint32_t index;

	// Try and find file
	if ( findFile( fileName, index ) ) {

		// Remove it from the open file table if it's there
		if ( this->openFiles.find( this->currentDirectoryListing[index] ) != this->openFiles.end() )
			this->openFiles.erase( this->currentDirectoryListing[index] );

		DirectoryEntry file = this->currentDirectoryListing[index];

		removeEntry( file, index, safe );
	}
}

/**
 * Change Directory
 * Description: Attempts to change to a directory within the current directory.
 */
void FAT32::cd( const string & directoryName ) {

	uint32_t index;

	// Try and find directory
	if ( findDirectory( directoryName, index ) ) {

		// Check special case of .. directory and root
		if ( directoryName.compare( ".." ) == 0 && formCluster( currentDirectoryListing[index].shortEntry ) == 0 ) {

			this->currentPath.clear();
			this->currentDirectoryFirstCluster = this->bpb.rootCluster;
			this->currentDirectoryListing = getDirectoryListing( this->bpb.rootCluster );

		} else {

			// .. means we are going up a directory
			if ( directoryName.compare( ".." ) == 0 )
				this->currentPath.pop_back();

			// Don't add . to currentPath
			else if ( directoryName.compare( "." ) != 0 )
				this->currentPath.push_back( directoryName );

			this->currentDirectoryFirstCluster = formCluster( currentDirectoryListing[index].shortEntry );
			this->currentDirectoryListing = getDirectoryListing( this->currentDirectoryFirstCluster );
		}		
	}
}

/**
 * List Files
 * Description: Lists all files in either the current directory or 
 *				given directory if it exists.
 */
void FAT32::ls( const string & directoryName ) const {

	vector<DirectoryEntry> listing = currentDirectoryListing;

	// Check if we should list files of a given directory
	if ( !directoryName.empty() ) {

		uint32_t index;

		// Try and find directory
		if ( findDirectory( directoryName, index ) )
			listing = getDirectoryListing( formCluster( currentDirectoryListing[index].shortEntry ) );

		// Directory not found
		else
			return;
	}

	// Print directory contents
	for ( uint32_t i = 0; i < listing.size(); i++ )
		cout << listing[i].name << " ";

	cout << "\n";
}

/**
 * Make Directory
 * Description: Creates a directory (like a file) and places
 *				. and .. entries in it.
 */
void FAT32::mkdir( const string & directoryName ) {

	// See if directory already exists
	if ( !directoryExists( directoryName ) ) {

		DirectoryEntry entry;

		if ( makeFile( directoryName, entry, true ) ) {

			// Add directory to current directory
			addFile( entry );

			// Refresh directory
			this->currentDirectoryListing = getDirectoryListing( this->currentDirectoryFirstCluster );

			// Get our newly added entry
			uint32_t index;
			findDirectory( entry.name, index );

			DirectoryEntry directory = this->currentDirectoryListing[index];

			vector<uint32_t> clusterChain;

			// Resize this directory to 1 cluster since it's new
			clusterChain.push_back( 0 );
			uint8_t * contents = resize( 1, clusterChain );	
			delete[] contents;

			// Need to update file info in case of crash
			directory.shortEntry.firstClusterHI = ( clusterChain[0] >> 16 );
			directory.shortEntry.firstClusterLO = ( clusterChain[0] & 0x0000FFFF );
			this->fatImage.seekp( directory.shortEntry.location );
			this->fatImage.write( reinterpret_cast<char *>( &directory.shortEntry ), DIR_ENTRY_SIZE );
			this->fatImage.flush();

			// Also update our temporary listing
			currentDirectoryListing[index].shortEntry.firstClusterHI = ( clusterChain[0] >> 16 );
			currentDirectoryListing[index].shortEntry.firstClusterLO = ( clusterChain[0] & 0x0000FFFF );

			// Temporarily change directories for adding dot and dotdot
			vector<DirectoryEntry> savedDirectoryListing = this->currentDirectoryListing;
			uint32_t savedCurrentDirectoryFirstCluster = this->currentDirectoryFirstCluster;

			this->currentDirectoryFirstCluster = formCluster( directory.shortEntry );
			this->currentDirectoryListing = getDirectoryListing( this->currentDirectoryFirstCluster );

			// Setup dot directory
			uint8_t dotName[11] = { '.', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
			DirectoryEntry dot;
			memcpy( dot.shortEntry.name, dotName, DIR_Name_LENGTH );
			dot.shortEntry.attributes = ATTR_DIRECTORY;
			dot.shortEntry.fileSize = 0;
			dot.shortEntry.createdTimeTenth = directory.shortEntry.createdTimeTenth;
			dot.shortEntry.createdTime = directory.shortEntry.createdTime;
			dot.shortEntry.createdDate = directory.shortEntry.createdDate;
			dot.shortEntry.lastAccessDate = directory.shortEntry.lastAccessDate;
			dot.shortEntry.writeTime = directory.shortEntry.writeTime;
			dot.shortEntry.writeDate = directory.shortEntry.writeDate;
			dot.shortEntry.firstClusterLO = directory.shortEntry.firstClusterLO;
			dot.shortEntry.firstClusterHI = directory.shortEntry.firstClusterHI;

			// Setup dotdot directory
			uint8_t dotdotName[11] = { '.', '.', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
			DirectoryEntry dotdot;
			memcpy( dotdot.shortEntry.name, dotdotName, DIR_Name_LENGTH );
			dotdot.shortEntry.attributes = ATTR_DIRECTORY;
			dotdot.shortEntry.fileSize = 0;
			dotdot.shortEntry.createdTimeTenth = directory.shortEntry.createdTimeTenth;
			dotdot.shortEntry.createdTime = directory.shortEntry.createdTime;
			dotdot.shortEntry.createdDate = directory.shortEntry.createdDate;
			dotdot.shortEntry.lastAccessDate = directory.shortEntry.lastAccessDate;
			dotdot.shortEntry.writeTime = directory.shortEntry.writeTime;
			dotdot.shortEntry.writeDate = directory.shortEntry.writeDate;

			// Root directory must always have cluster values of 0
			dotdot.shortEntry.firstClusterLO = savedCurrentDirectoryFirstCluster == this->bpb.rootCluster ?
													0 :
													savedCurrentDirectoryFirstCluster & 0x0000FFFF;

			dotdot.shortEntry.firstClusterHI = savedCurrentDirectoryFirstCluster == this->bpb.rootCluster ?
													0 :
													savedCurrentDirectoryFirstCluster >> 16;

			// Add files to directory
			addFile( dot );

			addFile( dotdot );

			// Restore old directory listing
			this->currentDirectoryFirstCluster = savedCurrentDirectoryFirstCluster;
			this->currentDirectoryListing = savedDirectoryListing;
		}
	}
}

/**
 * Remove Directory
 * Description: Atempts to remove an empty directory from the
 *				current directory.
 */
void FAT32::rmdir( const string & directoryName ) {

	// Don't let anyone remove . or .. manually
	if ( directoryName.compare(".") == 0 || directoryName.compare("..") == 0 ) {

		cout << "error: . and .. cannot be removed.\n";
		return;
	}

	uint32_t index;

	if ( findDirectory( directoryName, index ) ) {

		vector<DirectoryEntry> listing = getDirectoryListing( formCluster( this->currentDirectoryListing[index].shortEntry ) );

		// Check if directory is empty
		bool empty = true;
		for ( uint32_t i = 0; i < listing.size(); i++ )	{

			if ( listing[i].name.compare( "." ) != 0 && listing[i].name.compare( ".." ) != 0   ) {

				empty = false;
				break;
			}
		}

		if ( empty ) 
			removeEntry( this->currentDirectoryListing[index], index, false );

		else {

			cout << "error: directory not empty.\n";
			return;
		}
	}
}

/**
 * Size of File
 * Description: Attempts to print the size of the file or directory given. 
 */
void FAT32::size( const string & fileName ) const {

	uint32_t index;

	// Try and find entry
	if ( findFile( fileName, index ) )
		cout << this->currentDirectoryListing[index].shortEntry.fileSize << " bytes.\n";
}

/**
 * FAT32 Private Methods
 */

/**
 * Add File
 * Description: Adds file to current directory.
 */
void FAT32::addFile( DirectoryEntry & entry ) {

	// Read file contents
	vector<uint32_t> clusterChain;
	uint8_t * contents = getFileContents( this->currentDirectoryFirstCluster, clusterChain );

	// Look for enough free entries
	uint32_t size = clusterChain.size() * this->bytesPerCluster,
	         start = 0,
		     entriesNeeded = entry.longEntries.size() + 1;

	bool blockFound = false;

	// Parse contents
	uint32_t count = 0;
	for ( uint32_t i = 0; i < size; i += DIR_ENTRY_SIZE ) {

		uint8_t ordinal = contents[i];

		// Check if not free entry
		if ( ordinal == DIR_FREE_ENTRY || ordinal == DIR_LAST_FREE_ENTRY ) {

			count++;

			// Start tracking block
			if ( i > start && count == 1 ) 
				start = i;

			// Check if we have space
			if ( count == entriesNeeded ) {

				blockFound = true;
				break;
			}

		// Contiguous block broken
		} else if ( count > 0 ) {

			// Reset
			count = 0;
			blockFound = false;
		}
			
	}

	uint32_t currentPosition = start;

	// Check if we need to resize
	if ( !blockFound ) {

		uint32_t clustersNeeded;
		clustersNeeded = ceil( static_cast<double>( entriesNeeded ) / ( this->bytesPerCluster / DIR_ENTRY_SIZE ) );
		currentPosition = size;

		// Check if we have enough space in the file system
		if ( this->freeClusters.size() < clustersNeeded 
			|| ( size + ( clustersNeeded * this->bytesPerCluster ) ) > DIR_MAX_SIZE ) {

			cout << "Not enough space left to create file.\n";
			return;
		}

		// Otherwise resize
		else {

			delete[] contents;
			contents = resize( clustersNeeded, clusterChain );

			// Mark last contiguous free spot to newly allocated as free
			if ( start != 0 )
				for ( uint32_t i = start; i < size; i += DIR_ENTRY_SIZE )
					contents[i] = DIR_FREE_ENTRY;

			size = clusterChain.size() * this->bytesPerCluster;
		}
	}

	// Write new entries to contents

	// Write long entries
	for ( uint8_t i = 0; i < entry.longEntries.size(); i++ ) {

		memcpy( contents + currentPosition, &entry.longEntries[i], sizeof( entry.longEntries[i] ) - sizeof( uint32_t ) );
		currentPosition += DIR_ENTRY_SIZE;
	}

	// Write Short Entry
	memcpy( contents + currentPosition, &entry.shortEntry, sizeof( entry.shortEntry ) - sizeof( uint32_t ) );

	// Flush to disk
	writeFileContents( contents, clusterChain );
	this->fatImage.flush();

	delete[] contents;
}

/**
 * Append Long Name
 * Description: Appends a LongDirectoryEntry's name to a string currently
 *				being built to represent the full name.
 */
void FAT32::appendLongName( string & current, uint16_t * name, uint32_t size ) const {

	// Iterate through name
	for ( uint32_t i = 0; i < size; i++ ) {

		// Check if current character is padding space or null terminator
		if ( name[i] == LONG_NAME_TRAIL || name[i] == LONG_NAME_NULL )
			break;

		// Otherwise add it to our string being built
		else {

			char temp = *(name + i);
			current += temp;
		}
	}
}

/**
 * Calculate Checksum
 * Description: Uses the checksum algorithm from the specification
 *				to calculate a checksum on a shortname to be placed
 *				in Long Directory entries.
 */
inline uint8_t FAT32::calculateChecksum( const uint8_t * shortName ) const {

	uint8_t sum = 0;

	for ( uint8_t i = 0; i < DIR_Name_LENGTH; i++ )
		sum = ( ( sum & 1 ) ? 0x80 : 0 ) + ( sum >> 1 ) + shortName[i];

	return sum;
}

/**
 * Calculate Directory Entry Location
 * Description: Calculates exact byte location of a given directory entries relative byte
 *				to the directory with the cluster chain associated with it
 */
inline uint32_t FAT32::calculateDirectoryEntryLocation( uint32_t byte, vector<uint32_t> clusterChain ) const {

	return ( this->getFirstDataSectorOfCluster( 
				clusterChain[ byte / this->bytesPerCluster ] ) * this->bpb.bytesPerSector 
		   ) + ( byte % this->bytesPerCluster );
}

/**
 * Convert Long Name Segment
 * Description: Takes a piece of a given long name and sticks it 
 *				into one of a Long Directory's name fields.
 */
void FAT32::convertLongNameSegment( uint16_t * nameInStruct, uint8_t length, uint8_t & charLeft, bool & nullStored,
									 const string & name ) const {

	for ( uint8_t i = 0; i < length; i++ ) {

		if ( charLeft != 0 )
			nameInStruct[i] = name[ name.length() - charLeft-- ];

		else if ( !nullStored ) {

			nullStored = true;
			nameInStruct[i] = LONG_NAME_NULL;
		}

		else
			nameInStruct[i] = LONG_NAME_TRAIL;
	}

}

/**
 * Convert Short Name
 * Description: Converts a ShortEntry's name to a string. Also accounts
 *				for the implied '.'.
 */
const string FAT32::convertShortName( uint8_t * name ) const {

	string result = "";
	bool trailFound = false;
	bool connectionComplete = false;


	// Iterate through name
	for ( uint32_t i = 0; i < DIR_Name_LENGTH; i++ ) {

		// Check if we have encountered padding
		if ( name[i] == SHORT_NAME_SPACE_PAD ) {

			trailFound = true;
			continue;
		}

		// Otherwise keep appending to current
		else {

			// If we found a trail and we haven't already
			// added the implied '.', add it
			if ( !connectionComplete && trailFound ) {

				connectionComplete = true;
				result += '.';
			}

			char temp = *(name + i);
			result += temp;
		}
	}

	return result;
}

/**
 * File Exists
 * Description: Check if a file by the given name exists in the
 *				current directory.
 */
bool FAT32::fileExists( const string & fileName ) const {

	// Check if directoryName is valid
	if ( !isValidEntryName( fileName ) ) {

		cout << "error: file name may not contain /.\n";
		return true;
	}

	// Check if directory user want is in current directory
	for ( uint32_t i = 0; i < currentDirectoryListing.size(); i++ )
		if ( currentDirectoryListing[i].name.compare( fileName ) == 0 ) {

			cout << "error: file already exists.\n";
			return true;
		}

	return false;
}

/**
 * Directory Exists
 * Description: Check if a directory by the given name exists in the
 *				current directory.
 */
bool FAT32::directoryExists( const string & directoryName ) const {

	// Check if directoryName is valid
	if ( !isValidEntryName( directoryName ) ) {

		cout << "error: directory name may not contain /.\n";
		return true;
	}

	// Check if directory user want is in current directory
	for ( uint32_t i = 0; i < currentDirectoryListing.size(); i++ )
		if ( currentDirectoryListing[i].name.compare( directoryName ) == 0 ) {

			if ( currentDirectoryListing[i].shortEntry.attributes == ATTR_DIRECTORY )
				cout << "error: directory already exists.\n";

			else
				cout << "error: " << directoryName << " is a file.\n";

			return true;
		}

	return false;
}

/**
 * Find Directory
 * Description: Returns whether or not a directory is in the current directory and
 *				if it is, the passed index is set.
 */
bool FAT32::findDirectory( const string & directoryName, uint32_t & index ) const {

	// Check if directoryName is valid
	if ( !isValidEntryName( directoryName ) ) {

		cout << "error: directory name may not contain /.\n";
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

				cout << "error: " << directoryName << " is not a directory.\n";
				return false;
			}
			
		}

	cout << "error: " << directoryName << " not found.\n";
	return false;
}

/**
 * Find Entry
 * Description: Returns whether or not an entry is in the current directory and
 *				if it is, the passed index is set.
 */
bool FAT32::findEntry( const string & entryName, uint32_t & index ) const {

	// Check if entryName is valid
	if ( !isValidEntryName( entryName ) ) {

		cout << "error: entry name may not contain /.\n";
		return false;
	}

	// Check if directory user want is in current directory
	for ( uint32_t i = 0; i < currentDirectoryListing.size(); i++ )
		if ( currentDirectoryListing[i].name.compare( entryName ) == 0 ) {

			index = i;
			return true;
		}

	cout << "error: " << entryName << " not found.\n";
	return false;
}

/**
 * Find File
 * Description: Returns whether or not a file is in the current directory and
 *				if it is, the passed index is set.
 */
bool FAT32::findFile( const string & fileName, uint32_t & index ) const {

	// Check if fileName is valid
	if ( !isValidEntryName( fileName ) ) {

		cout << "error: file name may not contain /.\n";
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

				cout << "error: " << fileName << " is not a file.\n";
				return false;
			}
			
		}

	cout << "error: " << fileName << " not found.\n";
	return false;
}

/**
 * Form Cluster
 * Description: Concatenates the low and high order bits of a ShortDirectoryEntry
 *				to form the cluster number of that entry.
 */
inline uint32_t FAT32::formCluster( const ShortDirectoryEntry & entry ) const {

	uint32_t result = 0;
	result |= entry.firstClusterLO;
	result |= entry.firstClusterHI << 16;

	return result;
}

/**
 * Generate Basis Name
 * Description: Generates a basis-name from a long name. Will set if a 
 *				lossy conversion was applied.
 */
const string FAT32::generateBasisName( const string & longName, bool & lossyConversion ) const {

	string shortCopy;
	lossyConversion = false;

	for ( uint8_t i = 0; i < longName.length(); i++ ) {

		if ( longName[i] == '+' || longName[i] == ',' || longName[i] == ';' || longName[i] == '=' 
			|| longName[i] == '['  || longName[i] == ']' ) {

			lossyConversion = true;
			shortCopy += '_';
		}

		else if ( longName[i] == ' ' )
			continue;

		else if ( longName[i] == '.' && longName.find_last_of( "." ) != i )
			continue;

		else 
			shortCopy += toupper( longName[i] );
	}

    string basisName( DIR_Name_LENGTH, SHORT_NAME_SPACE_PAD );

    for ( uint8_t i = 0; i < 8 && i < shortCopy.length() && shortCopy[i] != '.'; i++ )
    	basisName[i] = shortCopy[i];

    size_t period = shortCopy.find_last_of( "." );
    if ( period != string::npos )
    	for( uint8_t i = 0; i < 3 && ( ( period + 1 ) + i ) < shortCopy.length(); i++ )
			basisName[ 8 + i ] = shortCopy[ ( period + 1 ) + i ];

	return basisName;
}

/**
 * Generate Numerical Tail
 * Description: Uses the numeric-tail algorithm to figure out
 *				what name a basis-name needs.
 */
string FAT32::generateNumericTail( string basisName ) const {

	// Need to only ever occupy up to the length of 999999
	char nBuffer[7] = {0};
	string primaryName = basisName.substr( 0, 8 );
	primaryName = primaryName.substr( 0, primaryName.find( " " ) );
	string extension = basisName.substr( 8, string::npos );
	string finalPrimaryName;

	for ( uint32_t start = 1; start <= 999999; start++ ) {

		// sprintf allows us to easily make a digit into a string
		sprintf( nBuffer, "%d", start );

		// Shorten primary name if necessary
		finalPrimaryName = primaryName.substr( 0, min( primaryName.length(), 8 - ( strlen( nBuffer ) + 1 ) ) ) + "~" + nBuffer;

		// Stop generating tails as soon as one works
		if ( !shortNameExists( finalPrimaryName + extension ) )
			break;
	}

	return finalPrimaryName + extension;
}

/**
 * Get Directory Listing
 * Description: Returns a list of DirectoryEntries for a given cluster.
 * Expects: cluster to be a valid data cluster.
 */
vector<DirectoryEntry> FAT32::getDirectoryListing( uint32_t cluster ) const {

	vector<uint32_t> clusterChain;
	uint8_t * contents = getFileContents( cluster, clusterChain );
	uint32_t size = clusterChain.size() * this->bytesPerCluster;
	deque<LongDirectoryEntry> longEntries;
	vector<DirectoryEntry> result;
	// Parse contents
	for ( uint32_t i = 0; i < size; i += DIR_ENTRY_SIZE ) {

		uint8_t ordinal = contents[i];
		uint8_t attribute =  contents[i + DIR_Attr];

		// Check if not free entry
		if ( ordinal != DIR_FREE_ENTRY ) {

			// Rest of entries ahead of this are free
			if ( ordinal == DIR_LAST_FREE_ENTRY )
				break;

			// Check if this entry is a long directory
			if ( ( attribute & ATTR_LONG_NAME_MASK ) == ATTR_LONG_NAME ) {

				LongDirectoryEntry tempLongEntry;
				memcpy( &tempLongEntry, contents+i, sizeof( LongDirectoryEntry ) - sizeof( uint32_t ) );

				// Store this location in case we ever need to remove this entry
				tempLongEntry.location = calculateDirectoryEntryLocation( i, clusterChain );

				longEntries.push_front( tempLongEntry );

			// Otherwise it's a file
			} else {

				string name = "";
				uint8_t attr = attribute & ( ATTR_DIRECTORY | ATTR_VOLUME_ID );

				ShortDirectoryEntry tempShortEntry;
				memcpy( &tempShortEntry, contents+i, sizeof( ShortDirectoryEntry ) - sizeof( uint32_t ) );
				tempShortEntry.location = calculateDirectoryEntryLocation( i, clusterChain );

				// Build long entry name if there were any
				if ( !longEntries.empty() )
					for ( uint32_t i = 0; i < longEntries.size(); i++ ) {

						appendLongName( name, longEntries[i].name1, sizeof( longEntries[i].name1 )/2 );
						appendLongName( name, longEntries[i].name2, sizeof( longEntries[i].name2 )/2 );
						appendLongName( name, longEntries[i].name3, sizeof( longEntries[i].name3 )/2 );
					}

				else
					name = convertShortName( tempShortEntry.name );

				// Validate attribute
				if ( attr == 0x00 || attr == ATTR_DIRECTORY || attr == ATTR_VOLUME_ID ) {

					// Add new DirectoryEntry
					DirectoryEntry tempDirectoryEntry;
					tempDirectoryEntry.name = name;
					tempDirectoryEntry.fullPath = this->getCurrentPath() + name;
					tempDirectoryEntry.shortEntry = tempShortEntry;
					tempDirectoryEntry.longEntries = longEntries;
					result.push_back( tempDirectoryEntry );

				} else {

					// Invalid entry, ignore
				}

				// Reset long entries
				longEntries.clear();
			}
		}
	}

	delete[] contents;

	return result;
}

/**
 * Get FAT Entry
 * Description: Returns the value of a FAT entry without its upper 4 bits.
 */
inline uint32_t FAT32::getFATEntry( uint32_t n ) const {

	return this->fat[n] & FAT_ENTRY_MASK;
}

/**
 * Get File Contents
 * Description: Returns a buffer of the contents of a file starting at a given
 *				initial cluster. Also sets the cluster chain of this buffer. 
 *				This function will cause the program to abort if we are given 
 *				a cluster that leads to an empty chain.
 */
uint8_t * FAT32::getFileContents( uint32_t initialCluster, vector<uint32_t> & clusterChain ) const {

	uint32_t nextCluster = initialCluster;

	// Build list of clusters for this file
	do {

		clusterChain.push_back( nextCluster );

	} while ( ( nextCluster = getFATEntry( nextCluster ) ) < EOC );

	uint32_t size = clusterChain.size() * this->bytesPerCluster;

	uint8_t * data = NULL;

	// Only allocate a buffer if we were able to make a chain
	if ( size > 0 ) {

		data = new uint8_t[ size ];
		uint8_t * temp = data;

		// Read in data
		for ( uint32_t i = 0; i < clusterChain.size(); i++ ) {

			this->fatImage.seekg( this->getFirstDataSectorOfCluster( clusterChain[i] ) * this->bpb.bytesPerSector  );

			for ( uint32_t j = 0; j < this->bpb.sectorsPerCluster ; j++ ) {

				this->fatImage.read( reinterpret_cast<char *>( temp ), this->bpb.bytesPerSector  );
				temp += this->bpb.bytesPerSector ;
			}
		}
	}

	if ( data == NULL ) {

		cout << "Failed to get file contents. Aborting.";
		exit( EXIT_SUCCESS );
	}

	return data;
}

/**
 * Get First Sector of Cluster
 * Description: Returns the first data sector of a given cluster.
 */
inline uint32_t FAT32::getFirstDataSectorOfCluster( uint32_t n ) const {

	return ( ( n - 2 ) * this->bpb.sectorsPerCluster ) + this->firstDataSector;
}

/**
 * Is Directory
 * Description: Checks if given entry is a directory.
 */
inline bool FAT32::isDirectory( const DirectoryEntry & entry ) const {

	return ( entry.shortEntry.attributes & ( ATTR_DIRECTORY | ATTR_VOLUME_ID ) ) == ATTR_DIRECTORY;
}

/**
 * Is File
 * Description: Checks if given entry is a file.
 */
inline bool FAT32::isFile( const DirectoryEntry & entry ) const {

	return ( entry.shortEntry.attributes & ( ATTR_DIRECTORY | ATTR_VOLUME_ID ) ) == 0x00;
}

/**
 * Is Free Cluster
 * Description: Checks if given cluster's value represents that it's free.
 */
inline bool FAT32::isFreeCluster( uint32_t value ) const {

	return ( value == FREE_CLUSTER );
}

/**
 * Is Valid Entry Name
 * Description: Checks if a given entry name is valid.
 */
inline bool FAT32::isValidEntryName( const string & entryName ) const {

	return entryName.find( "/" ) == string::npos;	
}

/**
 * Is Valid Open Mode
 * Description: Checks if a given file open mode is valid.
 */
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

/**
 * Make File
 * Description: Attempts to generate a DirectoryEntry for a given name
 *				Always makes a long name for simplicity purposes.
 *				Uses basis-name and numeric-tail generation for the file's
 *				Short Name Entry.
 */
bool FAT32::makeFile( const string & fileName, DirectoryEntry & entry, bool directory ) const {

	// Don't let anyone make . or .. from here
	if ( fileName.compare(".") == 0 || fileName.compare("..") == 0 ) {

		cout << "error: . and .. cannot be created.\n";
		return false;
	}

	string copy = fileName;
	deque<LongDirectoryEntry> longEntries;

	// Ignore trailing periods
	size_t trailing = copy.find_last_not_of( "." );
	if ( trailing != string::npos)
		copy.erase( trailing + 1 );

	// File name can be at max 255 characters
	if ( copy.length() <= 255 ) {

		// Total path length can be at max 260 characters
		if ( ( getCurrentPath().length() + copy.length() ) <= 260 ) {

			// Validate fileName against invaid bytes
			for ( unsigned int i = 0; i < copy.length(); i++ )
				if ( copy[i] < ' ' || copy[i] == '"' || copy[i] == '*' || copy[i] == '/' 
					|| copy[i] == ':' || copy[i] == '<'  || copy[i] == '>' || copy[i] == '?' 
					||  copy[i] == '\\'|| copy[i] == '|' ) {

					cout << "error: illegal character (" << copy[i] << ") in file name.\n";
					return false;
				}

			// Get our basis name
			bool lossyConversion = false;
			string basisName = generateBasisName( copy, lossyConversion );

			// See if our name fits within 8.3 naming conventions
			// Assuming fit literally means length without periods
			bool fits = false;
			uint8_t periodCount = 0;
			for ( uint8_t i = 0; i < copy.length(); i++ )
				if ( copy[i] == '.' )
					periodCount++;

			if (  ( copy.length() <= DIR_Name_LENGTH && copy.find( "." ) == string::npos ) 
				|| ( copy.length() <= DIR_Name_LENGTH + 1 && periodCount == 1 ) )
				fits = true;

			// Check if we even need to do the numeric-tail algorithm
			if ( lossyConversion || !fits || shortNameExists( basisName ) )
				basisName = generateNumericTail( basisName );

			// Setup Short Directory Entry
			ShortDirectoryEntry shortEntry;

			// Copy in basis name
			for ( uint8_t i = 0; i < DIR_Name_LENGTH; i++ )
				shortEntry.name[i] = basisName[i];

			uint8_t checksum = calculateChecksum( shortEntry.name );

			shortEntry.attributes = directory ? ATTR_DIRECTORY : ATTR_ARCHIVE;
			shortEntry.NTReserved = 0x00;
			shortEntry.firstClusterHI = 0x0000;
			shortEntry.firstClusterLO = 0x0000;
			shortEntry.fileSize = 0x00000000;

			// MS-DOS epoch is 01/01/1980
			timeval timeOfDay;
			gettimeofday( &timeOfDay, NULL );
			time_t timeNow = (time_t) timeOfDay.tv_sec;
			struct tm * tmNow = localtime( &timeNow );

			shortEntry.createdTimeTenth = ( ( timeOfDay.tv_usec / 1000 ) % 1000 ) / 5;

			// 0-4: Day of Month, 5-8: Month of Year, 9-15: Years since 1980
			uint16_t date = ( tmNow->tm_mday & 0x0000001F )
						| ( ( ( tmNow->tm_mon + 1 ) & 0x0000000F ) << 5 )
						| ( ( ( tmNow->tm_year - 80 ) & 0x000000EF ) << 9 );

			// 0-4: Seconds, 5-10: Minutes, 11-15: Hours
			uint16_t time = ( ( tmNow->tm_sec / 2 ) & 0x0000001F )
						| ( ( tmNow->tm_min & 0x0000002F ) << 5 )
						| ( ( tmNow->tm_hour & 0x0000001F ) << 11 );

			shortEntry.writeDate = shortEntry.lastAccessDate = shortEntry.createdDate = date;
			shortEntry.writeTime = shortEntry.createdTime = time;

			// Calculate number of Long Directory Entries needed
			uint8_t range = ceil( copy.length() / static_cast<double>( LONG_NAME_LENGTH ) );
			uint8_t charLeft = copy.length();
			bool nullStored = false;

			// Form long entry chain
			for ( uint8_t i = 1; i <= range; i++ ) {

				LongDirectoryEntry entry;

				entry.ordinal = ( i == range ) ? LAST_LONG_ENTRY | i : i;
				entry.attributes = ATTR_LONG_NAME;
				entry.type = 0;
				entry.checksum = checksum;
				entry.firstClusterLO = 0;

				convertLongNameSegment( entry.name1, sizeof( entry.name1 ) / 2, charLeft, nullStored, copy );
				convertLongNameSegment( entry.name2, sizeof( entry.name2 ) / 2, charLeft, nullStored, copy );
				convertLongNameSegment( entry.name3, sizeof( entry.name3 ) / 2, charLeft, nullStored, copy );

				longEntries.push_front( entry );
			}

			entry.name = copy;
			entry.shortEntry = shortEntry;
			entry.longEntries = longEntries;

			return true;

		} else {

			cout << "error: total path length must be less than 260 characters.\n";
			return false;

		} 

	} else {

		cout << "error: file name must be less than 256 characters.\n";
		return false;
	}
}

/**
 * File Open Mode to String
 * Description: Converts an open mode to a representative string.
 */
inline const string FAT32::modeToString( const uint8_t & mode ) const {

	switch ( mode ) {

		case READ: return "reading";
		case WRITE: return "writing";
		case READWRITE: return "reading and writing";
	}

	return "invalid mode";
}

/**
 * Remove Entry
 * Description: Guts of rm and rmdir. Removes an entry from the
 *              the file system and updates all necessary records.
 *				Actually marks a file as free but doesn't zero out unless
 *				safe is set to true.
 */
void FAT32::removeEntry( DirectoryEntry & entry, uint32_t index, bool safe ) {

	vector<uint32_t> clusterChain;

	// Check if we need to zero out file contents
	if ( safe )
		zeroOutFileContents( formCluster( entry.shortEntry ) );

	uint32_t nextCluster = formCluster( entry.shortEntry );

	// Build list of clusters ( potentially remaining if we crashed ) for this file
	do {

		clusterChain.push_back( nextCluster );

	// We must also check if the nextCluster is free because we may have crashed while deleting this
	} while ( ( nextCluster = getFATEntry( nextCluster ) ) < EOC && nextCluster != FREE_CLUSTER );	

	for ( vector<uint32_t>::reverse_iterator itr = clusterChain.rbegin(); itr != clusterChain.rend(); itr++ ) {

		// Skip free clusters
		if ( *itr != 0 ) {

			setClusterValue( *itr, FREE_CLUSTER );
			this->freeClusters.push_back( *itr );
			this->fsInfo.freeCount = this->freeClusters.size();
		}
	}

	// Update all FATs
	for ( uint8_t i = 0; i < this->bpb.numFATs; i++ ) {

		uint32_t fatLocation = this->bpb.reservedSectorCount * this->bpb.bytesPerSector + ( i * this->bpb.FATSz32 * this->bpb.bytesPerSector );
		this->fatImage.seekp( fatLocation );
		this->fatImage.write( reinterpret_cast<char *>( this->fat ), ( this->countOfClusters + 2 ) *  FAT_ENTRY_SIZE );
	}

	// Update FSInfo
	this->fatImage.seekp( this->bpb.FSInfo * this->bpb.bytesPerSector );
	this->fatImage.write( reinterpret_cast<char *>( &this->fsInfo ), sizeof( this->fsInfo ) );

	// Make sure this gets out to the disk first
	this->fatImage.flush();

	// Delete directory entry
	for ( uint32_t i = 0; i < entry.longEntries.size(); i++ ) {

		if ( safe )
			memset( &entry.longEntries[i], 0, sizeof( entry.longEntries[i] ) - sizeof( entry.longEntries[i].location ) );

		entry.longEntries[i].ordinal = DIR_FREE_ENTRY;
		this->fatImage.seekp( entry.longEntries[i].location );
		this->fatImage.write( reinterpret_cast<char *>( &entry.longEntries[i] ), DIR_ENTRY_SIZE );
	}

	// Check if this is the last entry in a directory
	if ( safe )
			memset( &entry.shortEntry, 0, sizeof( entry.shortEntry ) - sizeof( entry.shortEntry.location ) );
	entry.shortEntry.name[0] = ( index + 1 == this->currentDirectoryListing.size() ) ? DIR_LAST_FREE_ENTRY : DIR_FREE_ENTRY; 
	this->fatImage.seekp( entry.shortEntry.location );
	this->fatImage.write( reinterpret_cast<char *>( &entry.shortEntry ), DIR_ENTRY_SIZE );

	// Don't let OS wait to flush
	this->fatImage.flush();

	this->currentDirectoryListing.erase( this->currentDirectoryListing.begin() + index );
}

/**
 * Resize File
 * Description: Resizes a file (cluster chain) by a given amount. Updates
 *				the fat image as well.
 */
uint8_t * FAT32::resize( uint32_t amount, vector<uint32_t> & clusterChain ) {

	uint32_t savedAmount = amount;

	// Sepcial Case: Check if we are reszing an empty file
	if ( clusterChain[0] == 0 ) {

		// Reserve next free cluster and update linked list
		uint32_t nextCluster = this->freeClusters.front();
		setClusterValue( nextCluster, EOC );
		this->freeClusters.erase( this->freeClusters.begin() );
		this->fsInfo.freeCount = this->freeClusters.size();

		// Update chain
		clusterChain.pop_back();
		clusterChain.push_back( nextCluster );

		// Done with one cluster
		amount--;
	}

	for ( uint32_t i = 0; i < amount; i++ ) {

		uint32_t currentCluster = clusterChain.back();
		uint32_t nextCluster = this->freeClusters.front();

		// Reserve next free cluster and update linked list
		setClusterValue( currentCluster, nextCluster );
		setClusterValue( nextCluster, EOC );
		this->freeClusters.erase( this->freeClusters.begin() );
		this->fsInfo.freeCount = this->freeClusters.size();

		// Update chain
		clusterChain.pop_back();
		clusterChain.push_back( currentCluster );
		clusterChain.push_back( nextCluster );
	}

	// Update all FATs
	for ( uint8_t i = 0; i < this->bpb.numFATs; i++ ) {

		uint32_t fatLocation = this->bpb.reservedSectorCount * this->bpb.bytesPerSector + ( i * this->bpb.FATSz32 * this->bpb.bytesPerSector );
		this->fatImage.seekp( fatLocation );
		this->fatImage.write( reinterpret_cast<char *>( this->fat ), ( this->countOfClusters + 2 ) *  FAT_ENTRY_SIZE );
	}

	// Update FSInfo
	this->fatImage.seekp( this->bpb.FSInfo * this->bpb.bytesPerSector );
	this->fatImage.write( reinterpret_cast<char *>( &this->fsInfo ), sizeof( this->fsInfo ) );

	uint32_t size = clusterChain.size() * this->bytesPerCluster;

	uint8_t * data = NULL;

	// Only allocate a buffer if we were able to make a chain
	if ( size > 0 ) {

		// Zero out old file contents
		zeroOutFileContents( clusterChain[ clusterChain.size() - savedAmount ] );
		this->fatImage.flush();

		data = new uint8_t[ size ];
		uint8_t * temp = data;

		// Read in data
		for ( uint32_t i = 0; i < clusterChain.size(); i++ ) {

			this->fatImage.seekg( this->getFirstDataSectorOfCluster( clusterChain[i] ) * this->bpb.bytesPerSector  );

			for ( uint32_t j = 0; j < this->bpb.sectorsPerCluster ; j++ ) {

				this->fatImage.read( reinterpret_cast<char *>( temp ), this->bpb.bytesPerSector  );
				temp += this->bpb.bytesPerSector ;
			}
		}
	}

	if ( data == NULL ) {

		cout << "Failed to get file contents. Aborting.";
		exit( EXIT_SUCCESS );
	}

	return data;
}

/**
 * Short Name Exists
 * Description: Checks if a short name exists in the current directory.
 */
inline bool FAT32::shortNameExists( string name ) const {

	for ( uint32_t i = 0; i < currentDirectoryListing.size(); i++ )
		if ( strncmp( reinterpret_cast<const char *>( currentDirectoryListing[i].shortEntry.name ), name.c_str(), DIR_Name_LENGTH ) == 0 )
			return true;
	
	return false;
}

/**
 * Set Cluster Value
 * Description: Sets a cluster entry to a given value.
 */
inline void FAT32::setClusterValue( uint32_t n, uint32_t newValue ) {

	// Make sure we don't overwrite upper 4 bits
	newValue &= FAT_ENTRY_MASK;

	// Reset entry preserving upper 4 bits
	this->fat[n] &= ~FAT_ENTRY_MASK;

	// Set new value
	this->fat[n] |= newValue;
}

/**
 * Write File Contentss
 * Description: Writes the given file contents to a specified
 *				cluster chain.
 * Expects: clusterChain to correspond to contents.
 */
void FAT32::writeFileContents( const uint8_t * contents, const vector<uint32_t> & clusterChain ) {

	// Read in data
	for ( uint32_t i = 0; i < clusterChain.size(); i++ ) {

		this->fatImage.seekp( this->getFirstDataSectorOfCluster( clusterChain[i] ) * this->bpb.bytesPerSector  );

		for ( uint32_t j = 0; j < this->bpb.sectorsPerCluster ; j++ ) {

			this->fatImage.write( reinterpret_cast<const char *>( contents ), this->bpb.bytesPerSector  );
			contents += this->bpb.bytesPerSector ;
		}
	}
}

/**
 * Zero Out File Contents
 * Description: Zeros out a file for safety purposes.
 */
void FAT32::zeroOutFileContents( uint32_t initialCluster ) const {

	vector<uint32_t> clusterChain;
	uint32_t nextCluster = initialCluster;

	// Build list of clusters for this file
	do {

		clusterChain.push_back( nextCluster );

	} while ( ( nextCluster = getFATEntry( nextCluster ) ) < EOC );

	char * zeros = new char[this->bpb.bytesPerSector];
	memset( zeros, 0, this->bpb.bytesPerSector );

	// Write out zeros
	for ( uint32_t i = 0; i < clusterChain.size(); i++ ) {

		this->fatImage.seekp( this->getFirstDataSectorOfCluster( clusterChain[i] ) * this->bpb.bytesPerSector  );

		for ( uint32_t j = 0; j < this->bpb.sectorsPerCluster ; j++ ) 
			this->fatImage.write( zeros, this->bpb.bytesPerSector );
	}

	delete[] zeros;
}

/**
 * FAT_FS Functions
 */

/**
 * Less Than Operator for DirectoryEntry
 * Description: Used for std::map. A DirectoryEntry is considered less than
 *				another if its full path is less than the others.
 */
bool FAT_FS::operator< ( const DirectoryEntry & left, const DirectoryEntry & right ) {

	return operator<( left.fullPath, right.fullPath );
}
