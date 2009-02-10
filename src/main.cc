//
// main.cc for pekwm
// Copyright © 2003-2009 Claes Nästén <me@pekdon.net>
//
// main.cc for aewm++
// Copyright (C) 2000 Frank Hale <frankhale@yahoo.com>
// http://sapphire.sourceforge.net/
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "PWinObj.hh"
#include "PDecor.hh"
#include "Client.hh"
#include "Compat.hh"
#include "Frame.hh"
#include "WindowManager.hh"
#include "Util.hh"

#include <iostream>
#include <string>
#include <cstring>

extern "C" {
#include <unistd.h> // execlp
#include <locale.h>
}

using std::cout;
using std::endl;
using std::string;

namespace Info {

//! @brief Prints version
void
printVersion(void)
{
    cout << "pekwm: version " << VERSION << EXTRA_VERSION_INFO << endl;
}

//! @brief Prints version and availible options
void
printUsage(void)
{
    printVersion();
    cout << " --help       show this info." << endl;
    cout << " --version    show version info" << endl;
    cout << " --info       extended info. Use for bug reports." << endl;
    cout << " --display    display to connect to" << endl;
    cout << " --config     alternative config file" << endl;
}

//! @brief Prints version and build-time options
void
printInfo(void)
{
    printVersion();
    cout << "features: " << FEATURES << endl;
}

} // end namespace Info

//! @brief Main function of pekwm
int
main(int argc, char **argv)
{
    string config_file;
    string command_line;

    setlocale(LC_CTYPE, "");
    Util::iconv_init();

    // build commandline
    for (int i = 0; i < argc; ++i) {
        command_line = command_line + argv[i] + " ";
    }

    // get the args and test for different options
    for (int i = 1; i < argc; ++i)	{
        if ((strcmp("--display", argv[i]) == 0) && ((i + 1) < argc)) {
            setenv("DISPLAY", argv[++i], 1);
        } else if ((strcmp("--config", argv[i]) == 0) && ((i + 1) < argc)) {
            config_file = argv[++i];
        } else if (strcmp("--version", argv[i]) == 0) {
            Info::printVersion();
            exit(0);
        } else if (strcmp("--info", argv[i]) == 0) {
            Info::printInfo();
            exit(0);
        } else if (strcmp("--help", argv[i]) || ! strcmp("-h", argv[i]) == 0) {
            Info::printUsage();
            exit(0);
        }
    }

    // Get configuration file if none was specified as a parameter,
    // default to reading environment, if not set get ~/.pekwm/config
    if (config_file.size() == 0) {
        if (getenv("PEKWM_CONFIG_FILE") && strlen(getenv("PEKWM_CONFIG_FILE"))) {
            config_file = getenv("PEKWM_CONFIG_FILE");
        } else {
            config_file = string(getenv("HOME")) + string("/.pekwm/config");
        }
    }

#ifdef DEBUG
    cout << "Starting pekwm. Use this information in bug reports:" << endl;
    Info::printInfo();
#endif // DEBUG

    WindowManager::start(command_line, config_file);

    // see if we wanted to restart
    if (WindowManager::instance()->getRestartCommand().size() > 0) {
        string restart_command = WindowManager::instance()->getRestartCommand();

        // cleanup before restarting
        WindowManager::destroy();
        Util::iconv_deinit();

        execlp("/bin/sh", "sh" , "-c", restart_command.c_str(), (char*) 0);
    }

    WindowManager::destroy();
    Util::iconv_deinit();

    return 0;
}
