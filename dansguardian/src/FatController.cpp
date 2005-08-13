//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@jadeb//.com).
//For support go to http://groups.yahoo.com/group/dansguardian

//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include "platform.h"
#include <syslog.h>
#include "FatController.hpp"
#include <csignal>
#include <sys/stat.h>
#include "ConnectionHandler.hpp"
#include "DynamicURLList.hpp"
#include "String.hpp"
#include "Socket.hpp"
#include "UDSocket.hpp"
#include "SysV.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <sys/time.h>
#include <sys/poll.h>

#ifdef __GCCVER3
    #include <istream>
#else
    #include <istream.h>
#endif

extern OptionContainer o;
extern bool isDaemonised;

int FatController::controlIt(int pidfilefd) {

    o.lm.garbageCollect();

    serversock.reset();
    if (o.no_logger == 0) {
        ipcsock.reset();
    }
    else {
        ipcsock.close();
    }
    urllistsock.reset();
    peersock.reset();


    pid_t loggerpid = 0;  // to hold the logging process pid
    pid_t urllistpid = 0;  // to hold the logging process pid (zero to stop
                           // warning
    int rc, serversockfd, fds;

    serversockfd = serversock.getFD();

    if (serversockfd < 0) {
        if (!isDaemonised) {
            std::cerr << "Error creating server socket" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error creating server socket");
        return 1;  // if the socket fd is not +ve then the socket
                   // creation failed
    }

    if (o.no_logger == 0) {
        if (ipcsock.getFD() < 0) {
            if (!isDaemonised) {
               std::cerr << "Error creating ipc socket" << std::endl;
            }
            syslog(LOG_ERR, "%s","Error creating ipc socket");
            return 1;  // if the socket fd is not +ve then the socket
                       // creation failed
        }
    }

    bool needdrop = false;

    if (o.filter_port < 1024) {
        #ifdef DGDEBUG
            std::cout << "seteuiding for low port binding" << std::endl;
        #endif
        needdrop = true;
        int rc;
        #ifdef HAVE_SETREUID
            rc = setreuid((uid_t)-1, o.root_user);
        #else
            rc = seteuid(o.root_user);
        #endif
        if (rc == -1) {
            syslog(LOG_ERR, "%s","Unable to seteuid() to bind filter port.");
            #ifdef DGDEBUG
                std::cerr << "Unable to seteuid() to bind filter port." << std::endl;
            #endif
            return 1;
        }
    }
    if (o.filter_ip.length() > 6) {  // filter ip specified in conf
                       // listen/bind to a port and ip
        if (serversock.bind(o.filter_ip, o.filter_port)) {
            if (!isDaemonised) {
                std::cerr << "Error binding server socket (is something else running on the filter port and ip? [" << o.filter_port << " " << o.filter_ip << "])" << std::endl;
            }
            syslog(LOG_ERR, "%s","Error binding server socket (is something else running on the filter port and ip?");
            return 1;
        }
    }
    else {             // listen/bind to a port
        if (serversock.bind(o.filter_port)) {
            if (!isDaemonised) {
                std::cerr << "Error binding server socket (is something else running on the filter port? [" << o.filter_port << "])" << std::endl;
            }
            syslog(LOG_ERR, "%s","Error binding server socket (is something else running on the filter port?");
            return 1;
        }
    }

    if (needdrop) {
        int rc;
        #ifdef HAVE_SETREUID
            rc = setreuid((uid_t)-1, o.proxy_user);
        #else
            rc = seteuid(o.proxy_user);  // become low priv again
        #endif
        if (rc == -1) {
	    syslog(LOG_ERR, "%s","Unable to re-seteuid()");
	    #ifdef DGDEBUG
                std::cerr << "Unable to re-seteuid()" << std::endl;
            #endif
	    return 1;  // seteuid failed for some reason so exit with error
        }
    }

    // Needs deleting if its there
//    unlink(o.ipc_filename.c_str()); // this would normally be in a -r situation.
// disabled as requested by Christopher Weimann <csw@k12hq.com>
// Fri, 11 Feb 2005 15:42:28 -0500

    // Needs deleting if its there
    unlink(o.urlipc_filename.c_str()); // this would normally be in a -r situation.

    if (o.no_logger == 0) {
        if (ipcsock.bind((char*) o.ipc_filename.c_str())) {  // bind to file
            if (!isDaemonised) {
                std::cerr << "Error binding ipc server file (try using the SysV to stop DansGuardian then try starting it again or doing an 'rm " << o.ipc_filename << "')." << std::endl;
            }
            syslog(LOG_ERR, "%s","Error binding ipc server file (try using the SysV to stop DansGuardian then try starting it again or doing an 'rm /tmp/.dguardianipc' or whatever you have it configured to).");
            return 1;
        }
    }

    if (o.url_cache_number > 0) {
        if (urllistsock.bind((char*) o.urlipc_filename.c_str())) {  // bind to file
            if (!isDaemonised) {
                std::cerr << "Error binding urllistsock server file (try using the SysV to stop DansGuardian then try starting it again or doing an 'rm " << o.urlipc_filename << "')." << std::endl;
            }
            syslog(LOG_ERR, "%s","Error binding urllistsock server file (try using the SysV to stop DansGuardian then try starting it again or doing an 'rm /tmp/.dguardianurlipc' or whatever you have it configured to).");
            return 1;
        }
    }


    if (serversock.listen(256)) {  // set it to listen mode with a kernel
                                  // queue of 256 backlog connections
        if (!isDaemonised) {
            std::cerr << "Error listening to server socket" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error listening to server socket");
        return 1;
    }
    if (o.no_logger == 0) {
        if (ipcsock.listen(256)) {  // set it to listen mode with a kernel
                                  // queue of 256 backlog connections
            if (!isDaemonised) {
                std::cerr << "Error listening to ipc server file" << std::endl;
            }
            syslog(LOG_ERR, "%s","Error listening to ipc server file");
            return 1;
        }
    }

    if (o.url_cache_number > 0) {
        if (urllistsock.listen(256)) {  // set it to listen mode with a kernel
                                  // queue of 256 backlog connections
            if (!isDaemonised) {
                std::cerr << "Error listening to url ipc server file" << std::endl;
            }
            syslog(LOG_ERR, "%s","Error listening to url ipc server file");
            return 1;
        }
    }


    if (peersock.getFD() < 0) {
        if (!isDaemonised) {
            std::cerr << "Error creating peer socket" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error creating peer socket");
        return 1;  // if the socket fd is not +ve then the socket
                   // creation failed
    }
    peersock.close();  // We need to do this as we won't be using the
                       // fd created by the contructor as it will be
                       // set to the ones accepted().

    if (!daemonise(pidfilefd)) {  // become a detached daemon
        if (!isDaemonised) {
            std::cerr << "Error daemonising" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error daemonising");
        return 1;
    }

    // We are now a daemon so all errors need to go in the syslog, rather
    // than being reported on screen as we've detached from the console and
    // trying to write to stdout will not be nice.

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL)) {  // ignore SIGPIPE so we can handle
                                          // premature disconections better
        syslog(LOG_ERR, "%s","Error ignoring SIGPIPE");
        return(1);
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGHUP, &sa, NULL)) {  // ignore HUP
        syslog(LOG_ERR, "%s","Error ignoring HUP");
        return(1);
    }


    // Next thing we need to do is to split into two processes - one to
    // handle incoming TCP connections from the clients and one to handle
    // incoming UDS ipc from our forked children.  This helps reduce
    // bottlenecks by not having only one select() loop.

    if (o.no_logger == 0) {

        loggerpid = fork(); // make a child processes copy of self to be logger

        if (loggerpid == 0) {  // ma ma!  i am the child
            serversock.close();  // we don't need our copy of this so close it
            urllistsock.close();  // we don't need our copy of this so close it
            logListener(o.log_location, o.logconerror);
            #ifdef DGDEBUG
                std::cout << "Log listener exiting" << std::endl;
            #endif
            _exit(0); // is reccomended for child and daemons to use this instead
        }
    }

    if (o.url_cache_number > 0) {
        urllistpid = fork(); // make a child processes copy of self to be logger

        if (urllistpid == 0) {  // ma ma!  i am the child
            serversock.close();  // we don't need our copy of this so close it
            if (o.no_logger == 0) {
                ipcsock.close();  // we don't need our copy of this so close it
            }

            urlListListener(o.logconerror);
            #ifdef DGDEBUG
                std::cout << "URL List listener exiting" << std::endl;
            #endif
            _exit(0); // is reccomended for child and daemons to use this instead
        }
    }

    // I am the parent process here onwards.

    #ifdef DGDEBUG
        std::cout << "Parent process created children" << std::endl;
    #endif


    if (o.url_cache_number > 0) {
        urllistsock.close();  // we don't need our copy of this so close it
    }
    if (o.no_logger == 0) {
        ipcsock.close();  // we don't need our copy of this so close it
    }

    memset(&sa, 0, sizeof(sa));
    if (o.soft_restart == 0) {
        sa.sa_handler = &sig_term;  // register sig_term as our handler
    }
    else {
        sa.sa_handler = &sig_termsafe;
    }
    if (sigaction(SIGTERM, &sa, NULL)) {  // when the parent process gets a
                                          // sigterm we need to kill our
                                          // children which this will do,
                                          // then we need to exit
        syslog(LOG_ERR, "%s","Error registering SIGTERM handler");
        return(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &sig_hup;  // register sig_hup as our handler
    if (sigaction(SIGHUP, &sa, NULL)) {  // when the parent process gets a
                                          // sighup we need to kill our
                                          // children which this will do,
                                          // then we need to read config
        syslog(LOG_ERR, "%s","Error registering SIGHUP handler");
        return(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &sig_usr1;  // register sig_usr1 as our handler
    if (sigaction(SIGUSR1, &sa, NULL)) {  // when the parent process gets a
                                          // sigusr1 we need to hup our
                                          // children to make them exit
                                          // then we need to read fg config
        syslog(LOG_ERR, "%s","Error registering SIGUSR handler");
        return(1);
    }

    #ifdef DGDEBUG
        std::cout << "Parent process sig handlers done" << std::endl;
    #endif

    struct timeval quicksleep;  // used later on for a short sleep
    quicksleep.tv_sec = 0;
    quicksleep.tv_usec = 10000; // = 10ms = 0.01sec
    struct timeval qscopy;  // copy to use as select() can modify

    numchildren = 0;  // to keep count of our children
    busychildren = 0;  // to keep count of our children
    freechildren = 0;  // to keep count of our children

    childrenpids = new int[o.max_children];  // so when one exits we know
                                                 // who
    childrenstates = new int[o.max_children];  // so we know what
                                                    // they're up to
    fds = o.max_children + 1;

    pids = new struct pollfd[fds];

    int i;

    time_t tnow;
    time_t tmaxspare;

    time(&tmaxspare);

    #ifdef DGDEBUG
        std::cout << "Parent process pid structs allocated" << std::endl;
    #endif


    for(i = 0; i < o.max_children; i++) {
        childrenpids[i] = -1;
        childrenstates[i] = -1;
        pids[i + 1].fd = -1;
        pids[i + 1].events = POLLIN;

    }
    pids[0].fd = serversockfd;
    pids[0].events = POLLIN;


    #ifdef DGDEBUG
        std::cout << "Parent process pid structs zeroed" << std::endl;
    #endif

    failurecount = 0;  // as we don't exit on an error with select()
                           // due to the fact that these errors do happen
                           // every so often on a fully working, but busy
                           // system, we just watch for too many errors
                           // consecutivly.

    waitingfor = 0;
    rc = preFork(o.min_children);

    sleep(1);  // need to allow some of the forks to complete

    #ifdef DGDEBUG
        std::cout << "Parent process preforked rc:" << rc << std::endl;
        std::cout << "Parent process pid:" << getpid() << std::endl;
    #endif

    if (rc < 0) {
        ttg = true;
        syslog(LOG_ERR, "%s","Error creating initial fork pool - exiting...");
    }

    bool preforked = true;
    int tofind;
    reloadconfig = false;

    while (failurecount < 30 && !ttg && !reloadconfig) {

                                // loop, essentially, for ever until 30
                                // consecutive errors in which case something
                                // is badly wrong.
                                // OR, its timetogo - got a sigterm
                                // OR, we need to exit to reread config

        if (gentlereload) {
            #ifdef DGDEBUG
                cout << "gentle reload activated" << endl;
            #endif
            o.deleteFilterGroups();
            if (!o.readFilterGroupConf()) {
                reloadconfig = true;  // filter groups problem so lets
                // try and reload entire config instead
                // if that fails it will bomb out
            }
            else {
                o.filter_groups_list.reset();
                if (!o.readfgfile(o.filter_groups_list_location.c_str())) {
                    reloadconfig = true;  // filter groups problem...
                }
                else {
                    o.deleteCSPlugins();
                    if (!o.loadCSPlugins()) {
                        reloadconfig = true;  // content scan plugs problem
                    }
                    else {
                        o.lm.garbageCollect();
                        hupAllChildren();
                        preFork(o.min_children);
                        gentlereload = false;
                    }
                }
            }
            flushURLCache();
            continue;
        }

        // Lets take the opportunity to clean up our dead children if any

        for(i = 0; i <= o.max_children; i++) {
           pids[i].revents = 0;
        }

        mopUpAfterKids();

        rc = poll(pids, fds, 60 * 1000);

        mopUpAfterKids();



        if (rc < 0) {  // was an error

            #ifdef DGDEBUG
                std::cout << "errno:" << errno << " " << strerror(errno) << std::endl;
            #endif

            if (errno == EINTR) {
                continue;  // was interupted by a signal so restart
            }
            failurecount++;  // log the error/failure
            continue;  // then continue with the looping
        }

        tofind = rc;
        if (rc > 0) {
            if ((pids[0].revents & POLLIN) > 0) {
                tofind--;
            }

        }

        if (tofind > 0) {
            if (checkKidReadyStatus(tofind)) {
                preforked = false;  // we are no longer waiting last prefork
            }
        }

        freechildren = numchildren - busychildren;

        #ifdef DGDEBUG
            std::cout << "numchildren:" << numchildren << std::endl;
            std::cout << "busychildren:" << busychildren << std::endl;
            std::cout << "freechildren:" << freechildren << std::endl;
            std::cout << "waitingfor:" << waitingfor << std::endl << std::endl;
        #endif

        if (rc > 0) {
            if ((pids[0].revents & (POLLERR | POLLHUP | POLLNVAL)) > 0) {
                ttg = true;
                syslog(LOG_ERR, "%s","Error with main listening socket.  Exiting.");
                continue;
            }
            if ((pids[0].revents & POLLIN) > 0) {
                // socket ready to accept() a connection
                failurecount = 0;  // something is clearly working so reset count
                if (freechildren < 1 && numchildren < o.max_children) {
                    if (!preforked) {
                        rc = preFork(1);
                        if (rc < 0) {
                            syslog(LOG_ERR, "%s","Error forking 1 extra process.");
                            failurecount++;
                        }
                        preforked = true;
                    }
                    qscopy = quicksleep;  // use copy as select() can modify it
                    select(0, NULL, NULL, NULL, &qscopy);  // is a very quick sleep()
                    continue;
                }
                if (freechildren > 0) {
                    tellChildAccept(getFreeChild());
                }
                else {
                    qscopy = quicksleep;  // use copy as select() can modify it
                    select(0, NULL, NULL, NULL, &qscopy);  // is a very quick sleep()
                }
            }
        }

        if (freechildren < o.minspare_children && !preforked && numchildren < o.max_children) {
            rc = preFork(o.prefork_children);
            preforked = true;
            if (rc < 0) {
                syslog(LOG_ERR, "%s","Error forking preforkchildren extra processes.");
                failurecount++;
            }
        }

        if (freechildren <= o.maxspare_children) {
            time(&tmaxspare);
        }
        if (freechildren > o.maxspare_children) {
            time(&tnow);
            if ((tnow - tmaxspare) > (2 * 60)) { // 2 * 60
                cullChildren(freechildren - o.maxspare_children);
            }
        }

    }
    cullChildren(numchildren); // remove the fork pool of spare children
    for (int i = 0; i < o.max_children; i++) {
        if (pids[i + 1].fd != -1) {
            try {
                close(pids[i + 1].fd);
            } catch (exception& e) {}
        }
    }
    if (numchildren > 0) {
        hupAllChildren();
        sleep(2); // give them a small chance to exit nicely before we force
        // hmmmm I wonder if sleep() will get interupted by sigchlds?
    }
    if (numchildren > 0) {
        killAllChildren();
    }
    // we might not giving enough time for defuncts to be created and then
    // mopped but on exit or reload config they'll get mopped up
    sleep(1);
    mopUpAfterKids();



    delete[] childrenpids;
    delete[] childrenstates;
    delete[] pids;  // 3 deletes good, memory leaks bad

    if (failurecount >= 30) {
        syslog(LOG_ERR, "%s","Exiting due to high failure count.");
        #ifdef DGDEBUG
            std::cout << "Exiting due to high failure count." << std::endl;
        #endif
    }

    #ifdef DGDEBUG
        std::cout << "Main parent process exiting." << std::endl;
    #endif
    serversock.close();  // be nice and neat
    if (o.url_cache_number > 0) {
        urllistsock.close();
    }


    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    if (sigaction(SIGTERM, &sa, NULL)) {  // restore sig handler
                                          // in child process
        #ifdef DGDEBUG
            std::cerr << "Error resetting signal for SIGTERM" << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Error resetting signal for SIGTERM");
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGHUP, &sa, NULL)) {  // restore sig handler
                                          // in child process
        #ifdef DGDEBUG
            std::cerr << "Error resetting signal for SIGHUP" << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Error resetting signal for SIGHUP");
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGUSR1, &sa, NULL)) {  // restore sig handler
                                          // in child process
        #ifdef DGDEBUG
            std::cerr << "Error resetting signal for SIGUSR1" << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Error resetting signal for SIGUSR1");
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    if (sigaction(SIGPIPE, &sa, NULL)) {  // restore sig handler
                                          // in child process
        #ifdef DGDEBUG
            std::cerr << "Error resetting signal for SIGPIPE" << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Error resetting signal for SIGPIPE");
    }

    if (sig_term_killall) {
        struct sigaction sa, oldsa;
        memset(&sa, 0, sizeof(sa)); sa.sa_handler = SIG_IGN;
        sigaction(SIGTERM, &sa, &oldsa); // ignore sigterm for us
        kill(0, SIGTERM);  // send everyone in this process group a TERM
                           // which causes them to exit as the default action
                           // but it also seems to send itself a TERM
                           // so we ignore it
        sigaction(SIGTERM, &oldsa, NULL); // restore prev state
    }

    if (reloadconfig) {
        if( o.no_logger == 0 ) {
            ::kill(loggerpid, SIGTERM);  // get rid of logger
        }
        if (o.url_cache_number > 0) {
            ::kill(urllistpid, SIGTERM);  // get rid of url cache
        }
        return 2;
    }
    if (ttg) {
        if( o.no_logger == 0 ) {
            ::kill(loggerpid, SIGTERM);  // get rid of logger
        }
        if (o.url_cache_number > 0) {
            ::kill(urllistpid, SIGTERM);  // get rid of url cache
        }
        return 0;
    }
    if (o.logconerror == 1) {
        syslog(LOG_ERR, "%s","Main parent process exiting.");
    }
    return 1; // It is only possible to reach here with an error
}



// this is used by dansguardian.cpp, to, yep, test connection to the proxy
// If report is true we log the problem.  report as false allows the caller
// to test multiple times with no error
bool FatController::testProxy(std::string proxyip, int proxyport, bool report) {
    int sck_inet, conn;
    Socket sock;
    sck_inet = sock.getFD();
    if (sck_inet == -1) {
        if (report) {
            if (!isDaemonised) {
                std::cerr << "Error creating socket to test proxy connection" << std::endl;
            }
            syslog(LOG_ERR, "%s","Error creating socket to test proxy connection");
        }
        return false;
    }
    conn = sock.connect(proxyip, proxyport); // hmmm, I wonder what this do
    if (conn) {
        if (report) {
            if (!isDaemonised) {
                std::cerr << "Error connecting to parent proxy" << std::endl;
            }
            syslog(LOG_ERR, "%s","Error connecting to parent proxy");
        }
        return false;
    }
//    close(conn);  ** delete when confirmed not needed
    sock.close();
    return true;  // it worked!
}


bool FatController::daemonise(int pidfilefd) {

    if (o.no_daemon == 1) {
        return true;
    }

    #ifdef DGDEBUG
        return true;  // if debug mode is enabled we don't want to detach
    #endif

    if (isDaemonised) {
        return true;  // we are already daemonised so this must be a
                      // reload caused by a HUP
    }
    int nullfd = -1;
    if ((nullfd = open("/dev/null", O_WRONLY, 0)) == -1) {
        syslog(LOG_ERR, "%s","Couldn't open /dev/null");
        return false;
    }

    pid_t pid;
    if ((pid = fork()) < 0) {
        return false;
    }
    else if (pid != 0) {
        if (nullfd != -1) {
            close(nullfd);
        }
        exit(0);  // parent goes bye-bye
    }
    // child continues
    dup2(nullfd, 0);  // stdin
    dup2(nullfd, 1);  // stdout
    dup2(nullfd, 2); // stderr
    close(nullfd);

    setsid();  // become session leader
    chdir("/"); // change working directory
    umask(0); // clear our file mode creation mask

    SysV sysv;
    int rc = sysv.writePIDFile(pidfilefd);  // also closes the fd
    if (rc != 0) {
        syslog(LOG_ERR, "%s","Error writing to the dansguardian.pid file.");
        return false;
    }
    isDaemonised = true;

    return true;
}


int FatController::preFork(int num) {

    if (num < waitingfor) {
        return 3;  // waiting for forks already
    }

    #ifdef DGDEBUG
        std::cout << "attempting to prefork:" << num << std::endl;
    #endif

    int sv[2];
    pid_t  child_pid;

    while(num--)  {
        if (numchildren >= o.max_children) {
            return 2;  // too many - geddit?
        }
        if(socketpair(AF_UNIX,SOCK_STREAM,0,sv) < 0)  {
            return -1; // error
        }

        child_pid = fork();

        if (child_pid == -1) {  // fork failed, for example, if the
                                         // process is not allowed to create
                                         // any more
            syslog(LOG_ERR, "%s","Unable to fork() any more.");
            #ifdef DGDEBUG
                std::cout << "Unable to fork() any more." << std::endl;
                std::cout << strerror(errno) << std::endl;
                std::cout << "numchildren:" << numchildren << std::endl;
            #endif
            failurecount++;  // log the error/failure
                             // A DoS attack on a server allocated
                             // too many children in the conf will
                             // kill the server.  But this is user
                             // error.
            sleep(1);  // need to wait until we have a spare slot
            num--;
            continue;  // Nothing doing, go back to listening
        }
        else if(child_pid == 0) {


            // I am the child - I am alive!
            close(sv[0]);  // we only need our copy of this
            tidyUpForChild();
            if (!dropPrivCompletely()) {
                return -1; //error
            }
            // no need to deallocate memory etc as already done when fork()ed

            int rc = handleConnections(sv[1]);
            try {
                close(sv[1]);  // connection to parent
            } catch (exception& e) {}
            try {
                serversock.close(); // listening connection
            }
            catch (exception& e) {}
            _exit(rc); // baby go bye bye
        }
        else {  // must be parent
            close(sv[1]);
            addChild(getChildSlot(), sv[0], child_pid);
            #ifdef DGDEBUG
                std::cout << "Preforked parent added child to list" << std::endl;
            #endif
        }
    }
    return 1;  // parent returning
}


void FatController::tidyUpForChild() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    if (sigaction(SIGTERM, &sa, NULL)) {  // restore sig handler
                                          // in child process
        #ifdef DGDEBUG
            std::cerr << "Error resetting signal for SIGTERM" << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Error resetting signal for SIGTERM");
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGUSR1, &sa, NULL)) {  // restore sig handler
                                          // in child process
        #ifdef DGDEBUG
            std::cerr << "Error resetting signal for SIGUSR1" << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Error resetting signal for SIGUSR1");
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &sig_hup;
    if (sigaction(SIGHUP, &sa, NULL)) {  // restore sig handler
                                          // in child process
        #ifdef DGDEBUG
            std::cerr << "Error resetting signal for SIGHUP" << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Error resetting signal for SIGHUP");
    }
    // now close open socket pairs don't need

    for (int i = 0; i < o.max_children; i++) {
        if (pids[i + 1].fd != -1) {
            try {
                close(pids[i + 1].fd);
            } catch (exception& e) {}
        }
    }
    delete[] childrenpids;
    delete[] childrenstates;
    delete[] pids;  // 3 deletes good, memory leaks bad
}


int FatController::handleConnections(int pipe) {
    ConnectionHandler h;  // the class that handles the connections
    String ip;
    bool toldparentready = false;
    int cycle = o.maxage_children;
    int stat = 0;
    reloadconfig = false;

    while(cycle-- && !reloadconfig)  {
        if (!toldparentready) {
            if (sendReadyStatus(pipe) == -1) {  // non-blocking (timed)
                #ifdef DGDEBUG
                std::cout << "parent timed out telling it we're ready" << std::endl;
                #endif
                break;  // parent timed out telling it we're ready
                // so either parent gone or problem so lets exit this joint
            }
            toldparentready = true;
        }

        if (!getFDFromParent(pipe)) {// blocks waiting for a few mins
            continue;
        }
        toldparentready = false;

        if (peersockfd < 1 || peersockip.length() < 7) {
            continue;
        }

        h.handleConnection(peersockfd, peersockip, peersockport);  // deal with the connection
        close(peersockfd);
    }
    #ifdef DGDEBUG
        if (reloadconfig) {
            std::cout << "child been told to exit by hup" << std::endl;
        }
    #endif
    if (!toldparentready) {
        stat = 2;
    }
    return stat;
}


int FatController::getChildSlot() {
    int i;
    for(i = 0; i < o.max_children; i++) {
        if (childrenpids[i] == -1) {
            return i;
        }
    }
    return -1;
}


void FatController::addChild(int pos, int fd, pid_t child_pid) {
    childrenpids[pos] = (int)child_pid;
    childrenstates[pos] = 4; // busy waiting for init
    numchildren++;
    busychildren++;
    waitingfor++;
    pids[pos + 1].fd = fd;
}


void FatController::cullChildren(int num) {
    #ifdef DGDEBUG
        cout << "culling childs:" << num << endl;
    #endif
    int i;
    int count = 0;
    for(i = o.max_children - 1; i >= 0; i--) {
        if (childrenstates[i] == 0) {
            kill(childrenpids[i], SIGTERM);
            count++;
            childrenstates[i] = -2; // dieing
            numchildren--;
            try {
                close(pids[i + 1].fd);
            } catch (exception& e) {}
            pids[i + 1].fd = -1;
            if (count >= num) {
                break;
            }
        }
    }
}


void FatController::killAllChildren() {
    #ifdef DGDEBUG
        cout << "killing all childs:" << endl;
    #endif
    for(int i = o.max_children - 1; i >= 0; i--) {
        if (childrenstates[i] >= 0) {
            kill(childrenpids[i], SIGTERM);
            childrenstates[i] = -2; // dieing
            numchildren--;
        }
    }
}


void FatController::hupAllChildren() {
    #ifdef DGDEBUG
        cout << "huping all childs:" << endl;
    #endif
    for(int i = o.max_children - 1; i >= 0; i--) {
        if (childrenstates[i] >= 0) {
            kill(childrenpids[i], SIGHUP);
        }
    }
}


bool FatController::checkKidReadyStatus(int tofind) {
    bool found = false;
    char* buf = new char[5];
    int rc = -1; // for compiler warnings
    for(int f = 1; f <= o.max_children; f++) {

        if (tofind < 1) {
            break;  // no point looping through all if all found
        }
        if (pids[f].fd == -1) {
            continue;
        }
        if ((pids[f].revents & POLLIN) > 0) {
            if (childrenstates[f - 1] < 0) {
                tofind--;
                continue;
            }
            try {
                rc = getLineFromFD(pids[f].fd, buf, 4, 100);
            }
            catch (exception& e) {
                kill(childrenpids[f - 1], SIGTERM);
                deleteChild(childrenpids[f - 1], -1);
                tofind--;
                continue;
            }
            if (rc > 0) {
                if (buf[0] == '2') {
                    if (childrenstates[f - 1] == 4) {
                        waitingfor--;
                    }
                    childrenstates[f - 1] = 0;
                    busychildren--;
                    tofind--;
                }
            }
            else { // child -> parent communications failure so kill it
                kill(childrenpids[f - 1], SIGTERM);
                deleteChild(childrenpids[f - 1], -1);
                tofind--;
            }
        }
        if (childrenstates[f - 1] == 0) {
            found = true;
        }
    }
    // if unbusy found then true otherwise false
    delete[] buf;
    return found;
}

void FatController::deleteChild(int child_pid, int stat) {
    int i;
    for(i = 0; i < o.max_children; i++) {
        if (childrenpids[i] == child_pid) {
            childrenpids[i] = -1;
            if (childrenstates[i] == 1) {
                if (stat == 2) {
                     busychildren--;
                }
                else {
                    busychildren--;  // should only happen if kid goes screwy
                }
            }
            if (childrenstates[i] != -2) {  // -2 is when its been culled
                numchildren--;              // so no need to duplicate
                try {
                    close(pids[i + 1].fd);      // closing etc
                } catch (exception& e) {}
                pids[i + 1].fd = -1;
            }
            childrenstates[i] = -1; // unused
            break;
        }
    }  // never should happen that passed pid not know
       // unless its the logger or url cache process in which case we
}       // don't want to do anything anyway and this can only happen
       // when shutting down or restarting


int FatController::getFreeChild() {  // check that there is 1 free done
                                     // before calling
    int i;
    for(i = 0; i < o.max_children; i++) {
        if (childrenstates[i] == 0) {  // not busy (free)
            return i;
        }
    }
    return -1;
}


int FatController::sendReadyStatus(int pipe) { // blocks until timeout
    String message = "2\n";
    try {
        if (!writeToFd(pipe, message.toCharArray(), message.length(), 15)) {
            return -1;
        }
    }
    catch (exception& e) {
        return -1;
    }
    return 0;
}

void FatController::tellChildAccept(int num) {

    int fd = pids[num + 1].fd;
    String message = "G\n";
    try {
        writeToFd(fd, message.toCharArray(), message.length(), 5);
    } catch (exception& e) {
        kill(childrenpids[num], SIGTERM);
        deleteChild(childrenpids[num], -1);
        return;
    }
    char* buf = new char[5];
    try {
        getLineFromFD(fd, buf, 4, 5);
    } catch (exception& e) {
        kill(childrenpids[num], SIGTERM);
        deleteChild(childrenpids[num], -1);
        delete[] buf;
        return;
    }

    delete[] buf; // no need to check as the very fact the child sent
                  // something back is a good sign
    busychildren++;
    childrenstates[num] = 1; // busy

}



bool FatController::getFDFromParent(int fd) {
    String message;
    char* buf = new char[5];
    int rc;
    try {
        rc = getLineFromFD(fd, buf, 4, 360);  // blocks for a few mins
    }
    catch (exception& e) {
        delete[] buf;

        reloadconfig = true;
        return false;
    }
    // that way if child does nothing for a long time it will eventually
    // exit reducing the forkpool depending on o.maxage_children which is
    // usually 500 so max time a child hangs around is lonngggg
    // it needs to be a long block to stop the machine needing to page in
    // the process
    if (rc < 1) {
        delete[] buf;
        return false;
    }
    if (buf[0] != 'G') {
        delete[] buf;
        return false;
    }
    delete[] buf;


    sockaddr_in peer_adr_inet;
    socklen_t peer_adr_inet_length = sizeof(struct sockaddr_in);
    // OS X defines accept as:
    // int accept(int s, struct sockaddr *addr, int *addrlen);
    // but everyone else as:
    // int accept(int s, struct sockaddr *addr, socklen_t *addrlen);
#ifdef __DARWIN
    peersockfd = accept(serversock.getFD(), (struct sockaddr *) &peer_adr_inet, (int *)&peer_adr_inet_length);
#else
    peersockfd = accept(serversock.getFD(), (struct sockaddr *) &peer_adr_inet, &peer_adr_inet_length);
#endif
    peersockip = inet_ntoa(peer_adr_inet.sin_addr);
    peersockport = ntohs(peer_adr_inet.sin_port);
    // get the new socket which
    // describes the accepted connection

    try {
        writeToFd(fd, "K\n", 2, 10);  // need to make parent wait for OK
                                  // so effectively providing a lock
    }
    catch (exception& e) {
        try {
          close(peersockfd);
        }
        catch (exception& e) {
        }
        return false;
    }

    if (peersock.getFD() < 1) {
        if (o.logconerror == 1) {
            syslog(LOG_INFO, "%s","Error accepting. (Ignorable)");
        }
        return false;
    }
    return true;
}



bool FatController::writeToFd(int fd, const char* buff, int len, int timeout) {
    int actuallysent = 0;
    int sent;
    while (actuallysent < len) {
        if (!readyForOutput(fd, timeout)) {
            return false;
        }
        sent = send(fd, buff + actuallysent, len - actuallysent, 0);
        if (sent < 0) {
            if (errno == EINTR && !reloadconfig) {
                continue;  // was interupted by signal so restart
            }
            return false;
        }
        if (sent == 0) {
            return false; // other end is closed
        }
        actuallysent += sent;
    }
    return true;
}

bool FatController::readyForOutput(int fd, int timeout) {
    fd_set fdSet;
    FD_ZERO(&fdSet);  // clear the set
    FD_SET(fd, &fdSet);  // add fd to the set
    timeval t;  // timeval struct
    t.tv_sec = timeout;
    t.tv_usec = 0;
    if (selectEINTR(fd + 1, NULL, &fdSet, NULL, &t) < 1) {
        return false;  // on timeout or error
    }
    return true;
}

int FatController::getLineFromFD(int fd, char* buff, int size, int timeout) {
    if (!checkForInput(fd, timeout)) {
        return -1;
    }
    char b[1];
    int rc = -1; // only set to prevent compiler warnings
    int i = 0;
    b[0] = 0;
    while(i < (size - 1)) {
        while (true) {
            if (!checkForInput(fd, 12)) {
                break;
            }
            rc = recv(fd, b, 1, 0);
            if (rc < 0) {
                if (errno == EINTR && !reloadconfig) {
                    continue;
                }
            }
            break;
        }
        if (rc < 0) {
            return -1; // error
        }
        if (rc == 0) {  // eof, other end closed so
            b[0] = '\n'; // force it to return what read
        }
        buff[i] = b[0];
        i++;
        if (b[0] == '\n') {
            buff[i - 1] = '\0';
            break;
        }
    }
    buff[i] = '\0';
    return i;
}

bool FatController::checkForInput(int fd, int timeout) {
    fd_set fdSet;
    FD_ZERO(&fdSet);  // clear the set
    FD_SET(fd, &fdSet);  // add fd to the set
    if (timeout > 0) {
        timeval t;  // timeval struct
        t.tv_sec = timeout;
        t.tv_usec = 0;
        if (selectEINTR(fd + 1, &fdSet, NULL, NULL, &t) < 1) {
            return false;
        }
    }
    if (selectEINTR(fd + 1, &fdSet, NULL, NULL, NULL) < 1) {
        return false;
    }
    return true; // Innnppppuuuuttt!!!! </#5>
}

int FatController::selectEINTR(int numfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, struct timeval * timeout) {
    int rc;
    while (true) {  // using the while as a restart point with continue
        rc = select(numfds, readfds, writefds, exceptfds, timeout);
        if (rc < 0) {
            if (errno == EINTR && !reloadconfig) {
                continue;  // was interupted by a signal so restart
            }
        }
        break;  // end the while
    }
    return rc;  // return status
}


void FatController::mopUpAfterKids() {
    pid_t pid;
    int stat_val;
    while(true) {
        pid = waitpid(-1, &stat_val, WNOHANG);
        if (pid < 1) {
            break;
        }
        if (WIFEXITED(stat_val) ) {  // exited normally
            deleteChild((int)pid, WEXITSTATUS(stat_val));
        }
        else {
            deleteChild((int)pid, -1);
        }
    }
}


int FatController::logListener(std::string log_location, int logconerror) {
    #ifdef DGDEBUG
        std::cout << "log listener started" << std::endl;
    #endif
    if (!dropPrivCompletely()) {
        return 1; //error
    }
    UDSocket ipcpeersock;  // the socket which will contain the ipc connection
    ipcpeersock.close(); // don't need default fd as will be accept()ing
    int rc, ipcsockfd;
    char* logline = new char[8192];

    ofstream logfile(log_location.c_str(), ios::app);
    if (logfile.fail()) {
        syslog(LOG_ERR, "%s","Error opening/creating log file.");
        #ifdef DGDEBUG
            std::cout << "Error opening/creating log file: " << log_location << std::endl;
        #endif
        return 1;  // return with error
    }

    ipcsockfd = ipcsock.getFD();

    fd_set fdSet;  // our set of fds (only 1) that select monitors for us
    fd_set fdcpy;  // select modifes the set so we need to use a copy
    FD_ZERO(&fdSet);  // clear the set
    FD_SET(ipcsockfd, &fdSet);  // add ipcsock to the set

    while (true) {  // loop, essentially, for ever

        fdcpy = fdSet;  // take a copy
        rc = select(ipcsockfd + 1, &fdcpy, NULL, NULL, NULL);  // block
                                                  // until something happens
        if (rc < 0) {  // was an error
            if (errno == EINTR) {
                continue;  // was interupted by a signal so restart
            }
            if (logconerror == 1) {
                syslog(LOG_ERR, "%s","ipc rc<0. (Ignorable)");
            }
            continue;
        }
        if (rc < 1) {
            if (logconerror == 1) {
                syslog(LOG_ERR, "%s","ipc rc<0. (Ignorable)");
            }
            continue;
        }
        if (FD_ISSET(ipcsockfd, &fdcpy)) {
            #ifdef DGDEBUG
                std::cout << "received a log request" << std::endl;
            #endif
            ipcpeersock = ipcsock.accept();
            if (ipcpeersock.getFD() < 0) {
                ipcpeersock.close();
                if (logconerror == 1) {
                    syslog(LOG_ERR, "%s","Error accepting ipc. (Ignorable)");
                }
                continue; // if the fd of the new socket < 0 there was error
                          // but we ignore it as its not a problem
            }
            try {
                rc = ipcpeersock.getline(logline, 8192, 3);  // throws on err
            } catch (exception& e) {
                ipcpeersock.close();
                if (logconerror == 1) {
                    syslog(LOG_ERR, "%s","Error reading ipc. (Ignorable)");
                }
                continue;
            }
            logfile << logline << std::endl;  // append the line
            #ifdef DGDEBUG
                std::cout << logline << std::endl;
            #endif
            ipcpeersock.close();  // close the connection
            #ifdef DGDEBUG
                std::cout << "logged" << std::endl;
            #endif
            continue;  // go back to listening
        }
        // should never get here
        syslog(LOG_ERR, "%s","Something wicked has ipc happened");

    }
    delete[] logline;
    logfile.close();  // close the file and
    ipcsock.close();  // be nice and neat
    return 1; // It is only possible to reach here with an error

}


extern "C" {  // The kernel knows nothing of objects so
              // we have to have a lump of c


    void sig_term(int signo) {
        sig_term_killall = true;
        ttg = true;  // its time to go
    }

    void sig_termsafe(int signo) {
        ttg = true;  // its time to go
    }

    void sig_hup(int signo) {
        reloadconfig = true;
        #ifdef DGDEBUG
            std::cout << "HUP received." << std::endl;
        #endif
    }

    void sig_usr1(int signo) {
        gentlereload = true;
        #ifdef DGDEBUG
            std::cout << "USR1 received." << std::endl;
        #endif
    }

}

int FatController::urlListListener(int logconerror) {
    #ifdef DGDEBUG
            std::cout << "url listener started" << std::endl;
    #endif
    if (!dropPrivCompletely()) {
        return 1; //error
    }
    UDSocket ipcpeersock;  // the socket which will contain the ipc connection
    ipcpeersock.close(); // don't need default fd as will be accept()ing
    int rc, ipcsockfd;
    char* logline = new char[32000];
    String reply;
    DynamicURLList urllist;
    #ifdef DGDEBUG
            std::cout << "setting url list size-age:" << o.url_cache_number << "-" << o.url_cache_age << std::endl;
    #endif
    urllist.setListSize(o.url_cache_number, o.url_cache_age);
    ipcsockfd = urllistsock.getFD();
    #ifdef DGDEBUG
            std::cout << "url ipcsockfd:" << ipcsockfd << std::endl;
    #endif

    fd_set fdSet;  // our set of fds (only 1) that select monitors for us
    fd_set fdcpy;  // select modifes the set so we need to use a copy
    FD_ZERO(&fdSet);  // clear the set
    FD_SET(ipcsockfd, &fdSet);  // add ipcsock to the set

    #ifdef DGDEBUG
            std::cout << "url listener entering select()" << std::endl;
    #endif
    while (true) {  // loop, essentially, for ever

        fdcpy = fdSet;  // take a copy

        rc = select(ipcsockfd + 1, &fdcpy, NULL, NULL, NULL);  // block
                                                  // until something happens
        #ifdef DGDEBUG
            std::cout << "url listener select returned" << std::endl;
        #endif
        if (rc < 0) {  // was an error
            if (errno == EINTR) {
                continue;  // was interupted by a signal so restart
            }
            if (logconerror == 1) {
                syslog(LOG_ERR, "%s","url ipc rc<0. (Ignorable)");
            }
            continue;
        }
        if (FD_ISSET(ipcsockfd, &fdcpy)) {
            #ifdef DGDEBUG
                std::cout << "received an url request" << std::endl;
            #endif
            ipcpeersock = urllistsock.accept();
            if (ipcpeersock.getFD() < 0) {
                ipcpeersock.close();
                if (logconerror == 1) {
                    #ifdef DGDEBUG
                        std::cout << "Error accepting url ipc. (Ignorable)" << std::endl;
                    #endif
                    syslog(LOG_ERR, "%s","Error accepting url ipc. (Ignorable)");
                }
                continue; // if the fd of the new socket < 0 there was error
                          // but we ignore it as its not a problem
            }
            try {
                rc = ipcpeersock.getline(logline, 32000, 3);  // throws on err
            } catch (exception& e) {
                ipcpeersock.close();  // close the connection
                if (logconerror == 1) {
                    #ifdef DGDEBUG
                        std::cout << "Error reading ip ipc. (Ignorable)" << std::endl;
                        std::cerr << e.what() << std::endl;
                    #endif
                    syslog(LOG_ERR, "%s","Error reading ip ipc. (Ignorable)");
                    syslog(LOG_ERR, "%s", e.what());
                }
                continue;
            }
            if (logline[0] == 'F' && logline[1] == ' ') {
                ipcpeersock.close();  // close the connection
                urllist.flush();
                #ifdef DGDEBUG
                    std::cout << "url FLUSH request" << std::endl;
                #endif
                continue;
            }
            if (logline[0] == 'A' && logline[1] == ' ') {
                ipcpeersock.close();  // close the connection
                urllist.addEntry(logline + 2);
                #ifdef DGDEBUG
                    std::cout << "url add request:" << logline << std::endl;
                #endif
                continue;
            }
            #ifdef DGDEBUG
                std::cout << "url search request:" << logline << std::endl;
            #endif
            if (urllist.inURLList(logline)) {
                reply = "Y\n";
            }
            else {
                reply = "N\n";
            }
            try {
                ipcpeersock.writeString(reply.toCharArray());
            } catch (exception& e) {
                ipcpeersock.close();  // close the connection
                if (logconerror == 1) {
                    syslog(LOG_ERR, "%s","Error writing url ipc. (Ignorable)");
                    syslog(LOG_ERR, "%s", e.what());
                }
                continue;
            }
            ipcpeersock.close();  // close the connection
            #ifdef DGDEBUG
                std::cout << "url list reply:" << reply << std::endl;
            #endif
            continue;  // go back to listening
        }
   }
    delete[] logline;
    urllistsock.close();  // be nice and neat
    return 1; // It is only possible to reach here with an error

}

bool FatController::dropPrivCompletely() {

    // This is done to solve the problem where the total processes for the
    // uid rather than euid is taken for RLIMIT_NPROC and so can't fork()
    // as many as expected.
    // It is also more secure.
    //
    // Suggested fix by Lawrence Manning Tue 25th February 2003
    //

    int rc = seteuid(o.root_user);  // need to be root again to drop properly
    if (rc == -1) {
        syslog(LOG_ERR, "%s","Unable to seteuid(suid)");
        #ifdef DGDEBUG
            std::cout << strerror(errno) << std::endl;
        #endif
        return false;  // setuid failed for some reason so exit with error
    }
    rc = setuid(o.proxy_user);
    if (rc == -1) {
        syslog(LOG_ERR, "%s","Unable to setuid()");
        return false;  // setuid failed for some reason so exit with error
    }
    return true;
}

void FatController::flushURLCache() {
    if (o.url_cache_number < 1) {
        return;  // no cache running to flush
    }
    UDSocket fipcsock;
    if (fipcsock.getFD() < 0) {
        syslog(LOG_ERR, "%s","Error creating ipc socket to url cache for flush");
        return;
    }
    if (fipcsock.connect((char*) o.urlipc_filename.c_str()) < 0) {  // conn to dedicated url cach proc
        syslog(LOG_ERR, "%s","Error connecting via ipc to url cache for flush");
        #ifdef DGDEBUG
            std::cout << "Error connecting via ipc to url cache for flush" << std::endl;
        #endif
        return;
    }
    String request = "F \n";
    try {
        fipcsock.writeString(request.toCharArray());  // throws on err
    }
    catch (exception& e) {
        #ifdef DGDEBUG
            std::cerr << "Exception flushing url cache" << std::endl;
            std::cerr << e.what() << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Exception flushing url cache");
        syslog(LOG_ERR, "%s", e.what());
    }
    fipcsock.close();
}
