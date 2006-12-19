// Command line content scanning plugin

//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.

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

#include "../ContentScanner.hpp"
#include "../UDSocket.hpp"
#include "../OptionContainer.hpp"
#include "../RegExp.hpp"

#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <list>


// GLOBALS

extern OptionContainer o;
extern bool is_daemonised;


// IMPLEMENTATION

// class name is relevant
class commandlineinstance:public CSPlugin
{
public:
	commandlineinstance(ConfigVar & definition):CSPlugin(definition), usevirusregexp(false),
		submatch(0), arguments(NULL), numarguments(0), infectedcodes(NULL), numinfectedcodes(0),
		cleancodes(NULL), numcleancodes(0), defaultresult(-1) {};
	int scanFile(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user, int filtergroup,
		const char *ip, const char *filename);

	int init(void* args);

	~commandlineinstance() {
		delete[] infectedcodes; delete[] cleancodes;
		for (int i = 0; i < numarguments; i++)
			delete arguments[i];
		delete[] arguments;
	};
private:
	// regular expression for finding virus names in program output
	RegExp virusregexp;
	// whether or not the above is in use
	bool usevirusregexp;
	// which sub-match to take from the match
	int submatch;

	// path to command-line scanning program (+ initial arguments)
	String progname;
	// argument array (the above must be split on space for passing to exec)
	char** arguments; int numarguments;

	// return code(s) for infected files
	int* infectedcodes; int numinfectedcodes;
	// return code(s) for uninfected files
	int* cleancodes; int numcleancodes;

	// optional default result - can be used to e.g. define only cleancodes,
	// and have everything else default to infected.
	int defaultresult;
};

// class factory code *MUST* be included in every plugin

CSPlugin *commandlinecreate(ConfigVar & definition)
{
	return new commandlineinstance(definition);
}

// end of Class factory

// initialise plugin
int commandlineinstance::init(void* args)
{
	// always include these lists
	if (!readStandardLists()) {
		return DGCS_ERROR;
	}

	// read in program name
	progname = cv["progname"];
	if (progname.length() == 0) {
		if (!is_daemonised)
			std::cerr << "Command-line scanner: No program specified" << std::endl;
		syslog(LOG_ERR, "Command-line scanner: No program specified");
		return DGCS_ERROR;
	}

	// split into an argument array
	std::list<std::string> temparguments;
	char* result = strtok(progname.toCharArray()," ");
	while (result) {
		temparguments.push_back(std::string(result));
		result = strtok(NULL," ");
	}
	for (int i = 0; i < numarguments; i++)
		delete arguments[i];
	delete[] arguments;
	numarguments = temparguments.size();
	arguments = new char* [numarguments + 2];
	arguments[numarguments + 1] = NULL;
	int count = 0;
	for (std::list<std::string>::iterator i = temparguments.begin(); i != temparguments.end(); i++) {
		char* newthing = new char[i->length()];
		strcpy(newthing, i->c_str());
		arguments[count++] = newthing;
	}
	progname = cv["progname"];

#ifdef DGDEBUG
	std::cout << "Program and arguments: ";
	for (int i = 0; i < numarguments; i++) {
		std::cout << arguments[i] << " ";
	}
	std::cout << std::endl;
#endif

	// read in virus name regular expression
	String ucvirusregexp(cv["virusregexp"]);
	if (ucvirusregexp.length()) {
		usevirusregexp = true;
		if (!virusregexp.comp(ucvirusregexp.toCharArray())) {
			if (!is_daemonised)
				std::cerr << "Command-line scanner: Could not compile regular expression for extracting virus names" << std::endl;
			syslog(LOG_ERR, "Command-line scanner: Could not compile regular expression for extracting virus names");
			return DGCS_ERROR;
		}
		String ssubmatch(cv["submatch"]);
		if (ssubmatch.length())
			submatch = ssubmatch.toInteger();
	}

	// read in the lists of good and bad program return codes
	String sinfectedcodes(cv["infectedcodes"]);
	String scleancodes(cv["cleancodes"]);
	std::list<int> tempinfectedcodes;
	std::list<int> tempcleancodes;
	result = strtok(sinfectedcodes.toCharArray(),",");
#ifdef DGDEBUG
	std::cout << "Infected file return codes: ";
#endif
	while (result) {
		tempinfectedcodes.push_back(atoi(result));
#ifdef DGDEBUG
		std::cout << tempinfectedcodes.back() << " ";
#endif
		result = strtok(NULL,",");
	}
	result = strtok(scleancodes.toCharArray(),",");
#ifdef DGDEBUG
	std::cout << std::endl << "Clean file return codes: ";
#endif
	while (result) {
		tempcleancodes.push_back(atoi(result));
#ifdef DGDEBUG
		std::cout << tempcleancodes.back() << " ";
#endif
		result = strtok(NULL,",");
	}
#ifdef DGDEBUG
	std::cout << std::endl;
#endif
	
	// we need at least one of our three mechanisms (cleancodes, infectedcodes and virus names)
	// to be defined in order to make a decision about the nature of a scanning result.
	numcleancodes = tempcleancodes.size();
	numinfectedcodes = tempinfectedcodes.size();
	if (!(usevirusregexp || numcleancodes || numinfectedcodes)) {
		if (!is_daemonised)
			std::cerr << "Command-line scanner requires some mechanism for interpreting results. Please define cleancodes, infectedcodes, and/or a virusregexp." << std::endl;
		syslog(LOG_ERR,"Command-line scanner requires some mechanism for interpreting results. Please define cleancodes, infectedcodes, and/or a virusregexp.");
		return DGCS_ERROR;
	}

	// Copy return code lists out into static arrays
	delete[] infectedcodes;
	delete[] cleancodes;
	infectedcodes = new int[numinfectedcodes];
	cleancodes = new int[numcleancodes];
	count = 0;
	for (std::list<int>::iterator i = tempinfectedcodes.begin(); i != tempinfectedcodes.end(); i++)
		infectedcodes[count++] = *i;
	count = 0;
	for (std::list<int>::iterator i = tempcleancodes.begin(); i != tempcleancodes.end(); i++)
		cleancodes[count++] = *i;

	// read in default result type
	String sdefaultresult(cv["defaultresult"]);
	if (sdefaultresult.length()) {
		if (sdefaultresult == "clean") {
			defaultresult = 1;
		} else if (sdefaultresult == "infected") {
			defaultresult = 0;
		} else {
			if (!is_daemonised)
				std::cerr << "Command-line scanner: Default result value not understood" << std::endl;
			syslog(LOG_ERR,"Command-line scanner: Default result value not understood");
			return DGCS_WARNING;
		}
	}

	return DGCS_OK;
}


