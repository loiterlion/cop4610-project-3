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
 * Forward Declarations
 */

vector<string> tokenize( const string & input );
void printPrompt( const string & currentPath );
bool stringTouint32( const string & asString, const string & name, uint32_t & out );

int main( int argc, char * argv[] ) {

	string input, image;
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

			if ( tokens[0].compare( "exit" ) == 0 )
				break;

			else if ( tokens[0].compare( "fsinfo" ) == 0 ) {

				fat.fsinfo();

			} else if ( tokens[0].compare( "open" ) == 0 ) {

				if ( tokens.size() == 3 )
					fat.open( tokens[1], tokens[2] );

				else
					cout << "error: usage: open <file name> <mode>\n";

			} else if ( tokens[0].compare( "close" ) == 0 ) {

				if ( tokens.size() == 2 )
					fat.close( tokens[1] );

				else
					cout << "error: usage: close <file name>\n";

			} else if ( tokens[0].compare( "create" ) == 0 ) {

				if ( tokens.size() == 2 )
					fat.create( tokens[1] );

				else
					cout << "error: usage: create <file name>\n";
				
			} else if ( tokens[0].compare( "read" ) == 0 ) {

				if ( tokens.size() == 4 ) {

					// Check if numbers are actually numbers
					bool validNumber = true;
					string startPosStr = tokens[2], numBytesStr = tokens[3];
					for ( uint32_t i = 0; i < startPosStr.length(); i++ )
						if ( !isdigit( startPosStr[i] ) ) {

							validNumber = false;
							break;
						}

					// Possibly check numBytesStr as well
					if ( validNumber )
						for ( uint32_t i = 0; i < numBytesStr.length(); i++ )
							if ( !isdigit( numBytesStr[i] ) ) {

								validNumber = false;
								break;
							}

					if ( validNumber ) {

						uint32_t startPos, numBytes;

						// Try to convert arguments
						if ( stringTouint32( startPosStr, "start pos", startPos ) && stringTouint32( numBytesStr, "num bytes", numBytes ) )
							fat.read( tokens[1], startPos, numBytes );

					} else
						cout << "error: usage: read <file name> <start pos> <num bytes>\n";
					
				}

				else
					cout << "error: usage: read <file name> <start pos> <num bytes>\n";

			} else if ( tokens[0].compare( "write" ) == 0 ) {

				string quotedData;

				if ( tokens.size() >= 4 ) {

					// Per the specification we assume quoted_data will always
					// be surrounded by quotes so we don't need to do any
					// validation
					for ( uint32_t i = 3; i < tokens.size(); i++ )
						quotedData += tokens[i] += " ";

					// Remove quotes
					quotedData = quotedData.substr( 1, quotedData.length() - 3 );

					// Check if numbers are actually numbers
					bool validNumber = true;
					string startPosStr = tokens[2];
					for ( uint32_t i = 0; i < startPosStr.length(); i++ )
						if ( !isdigit( startPosStr[i] ) ) {

							validNumber = false;
							break;
						}

					if ( validNumber ) {

						uint32_t startPos;

						// Try to convert arguments
						if ( stringTouint32( startPosStr, "start pos", startPos ) )
							fat.write( tokens[1], startPos, quotedData );
					
					} else
						cout << "error: usage: write <file name> <start pos> <quoted data>\n";
					
				}

				else
					cout << "error: usage: write <file name> <start pos> <quoted data>\n";

			} else if ( tokens[0].compare( "rm" ) == 0 ) {

				if ( tokens.size() == 2 )
					fat.rm( tokens[1] );

				else
					cout << "error: usage: rm <file name>\n";

			} else if ( tokens[0].compare( "cd" ) == 0 ) {

				if ( tokens.size() == 2 )
					fat.cd( tokens[1] );

				else
					cout << "error: usage: cd <dir name>\n";
				
			} else if ( tokens[0].compare( "ls" ) == 0 ) {

				// List current directory
				if ( tokens.size() == 1 )
					fat.ls( "" );
				
				// Otherwise list specified directory
				else if ( tokens.size() == 2 )
					fat.ls( tokens[1] );

				else
					cout << "error: usage: ls [dir name]\n";

			} else if ( tokens[0].compare( "mkdir" ) == 0 ) {

				if ( tokens.size() == 2 )
					fat.mkdir( tokens[1] );

				else
					cout << "error: usage: mkdir <dir name>\n";
				
			} else if ( tokens[0].compare( "rmdir" ) == 0 ) {

				if ( tokens.size() == 2 )
					fat.rmdir( tokens[1] );

				else
					cout << "error: usage: rmdir <dir name>\n";

			} else if ( tokens[0].compare( "size" ) == 0 ) {

				if ( tokens.size() == 2 )
					fat.size( tokens[1] );

				else
					cout << "error: usage: size <file name>\n";
				
			} else if ( tokens[0].compare( "srm" ) == 0 ) {

				if ( tokens.size() == 2 )
					fat.rm( tokens[1], true );

				else
					cout << "error: usage: srm <file name>\n";

			// Invalid command
			} else {

				cout << "error: Invalid command, please try again.\n";
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