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


// INCLUDES

#include "platform.h"

#include "FatController.hpp"
#include "ConnectionHandler.hpp"
#include "DynamicURLList.hpp"
#include "DynamicIPList.hpp"
#include "String.hpp"
#include "SocketArray.hpp"
#include "UDSocket.hpp"
#include "SysV.hpp"

#include <syslog.h>
#include <csignal>
#include <sys/stat.h>
#include <pwd.h>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <sys/time.h>
#include <sys/poll.h>

#ifdef __SEGV_BACKTRACE
#include <execinfo.h>
#endif

#ifdef __GCCVER3
#include <istream>
#include <map>
#else
#include <istream.h>
#include <map.h>
#endif


// GLOBALS

// these are used in signal handlers - "volatile" indicates they can change at
// any time, and therefore value reading on them does not get optimised. since
// the values can get altered by outside influences, this is useful.
static volatile bool ttg = false;
static volatile bool gentlereload = false;
static volatile bool sig_term_killall = false;
volatile bool reloadconfig = false;

extern OptionContainer o;
extern bool is_daemonised;

int numchildren;  // to keep count of our children
int busychildren;  // to keep count of our children
int freechildren;  // to keep count of our children
int waitingfor;  // num procs waiting for to be preforked
int *childrenpids;  // so when one exits we know who
int *childrenstates;  // so we know what they're up to
struct pollfd *pids;
UDSocket **childsockets;
int failurecount;
int serversocketcount;
SocketArray serversockets;  // the sockets we will listen on for connections
UDSocket loggersock;  // the unix domain socket to be used for ipc with the forked children
UDSocket urllistsock;
UDSocket iplistsock;
Socket *peersock(NULL);  // the socket which will contain the connection

String peersockip;  // which will contain the connection ip
int peersockport;  // port connection originates


// DECLARATIONS

// Signal handlers
extern "C"
{
	void sig_chld(int signo);
	void sig_term(int signo);  // This is so we can kill our children
	void sig_termsafe(int signo);  // This is so we can kill our children safer
	void sig_hup(int signo);  // This is so we know if we should re-read our config.
	void sig_usr1(int signo);  // This is so we know if we should re-read our config but not kill current connections
	void sig_childterm(int signo);
#ifdef __SEGV_BACKTRACE
	void sig_segv(int signo/*, siginfo_t* info, void* p*/); // Generate a backtrace on segfault
#endif
}

// logging & URL cache processes
int log_listener(std::string log_location, bool logconerror, bool logsyslog);
int url_list_listener(bool logconerror);
// send flush message over URL cache IPC socket
void flush_urlcache();

// fork off into background
bool daemonise();
// create specified amount of child processes
int prefork(int num);

// check child process is ready to start work
bool check_kid_readystatus(int tofind);
// child process informs parent process that it is ready
int send_readystatus(UDSocket &pipe);

// child process main loop - sits waiting for incoming connections & processes them
int handle_connections(UDSocket &pipe);
// tell a non-busy child process to accept the incoming connection
void tellchild_accept(int num, int whichsock);
// child process accept()s connection from server socket
bool getsock_fromparent(UDSocket &fd/*, int &socknum*/);

// add known info about a child to our info lists
void addchild(int pos, int fd, pid_t child_pid);
// find ID of first non-busy child
int getfreechild();
// find an empty slot in our child info lists
int getchildslot();
// cull up to this number of non-busy children
void cullchildren(int num);
// delete this child from our info lists (pass in child's recorded exit value - currently unused)
void deletechild(int child_pid, int stat);
// clean up any dead child processes (calls deletechild with exit values)
void mopup_afterkids();

// tidy up resources for a brand new child process (uninstall signal handlers, delete copies of unnecessary data, etc.)
void tidyup_forchild();

// send SIGTERM or SIGHUP to call children
void kill_allchildren();
void hup_allchildren();

// setuid() to proxy user (not just seteuid()) - used by child processes & logger/URL cache for security & resource usage reasons
bool drop_priv_completely();



// IMPLEMENTATION

// signal handlers
extern "C"
{	// The kernel knows nothing of objects so
	// we have to have a lump of c
	void sig_term(int signo)
	{
		sig_term_killall = true;
		ttg = true;  // its time to go
	}
	void sig_termsafe(int signo)
	{
		ttg = true;  // its time to go
	}
	void sig_hup(int signo)
	{
		reloadconfig = true;
#ifdef DGDEBUG
		std::cout << "HUP received." << std::endl;
#endif
	}
	void sig_usr1(int signo)
	{
		gentlereload = true;
#ifdef DGDEBUG
		std::cout << "USR1 received." << std::endl;
#endif
	}
	void sig_childterm(int signo)
	{
#ifdef DGDEBUG
		std::cout << "TERM recieved." << std::endl;
#endif
		_exit(0);
	}
#ifdef __SEGV_BACKTRACE
	void sig_segv(int signo/*, siginfo_t* info, void* p*/)
	{
#ifdef DGDEBUG
		std::cout << "SEGV received." << std::endl;
#endif
/*		char* str = "UNKNOWN";
		switch (info->si_code)
		{
#ifdef SEGV_MAPERR
		case SEGV_MAPERR: str = "SEGV_MAPERR"; break;
#endif
#ifdef SEGV_ACCERR
		case SEGV_ACCERR: str = "SEGV_ACCERR"; break;
#endif
		}
		syslog(LOG_ERR, "Segfault: %s", str);*/
		// Generate backtrace
		void *addresses[10];
		char **strings;
		int c = backtrace(addresses, 10);
		strings = backtrace_symbols(addresses,c);
		printf("backtrace returned: %d\n", c);
		for (int i = 0; i < c; i++) {
			syslog(LOG_ERR, "%d: %X ", i, (int)addresses[i]);
			syslog(LOG_ERR, "%s", strings[i]);
		}
		// Kill off the current process
		raise(SIGTERM);
	}
#endif
}

// this is used by dansguardian.cpp, to, yep, test connection to the proxy
// If report is true we log the problem.  report as false allows the caller
// to test multiple times with no error
bool fc_testproxy(std::string proxyip, int proxyport, bool report)
{
	int sck_inet, conn;
	Socket sock;
	sck_inet = sock.getFD();
	if (sck_inet == -1) {
		if (report) {
			if (!is_daemonised) {
				std::cerr << "Error creating socket to test proxy connection" << std::endl;
			}
			syslog(LOG_ERR, "%s", "Error creating socket to test proxy connection");
		}
		return false;
	}
	conn = sock.connect(proxyip, proxyport);  // hmmm, I wonder what this do
	if (conn) {
		if (report) {
			if (!is_daemonised) {
				std::cerr << "Error connecting to parent proxy" << std::endl;
			}
			syslog(LOG_ERR, "%s", "Error connecting to parent proxy");
		}
		return false;
	}
	sock.close();
	return true;  // it worked!
}

// completely drop our privs - i.e. setuid, not just seteuid
bool drop_priv_completely()
{
	// This is done to solve the problem where the total processes for the
	// uid rather than euid is taken for RLIMIT_NPROC and so can't fork()
	// as many as expected.
	// It is also more secure.
	//
	// Suggested fix by Lawrence Manning Tue 25th February 2003
	//

	int rc = seteuid(o.root_user);  // need to be root again to drop properly
	if (rc == -1) {
		syslog(LOG_ERR, "%s", "Unable to seteuid(suid)");
#ifdef DGDEBUG
		std::cout << strerror(errno) << std::endl;
#endif
		return false;  // setuid failed for some reason so exit with error
	}
	rc = setuid(o.proxy_user);
	if (rc == -1) {
		syslog(LOG_ERR, "%s", "Unable to setuid()");
		return false;  // setuid failed for some reason so exit with error
	}
	return true;
}

// signal the URL cache to flush via IPC
void flush_urlcache()
{
	if (o.url_cache_number < 1) {
		return;  // no cache running to flush
	}
	UDSocket fipcsock;
	if (fipcsock.getFD() < 0) {
		syslog(LOG_ERR, "%s", "Error creating ipc socket to url cache for flush");
		return;
	}
	if (fipcsock.connect((char *) o.urlipc_filename.c_str()) < 0) {	// conn to dedicated url cach proc
		syslog(LOG_ERR, "%s", "Error connecting via ipc to url cache for flush");
#ifdef DGDEBUG
		std::cout << "Error connecting via ipc to url cache for flush" << std::endl;
#endif
		return;
	}
	String request("f\n");
	try {
		fipcsock.writeString(request.toCharArray());  // throws on err
	}
	catch(exception & e) {
#ifdef DGDEBUG
		std::cerr << "Exception flushing url cache" << std::endl;
		std::cerr << e.what() << std::endl;
#endif
		syslog(LOG_ERR, "%s", "Exception flushing url cache");
		syslog(LOG_ERR, "%s", e.what());
	}
}

