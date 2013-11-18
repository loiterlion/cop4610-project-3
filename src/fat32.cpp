#include "fat32.h"

using namespace FAT_FS;

/**
 * FAT32 Constructor
 */
FAT32::FAT32() {

	this->isInit = false;
}

/**
 * FAT32 Destructor
 */
FAT32::~FAT32() {

	// cleanup
	this->fatImage.close();
}

/**
 * Initialize
 * Description: Initializes the FAT32 FS with a given image file.
 * Expects: To not already be initialized.
 */
bool FAT32::init( const string & image ) {

	bool result = false;

	// Check if already initialized 
	if ( this->isInit )
		cout << "error: called init on FAT32 when already initialized" << endl;
	
	else {

		this->fatImage.open( image.c_str(), ios::in | ios::out | ios::binary );

		// Check if we opened file successfully
		if ( !this->fatImage.is_open() )
			this->isInit = result = false;

		else {

			// Read BIOS Parameter Block
			this->getPrimitive( BPB_BytsPerSec, this->bytesPerSector );
			this->getPrimitive( BPB_SecPerClus, this->sectorsPerCluster );
			this->getPrimitive( BPB_TotSec32, this->totalSectors );
			this->getPrimitive( BPB_NumFATs, this->numFats );
			this->getPrimitive( BPB_FATSz32, this->sectorsPerFAT );
			this->getPrimitive( BPB_RsvdSecCnt, this->reservedSectorCount );

			// Successful initialization
			this->isInit = result = true;
		}
	} 

	return result;
}

/**
 * Check Init
 * Description: Checks if FAT32 has already been initialized
 */
bool FAT32::checkInit() const {

	if ( !this->isInit ) {

		cout << "error: expected FAT32 to be initialized before calling any command!" << endl;
		return false;
	}

	return true;
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

/**
 * Commands
 */

/**
 * FS Info
 * Description: Prints out info for the loaded FAT32 FS.
 * Expects: To have been initialized.
 */
void FAT32::fsinfo() const {

	if ( !this->checkInit() )
		return;

	// + used to promote type to a printable number
	cout << "Bytes per sector: " << this->bytesPerSector
		 << "\nSectors per cluster: " << +this->sectorsPerCluster
		 << "\nTotal sectors: " << this->totalSectors
		 << "\nNumber of FATs: " << +this->numFats
		 << "\nSectors per FAT: " << this->sectorsPerFAT
		 << "\nNumber of free sectors: " << "UNIMPLEMENTED"
		 << endl;
}

void FAT32::open() {

	if ( !this->checkInit() )
		return;

	cout << "error: unimplmented" << endl;
}

void FAT32::close() {

	if ( !this->checkInit() )
		return;

	cout << "error: unimplmented" << endl;
}

void FAT32::create() {

	if ( !this->checkInit() )
		return;

	cout << "error: unimplmented" << endl;
}

void FAT32::read() {

	if ( !this->checkInit() )
		return;

	cout << "error: unimplmented" << endl;
}

void FAT32::write() {

	if ( !this->checkInit() )
		return;

	cout << "error: unimplmented" << endl;
}

void FAT32::rm() {

	if ( !this->checkInit() )
		return;

	cout << "error: unimplmented" << endl;
}

void FAT32::cd() {

	if ( !this->checkInit() )
		return;

	cout << "error: unimplmented" << endl;
}

void FAT32::ls() {

	if ( !this->checkInit() )
		return;

	cout << "error: unimplmented" << endl;
}

void FAT32::mkdir() {

	if ( !this->checkInit() )
		return;

	cout << "error: unimplmented" << endl;
}

void FAT32::rmdir() {

	if ( !this->checkInit() )
		return;

	cout << "error: unimplmented" << endl;
}

void FAT32::size() {

	if ( !this->checkInit() )
		return;

	cout << "error: unimplmented" << endl;
}

void FAT32::srm() {

	if ( !this->checkInit() )
		return;

	cout << "error: unimplmented" << endl;
}
