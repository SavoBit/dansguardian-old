#ifndef __HPP_CONFIGVAR
#define __HPP_CONFIGVAR

#include <iostream>
#include <vector>
#include <map>
#include "String.hpp"

class ConfigVar {
	public:
		ConfigVar();
		ConfigVar( const char * filename, const char * delimiter = "=" );
		int readVar( const char * filename, const char * delimiter = "=");

		String entry( const char * reference );

		String operator[] ( const char * reference );
	private:

		struct ltstr
		{
			bool operator()(String s1, String s2) const
			{
				return strcmp(s1.toCharArray(), s2.toCharArray()) < 0;
			}
		};

  		std::map<String, String, ltstr> params;
};

#endif