// Fork ourselves off into the background
bool daemonise()
{

	if (o.no_daemon) {
		return true;
	}
#ifdef DGDEBUG
	return true;  // if debug mode is enabled we don't want to detach
#endif

	if (is_daemonised) {
		return true;  // we are already daemonised so this must be a
		// reload caused by a HUP
	}
	int nullfd = -1;
	if ((nullfd = open("/dev/null", O_WRONLY, 0)) == -1) {
		syslog(LOG_ERR, "%s", "Couldn't open /dev/null");
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
	dup2(nullfd, 2);  // stderr
	close(nullfd);

	setsid();  // become session leader
	chdir("/");  // change working directory
	umask(0);  // clear our file mode creation mask

	is_daemonised = true;

	return true;
}


// *
// *
// *  child process code
// *
// *

// prefork specified num of children and set them handling connections
int prefork(int num)
{
	if (num < waitingfor) {
		return 3;  // waiting for forks already
	}
#ifdef DGDEBUG
	std::cout << "attempting to prefork:" << num << std::endl;
#endif
	int sv[2];
	pid_t child_pid;
	while (num--) {
		if (numchildren >= o.max_children) {
			return 2;  // too many - geddit?
		}
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
			return -1;  // error
		}

		child_pid = fork();

		if (child_pid == -1) {	// fork failed, for example, if the
			// process is not allowed to create
			// any more
			syslog(LOG_ERR, "%s", "Unable to fork() any more.");
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
		else if (child_pid == 0) {
			// I am the child - I am alive!
			close(sv[0]);  // we only need our copy of this
			tidyup_forchild();
			if (!drop_priv_completely()) {
				return -1;  //error
			}
			// no need to deallocate memory etc as already done when fork()ed

			// right - let's do our job!
			UDSocket sock(sv[1]);
			int rc = handle_connections(sock);

			// ok - job done, time to tidy up.
			_exit(rc);  // baby go bye bye
		} else {
			// I am the parent
			// close the end of the socketpair we don't need
			close(sv[1]);
			// add the child and its FD/PID to an empty child slot
			addchild(getchildslot(), sv[0], child_pid);
#ifdef DGDEBUG
			std::cout << "Preforked parent added child to list" << std::endl;
#endif
		}
	}
	return 1;  // parent returning
}

// cleaning up for brand new child processes - only the parent needs the signal handlers installed, and so forth
void tidyup_forchild()
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &sig_childterm;
	if (sigaction(SIGTERM, &sa, NULL)) {	// restore sig handler
		// in child process
#ifdef DGDEBUG
		std::cerr << "Error resetting signal for SIGTERM" << std::endl;
#endif
		syslog(LOG_ERR, "%s", "Error resetting signal for SIGTERM");
	}
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGUSR1, &sa, NULL)) {	// restore sig handler
		// in child process
#ifdef DGDEBUG
		std::cerr << "Error resetting signal for SIGUSR1" << std::endl;
#endif
		syslog(LOG_ERR, "%s", "Error resetting signal for SIGUSR1");
	}
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &sig_hup;
	if (sigaction(SIGHUP, &sa, NULL)) {	// restore sig handler
		// in child process
#ifdef DGDEBUG
		std::cerr << "Error resetting signal for SIGHUP" << std::endl;
#endif
		syslog(LOG_ERR, "%s", "Error resetting signal for SIGHUP");
	}
	// now close open socket pairs don't need

	for (int i = 0; i < o.max_children; i++) {
		if (pids[i].fd != -1) {
			delete childsockets[i];
		}
	}
	delete[]childrenpids;
	delete[]childrenstates;
	delete[]childsockets;
	delete[]pids;  // 4 deletes good, memory leaks bad
}

// send Ready signal to parent process over the socketpair (used in handle_connections)
int send_readystatus(UDSocket &pipe)
{				// blocks until timeout
	String message("2\n");
	try {
		if (!pipe.writeToSocket(message.toCharArray(), message.length(), 0, 15, true, true)) {
			return -1;
		}
	}
	catch(exception & e) {
		return -1;
	}
	return 0;
}

