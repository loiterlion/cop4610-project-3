#include "fat32.h"
#include "limitsfix.h"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdint.h>
#include <string>
#include <unistd.h>

using namespace std;

/**
 * enums
 */

enum Command { INVALID, FSINFO, OPEN, CLOSE, CREATE, READ, WRITE, RM, CD, LS, MKDIR, RMDIR, SIZE, SRM, EXIT };

/**
 * Forward Declarations
 */

vector<string> tokenize( const string & input );
void printPrompt( const string & currentPath );
Command stringToCommand( const string & str  );
bool stringTouint32( const string & asString, const string & name, uint32_t & out );

int main( int argc, char * argv[] ) {

	string input, image;
	Command command;
	fstream fatImage;

	if ( argc == 2 )
		image = argv[1];

	else {

		cout << "usage: fmod <FAT32 Image>" << endl;
		exit( EXIT_SUCCESS );
	}

	fatImage.open( image.c_str(), ios::in | ios::out | ios::binary );

	// Check if we opened file successfully
	if ( !fatImage.is_open() ) {

		cout << "error: failed to open " + image << "." << endl;
		exit( EXIT_SUCCESS );
	}

	// Setup FAT32
	FAT_FS::FAT32 fat( fatImage );

	printPrompt( fat.getCurrentPath() );

	// Get input for first time
	getline( cin, input, '\n' );
	vector<string> tokens = tokenize( input );

	// Continue prompting until we get EXIT or something happens to our input stream
	while ( cin.good() ) {

		if ( !tokens.empty() ) {

			if ( ( command = stringToCommand( tokens[0] ) ) == EXIT )
				break;

			else {

				// Run command
				switch ( command ) {

					case INVALID: {

						cout << "error: Invalid command, please try again.\n";
						break;
					}

					case FSINFO: {

						fat.fsinfo();
						break;
					}

					case OPEN: {

						if ( tokens.size() == 3 )
							fat.open( tokens[1], tokens[2] );

						else
							cout << "error: usage: open <file name> <mode>\n";

						break;
					}

					case CLOSE: {

						if ( tokens.size() == 2 )
							fat.close( tokens[1] );

						else
							cout << "error: usage: close <file name>\n";

						break;
					}

					case CREATE: {

						fat.create();
						break;
					}

					case READ: {

						if ( tokens.size() == 4 ) {

							uint32_t startPos, numBytes;

							// Try to convert arguments
							if ( !stringTouint32( tokens[2], "start pos", startPos ) 
								|| !stringTouint32( tokens[3], "num bytes", numBytes ) )
								break;

							fat.read( tokens[1], startPos, numBytes );
						}

						else
							cout << "error: usage: read <file name> <start pos> <num bytes>\n";

						break;
					}

					case WRITE: {

						fat.write();
						break;
					}

					case RM: {

						if ( tokens.size() == 2 )
							fat.rm( tokens[1] );

						else
							cout << "error: usage: rm <file name>\n";

						break;
					}

					case CD: {

						if ( tokens.size() == 2 )
							fat.cd( tokens[1] );

						else
							cout << "error: usage: cd <dir name>\n";

						break;
					}

					case LS: {

						// List current directory
						if ( tokens.size() == 1 )
							fat.ls( "" );
						
						// Otherwise list specified directory
						else if ( tokens.size() == 2 )
							fat.ls( tokens[1] );

						else
							cout << "error: usage: ls [dir name]\n";

						break;
					}

					case MKDIR: {

						fat.mkdir();
						break;
					}

					case RMDIR: {

						fat.rmdir();
						break;
					}

					case SIZE: {

						if ( tokens.size() == 2 )
							fat.size( tokens[1] );

						else
							cout << "error: usage: size <file name>\n";

						break;
					}

					case SRM: {

						if ( tokens.size() == 2 )
							fat.rm( tokens[1], true );

						else
							cout << "error: usage: srm <file name>\n";

						break;
					}

					case EXIT: {

						// case should never occur
						break;
					}
				}
			}
		}

		// Ask for input again and process
		printPrompt( fat.getCurrentPath() );
		getline( cin, input, '\n' );
		tokens = tokenize( input );
	}

	// Cleanup
	fatImage.close();

	cout << "\nClosing fmod." << endl;
	return 0;
}

/**
 * Tokenize String
 * Description: Returns a list of tokens from a string split apart
 *				by spaces (multiple ignored).
 */
vector<string> tokenize( const string & input ) {

	vector<string> result;
	stringstream stringStream( input );
	string temp;

	// operator>> of stringstream will ignore extra
	// spaces just like cin >>
	while ( stringStream >> temp )
		result.push_back( temp );

	return result;
}

/**
 * String to uint32_t
 * Description: Attempts to convert a string to a uint32_t. Returns whether
 *				or not the operation succeeded.
 */
bool stringTouint32( const string & asString, const string & name, uint32_t & out ) {

	unsigned long int converted = strtol( asString.c_str(), NULL, 10 );

	// Do an error/range check
	if ( ( errno == ERANGE && converted == numeric_limits<unsigned long>::max() ) || converted > numeric_limits<uint32_t>::max() ) {

		cout << name << " too large. Must be less than " << numeric_limits<uint32_t>::max() << "\n";
		return false;
	}

	out = static_cast<uint32_t>( converted );
	return true;
}

/**
 * Primpt Prompt
 * Description: Prints command prompt in form username[fs-image-name]> .
 */
void printPrompt( const string & currentPath ) {

	char login[LOGIN_NAME_MAX] = { 0 };

	getlogin_r( login, LOGIN_NAME_MAX );

	cout << login << '[' << currentPath << ']' << "> "; 
}

/**
 * String to Command
 * Description: Converts a string to its Command form.
 * Returns: The strings respective Command; otherwise INVALID.
 */
Command stringToCommand( const string & str  ) {

	Command result = INVALID;

	if ( str.compare( "fsinfo" ) == 0 )
		result = FSINFO;

	else if ( str.compare( "open" ) == 0 )
		result = OPEN;

	else if ( str.compare( "close" ) == 0 )
		result = CLOSE;

	else if ( str.compare( "create" ) == 0 )
		result = CREATE;

	else if ( str.compare( "read" ) == 0 )
		result = READ;

	else if ( str.compare( "write" ) == 0 )
		result = WRITE;

	else if ( str.compare( "rm" ) == 0 )
		result = RM;

	else if ( str.compare( "cd" ) == 0 )
		result = CD;

	else if ( str.compare( "ls" ) == 0 )
		result = LS;

	else if ( str.compare( "mkdir" ) == 0 )
		result = MKDIR;

	else if ( str.compare( "rmdir" ) == 0 )
		result = RMDIR;

	else if ( str.compare( "size" ) == 0 )
		result = SIZE;

	else if ( str.compare( "srm" ) == 0 )
		result = SRM;

	else if ( str.compare( "exit" ) == 0 )
		result = EXIT;

	return result;
}