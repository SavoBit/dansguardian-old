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

DMPlugin::DMPlugin( ConfigVar & definition )
{
    // nothing to see here; move along
}

DMPluginLoader::DMPluginLoader( ) throw(std::runtime_error)
{
    handle = NULL;
    create_it  = NULL;
    destroy_it = NULL;
    isGood = false;
    if (lt_dlinit() != 0)
           throw std::runtime_error("Can\'t initialise libltdl");
}

DMPluginLoader::DMPluginLoader( const DMPluginLoader & a ) throw(std::runtime_error)
{
    handle     = a.handle;
    create_it  = a.create_it;        // used to create said plugin
    destroy_it = a.destroy_it;        // to destroy (delete) it
    isGood     = a.isGood;
    if (lt_dlinit() != 0)
           throw std::runtime_error("Can\'t initialise libltdl");
}

DMPluginLoader::~DMPluginLoader()
{
    lt_dlexit();
    return;
}

DMPluginLoader::DMPluginLoader( const char * pluginConfigPath ) throw(std::runtime_error)
{
    if (lt_dlinit() != 0)
           throw std::runtime_error("Can\'t initialise libltdl");
    isGood = false;

    if (cv.readVar(pluginConfigPath, "=" ) > 0) {
        if (!isDaemonised) {
            std::cerr << "Unable to load plugin config: " << pluginConfigPath << std::endl;
        }
        syslog( LOG_ERR, "Unable to load plugin config %s\n", pluginConfigPath);
        return;
    }

    String pluginpath = cv["libpath"];

    if (pluginpath.length() < 1) {
        if (!isDaemonised) {
            std::cerr << "Unable read plugin config libpath variable: " << pluginConfigPath << std::endl;
        }
        syslog( LOG_ERR, "Unable read plugin config libpath variable %s\n", pluginConfigPath);
        return;
    }

    handle = lt_dlopen(pluginpath.toCharArray());

    if ( handle == NULL ){
        handle = NULL;
//        pluginname = "";
        create_it  = NULL;
        destroy_it = NULL;
        if (!isDaemonised) {
            std::cerr << "Unable to load plugin: " << pluginpath << " " << lt_dlerror() << std::endl;
        }
        syslog( LOG_ERR, "Unable to load plugin %s - %s\n", pluginpath.toCharArray(), lt_dlerror() );
        return;
    }
//    setname( pluginName );

    create_it  = (dmcreate_t*)  lt_dlsym( handle, "create");
    destroy_it = (dmdestroy_t*) lt_dlsym( handle, "destroy");
    isGood = true;
    return;
}


DMPlugin * DMPluginLoader::create()
{
	if ( create_it ){
		return create_it( cv );
	} else {
		return NULL;
	}
	return NULL;
}

void DMPluginLoader::destroy( DMPlugin * object )
{
	if ( object ){
		if ( destroy_it ){
			destroy_it( object );
			return;
		} else {
			return;
		}
	}
	return;
}

