#include "ConfigVar.hpp"
#include <vector>
#include <iostream>
#include <fstream>

ConfigVar::ConfigVar()
{
}

ConfigVar::ConfigVar( const char * filename, const char * delimiter )
{
    readVar( filename, delimiter );
}

int ConfigVar::readVar( const char * filename, const char * delimiter )
{
    /* get options from a file ... */
    std::ifstream input ( filename );
    char buffer[ 2048 ];

    params.clear();

    if ( !input ) return 1;

    while ( input ){
        if ( !input.getline( buffer, sizeof( buffer )) ) {
            break;
        }

        char * command = strtok( buffer, delimiter );
        if ( !command ) continue;

        char * parameter = strtok( NULL, delimiter );
        if ( !parameter ) continue;

        /* strip delimiters */
        while ( *parameter == '"' || *parameter == '\'' || *parameter == ' ' ) parameter++;
        int offset = strlen( parameter ) - 1;

        while ( parameter[ offset ] == '"' || parameter[ offset ] == '\'' ) parameter[ offset-- ] = '\0';

        offset = strlen( command ) - 1;
        while ( command[ offset ] == ' ' ) command[ offset-- ] = '\0';

        params[ command ] = parameter;
    }

    input.close();

    return 0;
}

String ConfigVar::entry( const char * reference )
{
    return params[ reference ];
}

String ConfigVar::operator[] ( const char * reference )
{
    return params[ reference ];
}