// handle any connections received by this child (also tell parent we're ready each time we become idle)
int handle_connections(UDSocket &pipe)
{
	ConnectionHandler h;  // the class that handles the connections
	String ip;
	bool toldparentready = false;
	int cycle = o.maxage_children;
	int stat = 0;
	reloadconfig = false;

	// stay alive both for the maximum allowed age of child processes, and whilst we aren't supposed to be re-reading configuration
	while (cycle-- && !reloadconfig) {
		if (!toldparentready) {
			if (send_readystatus(pipe) == -1) {	// non-blocking (timed)
#ifdef DGDEBUG
				std::cout << "parent timed out telling it we're ready" << std::endl;
#endif
				break;  // parent timed out telling it we're ready
				// so either parent gone or problem so lets exit this joint
			}
			toldparentready = true;
		}

		//int socknum;

		if (!getsock_fromparent(pipe/*, socknum*/)) {	// blocks waiting for a few mins
			continue;
		}
		toldparentready = false;

		// now check the connection is actually good
		if (peersock->getFD() < 0 || peersockip.length() < 7) {
			if (o.logconerror)
				syslog(LOG_INFO, "Error accepting. (Ignorable)");
			continue;
		}

		h.handleConnection(*peersock, peersockip, peersockport/*, socknum*/);  // deal with the connection
		delete peersock;
	}
	if (!(++cycle) && o.logchildprocs)
		syslog(LOG_ERR, "Child has handled %d requests and is exiting", o.maxage_children);
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

// the parent process recieves connections - children receive notifications of this over their socketpair, and accept() them for handling
bool getsock_fromparent(UDSocket &fd/*, int &socknum*/)
{
	String message;
	char buf;
	int rc;
	try {
		rc = fd.readFromSocket(&buf, 1, 0, 360, true, true);  // blocks for a few mins
	}
	catch(exception & e) {
		// whoop! we received a SIGHUP. we should reload our configuration - and no, we didn't get an FD.

		reloadconfig = true;
		return false;
	}
	// that way if child does nothing for a long time it will eventually
	// exit reducing the forkpool depending on o.maxage_children which is
	// usually 500 so max time a child hangs around is lonngggg
	// it needs to be a long block to stop the machine needing to page in
	// the process

	// check the message from the parent
	if (rc < 1) {
		return false;
	}

	// woo! we have a connection. accept it.
	//socknum = (int)buf;
	peersock = serversockets[buf]->accept();
	peersockip = peersock->getPeerIP();
	peersockport = peersock->getPeerSourcePort();

	try {
		fd.writeToSockete("K", 1, 0, 10, true);  // need to make parent wait for OK
		// so effectively providing a lock
	}
	catch(exception & e) {
		if (o.logconerror)
			syslog(LOG_ERR, "Error telling parent we accepted: %s", e.what());
		peersock->close();
		return false;
	}

	return true;
}


// *
// *
// * end of child process code
// *
// *


// *
// *
// * start of child process handling (minus prefork)
// *
// *

// look for any dead children, and clean them up
void mopup_afterkids()
{
	pid_t pid;
	int stat_val;
	while (true) {
		pid = waitpid(-1, &stat_val, WNOHANG);
		if (pid < 1) {
			break;
		}
		if (WIFEXITED(stat_val)) {	// exited normally
			deletechild((int) pid, WEXITSTATUS(stat_val));
		} else {
			deletechild((int) pid, -1);
		}
	}
}

// get a free slot in out PID list, if there is one - return -1 if not
int getchildslot()
{
	int i;
	for (i = 0; i < o.max_children; i++) {
		if (childrenpids[i] == -1) {
			return i;
		}
	}
	return -1;
}

// add the given child, including FD & PID, to the given slot in our lists
void addchild(int pos, int fd, pid_t child_pid)
{
	childrenpids[pos] = (int) child_pid;
	childrenstates[pos] = 4;  // busy waiting for init
	numchildren++;
	busychildren++;
	waitingfor++;
	pids[pos].fd = fd;
	UDSocket* sock = new UDSocket(fd);
	childsockets[pos] = sock;
}

// kill give number of non-busy children
void cullchildren(int num)
{
#ifdef DGDEBUG
	cout << "culling childs:" << num << endl;
#endif
	int i;
	int count = 0;
	for (i = o.max_children - 1; i >= 0; i--) {
		if (childrenstates[i] == 0) {
			kill(childrenpids[i], SIGTERM);
			count++;
			childrenstates[i] = -2;  // dieing
			numchildren--;
			delete childsockets[i];
			childsockets[i] = NULL;
			pids[i].fd = -1;
			if (count >= num) {
				break;
			}
		}
	}
}

// send SIGTERM to all child processes
void kill_allchildren()
{
#ifdef DGDEBUG
	cout << "killing all childs:" << endl;
#endif
	for (int i = o.max_children - 1; i >= 0; i--) {
		if (childrenstates[i] >= 0) {
			kill(childrenpids[i], SIGTERM);
			childrenstates[i] = -2;  // dieing
			numchildren--;
			delete childsockets[i];
			childsockets[i] = NULL;
			pids[i].fd = -1;
		}
	}
}

// send SIGHUP to all child processes
void hup_allchildren()
{
#ifdef DGDEBUG
	cout << "huping all childs:" << endl;
#endif
	for (int i = o.max_children - 1; i >= 0; i--) {
		if (childrenstates[i] >= 0) {
			kill(childrenpids[i], SIGHUP);
		}
	}
}

// attempt to receive the message from the child's send_readystatus call
bool check_kid_readystatus(int tofind)
{
	bool found = false;
	char *buf = new char[5];
	int rc = -1;  // for compiler warnings
	for (int f = 0; f < o.max_children; f++) {

		if (tofind < 1) {
			break;  // no point looping through all if all found
		}
		if (pids[f].fd == -1) {
			continue;
		}
		if ((pids[f].revents & POLLIN) > 0) {
			if (childrenstates[f] < 0) {
				tofind--;
				continue;
			}
			try {
				rc = childsockets[f]->getLine(buf, 4, 100, true);
			}
			catch(exception & e) {
				kill(childrenpids[f], SIGTERM);
				deletechild(childrenpids[f], -1);
				tofind--;
				continue;
			}
			if (rc > 0) {
				if (buf[0] == '2') {
					if (childrenstates[f] == 4) {
						waitingfor--;
					}
					childrenstates[f] = 0;
					busychildren--;
					tofind--;
				}
			} else {	// child -> parent communications failure so kill it
				kill(childrenpids[f], SIGTERM);
				deletechild(childrenpids[f], -1);
				tofind--;
			}
		}
		if (childrenstates[f] == 0) {
			found = true;
		} else {
			found = false;
		}
	}
	// if unbusy found then true otherwise false
	delete[]buf;
	return found;
}

// remove child from our PID/FD and slot lists
void deletechild(int child_pid, int stat)
{
	int i;
	for (i = 0; i < o.max_children; i++) {
		if (childrenpids[i] == child_pid) {
			childrenpids[i] = -1;
			if (childrenstates[i] == 1) {
				//if (stat == 2) {
					busychildren--;
				/*} else {
					busychildren--;  // should only happen if kid goes screwy
					// child calls _exit with return value of handle_connections,
					// so if it's nonzero, it means there was an error in there somewhere
				}*/
			}
			if (childrenstates[i] != -2) {	// -2 is when its been culled
				numchildren--;  // so no need to duplicater
				delete childsockets[i];
				childsockets[i] = NULL;
				pids[i].fd = -1;
			}
			childrenstates[i] = -1;  // unused
			break;
		}
	}
	// never should happen that passed pid is not known,
	// unless its the logger or url cache process, in which case we
	// don't want to do anything anyway. and this can only happen
	// when shutting down or restarting.
}

// get the index of the first non-busy child
int getfreechild()
{				// check that there is 1 free done
	// before calling
	int i;
	for (i = 0; i < o.max_children; i++) {
		if (childrenstates[i] == 0) {	// not busy (free)
			return i;
		}
	}
	return -1;
}

// tell given child process to accept an incoming connection
void tellchild_accept(int num, int whichsock)
{
	// include server socket number in message
	try {
		childsockets[num]->writeToSockete((char*)&whichsock, 1, 0, 5, true);
	} catch(exception & e) {
		kill(childrenpids[num], SIGTERM);
		deletechild(childrenpids[num], -1);
		return;
	}

	// check for response from child
	char buf;
	try {
		childsockets[num]->readFromSocket(&buf, 1, 0, 5, false, true);
	} catch(exception & e) {
		kill(childrenpids[num], SIGTERM);
		deletechild(childrenpids[num], -1);
		return;
	}
	// no need to check what it actually contains,
	// as the very fact the child sent something back is a good sign 
	busychildren++;
	childrenstates[num] = 1;  // busy
}


// *
// *
// * end of child process handling code
// *
// *


// *
// *
// * logger, IP list and URL cache main loops
// *
// *


int log_listener(std::string log_location, bool logconerror, bool logsyslog)
{
#ifdef DGDEBUG
	std::cout << "log listener started" << std::endl;
#endif
	if (!drop_priv_completely()) {
		return 1;  //error
	}
	UDSocket* ipcpeersock;  // the socket which will contain the ipc connection
	int rc, ipcsockfd;

	char *logline = new char[8192];

#ifdef __EMAIL
	// Email notification patch by J. Gauthier
	char *vbody_temp = new char[8192];   
   
	map<string,int> violation_map;
	map<string,int> timestamp_map;   
	map<string,string> vbody_map;

	int curv_tmp, stamp_tmp, byuser;
	int smtppid;
#endif
	
	//String where, what, how;
	std::string cr("\n");
   
	std::string where, what, how, cat, clienthost, from, who, mimetype, useragent;
	int port = 80, size = 0, isnaughty = 0, isexception = 0, code = 200;
	int cachehit = 0, wasinfected = 0, wasscanned = 0, naughtiness = 0, filtergroup = 0;
	long tv_sec = 0, tv_usec = 0;
	int contentmodified = 0, urlmodified = 0, headermodified = 0;

	ofstream* logfile = NULL;
	if (!logsyslog) {
		logfile = new ofstream(log_location.c_str(), ios::app);
		if (logfile->fail()) {
			syslog(LOG_ERR, "Error opening/creating log file.");
#ifdef DGDEBUG
			std::cout << "Error opening/creating log file: " << log_location << std::endl;
#endif
			delete logfile;
			return 1;  // return with error
		}
	}

	ipcsockfd = loggersock.getFD();

	fd_set fdSet;  // our set of fds (only 1) that select monitors for us
	fd_set fdcpy;  // select modifies the set so we need to use a copy
	FD_ZERO(&fdSet);  // clear the set
	FD_SET(ipcsockfd, &fdSet);  // add ipcsock to the set
	while (true) {		// loop, essentially, for ever
		fdcpy = fdSet;  // take a copy
		rc = select(ipcsockfd + 1, &fdcpy, NULL, NULL, NULL);  // block

		// until something happens
		if (rc < 0) {	// was an error
			if (errno == EINTR) {
				continue;  // was interupted by a signal so restart
			}
			if (logconerror) {
				syslog(LOG_ERR, "ipc rc<0. (Ignorable)");
			}
			continue;
		}
		if (rc < 1) {
			if (logconerror) {
				syslog(LOG_ERR, "ipc rc<1. (Ignorable)");
			}
			continue;
		}
		if (FD_ISSET(ipcsockfd, &fdcpy)) {
#ifdef DGDEBUG
			std::cout << "received a log request" << std::endl;
#endif
			ipcpeersock = loggersock.accept();
			if (ipcpeersock->getFD() < 0) {
				delete ipcpeersock;
				if (logconerror) {
					syslog(LOG_ERR, "Error accepting ipc. (Ignorable)");
				}
				continue;  // if the fd of the new socket < 0 there was error
				// but we ignore it as its not a problem
			}

			// Formatting code migration from ConnectionHandler
			// and email notification code based on patch provided
			// by J. Gauthier

			// read in the various parts of the log string
			bool error = false;
			int itemcount = 0;
			while(itemcount < (o.log_user_agent ? 24 : 23)) {
				try {
					rc = ipcpeersock->getLine(logline, 8192, 3, true);  // throws on err
					if (rc < 0) {
						delete ipcpeersock;
						if (!is_daemonised)
							std::cout << "Error reading from log socket" <<std::endl;
						syslog(LOG_ERR, "Error reading from log socket");
						error = true;
						break;
					}
					else {
						switch (itemcount) {
						case 0:
							isexception = atoi(logline);
							break;
						case 1:
							cat = logline;
							break;
						case 2:
							isnaughty = atoi(logline);
							break;
						case 3:
							naughtiness = atoi(logline);
							break;
						case 4:
							where = logline;
							break;
						case 5:
							what = logline;
							break;
						case 6:
							how = logline;
							break;
						case 7:
							who = logline;
							break;
						case 8:
							from = logline;
							break;
						case 9:
							port = atoi(logline);
							break;
						case 10:
							wasscanned = atoi(logline);
							break;
						case 11:
							wasinfected = atoi(logline);
							break;
						case 12:
							contentmodified = atoi(logline);
							break;
						case 13:
							urlmodified = atoi(logline);
							break;
						case 14:
							headermodified = atoi(logline);
							break;
						case 15:
							size = atoi(logline);
							break;
						case 16:
							filtergroup = atoi(logline);
							break;
						case 17:
							code = atoi(logline);
							break;
						case 18:
							cachehit = atoi(logline);
							break;
						case 19:
							mimetype = logline;
							break;
						case 20:
							tv_sec = atol(logline);
							break;
						case 21:
							tv_usec = atol(logline);
							break;
						case 22:
							clienthost = logline;
							break;
						case 23:
							useragent = logline;
							break;
						}
					}
				}
				catch(exception & e) {
					delete ipcpeersock;
					if (logconerror)
						syslog(LOG_ERR, "Error reading ipc. (Ignorable)");
					error = true;
					break;
				}
				itemcount++;
			}

			// don't build the log line if we couldn't read all the component parts
			if (error)
				continue;

			// Start building the log line

			if (port != 0 && port != 80) {
				// put port numbers of non-standard HTTP requests into the logged URL
				String newwhere(where.c_str());
				if (newwhere.after("://").contains("/")) {
					String proto, host, path;
					proto = newwhere.before("://");
					host = newwhere.after("://");
					path = host.after("/");
					host = host.before("/");
					newwhere = proto;
					newwhere += "://";
					newwhere += host;
					newwhere += ":";
					newwhere += String((int) port);
					newwhere += "/";
					newwhere += path;
					where = newwhere.toCharArray();
				} else {
					where += ":";
					where += String((int) port).toCharArray();
				}
			}

			// stamp log entries so they stand out/can be searched
			if (isnaughty) {
				what = "*DENIED* " + what;
			}
			else if (isexception && (o.log_exception_hits == 2)) {
				what = "*EXCEPTION* " + what;
			}
		   
			if (wasscanned) {
				if (wasinfected) {
					what = "*INFECTED* " + what;
				} else {
					what = "*SCANNED* " + what;
				}
			}
			if (contentmodified) {
				what = "*CONTENTMOD* " + what;
			}
			if (urlmodified) {
				what = "*URLMOD* " + what;
			}
			if (headermodified) {
				what = "*HEADERMOD* " + what;
			}

			std::string builtline, year, month, day, hour, min, sec, when, ssize, sweight, vbody, utime;
			struct timeval theend;

			// create a string representation of UNIX timestamp if desired
			if (o.log_timestamp || (o.log_file_format == 3)) {
				gettimeofday(&theend, NULL);
				String temp((int) (theend.tv_usec / 1000));
				while (temp.length() < 3) {
					temp = "0" + temp;
				}
				if (temp.length() > 3) {
					temp = "999";
				}
				utime = temp.toCharArray();
				utime = "." + utime;
				utime = String((int) theend.tv_sec).toCharArray() + utime;
			}

			if (o.log_file_format != 3) {
				// "when" not used in format 3, and not if logging timestamps instead
				String temp;
				time_t tnow;  // to hold the result from time()
				struct tm *tmnow;  // to hold the result from localtime()
				time(&tnow);  // get the time after the lock so all entries in order
				tmnow = localtime(&tnow);  // convert to local time (BST, etc)
				year = String(tmnow->tm_year + 1900).toCharArray();
				month = String(tmnow->tm_mon + 1).toCharArray();
				day = String(tmnow->tm_mday).toCharArray();
				hour = String(tmnow->tm_hour).toCharArray();
				temp = String(tmnow->tm_min);
				if (temp.length() == 1) {
					temp = "0" + temp;
				}
				min = temp.toCharArray();
				temp = String(tmnow->tm_sec);
				if (temp.length() == 1) {
					temp = "0" + temp;
				}
				sec = temp.toCharArray();
				when = year + "." + month + "." + day + " " + hour + ":" + min + ":" + sec;
				// truncate long log items
				/*if ((o.max_logitem_length > 0) && (when.length() > o.max_logitem_length))
					when.resize(o.max_logitem_length);*/
				// append timestamp if desired
				if (o.log_timestamp)
					when += " " + utime;
					
			}

			sweight = String(naughtiness).toCharArray();
			ssize = String(size).toCharArray();

			// truncate long log items
			if (o.max_logitem_length > 0) {
				//where.limitLength(o.max_logitem_length);
				if (cat.length() > o.max_logitem_length)
					cat.resize(o.max_logitem_length);
				if (what.length() > o.max_logitem_length)
					what.resize(o.max_logitem_length);
				if (where.length() > o.max_logitem_length)
					where.resize(o.max_logitem_length);
				/*if (who.length() > o.max_logitem_length)
					who.resize(o.max_logitem_length);
				if (from.length() > o.max_logitem_length)
					from.resize(o.max_logitem_length);
				if (how.length() > o.max_logitem_length)
					how.resize(o.max_logitem_length);
				if (ssize.length() > o.max_logitem_length)
					ssize.resize(o.max_logitem_length);*/
			}

			// blank out IP, hostname and username if desired
			if (o.anonymise_logs) {
				who = "";
				from = "0.0.0.0";
				//delete clienthost;
				clienthost[0]=0;
			}

			std::string stringcode(String(code).toCharArray());
			std::string stringgroup(String(filtergroup+1).toCharArray());

			switch (o.log_file_format) {
			case 4:
				builtline = when +"\t"+ who + "\t" + from + "\t" + where + "\t" + what + "\t" + how
					+ "\t" + ssize + "\t" + sweight + "\t" + cat +  "\t" + stringgroup + "\t"
					+ stringcode + "\t" + mimetype + "\t" + clienthost + "\t" + o.fg[filtergroup]->name
					+ "\t" + (o.log_user_agent ? useragent : "-");
				break;
			case 3:
				{
					// as certain bits of info are logged in format 3, their creation is best done here, not in all cases.
					std::string duration, hier, hitmiss;
					long durationsecs, durationusecs;
					durationsecs = (theend.tv_sec - tv_sec);
					durationusecs = theend.tv_usec - tv_usec;
					durationusecs = (durationusecs / 1000) + durationsecs * 1000;
					String temp((int) durationusecs);
					while (temp.length() < 6) {
						temp = " " + temp;
					}
					duration = temp.toCharArray();

					if (code == 403) {
						hitmiss = "TCP_DENIED/403";
					} else {
						if (cachehit) {
							hitmiss = "TCP_HIT/";
							hitmiss += stringcode;
						} else {
							hitmiss = "TCP_MISS/";
							hitmiss += stringcode;
						}
					}
					hier = "DEFAULT_PARENT/";
					hier += o.proxy_ip;

					/*if (o.max_logitem_length > 0) {
						if (utime.length() > o.max_logitem_length)
							utime.resize(o.max_logitem_length);
						if (duration.length() > o.max_logitem_length)
							duration.resize(o.max_logitem_length);
						if (hier.length() > o.max_logitem_length)
							hier.resize(o.max_logitem_length);
						if (hitmiss.length() > o.max_logitem_length)
							hitmiss.resize(o.max_logitem_length);
					}*/

					builtline = utime + " " + duration + " " + ( (clienthost.length() > 0) ? clienthost : from) + " " + hitmiss + " " + ssize + " "
						+ how + " " + where + " " + who + " " + hier + " " + mimetype ;
					break;
				}
			case 2:
				builtline = "\"" + when  +"\",\""+ who + "\",\"" + from + "\",\"" + where + "\",\"" + what + "\",\""
					+ how + "\",\"" + ssize + "\",\"" + sweight + "\",\"" + cat +  "\",\"" + stringgroup + "\",\""
					+ stringcode + "\",\"" + mimetype + "\",\"" + clienthost + "\",\"" + o.fg[filtergroup]->name + "\",\""
					+ (o.log_user_agent ? useragent : "-") + "\"";
				break;
			default:
				builtline = when +" "+ who + " " + from + " " + where + " " + what + " "
					+ how + " " + ssize + " " + sweight + " " + cat +  " " + stringgroup + " "
					+ stringcode + " " + mimetype + " " + clienthost + " " + o.fg[filtergroup]->name + " "
					+ (o.log_user_agent ? useragent : "-");
			}

			if (!logsyslog)
				*logfile << builtline << std::endl;  // append the line
			else
				syslog(LOG_INFO, "%s", builtline.c_str());
#ifdef DGDEBUG
			std::cout << itemcount << " " << builtline << std::endl;
#endif
			delete ipcpeersock;  // close the connection

#ifdef __EMAIL
			// do the notification work here, but fork for speed
			if (o.fg[filtergroup]->use_smtp==true) {

				// run through the gambit to find out of we're sending notification
				// because if we're not.. then fork()ing is a waste of time.

				// virus
				if ((wasscanned && wasinfected) && (o.fg[filtergroup]->notifyav))  {
					if ((smtppid = fork() == 0))  {
						FILE* mail = popen (o.mailer.c_str(), "w");
						if (mail==NULL) {
							syslog(LOG_ERR, "Unable to contact defined mailer.");
						}
						else {
							fprintf(mail, "To: %s\n", o.fg[filtergroup]->avadmin.c_str());
							fprintf(mail, "From: %s\n", o.fg[filtergroup]->mailfrom.c_str());
							fprintf(mail, "Subject: %s\n", o.fg[filtergroup]->avsubject.c_str());
							fprintf(mail, "A virus was detected by DansGuardian.\n\n");
							fprintf(mail, "%-10s%s\n", "Data/Time:", when.c_str());
							if (who != "-")
								fprintf(mail, "%-10s%s\n", "User:", who.c_str());
							fprintf(mail, "%-10s%s (%s)\n", "From:", from.c_str(),  ((clienthost.length() > 0) ? clienthost.c_str() : "-"));
							fprintf(mail, "%-10s%s\n", "Where:", where.c_str());
							// specifically, the virus name comes after message 1100 ("Virus or bad content detected.")
							String swhat(what);
							fprintf(mail, "%-10s%s\n", "Why:", swhat.after(o.language_list.getTranslation(1100)).toCharArray() + 1);
							fprintf(mail, "%-10s%s\n", "Method:", how.c_str());
							fprintf(mail, "%-10s%s\n", "Size:", ssize.c_str());
							fprintf(mail, "%-10s%s\n", "Weight:", sweight.c_str());
							if (cat.c_str()!=NULL)
								fprintf(mail, "%-10s%s\n", "Category:", cat.c_str());
							fprintf(mail, "%-10s%s\n", "Mime type:", mimetype.c_str());
							fprintf(mail, "%-10s%s\n", "Group:", o.fg[filtergroup]->name.c_str());
							fprintf(mail, "%-10s%s\n", "HTTP resp:", stringcode.c_str());

							pclose(mail);
						}
						// child exits
						_exit(0);				 
					}
				}

				// naughty OR virus 
				else if ((isnaughty || (wasscanned && wasinfected)) && (o.fg[filtergroup]->notifycontent)) {
					byuser = o.fg[filtergroup]->byuser;

					// if no violations so far by this user/group,
					// reset threshold counters 
					if (byuser) {
						if (!violation_map[who]) {
							// set the time of the first violation
							timestamp_map[who] = time(0);
							vbody_map[who] = "";
						}
					}
					else if (!o.fg[filtergroup]->current_violations) {
						// set the time of the first violation 
						o.fg[filtergroup]->threshold_stamp = time(0);
						o.fg[filtergroup]->violationbody="";
					}

					// increase per-user or per-group violation count
					if (byuser)
						violation_map[who]++;
					else
						o.fg[filtergroup]->current_violations++;

					// construct email report
					sprintf(vbody_temp, "%-10s%s\n", "Data/Time:", when.c_str());
					vbody+=vbody_temp;

					if ((!byuser) && (who != "-")) {
						sprintf(vbody_temp, "%-10s%s\n", "User:", who.c_str());
						vbody+=vbody_temp;
					}
					sprintf(vbody_temp, "%-10s%s (%s)\n", "From:", from.c_str(),  ((clienthost.length() > 0) ? clienthost.c_str() : "-"));
					vbody+=vbody_temp;
					sprintf(vbody_temp, "%-10s%s\n", "Where:", where.c_str());
					vbody+=vbody_temp;
					sprintf(vbody_temp, "%-10s%s\n", "Why:", what.c_str());
					vbody+=vbody_temp;
					sprintf(vbody_temp, "%-10s%s\n", "Method:", how.c_str());
					vbody+=vbody_temp;
					sprintf(vbody_temp, "%-10s%s\n", "Size:", ssize.c_str());
					vbody+=vbody_temp;
					sprintf(vbody_temp, "%-10s%s\n", "Weight:", sweight.c_str());
					vbody+=vbody_temp;
					if (cat.c_str()!=NULL) {
						sprintf(vbody_temp, "%-10s%s\n", "Category:", cat.c_str());
						vbody+=vbody_temp;
					}
					sprintf(vbody_temp, "%-10s%s\n", "Mime type:", mimetype.c_str());
					vbody+=vbody_temp;
					sprintf(vbody_temp, "%-10s%s\n", "Group:", o.fg[filtergroup]->name.c_str());
					vbody+=vbody_temp;
					sprintf(vbody_temp, "%-10s%s\n\n", "HTTP resp:", stringcode.c_str());
					vbody+=vbody_temp;
					
					// store the report with the group/user
					if (byuser) {
						vbody_map[who]+=vbody;
						curv_tmp = violation_map[who];
						stamp_tmp = timestamp_map[who];
					}
					else {
						o.fg[filtergroup]->violationbody+=vbody;
						curv_tmp = o.fg[filtergroup]->current_violations;
						stamp_tmp = o.fg[filtergroup]->threshold_stamp;
					}

					// if threshold exceeded, send mail
					if (curv_tmp >= o.fg[filtergroup]->violations) {
						if ((o.fg[filtergroup]->threshold == 0) || ( (time(0) - stamp_tmp) <= o.fg[filtergroup]->threshold)) {
							if ((smtppid = fork() == 0)) {
								FILE* mail = popen (o.mailer.c_str(), "w");
								if (mail==NULL) {
									syslog(LOG_ERR, "Unable to contact defined mailer.");
								}
								else {
									fprintf(mail, "To: %s\n", o.fg[filtergroup]->contentadmin.c_str());
									fprintf(mail, "From: %s\n", o.fg[filtergroup]->mailfrom.c_str());

									if (byuser)
										fprintf(mail, "Subject: %s (%s)\n", o.fg[filtergroup]->contentsubject.c_str(), who.c_str());
									else
										fprintf(mail, "Subject: %s\n", o.fg[filtergroup]->contentsubject.c_str());

									fprintf(mail, "%i violation%s ha%s occured within %i seconds.\n",
										curv_tmp,
										(curv_tmp==1)?"":"s",
										(curv_tmp==1)?"s":"ve",									 
										o.fg[filtergroup]->threshold);

									fprintf(mail, "%s\n\n", "This exceeds the notification threshold.");
									if (byuser)
										fprintf(mail, "%s", vbody_map[who].c_str());
									else
										fprintf(mail, "%s", o.fg[filtergroup]->violationbody.c_str());
										pclose(mail);
									}
								// child exits
								_exit(0);
							}
						}
						if (byuser)
							violation_map[who]=0;
						else
							o.fg[filtergroup]->current_violations=0;
					}
				} // end naughty OR virus
			} // end usesmtp
#endif

			continue;  // go back to listening
		}
	}
	// should never get here
	syslog(LOG_ERR, "%s", "Something wicked has ipc happened");

	if (logfile) {
		logfile->close();  // close the file
		delete logfile;
	}
	loggersock.close();
	return 1;  // It is only possible to reach here with an error
}

int url_list_listener(bool logconerror)
{
#ifdef DGDEBUG
	std::cout << "url listener started" << std::endl;
#endif
	if (!drop_priv_completely()) {
		return 1;  //error
	}
	UDSocket* ipcpeersock = NULL;  // the socket which will contain the ipc connection
	int rc, ipcsockfd;
	char *logline = new char[32000];
	char reply;
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
	while (true) {		// loop, essentially, for ever

		fdcpy = fdSet;  // take a copy

		rc = select(ipcsockfd + 1, &fdcpy, NULL, NULL, NULL);  // block
		// until something happens
#ifdef DGDEBUG
		std::cout << "url listener select returned" << std::endl;
#endif
		if (rc < 0) {	// was an error
			if (errno == EINTR) {
				continue;  // was interupted by a signal so restart
			}
			if (logconerror) {
				syslog(LOG_ERR, "%s", "url ipc rc<0. (Ignorable)");
			}
			continue;
		}
		if (FD_ISSET(ipcsockfd, &fdcpy)) {
#ifdef DGDEBUG
			std::cout << "received an url request" << std::endl;
#endif
			ipcpeersock = urllistsock.accept();
			if (ipcpeersock->getFD() < 0) {
				delete ipcpeersock;
				if (logconerror) {
#ifdef DGDEBUG
					std::cout << "Error accepting url ipc. (Ignorable)" << std::endl;
#endif
					syslog(LOG_ERR, "%s", "Error accepting url ipc. (Ignorable)");
				}
				continue;  // if the fd of the new socket < 0 there was error
				// but we ignore it as its not a problem
			}
			try {
				rc = ipcpeersock->getLine(logline, 32000, 3, true);  // throws on err
			}
			catch(exception & e) {
				delete ipcpeersock;  // close the connection
				if (logconerror) {
#ifdef DGDEBUG
					std::cout << "Error reading url ipc. (Ignorable)" << std::endl;
					std::cerr << e.what() << std::endl;
#endif
					syslog(LOG_ERR, "%s", "Error reading url ipc. (Ignorable)");
					syslog(LOG_ERR, "%s", e.what());
				}
				continue;
			}
			// check the command type
			// f: flush the cache
			// g: add a URL to the cache
			// everything else: search the cache
			// n.b. we use command characters with ASCII encoding
			// > 100, because we can have up to 99 filter groups, and
			// group no. plus 1 is the first character in the 'everything else'
			// case.
			if (logline[0] == 'f') {
				delete ipcpeersock;  // close the connection
				urllist.flush();
#ifdef DGDEBUG
				std::cout << "url FLUSH request" << std::endl;
#endif
				continue;
			}
			if (logline[0] == 'g') {
				delete ipcpeersock;  // close the connection
				urllist.addEntry(logline + 2, logline[1]-1);
				continue;
			}
			if (urllist.inURLList(logline + 1, logline[0]-1)) {
				reply = 'Y';
			} else {
				reply = 'N';
			}
			try {
				ipcpeersock->writeToSockete(&reply, 1, 0, 6);
			}
			catch(exception & e) {
				delete ipcpeersock;  // close the connection
				if (logconerror) {
					syslog(LOG_ERR, "%s", "Error writing url ipc. (Ignorable)");
					syslog(LOG_ERR, "%s", e.what());
				}
				continue;
			}
			delete ipcpeersock;  // close the connection
#ifdef DGDEBUG
			std::cout << "url list reply: " << reply << std::endl;
#endif
			continue;  // go back to listening
		}
	}
	delete[]logline;
	urllistsock.close();  // be nice and neat
	return 1;  // It is only possible to reach here with an error
}

int ip_list_listener(std::string stat_location, bool logconerror) {
#ifdef DGDEBUG
	std::cout << "ip listener started" << std::endl;
#endif
	if (!drop_priv_completely()) {
		return 1;  //error
	}
	UDSocket *ipcpeersock;
	int rc, ipcsockfd;
	char* inbuff = new char[16];

	// pass in size of list, and max. age of entries (7 days, apparently)
	DynamicIPList iplist(o.max_ips, 604799);

	ipcsockfd = iplistsock.getFD();

	unsigned long int ip;
	char reply;
	struct in_addr inaddr;

	struct timeval sleep;  // used later on for a short sleep
	sleep.tv_sec = 180;
	sleep.tv_usec = 0;
	struct timeval scopy;  // copy to use as select() can modify

	int maxusage = 0;  // usage statistics:
		// current & highest no. of concurrent IPs using the filter

	double elapsed = 0;  // keep a 3 minute counter so license statistics
	time_t before;   // are written even on busy networks (don't rely on timeout)

	fd_set fdSet;  // our set of fds (only 1) that select monitors for us
	fd_set fdcpy;  // select modifes the set so we need to use a copy
	FD_ZERO(&fdSet);  // clear the set
	FD_SET(ipcsockfd, &fdSet);  // add ipcsock to the set

#ifdef DGDEBUG
	std::cout << "ip listener entering select()" << std::endl;
#endif
	scopy = sleep;
	// loop, essentially, for ever
	while (true) {
		fdcpy = fdSet;  // take a copy
		before = time(NULL);
		rc = select(ipcsockfd + 1, &fdcpy, NULL, NULL, &scopy);  // block until something happens
		elapsed += difftime(time(NULL), before);
#ifdef DGDEBUG
		std::cout << "ip listener select returned: " << rc << ", 3 min timer: " << elapsed << ", scopy: " << scopy.tv_sec << " " << scopy.tv_usec << std::endl;
#endif
		if (rc < 0) {  // was an error
			if (errno == EINTR) {
				continue;  // was interupted by a signal so restart
			}
			if (logconerror) {
				syslog(LOG_ERR, "ip ipc rc<0. (Ignorable)");
			}
			continue;
		}
		if (rc == 0 || elapsed >= 180) {
#ifdef DGDEBUG
			std::cout << "ips in list: " << iplist.getNumberOfItems() << std::endl;
			std::cout << "purging old ip entries" << std::endl;
			std::cout << "ips in list: " << iplist.getNumberOfItems() << std::endl;
#endif
			// should only get here after a timeout
			iplist.purgeOldEntries();
			// write usage statistics
			int currusage = iplist.getNumberOfItems();
			if (currusage > maxusage)
				maxusage = currusage;
			String usagestats;
			usagestats += String(currusage) + "\n" + String(maxusage) + "\n";
#ifdef DGDEBUG
			std::cout << "writing usage stats: " << currusage << " " << maxusage << std::endl;
#endif
			int statfd = open(stat_location.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			if (statfd > 0) {
				write(statfd, usagestats.toCharArray(), usagestats.length());
			}
			close(statfd);
			// reset sleep timer
			scopy = sleep;
			elapsed = 0;
			// only skip back to top of loop if there was a genuine timeout
			if (rc == 0)
				continue;
		}
		if (FD_ISSET(ipcsockfd, &fdcpy)) {
#ifdef DGDEBUG
			std::cout << "received an ip request" << std::endl;
#endif
			ipcpeersock = iplistsock.accept();
			if (ipcpeersock->getFD() < 0) {
				delete ipcpeersock;
				if (logconerror) {
#ifdef DGDEBUG
					std::cout << "Error accepting ip ipc. (Ignorable)" << std::endl;
#endif
					syslog(LOG_ERR, "Error accepting ip ipc. (Ignorable)");
				}
				continue; // if the fd of the new socket < 0 there was error
						  // but we ignore it as its not a problem
			}
			try {
				rc = ipcpeersock->getLine(inbuff, 16, 3);  // throws on err
			} catch (exception& e) {
				delete ipcpeersock;
				if (logconerror) {
#ifdef DGDEBUG
					std::cout << "Error reading ip ipc. (Ignorable)" << std::endl;
#endif
					syslog(LOG_ERR, "Error reading ip ipc. (Ignorable)");
				}
				continue;
			}
#ifdef DGDEBUG
			std::cout << "recieved ip:" << inbuff << std::endl;
#endif
			inet_aton(inbuff, &inaddr);
			ip = inaddr.s_addr;
			// is the ip in our list? this also takes care of adding it if not.
			if (iplist.inList(ip))
				reply = 'Y';
			else
				reply = 'N';
			try {
				ipcpeersock->writeToSockete(&reply, 1, 0, 6);
			} catch (exception& e) {
				delete ipcpeersock;
				if (logconerror) {
#ifdef DGDEBUG
					std::cout << "Error writing ip ipc. (Ignorable)" << std::endl;
#endif
					syslog(LOG_ERR, "Error writing ip ipc. (Ignorable)");
				}
				continue;
			}
			delete ipcpeersock;  // close the connection
#ifdef DGDEBUG
			std::cout << "ip list reply: " << reply << std::endl;
#endif
			continue;  // go back to listening
		}
	}
	delete[] inbuff;
	iplistsock.close();  // be nice and neat
	return 1; // It is only possible to reach here with an error
}


// *
// *
// * end logger, IP list and URL cache code
// *
// *


// Does lots and lots of things - forks off url cache & logger processes, preforks child processes for connection handling, does tidying up on exit
// also handles the various signalling options DG supports (reload config, flush cache, kill all processes etc.)
int fc_controlit()
{
	int rc, fds;

	o.lm.garbageCollect();

	// allocate & create our server sockets
	serversocketcount = o.filter_ip.size();

	serversockets.reset(serversocketcount);
	int *serversockfds = serversockets.getFDAll();

	for (int i = 0; i < serversocketcount; i++) {
		// if the socket fd is not +ve then the socket creation failed
		if (serversockfds[i] < 0) {
			if (!is_daemonised) {
				std::cerr << "Error creating server socket " << i << std::endl;
			}
			syslog(LOG_ERR, "Error creating server socket %d", i);
			return 1;
		}
	}


	if (o.no_logger) {
		loggersock.close();
	} else {
		loggersock.reset();
	}
	if (o.url_cache_number > 0) {
		urllistsock.reset();
	} else {
		urllistsock.close();
	}
	if (o.max_ips > 0) {
		iplistsock.reset();
	} else {
		iplistsock.close();
	}

	pid_t loggerpid = 0;  // to hold the logging process pid
	pid_t urllistpid = 0;  // url cache process id
	pid_t iplistpid = 0; // ip cache process id

	if (!o.no_logger) {
		if (loggersock.getFD() < 0) {
			if (!is_daemonised) {
				std::cerr << "Error creating ipc socket" << std::endl;
			}
			syslog(LOG_ERR, "%s", "Error creating ipc socket");
			return 1; 
		}
	}

	// Made unconditional such that we have root privs when creating pidfile & deleting old IPC sockets
	// PRA 10-10-2005
	/*bool needdrop = false;

	if (o.filter_port < 1024) {*/
#ifdef DGDEBUG
		std::cout << "seteuiding for low port binding/pidfile creation" << std::endl;
#endif
		//needdrop = true;
#ifdef HAVE_SETREUID
		rc = setreuid((uid_t) - 1, o.root_user);
#else
		rc = seteuid(o.root_user);
#endif
		if (rc == -1) {
			syslog(LOG_ERR, "%s", "Unable to seteuid() to bind filter port.");
#ifdef DGDEBUG
			std::cerr << "Unable to seteuid() to bind filter port." << std::endl;
#endif
			return 1;
		}
	//}

	// we have to open/create as root before drop privs
	int pidfilefd = sysv_openpidfile(o.pid_filename);
	if (pidfilefd < 0) {
		syslog(LOG_ERR, "%s", "Error creating/opening pid file.");
		std::cerr << "Error creating/opening pid file:" << o.pid_filename << std::endl;
		return 1;
	}

	// we expect to find a valid filter ip 0 specified in conf if multiple IPs are in use.
	// if we don't find one, bind to any, as per old behaviour.
	if (o.filter_ip[0].length() > 6) {
		if (serversockets.bindAll(o.filter_ip, o.filter_port)) {
			if (!is_daemonised) {
				std::cerr << "Error binding server socket (is something else running on the filter port and ip?" << std::endl;
			}
			syslog(LOG_ERR, "Error binding server socket (is something else running on the filter port and ip?");
		return 1;
		}
	} else {
		// listen/bind to a port on any interface
		if (serversockets.bindSingle(o.filter_port)) {
			if (!is_daemonised) {
				std::cerr << "Error binding server socket: [" << o.filter_port << "] (" << strerror(errno) << ")" << std::endl;
			}
			syslog(LOG_ERR, "Error binding server socket: [%d] (%s)", o.filter_port, strerror(errno));
			return 1;
		}
	}

	// Made unconditional for same reasons as above
	//if (needdrop) {
#ifdef HAVE_SETREUID
		rc = setreuid((uid_t) - 1, o.proxy_user);
#else
		rc = seteuid(o.proxy_user);  // become low priv again
#endif
		if (rc == -1) {
			syslog(LOG_ERR, "Unable to re-seteuid()");
#ifdef DGDEBUG
			std::cerr << "Unable to re-seteuid()" << std::endl;
#endif
			return 1;  // seteuid failed for some reason so exit with error
		}
	//}

	// Needs deleting if its there
	unlink(o.ipc_filename.c_str());  // this would normally be in a -r situation.
	// disabled as requested by Christopher Weimann <csw@k12hq.com>
	// Fri, 11 Feb 2005 15:42:28 -0500
	// re-enabled temporarily
	unlink(o.urlipc_filename.c_str());
	unlink(o.ipipc_filename.c_str());

	if (!o.no_logger) {
		if (loggersock.bind((char *) o.ipc_filename.c_str())) {	// bind to file
			if (!is_daemonised) {
				std::cerr << "Error binding ipc server file (try using the SysV to stop DansGuardian then try starting it again or doing an 'rm " << o.ipc_filename << "')." << std::endl;
			}
			syslog(LOG_ERR, "Error binding ipc server file (try using the SysV to stop DansGuardian then try starting it again or doing an 'rm %s').", o.ipc_filename.c_str());
			return 1;
		}
		if (loggersock.listen(256)) {	// set it to listen mode with a kernel
			// queue of 256 backlog connections
			if (!is_daemonised) {
				std::cerr << "Error listening to ipc server file" << std::endl;
			}
			syslog(LOG_ERR, "Error listening to ipc server file");
			return 1;
		}
	}

	if (o.url_cache_number > 0) {
		if (urllistsock.bind((char *) o.urlipc_filename.c_str())) {	// bind to file
			if (!is_daemonised) {
				std::cerr << "Error binding urllistsock server file (try using the SysV to stop DansGuardian then try starting it again or doing an 'rm " << o.urlipc_filename << "')." << std::endl;
			}
			syslog(LOG_ERR, "Error binding urllistsock server file (try using the SysV to stop DansGuardian then try starting it again or doing an 'rm %s').", o.urlipc_filename.c_str());
			return 1;
		}
		if (urllistsock.listen(256)) {	// set it to listen mode with a kernel
			// queue of 256 backlog connections
			if (!is_daemonised) {
				std::cerr << "Error listening to url ipc server file" << std::endl;
			}
			syslog(LOG_ERR, "Error listening to url ipc server file");
			return 1;
		}
	}

	if (o.max_ips > 0) {
		if (iplistsock.bind((char*) o.ipipc_filename.c_str())) {	// bind to file
			if (!is_daemonised) {
				std::cerr << "Error binding iplistsock server file (try using the SysV to stop DansGuardian then try starting it again or doing an 'rm " << o.ipipc_filename << "')." << std::endl;
			}
			syslog(LOG_ERR, "Error binding iplistsock server file (try using the SysV to stop DansGuardian then try starting it again or doing an 'rm %s').", o.ipipc_filename.c_str());
			return 1;
		}
		if (iplistsock.listen(256)) {	// set it to listen mode with a kernel
			// queue of 256 backlog connections
			if (!is_daemonised) {
				std::cerr << "Error listening to ip ipc server file" << std::endl;
			}
			syslog(LOG_ERR, "Error listening to ip ipc server file");
			return 1;
		}
	}

	if (serversockets.listenAll(256)) {	// set it to listen mode with a kernel
		// queue of 256 backlog connections
		if (!is_daemonised) {
			std::cerr << "Error listening to server socket" << std::endl;
		}
		syslog(LOG_ERR, "Error listening to server socket");
		return 1;
	}

	if (!daemonise()) {	// become a detached daemon
		if (!is_daemonised) {
			std::cerr << "Error daemonising" << std::endl;
		}
		syslog(LOG_ERR, "Error daemonising");
		return 1;
	}

	// this has to be done after daemonise to ensure we get the correct PID.
	rc = sysv_writepidfile(pidfilefd);  // also closes the fd
	if (rc != 0) {
		syslog(LOG_ERR, "Error writing to the dansguardian.pid file: %s", strerror(errno));
		return false;
	}
	// We are now a daemon so all errors need to go in the syslog, rather
	// than being reported on screen as we've detached from the console and
	// trying to write to stdout will not be nice.

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL)) {	// ignore SIGPIPE so we can handle
		// premature disconections better
		syslog(LOG_ERR, "%s", "Error ignoring SIGPIPE");
		return (1);
	}
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &sa, NULL)) {	// ignore HUP
		syslog(LOG_ERR, "%s", "Error ignoring HUP");
		return (1);
	}

	// Next thing we need to do is to split into two processes - one to
	// handle incoming TCP connections from the clients and one to handle
	// incoming UDS ipc from our forked children.  This helps reduce
	// bottlenecks by not having only one select() loop.
	if (!o.no_logger) {

		loggerpid = fork();  // make a child processes copy of self to be logger

		if (loggerpid == 0) {	// ma ma!  i am the child
			serversockets.deleteAll();  // we don't need our copy of this so close it
			delete[] serversockfds;
			urllistsock.close();  // we don't need our copy of this so close it
			log_listener(o.log_location, o.logconerror, o.log_syslog);
#ifdef DGDEBUG
			std::cout << "Log listener exiting" << std::endl;
#endif
			_exit(0);  // is reccomended for child and daemons to use this instead
		}
	}

	// Same for URL list listener
	if (o.url_cache_number > 0) {
		urllistpid = fork();
		if (urllistpid == 0) {	// ma ma!  i am the child
			serversockets.deleteAll(); // we don't need our copy of this so close it
			delete[] serversockfds;
			if (!o.no_logger) {
				loggersock.close();  // we don't need our copy of this so close it
			}
			url_list_listener(o.logconerror);
#ifdef DGDEBUG
			std::cout << "URL List listener exiting" << std::endl;
#endif
			_exit(0);  // is reccomended for child and daemons to use this instead
		}
	}

	// and for IP list listener
	if (o.max_ips > 0) {
		iplistpid = fork();
		if (iplistpid == 0) {	// ma ma!  i am the child
			serversockets.deleteAll(); // we don't need our copy of this so close it
			delete[] serversockfds;
			if (!o.no_logger) {
				loggersock.close();  // we don't need our copy of this so close it
			}
			ip_list_listener(o.stat_location, o.logconerror);
#ifdef DGDEBUG
			std::cout << "IP List listener exiting" << std::endl;
#endif
			_exit(0);  // is reccomended for child and daemons to use this instead
		}
	}

	// I am the parent process here onwards.

