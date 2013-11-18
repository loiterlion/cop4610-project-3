#include "limitsfix.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
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

void printPrompt( const string & image );
Command stringToCommand( const string & str  );

int main( int argc, char * argv[] ) {

	string input, image;
	Command command;
	fstream imageFile;

	if ( argc == 2 )
		image = argv[1];

	else {

		cout << "usage: fmod <FAT Image>\n";
		exit( EXIT_SUCCESS );
	}

	// Open image
	imageFile.open( image.c_str(), ios::in | ios::out | ios::binary );

	if ( !imageFile.is_open() ) {

		cout << "error: failed to open " << image << endl;
		exit( EXIT_SUCCESS );
	}

	printPrompt( image );

	// Continue prompting until we get EXIT or something happens to our input stream
	while ( getline( cin, input, '\n' ).good() && ( command = stringToCommand( input ) ) != EXIT ) {

		switch ( command ) {

			case INVALID: {

				cout << "error: Invalid command, please try again.\n";
				break;
			}

			case FSINFO: {

				cout << "error: " << input << " unimplemented\n";
				break;
			}

			case OPEN: {

				cout << "error: " << input << " unimplemented\n";
				break;
			}

			case CLOSE: {

				cout << "error: " << input << " unimplemented\n";
				break;
			}

			case CREATE: {

				cout << "error: " << input << " unimplemented\n";
				break;
			}

			case READ: {

				cout << "error: " << input << " unimplemented\n";
				break;
			}

			case WRITE: {

				cout << "error: " << input << " unimplemented\n";
				break;
			}

			case RM: {

				cout << "error: " << input << " unimplemented\n";
				break;
			}

			case CD: {

				cout << "error: " << input << " unimplemented\n";
				break;
			}

			case LS: {

				cout << "error: " << input << " unimplemented\n";
				break;
			}

			case MKDIR: {

				cout << "error: " << input << " unimplemented\n";
				break;
			}

			case RMDIR: {

				cout << "error: " << input << " unimplemented\n";
				break;
			}

			case SIZE: {

				cout << "error: " << input << " unimplemented\n";
				break;
			}

			case SRM: {

				cout << "error: " << input << " unimplemented\n";
				break;
			}

			case EXIT: {

				// case should never occur
				break;
			}
		}

		printPrompt( image );
	}

	// Cleanup
	imageFile.close();

	cout << "\nClosing fmod.\n";
	return 0;
}

/**
 * Primpt Prompt
 * Description: Prints command prompt in form username[fs-image-name]> 
 */
void printPrompt( const string & image ) {

	char login[LOGIN_NAME_MAX] = { 0 };

	getlogin_r( login, LOGIN_NAME_MAX );

	cout << login << '[' << image << ']' << "> "; 
}

/**
 * String to Command
 * Description: Converts a string to its Command form
 * Returns: The strings respective Command; otherwise INVALID
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