// no need to replace the inheritied scanMemory() which just calls scanFile()
// there is no capability to scan memory with commandlinescan as we pass it
// a file name to scan.  So we save the memory to disk and pass that.
// Then delete the temp file.
int commandlineinstance::scanFile(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user, int filtergroup,
	const char *ip, const char *filename)
{
	// create socket pairs for child (scanner) process's stdout & stderr
	int scannerstdout[2];
	int scannerstderr[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, scannerstdout) == -1) {
		lastmessage = "Cannot create sockets for communicating with scanner";
		syslog(LOG_ERR, "Cannot open socket pair for command-line scanner's stdout: %s", strerror(errno));
		return DGCS_SCANERROR;
	}
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, scannerstderr) == -1) {
		lastmessage = "Cannot create sockets for communicating with scanner";
		syslog(LOG_ERR, "Cannot open socket pair for command-line scanner's stderr: %s", strerror(errno));
		return DGCS_SCANERROR;
	}
	int f = fork();
	if (f == 0) {
#ifdef DGDEBUG
		std::cout << "Running: " << progname.toCharArray() << " " << filename << std::endl;
#endif
		// close read ends of sockets
		close(scannerstdout[0]);
		close(scannerstderr[0]);
		// bind stdout & stderr
		dup2(scannerstdout[1], 1);
		dup2(scannerstderr[1], 2);
		// execute scanner
		arguments[numarguments] = (char*)filename;
		execv(arguments[0], arguments);
		// if we get here, an error occurred!
		syslog(LOG_ERR, "Cannot exec command-line scanner (command \"%s %s\"): %s", progname.toCharArray(), filename, strerror(errno));
		_exit(255);
	} else if (f == -1) {
		lastmessage = "Cannot launch scanner";
		syslog(LOG_ERR, "Cannot fork to launch command-line scanner (command \"%s %s\"): %s", progname.toCharArray(), filename, strerror(errno));
		return DGCS_SCANERROR;
	}

	// close write ends of sockets
	close(scannerstdout[1]);
	close(scannerstderr[1]);

	char buff[8192];
	std::string result;
	FILE *readme = fdopen(scannerstdout[0], "r");
	while (fgets(buff, 8192, readme) != NULL) {
#ifndef DGDEBUG
		if (usevirusregexp)
#endif
			result += buff;
	}
	fclose(readme);
	readme = fdopen(scannerstderr[0], "r");
	while (fgets(buff, 8192, readme) != NULL) {
#ifndef DGDEBUG
		if (usevirusregexp)
#endif
			result += buff;
	}
	fclose(readme);

	// close read ends too now
	close(scannerstdout[0]);
	close(scannerstderr[0]);

	// wait for scanner to quit & retrieve exit status
	int returncode;
	if (waitpid(f,&returncode,0) == -1) {
		lastmessage = "Cannot get scanner return code";
		syslog(LOG_ERR, "Cannot get command-line scanner return code: %s", strerror(errno));
		return DGCS_SCANERROR;
	}
	returncode = WEXITSTATUS(returncode);
	
#ifdef DGDEBUG
	std::cout << "Scanner result" << std::endl << "--------------" << std::endl << result << std::endl << "--------------" << std::endl << "Code: " << returncode << std::endl;
#endif
	if (returncode == 255) {
		lastmessage = "Cannot get scanner return code";
		syslog(LOG_ERR, "Cannot get command-line scanner return code: scanner exec failed");
		return DGCS_SCANERROR;
	}

	lastvirusname = "Unknown";

	if (usevirusregexp) {
		virusregexp.match(result.c_str());
		if (virusregexp.matched()) {
			lastvirusname = virusregexp.result(submatch);
			return DGCS_INFECTED;
		}
	}

	if (cleancodes) {
		for (int i = 0; i < numcleancodes; i++) {
			if (returncode == cleancodes[i])
				return DGCS_CLEAN;
		}
	}

	if (infectedcodes) {
		for (int i = 0; i < numinfectedcodes; i++) {
			if (returncode == infectedcodes[i])
				return DGCS_INFECTED;
		}
	}

	if (defaultresult == 1)
		return DGCS_CLEAN;
	else if (defaultresult == 0)
		return DGCS_INFECTED;

	return DGCS_SCANERROR;
}
