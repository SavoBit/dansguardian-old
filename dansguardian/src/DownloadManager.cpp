#include "platform.h"
#include "DownloadManager.hpp"
#include "ConfigVar.hpp"
#include "OptionContainer.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>

extern bool isDaemonised;

extern dmcreate_t defaultdmcreate;
extern dmdestroy_t defaultdmdestroy;

#ifdef __FANCYDM
extern dmcreate_t fancydmcreate;
extern dmdestroy_t fancydmdestroy;
#endif

DMPlugin::DMPlugin( ConfigVar & definition )
{
    // nothing to see here; move along
}

DMPluginLoader::DMPluginLoader( )
{
    create_it  = NULL;
    destroy_it = NULL;
    isGood = false;
}

DMPluginLoader::DMPluginLoader( const DMPluginLoader & a )
{
    create_it  = a.create_it;        // used to create said plugin
    destroy_it = a.destroy_it;
    isGood     = a.isGood;
}

DMPluginLoader::~DMPluginLoader()
{
    return;
}

DMPluginLoader::DMPluginLoader( const char * pluginConfigPath )
{
    isGood = false;

    if (cv.readVar(pluginConfigPath, "=" ) > 0) {
        if (!isDaemonised) {
            std::cerr << "Unable to load plugin config: " << pluginConfigPath << std::endl;
        }
        syslog( LOG_ERR, "Unable to load plugin config %s\n", pluginConfigPath);
        return;
    }

    String plugname = cv["plugname"];

    if (plugname.length() < 1) {
        if (!isDaemonised) {
            std::cerr << "Unable read plugin config plugname variable: " << pluginConfigPath << std::endl;
        }
        syslog( LOG_ERR, "Unable read plugin config plugname variable %s\n", pluginConfigPath);
        return;
    }

    if (plugname == "default") {
#ifdef DGDEBUG
        std::cout << "Enabling default DM plugin" << std::endl;
#endif
        create_it  = (dmcreate_t*)  defaultdmcreate;
        destroy_it = (dmdestroy_t*) defaultdmdestroy;
        isGood = true;
        return;
    }

#ifdef __FANCYDM
    if (plugname == "fancy") {
#warning foo
#ifdef DGDEBUG
        std::cout << "Enabling fancy DM plugin" << std::endl;
#endif
        create_it  = (dmcreate_t*)  fancydmcreate;
        destroy_it = (dmdestroy_t*)  fancydmdestroy;
        isGood = true;
        return;
    }
#endif

    create_it  = NULL;
    destroy_it = NULL;
    if (!isDaemonised) {
        std::cerr << "Unable to load plugin: " << plugname << std::endl;
    }
    syslog( LOG_ERR, "Unable to load plugin %s\n", plugname.toCharArray() );
    return;

}


DMPlugin * DMPluginLoader::create()
{
#ifdef DGDEBUG
        std::cout << "Creating DM" << std::endl;
#endif
    if ( create_it ){
        return &create_it( cv );
    }
    return NULL;
}

void DMPluginLoader::destroy( DMPlugin * object )
{
    if ( object ){
        if ( destroy_it ){
            destroy_it(object);
        }
    }
    return;
}