#ifdef DGDEBUG
	std::cout << "Parent process created children" << std::endl;
#endif

	if (o.url_cache_number > 0) {
		urllistsock.close();  // we don't need our copy of this so close it
	}
	if (!o.no_logger) {
		loggersock.close();  // we don't need our copy of this so close it
	}
	if (o.max_ips > 0) {
		iplistsock.close();
	}

	memset(&sa, 0, sizeof(sa));
	if (!o.soft_restart) {
		sa.sa_handler = &sig_term;  // register sig_term as our handler
	} else {
		sa.sa_handler = &sig_termsafe;
	}
	if (sigaction(SIGTERM, &sa, NULL)) {	// when the parent process gets a
		// sigterm we need to kill our
		// children which this will do,
		// then we need to exit
		syslog(LOG_ERR, "Error registering SIGTERM handler");
		return (1);
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &sig_hup;  // register sig_hup as our handler
	if (sigaction(SIGHUP, &sa, NULL)) {	// when the parent process gets a
		// sighup we need to kill our
		// children which this will do,
		// then we need to read config
		syslog(LOG_ERR, "Error registering SIGHUP handler");
		return (1);
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &sig_usr1;  // register sig_usr1 as our handler
	if (sigaction(SIGUSR1, &sa, NULL)) {	// when the parent process gets a
		// sigusr1 we need to hup our
		// children to make them exit
		// then we need to read fg config
		syslog(LOG_ERR, "Error registering SIGUSR handler");
		return (1);
	}

#ifdef __SEGV_BACKTRACE
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &sig_segv;
	if (sigaction(SIGSEGV, &sa, NULL)) {
		syslog(LOG_ERR, "Error registering SIGSEGV handler");
		return 1;
	}
#endif

#ifdef DGDEBUG
	std::cout << "Parent process sig handlers done" << std::endl;
#endif

	numchildren = 0;  // to keep count of our children
	busychildren = 0;  // to keep count of our children
	freechildren = 0;  // to keep count of our children

	childrenpids = new int[o.max_children];  // so when one exits we know who
	childrenstates = new int[o.max_children];  // so we know what they're up to
	childsockets = new UDSocket* [o.max_children];
	fds = o.max_children + serversocketcount;

	pids = new struct pollfd[fds];

	int i;

	time_t tnow;
	time_t tmaxspare;

	time(&tmaxspare);

#ifdef DGDEBUG
	std::cout << "Parent process pid structs allocated" << std::endl;
#endif

	// store child fds...
	for (i = 0; i < o.max_children; i++) {
		childrenpids[i] = -1;
		childrenstates[i] = -1;
		pids[i].fd = -1;
		pids[i].events = POLLIN;

	}
	// ...and server fds
	for (i = o.max_children; i < fds; i++) {
		pids[i].fd = serversockfds[i - o.max_children];
		pids[i].events = POLLIN;
	}

#ifdef DGDEBUG
	std::cout << "Parent process pid structs zeroed" << std::endl;
#endif

	failurecount = 0;  // as we don't exit on an error with select()
	// due to the fact that these errors do happen
	// every so often on a fully working, but busy
	// system, we just watch for too many errors
	// consecutivly.

	waitingfor = 0;
	rc = prefork(o.min_children);

	sleep(1);  // need to allow some of the forks to complete

#ifdef DGDEBUG
	std::cout << "Parent process preforked rc:" << rc << std::endl;
	std::cout << "Parent process pid:" << getpid() << std::endl;
#endif

	if (rc < 0) {
		ttg = true;
		syslog(LOG_ERR, "%s", "Error creating initial fork pool - exiting...");
	}

	bool preforked = true;
	int tofind;
	reloadconfig = false;

	syslog(LOG_INFO, "Started sucessfully.");

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
			} else {
				if (o.use_filter_groups_list) {
					o.filter_groups_list.reset();
					if (!o.doReadItemList(o.filter_groups_list_location.c_str(),&(o.filter_groups_list),"filtergroupslist",true))
						reloadconfig = true;  // filter groups problem...
				}
				if (!reloadconfig) {
					o.deletePlugins(o.csplugins);
					if (!o.loadCSPlugins())
						reloadconfig = true;  // content scan plugs problem
					if (!reloadconfig) {
						o.deletePlugins(o.authplugins);
						if (!o.loadAuthPlugins())
							reloadconfig = true;  // auth plugs problem
					}
					if (!reloadconfig) {
						o.lm.garbageCollect();
						hup_allchildren();
						prefork(o.min_children);
						gentlereload = false;
						// everything ok - no full reload needed
						// clear gentle reload flag for next run of the loop
					}
				}
			}
			flush_urlcache();
			continue;
		}

		// Lets take the opportunity to clean up our dead children if any
		for (i = 0; i < fds; i++) {
			pids[i].revents = 0;
		}
		mopup_afterkids();
		rc = poll(pids, fds, 60 * 1000);
		mopup_afterkids();

		if (rc < 0) {	// was an error
#ifdef DGDEBUG
			std::cout << "errno:" << errno << " " << strerror(errno) << std::endl;
#endif

			if (errno == EINTR) {
				continue;  // was interupted by a signal so restart
			}
			if (o.logconerror)
				syslog(LOG_ERR, "Error polling child process sockets: %s", strerror(errno));
			failurecount++;  // log the error/failure
			continue;  // then continue with the looping
		}

		tofind = rc;
		if (rc > 0) {
			for (i = o.max_children; i < fds; i++) {
				if ((pids[i].revents & POLLIN) > 0) {
					tofind--;
				}
			}
		}

		if (tofind > 0) {
			if (check_kid_readystatus(tofind)) {
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
			for (i = o.max_children; i < fds; i++) {
				if ((pids[i].revents & (POLLERR | POLLHUP | POLLNVAL)) > 0) {
					ttg = true;
					syslog(LOG_ERR, "Error with main listening socket.  Exiting.");
					continue;
				}
				if ((pids[i].revents & POLLIN) > 0) {
					// socket ready to accept() a connection
					failurecount = 0;  // something is clearly working so reset count
					if (freechildren < 1 && numchildren < o.max_children) {
						if (!preforked) {
							int num = o.prefork_children;
							if ((o.max_children - numchildren) < num)
								num = o.max_children - numchildren;
							if (o.logchildprocs)
								syslog(LOG_ERR, "Under load - Spawning %d process(es)", num);
							rc = prefork(num);
							if (rc < 0) {
								syslog(LOG_ERR, "Error forking %d extra process(es).", num);
								failurecount++;
							}
							preforked = true;
						}
						continue;
					}
					if (freechildren > 0) {
#ifdef DGDEBUG
						std::cout<<"telling child to accept "<<(i-o.max_children)<<std::endl;
#endif
						tellchild_accept(getfreechild(), i - o.max_children);
					} else {
						usleep(1000);
					}
				}
			}
		}

		if (freechildren < o.minspare_children && !preforked && numchildren < o.max_children) {
			if (o.logchildprocs)
				syslog(LOG_ERR, "Fewer than %d free children - Spawning %d process(es)", o.minspare_children, o.prefork_children);
			rc = prefork(o.prefork_children);
			preforked = true;
			if (rc < 0) {
				syslog(LOG_ERR, "Error forking preforkchildren extra processes.");
				failurecount++;
			}
		}

		if (freechildren <= o.maxspare_children) {
			time(&tmaxspare);
		}
		if (freechildren > o.maxspare_children) {
			time(&tnow);
			if ((tnow - tmaxspare) > (2 * 60)) {	// 2 * 60
				if (o.logchildprocs)
					syslog(LOG_ERR, "More than %d free children - Killing %d process(es)", o.maxspare_children, freechildren - o.maxspare_children);
				cullchildren(freechildren - o.maxspare_children);
			}
		}

		// yield CPU for a while. this loop does not explicitly sleep other
		// than when polling FDs; when connections are coming in at a rate of
		// knots, it effectively turns into a busy loop. this should help.
		//usleep(1000);
	}
	cullchildren(numchildren);  // remove the fork pool of spare children
	for (int i = 0; i < o.max_children; i++) {
		if (pids[i].fd != -1) {
			delete childsockets[i];
			childsockets[i] = NULL;
		}
	}
	if (numchildren > 0) {
		hup_allchildren();
		sleep(2);  // give them a small chance to exit nicely before we force
		// hmmmm I wonder if sleep() will get interupted by sigchlds?
	}
	if (numchildren > 0) {
		kill_allchildren();
	}
	// we might not giving enough time for defuncts to be created and then
	// mopped but on exit or reload config they'll get mopped up
	sleep(1);
	mopup_afterkids();

	delete[]childrenpids;
	delete[]childrenstates;
	delete[]childsockets;
	delete[]pids;  // 4 deletes good, memory leaks bad

	if (failurecount >= 30) {
		syslog(LOG_ERR, "%s", "Exiting due to high failure count.");
#ifdef DGDEBUG
		std::cout << "Exiting due to high failure count." << std::endl;
#endif
	}
