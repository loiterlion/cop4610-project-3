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
 */
void FAT32::fsinfo() const {

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

void FAT32::ls() {

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
