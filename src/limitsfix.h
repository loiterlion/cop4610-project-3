#pragma once

// Lets us use LOGIN_NAME_MAX on Mac OS X which
// still hasn't defined LOGIN_NAME_MAX
#ifndef LOGIN_NAME_MAX
	#ifdef _POSIX_LOGIN_NAME_MAX
		#define LOGIN_NAME_MAX _POSIX_LOGIN_NAME_MAX
	#else
		#define LOGIN_NAME_MAX 9
	#endif
#endif