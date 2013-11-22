#include "fat32.h"
#include "limitsfix.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
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
void printPrompt( const vector<string> & currentPath );
Command stringToCommand( const string & str  );

int main( int argc, char * argv[] ) {

	string input, image;
	Command command;
	fstream fatImage;

	if ( argc == 2 )
		image = argv[1];

	else {

		cout << "usage: fmod <FAT Image>" << endl;
		exit( EXIT_SUCCESS );
	}

	fatImage.open( image.c_str(), ios::in | ios::out | ios::binary );

	// Check if we opened file successfully
	if ( !fatImage.is_open() ) {

		cout << "error: failed to open " + image << endl;
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

						cout << "error: Invalid command, please try again" << endl;
						break;
					}

					case FSINFO: {

						fat.fsinfo();
						break;
					}

					case OPEN: {

						fat.open();
						break;
					}

					case CLOSE: {

						fat.close();
						break;
					}

					case CREATE: {

						fat.create();
						break;
					}

					case READ: {

						fat.read();
						break;
					}

					case WRITE: {

						fat.write();
						break;
					}

					case RM: {

						fat.rm();
						break;
					}

					case CD: {

						if ( tokens.size() == 2 ) {

							if ( tokens[1].find( "/" ) == string::npos )
								fat.cd( tokens[1] );

							else
								cout << "error: directory name may not contain /" << endl;
						} 

						else
							cout << "error: usage: cd [dir_name]" << endl;

						break;
					}

					case LS: {

						if ( tokens.size() == 1 ) {
							
							fat.ls( "" );
						
						} else if ( tokens.size() == 2 ) {

							if ( tokens[1].find( "/" ) == string::npos )
								fat.ls( tokens[1] );

							else
								cout << "error: directory name may not contain /" << endl;
						} 

						else
							cout << "error: usage: ls [dir_name]" << endl;

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

						fat.size();
						break;
					}

					case SRM: {

						fat.srm();
						break;
					}

					case EXIT: {

						// case should never occur
						break;
					}
				}
			}
		}

		printPrompt( fat.getCurrentPath() );
		getline( cin, input, '\n' );
		tokens = tokenize( input );
	}

	// Cleanup
	fatImage.close();

	cout << "\nClosing fmod" << endl;
	return 0;
}

vector<string> tokenize( const string & input ) {

	vector<string> result;
	stringstream stringStream( input );
	string temp;

	while ( stringStream >> temp )
		result.push_back( temp );

	return result;
}

/**
 * Primpt Prompt
 * Description: Prints command prompt in form username[fs-image-name]> .
 */
void printPrompt( const vector<string> & currentPath ) {

	char login[LOGIN_NAME_MAX] = { 0 };

	getlogin_r( login, LOGIN_NAME_MAX );

	string path = "/";

	for ( uint32_t i = 0; i < currentPath.size(); i++ )
		path += currentPath[i] + "/";

	cout << login << '[' << path << ']' << "> "; 
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