#ifdef DGDEBUG
	std::cout << "Main parent process exiting." << std::endl;
#endif

	serversockets.deleteAll();
	delete[] serversockfds; // be nice and neat

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	if (sigaction(SIGTERM, &sa, NULL)) {	// restore sig handler
		// in child process
#ifdef DGDEBUG
		std::cerr << "Error resetting signal for SIGTERM" << std::endl;
#endif
		syslog(LOG_ERR, "%s", "Error resetting signal for SIGTERM");
	}
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &sa, NULL)) {	// restore sig handler
		// in child process
#ifdef DGDEBUG
		std::cerr << "Error resetting signal for SIGHUP" << std::endl;
#endif
		syslog(LOG_ERR, "%s", "Error resetting signal for SIGHUP");
	}
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGUSR1, &sa, NULL)) {	// restore sig handler
		// in child process
#ifdef DGDEBUG
		std::cerr << "Error resetting signal for SIGUSR1" << std::endl;
#endif
		syslog(LOG_ERR, "%s", "Error resetting signal for SIGUSR1");
	}
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	if (sigaction(SIGPIPE, &sa, NULL)) {	// restore sig handler
		// in child process
#ifdef DGDEBUG
		std::cerr << "Error resetting signal for SIGPIPE" << std::endl;
#endif
		syslog(LOG_ERR, "%s", "Error resetting signal for SIGPIPE");
	}

	if (sig_term_killall) {
		struct sigaction sa, oldsa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = SIG_IGN;
		sigaction(SIGTERM, &sa, &oldsa);  // ignore sigterm for us
		kill(0, SIGTERM);  // send everyone in this process group a TERM
		// which causes them to exit as the default action
		// but it also seems to send itself a TERM
		// so we ignore it
		sigaction(SIGTERM, &oldsa, NULL);  // restore prev state
	}

	if (reloadconfig || ttg) {
		if (!o.no_logger)
			::kill(loggerpid, SIGTERM);  // get rid of logger
		if (o.url_cache_number > 0)
			::kill(urllistpid, SIGTERM);  // get rid of url cache
		if (o.max_ips > 0)
			::kill(iplistpid, SIGTERM); // get rid of iplist
		return reloadconfig ? 2 : 0;
	}
	if (o.logconerror) {
		syslog(LOG_ERR, "%s", "Main parent process exiting.");
	}
	return 1;  // It is only possible to reach here with an error
}
