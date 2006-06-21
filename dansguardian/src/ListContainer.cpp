// ListContainer - class for both item and phrase lists

//Please refer to http://dansguardian.org/?page=copyright
//for the license for this code.
//Written by Daniel Barron (daniel@//jadeb/.com).
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

#include <syslog.h>
#include <algorithm>
#include "ListContainer.hpp"
#include "OptionContainer.hpp"
#include "RegExp.hpp"
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include "String.hpp"
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <sys/time.h>


// GLOBALS

extern bool is_daemonised;
extern OptionContainer o;


// DEFINES

#define ROOTNODESIZE 260
#define MAXROOTLINKS ROOTNODESIZE-4
#define GRAPHENTRYSIZE 64
#define MAXLINKS GRAPHENTRYSIZE-4
#define ROOTOFFSET ROOTNODESIZE - GRAPHENTRYSIZE


// IMPLEMENTATION

// Constructor - set default values
ListContainer::ListContainer():refcount(1), parent(false), filedate(0), used(false), bannedpfiledate(0), exceptionpfiledate(0), weightedpfiledate(0),
	sourceisexception(false), sourcestartswith(false), sourcefilters(0), data(NULL), realgraphdata(NULL), maxchildnodes(0), graphitems(0),
	data_length(0), data_memory(0), items(0), isSW(false), issorted(false), graphused(false), force_quick_search(0),
	sthour(0), stmin(0), endhour(0), endmin(0), istimelimited(false)
{
}

// delete the memory block when the class is destryed
ListContainer::~ListContainer()
{
	delete[]data;
	if (graphused) {
		free(realgraphdata);
	}
	for (unsigned int i = 0; i < morelists.size(); i++) {
		o.lm.deRefList(morelists[i]);
	}
}

// for both types of list - clear & reset all values
void ListContainer::reset()
{
	delete[]data;
	if (graphused)
		free(realgraphdata);
	for (unsigned int i = 0; i < morelists.size(); i++) {
		o.lm.deRefList(morelists[i]);
	}
	data = NULL;
	realgraphdata = NULL;
	maxchildnodes = 0;
	graphitems = 0;
	data_length = 0;
	data_memory = 0;
	items = 0;
	isSW = false;
	issorted = false;
	graphused = false;
	force_quick_search = 0;
	sthour = 0;
	stmin = 0;
	endhour = 0;
	endmin = 0;
	days = "";
	timetag = "";
	category = "";
	istimelimited = false;
	combilist.clear();
	slowgraph.clear();
	list.clear();
	weight.clear();
	itemtype.clear();
	morelists.clear();
	listcategory.clear();
	categoryindex.clear();
	used = false;
	parent = false;
	bannedpfile = "";
	exceptionpfile = "";
	weightedpfile = "";
	bannedpfiledate = 0;
	exceptionpfiledate = 0;
	weightedpfiledate = 0;
}

// for item lists - during a config reload, can we simply retain the already loaded list?
bool ListContainer::previousUseItem(const char *filename, bool startswith, int filters)
{
	String f = filename;
	if (f == sourcefile && startswith == sourcestartswith && filters == sourcefilters) {
		return true;
	}
	return false;
}

// for phrase lists - read in the given file, which may be an exception list
bool ListContainer::readPhraseList(const char *filename, bool isexception)
{
	sourcefile = filename;
	sourceisexception = isexception;
	std::string linebuffer;  // a string line buffer ;)
	String temp;  // a String for temporary manipulation
	String line;
	String lcat;
	int catindex;
	int len = getFileLength(filename);
	if (len < 0) {
		if (!is_daemonised) {
			std::cerr << "Error reading file (does it exist?): " << filename << std::endl;
		}
		syslog(LOG_ERR, "%s", "Error reading file (does it exist?): ");
		syslog(LOG_ERR, "%s", filename);
		return false;
	}
	if (len < 2) {
		return true;  // its blank - perhaps due to webmin editing
		// just return
	}
	filedate = getFileDate(filename);
	increaseMemoryBy(len + 1);  // Allocate some memory to hold file
	ifstream listfile(filename, ios::in);  // open the file for reading
	if (!listfile.good()) {
		if (!is_daemonised) {
			std::cerr << "Error opening file (does it exist?): " << filename << std::endl;
		}
		syslog(LOG_ERR, "%s", "Error opening file (does it exist?): ");
		syslog(LOG_ERR, "%s", filename);
		return false;
	}
	lcat = "";
	catindex = -1;
	while (!listfile.eof()) {	// keep going until end of file
		getline(listfile, linebuffer);  // grab a line
		if (linebuffer.length() != 0) {	// sanity checkin
			line = linebuffer.c_str();
			line.removeWhiteSpace();
			line.toLower();  // tidy up
			if (line.startsWith("<")) {
				readPhraseListHelper(line, isexception, catindex);
			}
			// handle included list files
			else if (line.startsWith(".")) {
				temp = line.after(".include<").before(">");
				if (temp.length() > 0) {
					if (!readPhraseList(temp.toCharArray(), isexception)) {
						listfile.close();
						return false;
					}
				}
			}
			// phrase lists can be categorised (but not time limited)
			else if (line.startsWith("#listcategory:")) {
				//use the original line so as to preserve case in category names
				temp = linebuffer.c_str();
				lcat = temp.after("\"").before("\"");
				// this serves two purposes: returning the index of the category string
				// if it is already in our category list, and adding it to the list (also
				// returning index) if it is not.
				catindex = getCategoryIndex(&lcat);
#ifdef DGDEBUG
				std::cout << "List category: " << lcat << std::endl;
				std::cout << "Category list index: " << catindex << std::endl;
#endif
				continue;
			}
		}
	}
	listfile.close();
	return true;  // sucessful read
}

// for phrase lists - helper function for readPhraseList
void ListContainer::readPhraseListHelper(String line, bool isexception, int catindex)
{
	// read in weighting value, if there
	int weighting = line.after("><").before(">").toInteger();
	// defaults to 0
	int type;
	if (weighting != 0) {
		// this is a weighted phrase
		type = 1;
		line = line.before("><") + ">";
	} else {
		if (isexception) {
			// this is an exception phrase
			type = -1;
		} else {
			type = 0;
		}
	}

	if (line.after(">,").length() > 2) {
		// push type & weighting for all phrases on this line onto the combi list
		while (line.length() > 2) {
			line = line.after("<");
			// combination banned, weighted, or exception
			readPhraseListHelper2(line.before(">"), type + 11, weighting, catindex);
			line = line.after(">,");
		}
		// end of combi marker
		readPhraseListHelper2("", type + 21, weighting, catindex);
	} else {
		line = line.after("<").before(">");
		// push type & weighting for this individual phrase onto combi list (not combination phrase)
		readPhraseListHelper2(line, type, weighting, catindex);
	}
}

// for phrase lists - push phrase, type, weighting & category onto combi list
void ListContainer::readPhraseListHelper2(String phrase, int type, int weighting, int catindex)
{
	// -1=exception
	// 0=banned
	// 1=weighted
	// 10 = combination exception
	// 11 = combination banned
	// 12 = combination weighted
	// 20,21,22 = end of combi marker

	if (type > 19) {
		combilist.push_back(-2);  // mark an end of a combi
		combilist.push_back(type - 21);  // store the combi type
		combilist.push_back(weighting);  // store the combi weight
		combilist.push_back(catindex);  // store the combi category
		return;
	}

	if (phrase.length() > 127) {
		if (!is_daemonised) {
			std::cerr << "Phrase length too long, truncating: " << phrase << std::endl;
		}
		syslog(LOG_ERR, "%s", "Phrase length too long, truncating:");
		syslog(LOG_ERR, "%s", phrase.toCharArray());
		phrase = phrase.subString(0, 127);
	}

	if (phrase.length() < 1) {	// its too small to use
		return;
	}

	if (type < 10) {
		if (!addToItemListPhrase(phrase.toCharArray(), phrase.length(), type, weighting, false, catindex)) {
			if (!is_daemonised) {
				std::cerr << "Duplicate phrase, dropping: " << phrase << std::endl;
			}
			syslog(LOG_ERR, "%s", "Duplicate phrase, dropping:");
			syslog(LOG_ERR, "%s", phrase.toCharArray());
		}
		return;
	}
	// must be a combi or end marker if got here

	// must be a combi if got here

	addToItemListPhrase(phrase.toCharArray(), phrase.length(), type, weighting, true, catindex);
}

// for item lists - add phrases to list proper
bool ListContainer::addToItemListPhrase(char *s, int len, int type, int weighting, bool combi, int catindex)
{
	int i;
	list.push_back(data_length);
	for (i = 0; i < len; i++) {
		data[data_length + i] = s[i];
	}
	data[data_length + len] = 0;
	data_length += len + 1;
	if (combi) {
		// if this is a combination item, store the ID of the current item on the combi list
		combilist.push_back(items);
	}
	items++;
	weight.push_back(weighting);
	itemtype.push_back(type);
	categoryindex.push_back(catindex);
	return true;
}

// for item lists - read item list from file. checkme - what is startswith? is it used? what is filters?
bool ListContainer::readItemList(const char *filename, bool startswith, int filters)
{
#ifdef DGDEBUG
	if (filters != 32)
		std::cout << "Converting to lowercase" << std::endl;
#endif
	sourcefile = filename;
	sourcestartswith = startswith;
	sourcefilters = filters;
	std::string linebuffer;
	RegExp re;
	re.comp("^.*\\:[0-9]+\\/.*");
#ifdef DGDEBUG
	std::cout << filename << std::endl;
#endif
	filedate = getFileDate(filename);
	if (isCacheFileNewer(filename)) {	// see if cached .process file
		linebuffer = filename;  //         is available and up to date
		linebuffer += ".processed";

		if (getFileLength(linebuffer.c_str()) >= 4000) {
			// don't bother with small files
			if (!readProcessedItemList(linebuffer.c_str(), startswith, filters)) {	// read cached
				return false;
			}
			issorted = true;  // don't bother sorting cached file
			return true;

		}
	}
	int len = getFileLength(filename);
	if (len < 0) {
		if (!is_daemonised) {
			std::cerr << "Error reading file: " << filename << std::endl;
		}
		syslog(LOG_ERR, "%s", "Error reading file:");
		syslog(LOG_ERR, "%s", filename);
		return false;
	}
	if (len < 2) {
		return true;  // its blank - perhaps due to webmin editing
		// just return
	}
	increaseMemoryBy(len + 1);  // Allocate some memory to hold file
	// The plus one is to cope with files not ending in a new line
	ifstream listfile(filename, ios::in);
	if (!listfile.good()) {
		if (!is_daemonised) {
			std::cerr << "Error opening :" << filename << std::endl;
		}
		syslog(LOG_ERR, "%s", "Error opening :");
		syslog(LOG_ERR, "%s", filename);
		return false;
	}
	String temp, inc, hostname, url;
	while (!listfile.eof()) {
		getline(listfile, linebuffer);
		if (linebuffer.length() < 2)
			continue;  // its jibberish

		temp = linebuffer.c_str();

		//item lists (URLs, domains) can be both categorised and time-limited
		if (linebuffer[0] == '#') {
			if (temp.startsWith("#time: ")) {	// see if we have a time tag
				if (!readTimeTag(&temp)) {
					return false;
				}
				continue;
			}
			else if (temp.startsWith("#listcategory:")) {
				category = temp.after("\"").before("\"");
#ifdef DGDEBUG
				std::cout << "found item list category: " << category << std::endl;
#endif
				continue;
			}
			continue;  // it's a comment
		}

		if (temp.contains("#")) {
			temp = temp.before("#");  // tidy up
		}
		temp.removeWhiteSpace();  // tidy up and make it handle CRLF files
		if (temp.startsWith(".Include<")) {	// see if we have another list
			inc = temp.after(".Include<");  // to include
			inc = inc.before(">");
			if (!readAnotherItemList(inc.toCharArray(), startswith, filters)) {	// read it
				listfile.close();
				return false;
			}
			continue;
		}
		if (temp.endsWith("/")) {
			temp.chop();  // tidy up
		}
		if (temp.startsWith("ftp://")) {
			temp = temp.after("ftp://");  // tidy up
		}
		if (filters == 1) {	// remove port addresses
			if (temp.contains(":")) {	// quicker than full regexp
				if (re.match(temp.toCharArray())) {
					hostname = temp.before(":");
					url = temp.after("/");
					temp = hostname + "/" + url;
				}
			}
		}
		if (filters != 32) {
			temp.toLower();  // tidy up - but don't make regex lists lowercase!
		}
		addToItemList(temp.toCharArray(), temp.length());  // add to unsorted
		// list
	}
	listfile.close();
	return true;  // sucessful read
}

// for item lists - read nested item lists
bool ListContainer::readAnotherItemList(const char *filename, bool startswith, int filters)
{

	int result = o.lm.newItemList(filename, startswith, filters, false);
	if (result < 0) {
		if (!is_daemonised) {
			std::cerr << "Error opening file:" << filename << std::endl;
		}
		syslog(LOG_ERR, "%s", "Error opening file:");
		syslog(LOG_ERR, "%s", filename);
		return false;
	}
	morelists.push_back((unsigned) result);
	return true;
}

// for item lists - is this item in the list?
bool ListContainer::inList(char *string)
{
	if (findInList(string) != NULL) {
		return true;
	}
	return false;
}

// for item lists - is an item in the list that ends with this string?
bool ListContainer::inListEndsWith(char *string)
{
	if (isNow()) {
		if (items > 0) {
			if (searchREW(0, items - 1, string) >= 0) {
				lastcategory = category;
				return true;
			}
		}
		bool rc;
		for (unsigned int i = 0; i < morelists.size(); i++) {
			rc = (*o.lm.l[morelists[i]]).inListEndsWith(string);
			if (rc) {
				lastcategory = (*o.lm.l[morelists[i]]).lastcategory;
				return true;
			}
		}
	}
	return false;
}

// for item lists - is an item in the list that starts with this string?
bool ListContainer::inListStartsWith(char *string)
{
	if (isNow()) {
		if (items > 0) {
			if (searchRSW(0, items - 1, string) >= 0) {
				lastcategory = category;
				return true;
			}
		}
		bool rc;
		for (unsigned int i = 0; i < morelists.size(); i++) {
			rc = (*o.lm.l[morelists[i]]).inListStartsWith(string);
			if (rc) {
				lastcategory = (*o.lm.l[morelists[i]]).lastcategory;
				return true;
			}
		}
	}
	return false;
}

// find pointer to the part of the data array containing this string
char *ListContainer::findInList(char *string)
{
	if (isNow()) {
		if (items > 0) {
			int r;
			if (isSW) {
				r = searchRSWF(0, items - 1, string);
			} else {
				r = searchREWF(0, items - 1, string);
			}
			if (r >= 0) {
				lastcategory = category;
				return (data + list[r]);
			}
		}
		char *rc;
		for (unsigned int i = 0; i < morelists.size(); i++) {
			rc = (*o.lm.l[morelists[i]]).findInList(string);
			if (rc != NULL) {
				lastcategory = (*o.lm.l[morelists[i]]).lastcategory;
				return rc;
			}
		}
	}
	return NULL;
}

// find an item in the list which starts with this
char *ListContainer::findStartsWith(char *string)
{
	if (isNow()) {
		if (items > 0) {
			int r = searchRSW(0, items - 1, string);
			if (r >= 0) {
				lastcategory = category;
				return (data + list[r]);
			}
		}
		char *rc;
		for (unsigned int i = 0; i < morelists.size(); i++) {
			rc = (*o.lm.l[morelists[i]]).findStartsWith(string);
			if (rc != NULL) {
				lastcategory = (*o.lm.l[morelists[i]]).lastcategory;
				return rc;
			}
		}
	}
	return NULL;
}

char *ListContainer::findStartsWithPartial(char *string)
{
	if (isNow()) {
		if (items > 0) {
			int r = searchRSW(0, items - 1, string);
			if (r >= 0) {
				lastcategory = category;
				return (data + list[r]);
			}
			if (r < -1) {
				r = 0 - r - 2;
				lastcategory = category;
				return (data + list[r]);  // nearest match
			}
		}
		char *rc;
		for (unsigned int i = 0; i < morelists.size(); i++) {
			rc = (*o.lm.l[morelists[i]]).findStartsWithPartial(string);
			if (rc != NULL) {
				lastcategory = (*o.lm.l[morelists[i]]).lastcategory;
				return rc;
			}
		}
	}
	return NULL;
}

char *ListContainer::findEndsWith(char *string)
{
	if (isNow()) {
		if (items > 0) {
			int r = searchREW(0, items - 1, string);
			if (r >= 0) {
				lastcategory = category;
				return (data + list[r]);
			}
		}
		char *rc;
		for (unsigned int i = 0; i < morelists.size(); i++) {
			rc = (*o.lm.l[morelists[i]]).findEndsWith(string);
			if (rc != NULL) {
				lastcategory = (*o.lm.l[morelists[i]]).lastcategory;
				return rc;
			}
		}
	}
	return NULL;
}

std::string ListContainer::getItemAt(char *index)
{
	std::string s = index;
	return s;
}

std::string ListContainer::getItemAtInt(int index)
{
	char *o = data + list[index];
	std::string s = o;
	return s;
}

int ListContainer::getWeightAt(unsigned int index)
{
	return weight[index];
}

int ListContainer::getTypeAt(unsigned int index)
{
	return itemtype[index];
}

void ListContainer::endsWithSort()
{				// sort by ending of line
	for (unsigned int i = 0; i < morelists.size(); i++) {
		(*o.lm.l[morelists[i]]).endsWithSort();
	}
	if (items < 2 || issorted)
		return;
	quicksortEW(0, items - 1);
	isSW = false;
	issorted = true;
	return;
}

void ListContainer::startsWithSort()
{				// sort by start of line
	for (unsigned int i = 0; i < morelists.size(); i++) {
		(*o.lm.l[morelists[i]]).startsWithSort();
	}
	if (items < 2 || issorted)
		return;
	quicksortSW(0, items - 1);
	isSW = true;
	issorted = true;
	return;
}

bool ListContainer::createCacheFile()
{
	unsigned int i;
	for (i = 0; i < morelists.size(); i++) {
		(*o.lm.l[morelists[i]]).createCacheFile();
	}
	if (isCacheFileNewer(sourcefile.toCharArray())) {	// only do if it needs updating
		return true;
	}
	if (items < 1000) {	// There is little to gain when there are so few
		return true;
	}
	String f = sourcefile;
	f += ".processed";
#ifdef DGDEBUG
	std::cout << "creating processed file:" << f << std::endl;
#endif
	ofstream listfile(f.toCharArray(), ios::out);
	if (listfile.fail()) {
		if (!is_daemonised) {
			std::cerr << "Error creating cache file." << std::endl;
			std::cerr << "Do you have write access to this area:" << std::endl;
			std::cerr << f << std::endl;
		}
		syslog(LOG_ERR, "%s", "Error cache file.");
		syslog(LOG_ERR, "%s", "Do you have write access to this area:");
		syslog(LOG_ERR, "%s", f.toCharArray());
		return false;
	}

	for (i = 0; i < morelists.size(); i++) {
		f = ".Include<";
		f += (*o.lm.l[morelists[i]]).sourcefile;
		f += ">\n";
		listfile.write(f.toCharArray(), f.length());
	}
	if (timetag.length() > 10) {	// no point in writing corrupt short one
		f = timetag;
		f += "\n";
		listfile.write(f.toCharArray(), f.length());
	}
	if (category.length() > 2) {	// no point in writing corrupt short one
		// lengthened from 4 to allow "ADs" category to be preserved in processed files
		f = "#listcategory:\"";
		f += category;
		f += "\"\n";
		listfile.write(f.toCharArray(), f.length());
	}

	char *offset;
	for (i = 0; i < (unsigned) items; i++) {	// write the entries in order
		offset = data + list[i];
		listfile.write(offset, strlen(offset));
		listfile.put('\n');  // newline per entry
	}
	listfile.close();
	return true;
}

void ListContainer::makeGraph(int fqs)
{
	force_quick_search = fqs;
	if (data_length == 0)
		return;
	int i;
	if (force_quick_search == 1) {
		for (i = 0; i < items; i++) {
			slowgraph.push_back(i);
		}
		return;
	}
	std::string s;
	std::string lasts;
	graphused = true;

#ifdef DGDEBUG
	std::cout << "Bytes needed for phrase tree in worst-case scenario: " << (sizeof(int) * ((GRAPHENTRYSIZE * data_length) + ROOTOFFSET)) << std::endl;
	prolificroot = false;
	secondmaxchildnodes = 0;
#endif

	realgraphdata = (int*) calloc((GRAPHENTRYSIZE * data_length) + ROOTOFFSET, sizeof(int));
	graphitems++;
	std::deque<unsigned int> sizelist;

	for (i = 0; i < items; i++) {
		sizelist.push_back(i);
	}
	graphSizeSort(0, items - 1, &sizelist);

	for (i = 0; i < items; i++) {
		s = getItemAt(data + list[sizelist[i]]);
		graphAdd(s.c_str(), 0, sizelist[i]);
	}

#ifdef DGDEBUG
	std::cout << "Bytes actually needed for phrase tree: " << (sizeof(int) * ((GRAPHENTRYSIZE * graphitems) + ROOTOFFSET)) << std::endl;
	std::cout << "Most prolific node has " << maxchildnodes << " children" << std::endl;
	std::cout << "It " << (prolificroot ? "is" : "is not") << " the root node" << std::endl;
	std::cout << "Second most prolific node has " << secondmaxchildnodes << " children" << std::endl;
#endif

	realgraphdata = (int*) realloc(realgraphdata, sizeof(int) * ((GRAPHENTRYSIZE * graphitems) + ROOTOFFSET));

	int ml = realgraphdata[2];
	int branches;

	for (i = ml - 1; i >= 0; i--) {
		branches = graphFindBranches(realgraphdata[4 + i]);
		if (branches < 12) {	// quicker to use B-M on node with few branches
			graphCopyNodePhrases(realgraphdata[4 + i]);
			// remove link to this node and so effectively remove all nodes
			// it links to but don't recover the memory as its not worth it
			for (int j = i; j < ml; j++) {
				realgraphdata[4 + j] = realgraphdata[4 + j + 1];
			}
			realgraphdata[2]--;
		}
	}
}

void ListContainer::graphSizeSort(int l, int r, std::deque<unsigned int> *sizelist)
{
	if (r <= l)
		return;
	unsigned int e;
	int k;
	unsigned int v = getItemAtInt((*sizelist)[r]).length();
	int i = l - 1, j = r, p = i, q = r;
	for (;;) {
		while (getItemAtInt((*sizelist)[++i]).length() < v);
		while (v < getItemAtInt((*sizelist)[--j]).length()) {
			if (j == l)
				break;
		}
		if (i >= j)
			break;
		e = (*sizelist)[i];
		(*sizelist)[i] = (*sizelist)[j];
		(*sizelist)[j] = e;
		if (v == getItemAtInt((*sizelist)[i]).length()) {
			p++;
			e = (*sizelist)[p];
			(*sizelist)[p] = (*sizelist)[i];
			(*sizelist)[i] = e;
		}
		if (v == getItemAtInt((*sizelist)[j]).length()) {
			q--;
			e = (*sizelist)[q];
			(*sizelist)[q] = (*sizelist)[j];
			(*sizelist)[j] = e;
		}
	}
	e = (*sizelist)[i];
	(*sizelist)[i] = (*sizelist)[r];
	(*sizelist)[r] = e;
	j = i - 1;
	i++;
	for (k = l; k <= p; k++, j--) {
		e = (*sizelist)[k];
		(*sizelist)[k] = (*sizelist)[j];
		(*sizelist)[j] = e;
	}
	for (k = r - 1; k >= q; k--, i++) {
		e = (*sizelist)[k];
		(*sizelist)[k] = (*sizelist)[i];
		(*sizelist)[i] = e;
	}
	graphSizeSort(l, j, sizelist);
	graphSizeSort(i, r, sizelist);
}

// find the total number of children a node has, along all branches
int ListContainer::graphFindBranches(unsigned int pos)
{
	int branches = 0;
	int * graphdata;
	if (pos == 0)
		graphdata = realgraphdata;
	else
		graphdata = realgraphdata + ROOTOFFSET;
	int links = graphdata[pos * GRAPHENTRYSIZE + 2];
	for (int i = 0; i < links; i++) {
		branches += graphFindBranches(graphdata[pos * GRAPHENTRYSIZE + 4 + i]);
	}
	if (links > 1) {
		branches += links - 1;
	}

	return branches;
}

// copy all phrases starting from a given root link into the slowgraph
void ListContainer::graphCopyNodePhrases(unsigned int pos)
{
	int * graphdata;
	if (pos == 0)
		graphdata = realgraphdata;
	else
		graphdata = realgraphdata + ROOTOFFSET;
	int links = graphdata[pos * GRAPHENTRYSIZE + 2];
	int i;
	for (i = 0; i < links; i++) {
		graphCopyNodePhrases(graphdata[pos * GRAPHENTRYSIZE + 4 + i]);
	}
	bool found = false;
	int size = slowgraph.size();
	unsigned int phrasenumber = graphdata[pos * GRAPHENTRYSIZE + 3];
	for (i = 0; i < size; i++) {
		if (slowgraph[i] == phrasenumber) {
			found = true;
			break;
		}
	}
	if (!found) {
		slowgraph.push_back(phrasenumber);
	}
}

int ListContainer::bmsearch(char *file, int fl, std::string s)
{
	// must match all
	int j, l;  // counters
	int p;  // to hold precalcuated value for speed
	bool match;  // flag
	int qsBc[256];  // Quick Search Boyer Moore shift table (256 alphabet)
	char *k;  // pointer used in matching

	int count = 0;

	int pl = s.length();
	char *phrase = new char[pl + 1];
	for (j = 0; j < pl; j++) {
		phrase[j] = s[j];
	}
	phrase[pl] = 0;

	if (fl < pl)
		return 0;  // reality checking
	if (pl > 126)
		return 0;  // reality checking

	// For speed we append the phrase to the end of the memory block so it
	// is always found, thus eliminating some checking.  This is possible as
	// we know an extra 127 bytes have been provided by NaughtyFilter.cpp
	// and also the OptionContainer does not allow phrase lengths greater
	// than 126 chars
	k = file + fl;
	for (j = 0; j < pl; j++) {
		k[j] = s[j];
	}

	// Next we need to make the Quick Search Boyer Moore shift table

	p = pl + 1;
	for (j = 0; j < 256; j++) {	// Preprocessing
		qsBc[j] = p;
	}
	for (j = 0; j < pl; j++) {	// Preprocessing
		qsBc[(unsigned char) phrase[j]] = pl - j;
	}

	// Now do the searching!

	for (j = 0;;) {
		k = file + j;
		match = true;
		for (l = 0; l < pl; l++) {	// quiv, but faster, memcmp()
			if (k[l] != phrase[l]) {
				match = false;
				break;
			}
		}
		if (match) {
			if (j >= fl) {
				break;  // is the end of file marker
			}
			count++;
		}
		j += qsBc[(unsigned char) file[j + pl]];  // shift
	}
	delete[]phrase;
	return count;
}

// Format of the data is each entry has GRAPHENTRYSIZE int values with format of:
// [letter][last letter flag][num links][from phrase][link0][link1]...

void ListContainer::graphSearch(std::deque<unsigned int>& result, char *doc, int len)
{
	int i, j, k;
	int sl;
	int ppos;
	int * graphdata = realgraphdata;
	
	//do standard quick search on short branches (or everything, if force_quick_search is on)
	sl = slowgraph.size();
	for (i = 0; i < sl; i++) {
		ppos = slowgraph[i];
		j = bmsearch(doc, len, getItemAtInt(ppos));
		for (k = 0; k < j; k++) {
			result.push_back(ppos);
		}
	}
	
	if (force_quick_search == 1 || graphitems == 0) {
		return;
	}
	
	int ml;
	int *stack = new int[maxchildnodes];
	int stacksize = 0;
	unsigned char p;
	int pos;
	int depth;
	// number of links from root node to first letter of phrase
	ml = graphdata[2] + 4;
	// iterate over entire document
	for (i = 0; i < len; i++) {
		// iterate over all children of the root node
		for (j = 4; j < ml; j++) {
			// grab the endpoint of this link
			pos = graphdata[j];

			// now comes the main graph search!
			// this is basically a depth-first tree search
			depth = 0;
			while (true) {
				// get the address of the link endpoint and the data actually stored at it
				// note that this only works for GRAPHENTRYSIZE == 64
				ppos = pos << 6;
				if (ppos == 0) {
					graphdata = realgraphdata;
				} else {
					graphdata = realgraphdata + ROOTOFFSET;
				}
				p = graphdata[ppos];

				// does the character at this string depth match the relevant character in the node we're currently looking at?
				if (p == doc[i + depth]) {
					// it does!
					// is this graph node marked as being the end of a phrase?
					if (graphdata[ppos + 1] == 1) {
						// it is, so store the pointer to the matched phrase.
						result.push_back(graphdata[ppos + 3]);
					}
					// grab this node's number of children
					sl = graphdata[ppos + 2];
					if (sl > 0) {
						// add the node's children (bar the first) to the stack for later examination
						// and signal that we're moving along the current string by one.
						depth++;
						stacksize=0;
						for (k = 1; k < sl; k++) {
							stack[stacksize++] = graphdata[ppos + 4 + k];
						}
						// zip straight to the first child of the matched node
						// (this is the magic that makes it depth first)
						pos = graphdata[ppos + 4];
						continue;
					}
					// if we just matched a node that has no children,
					// we can stop searching. there should be no case in
					// which the node was not also marked as end of phrase.
					else break;
				}

				if (stacksize > 0) {
					// if we get here, we have discounted one branch, but
					// we still have more branches to examine at this level.
					// but do we ever actually need to backtrack here?
					// after all, surely a character will only ever match
					// one node at any given level. so perhaps *only* store
					// a stack of children from the most recently matched node,
					// so that this code will never initiate backtracking.
					// the stack would then be allowed to be reduced to 63,
					// since we only have max GRAPHENTRYSIZE children on a node, and
					// we'd set stacksize to 0 before we do any adding, to
					// effectively clear it beforehand.
					pos = stack[--stacksize];
					continue;
				}
				// if we get here, we've discounted all branches at this depth, and the search is over.
				break;
			}
		}
	}
	delete[]stack;
}

void ListContainer::graphAdd(String s, int inx, int item)
{
	unsigned char p = s.charAt(0);
	unsigned char c;
	bool found = false;
	String t;
	int i, px, it;
	int numlinks;
	int *graphdata;
	int *graphdata2 = realgraphdata + ROOTOFFSET;
	if (inx == 0)
		graphdata = realgraphdata;
	else
		graphdata = realgraphdata + ROOTOFFSET;
	//iterate over the input node's immediate children
	for (i = 0; i < graphdata[inx * GRAPHENTRYSIZE + 2]; i++) {
		//grab the character from this child
		c = (unsigned char) graphdata2[(graphdata[inx * GRAPHENTRYSIZE + 4 + i]) * GRAPHENTRYSIZE];
		if (p == c) {
			//it matches the first char of our string!
			//keep searching, starting from here, to see if the entire phrase is already in the graph
			t = s;
			t.lop();
			if (t.length() > 0) {
				graphAdd(t, graphdata[inx * GRAPHENTRYSIZE + 4 + i], item);
				return;
			}
			found = true;
			
			// this means the phrase is already there
			// as part of an existing phrase
			
			//check the end of word flag on the child
			px = graphdata2[(graphdata[inx * GRAPHENTRYSIZE + 4 + i]) * GRAPHENTRYSIZE + 1];
			if (px == 1) {
				// the exact phrase is already there
				px = graphdata2[(graphdata[inx * GRAPHENTRYSIZE + 4 + i]) * GRAPHENTRYSIZE + 3];
				it = itemtype[px];

				if ((it > 9 && itemtype[item] < 10) || itemtype[item] == -1) {
					// exists as a combi entry already
					// if got here existing entry must be a combi AND
					// new entry is not a combi so we overwrite the
					// existing values as combi values and types are
					// stored in the combilist
					// OR
					// its a an exception
					// exception phrases take precedence
					itemtype[px] = itemtype[item];
					weight[px] = weight[item];
					categoryindex[px] = categoryindex[item];
				}
			}
		}
	}
	// the phrase wasn't already in the list, so add it
	if (!found) {
		i = graphitems;
		graphitems++;
		numlinks = graphdata[inx * GRAPHENTRYSIZE + 2];
		if ((inx == 0) ? ((numlinks + 1) > MAXROOTLINKS) : ((numlinks +1) > MAXLINKS)) {
			syslog(LOG_ERR, "More than %d links from this node! 1", (inx == 0) ? MAXROOTLINKS : MAXLINKS);
			if (!is_daemonised)
				std::cout << "More than " << ((inx == 0) ? MAXROOTLINKS : MAXLINKS) << " links from this node! 1" << std::endl;
			exit(1);
		}
		if ((numlinks + 1) > maxchildnodes) {
			maxchildnodes = numlinks + 1;
#ifdef DGDEBUG
			prolificroot = (inx == 0);
#endif
		}
#ifdef DGDEBUG
		else if ((numlinks + 1) > secondmaxchildnodes)
			secondmaxchildnodes = numlinks + 1;
#endif
		graphdata[inx * GRAPHENTRYSIZE + 2] = numlinks + 1;
		graphdata[inx * GRAPHENTRYSIZE + 4 + numlinks] = i;
		graphdata2[i * GRAPHENTRYSIZE] = p;
		graphdata2[i * GRAPHENTRYSIZE + 3] = item;
		s.lop();
		// iterate over remaining characters and add child nodes
		while (s.length() > 0) {
			numlinks = graphdata2[i * GRAPHENTRYSIZE + 2];
			if ((inx == 0) ? ((numlinks + 1) > MAXROOTLINKS) : ((numlinks +1) > MAXLINKS)) {
				syslog(LOG_ERR, "More than %d links from this node! 2", (inx == 0) ? MAXROOTLINKS : MAXLINKS);
				if (!is_daemonised)
					std::cout << "More than " << ((inx == 0) ? MAXROOTLINKS : MAXLINKS) << " links from this node! 2" << std::endl;
				exit(1);
			}
			if ((numlinks + 1) > maxchildnodes) {
				maxchildnodes = numlinks + 1;
#ifdef DGDEBUG
				prolificroot = (inx == 0);
#endif
			}
#ifdef DGDEBUG
			else if ((numlinks + 1) > secondmaxchildnodes)
				secondmaxchildnodes = numlinks + 1;
#endif
			graphdata2[i * GRAPHENTRYSIZE + 2] = numlinks + 1;
			graphdata2[i * GRAPHENTRYSIZE + 4 + numlinks] = i + 1;
			i++;
			graphitems++;
			p = s.charAt(0);
			graphdata2[i * GRAPHENTRYSIZE] = p;
			graphdata2[i * GRAPHENTRYSIZE + 3] = item;
			s.lop();
		}
		graphdata2[i * GRAPHENTRYSIZE + 1] = 1;
	}
}

// quicksort with 3 way partitioning sorted by the end of the line
void ListContainer::quicksortEW(int l, int r)
{
	if (r <= l)
		return;
	unsigned int e;
	int k;
	char *v = data + list[r];
	int i = l - 1, j = r, p = i, q = r;
	for (;;) {
		while (greaterThanEWF(data + list[++i], v) > 0);
		while (greaterThanEWF(v, data + list[--j]) > 0) {
			if (j == l)
				break;
		}
		if (i >= j)
			break;
		e = list[i];
		list[i] = list[j];
		list[j] = e;
		if (greaterThanEWF(v, data + list[i]) == 0) {
			p++;
			e = list[p];
			list[p] = list[i];
			list[i] = e;
		}
		if (greaterThanEWF(v, data + list[j]) == 0) {
			q--;
			e = list[q];
			list[q] = list[j];
			list[j] = e;
		}
	}
	e = list[i];
	list[i] = list[r];
	list[r] = e;
	j = i - 1;
	i++;
	for (k = l; k <= p; k++, j--) {
		e = list[k];
		list[k] = list[j];
		list[j] = e;
	}
	for (k = r - 1; k >= q; k--, i++) {
		e = list[k];
		list[k] = list[i];
		list[i] = e;
	}
	quicksortEW(l, j);
	quicksortEW(i, r);
}

// quicksort with 3 way partitioning sorted by the start of the line
void ListContainer::quicksortSW(int l, int r)
{
	if (r <= l)
		return;
	unsigned int e;
	int k;
	char *v = data + list[r];
	int i = l - 1, j = r, p = i, q = r;
	for (;;) {
		while (greaterThanSWF(data + list[++i], v) > 0);
		while (greaterThanSWF(v, data + list[--j]) > 0) {
			if (j == l)
				break;
		}
		if (i >= j)
			break;
		e = list[i];
		list[i] = list[j];
		list[j] = e;
		if (greaterThanSWF(v, data + list[i]) == 0) {
			p++;
			e = list[p];
			list[p] = list[i];
			list[i] = e;
		}
		if (greaterThanSWF(v, data + list[j]) == 0) {
			q--;
			e = list[q];
			list[q] = list[j];
			list[j] = e;
		}
	}
	e = list[i];
	list[i] = list[r];
	list[r] = e;
	j = i - 1;
	i++;
	for (k = l; k <= p; k++, j--) {
		e = list[k];
		list[k] = list[j];
		list[j] = e;
	}
	for (k = r - 1; k >= q; k--, i++) {
		e = list[k];
		list[k] = list[i];
		list[i] = e;
	}
	quicksortSW(l, j);
	quicksortSW(i, r);
}

bool ListContainer::readProcessedItemList(const char *filename, bool startswith, int filters)
{
#ifdef DGDEBUG
	std::cout << "reading processed file:" << filename << std::endl;
#endif
	int len = getFileLength(filename);
	int slen, i;
	if (len < 0) {
		if (!is_daemonised) {
			std::cerr << "Error reading file: " << filename << std::endl;
		}
		syslog(LOG_ERR, "%s", "Error reading file:");
		syslog(LOG_ERR, "%s", filename);
		return false;
	}
	if (len < 5) {
		if (!is_daemonised) {
			std::cerr << "File too small (less than 5 bytes - is it corrupt?): " << filename << std::endl;
		}
		syslog(LOG_ERR, "%s", "File too small (less than 5 bytes - is it corrupt?):");
		syslog(LOG_ERR, "%s", filename);
		return false;
	}
	increaseMemoryBy(len + 2);
	ifstream listfile(filename, ios::in);
	if (!listfile.good()) {
		if (!is_daemonised) {
			std::cerr << "Error opening: " << filename << std::endl;
		}
		syslog(LOG_ERR, "%s", "Error opening:");
		syslog(LOG_ERR, "%s", filename);
		return false;
	}
	std::string linebuffer;
	String temp, inc;
	while (!listfile.eof()) {
		getline(listfile, linebuffer);
		if (linebuffer[0] == '.') {
			temp = linebuffer.c_str();
			if (temp.startsWith(".Include<")) {	// see if we have another list
				inc = temp.after(".Include<").before(">");  // to include
				if (!readAnotherItemList(inc.toCharArray(), startswith, filters)) {	// read it
					listfile.close();
					return false;
				}
				continue;
			}
		}
		else if (linebuffer[0] == '#') {
			temp = linebuffer.c_str();
			if (temp.startsWith("#time: ")) {	// see if we have a time tag
				if (!readTimeTag(&temp)) {
					return false;
				}
				continue;
			}
			else if (temp.startsWith("#listcategory:")) {	// see if we have a category
				category = temp.after("\"").before("\"");
#ifdef DGDEBUG
				std::cout << "found item processed list category: " << category << std::endl;
#endif
				continue;
			}
			continue;  // it's a comment man
		}
		slen = linebuffer.length();
		if (slen < 3)
			continue;
		i = slen - 1;
		list.push_back(data_length);
		for (; i >= 0; i--) {
			data[data_length + i] = linebuffer[i];
		}
		data[data_length + slen] = 0;
		data_length += slen + 1;
		items++;
	}
	listfile.close();
	return true;
}

void ListContainer::addToItemList(char *s, int len)
{
	int i;
	list.push_back(data_length);
	for (i = 0; i < len; i++) {
		data[data_length + i] = s[i];
	}
	data[data_length + len] = 0;
	data_length += len + 1;
	items++;
}

int ListContainer::searchRSWF(int a, int s, const char *p)
{
	if (a > s)
		return (-1 - a);
	int m = (a + s) / 2;
	int r = greaterThanSWF(p, data + list[m]);
	if (r == 0)
		return m;
	if (r == -1)
		return searchRSWF(m + 1, s, p);
	if (a == s)
		return (-1 - a);
	return searchRSWF(a, m - 1, p);
}

int ListContainer::searchRSW(int a, int s, const char *p)
{
	if (a > s)
		return (-1 - a);
	int m = (a + s) / 2;
	int r = greaterThanSW(p, data + list[m]);
	if (r == 0)
		return m;
	if (r == -1)
		return searchRSW(m + 1, s, p);
	if (a == s)
		return (-1 - a);
	return searchRSW(a, m - 1, p);
}

int ListContainer::searchREWF(int a, int s, const char *p)
{
	if (a > s)
		return (-1 - a);
	int m = (a + s) / 2;
	int r = greaterThanEWF(p, data + list[m]);
	if (r == 0)
		return m;
	if (r == -1)
		return searchREWF(m + 1, s, p);
	if (a == s)
		return (-1 - a);
	return searchREWF(a, m - 1, p);
}

int ListContainer::searchREW(int a, int s, const char *p)
{
	if (a > s)
		return (-1 - a);
	int m = (a + s) / 2;
	int r = greaterThanEW(p, data + list[m]);
	if (r == 0)
		return m;
	if (r == -1)
		return searchREW(m + 1, s, p);
	if (a == s)
		return (-1 - a);
	return searchREW(a, m - 1, p);
}

int ListContainer::greaterThanEWF(const char *a, const char *b)
{
	int alen = strlen(a);
	int blen = strlen(b);
	int maxlen = alen < blen ? alen : blen;
	char *apos = (char *) a + alen - 1;
	char *bpos = (char *) b + blen - 1;
	unsigned char achar;
	unsigned char bchar;
	while (maxlen > 0) {
		achar = apos[0];
		bchar = bpos[0];
		if (achar > bchar) {
			return 1;
		}
		if (achar < bchar) {
			return -1;
		}
		maxlen--;
		apos--;
		bpos--;
	}
	if (alen > blen) {
		return 1;
	}
	if (alen < blen) {
		return -1;
	}
	return 0;  // both equal
}

int ListContainer::greaterThanSWF(const char *a, const char *b)
{
	int alen = strlen(a);
	int blen = strlen(b);
	int maxlen = alen < blen ? alen : blen;
	char *apos = (char *) a;
	char *bpos = (char *) b;
	int i = 0;
	unsigned char achar;
	unsigned char bchar;
	while (i < maxlen) {
		achar = apos[0];
		bchar = bpos[0];
		if (achar > bchar) {
			return 1;
		}
		if (achar < bchar) {
			return -1;
		}
		i++;
		apos++;
		bpos++;
	}
	if (alen > blen) {
		return 1;
	}
	if (alen < blen) {
		return -1;
	}
	return 0;  // both equal
}

int ListContainer::greaterThanSW(const char *a, const char *b)
{
	int alen = strlen(a);
	int blen = strlen(b);

	int maxlen = alen < blen ? alen : blen;
	int i = 0;  // this used to be set to 1 - I don't know why
	unsigned char achar;
	unsigned char bchar;
	// compare from the front of the URL forwards; then if we find a mismatch,
	// look for shorter or alphabetically different URLs
	while (i < maxlen) {
		achar = a[i];
		bchar = b[i];
		if (achar > bchar) {
			return 1;
		}
		if (achar < bchar) {
			return -1;
		}
		i++;
	}
	
	// if the URLs didn't match and the one the user is browsing to is longer
	// than what we just compared against, we need to compare against longer URLs,
	// but only if the next character is actually part of a folder name rather than a separator.
	// ('cos if it *is* a separator, it doesn't matter that the overall URL is longer, the
	// beginning of it matches a banned URL.)
	if ((alen > blen) && !(a[blen] == '/' || a[blen]=='?' || a[blen]=='&' || a[blen]=='='))
		return 1;

	// if the banned URL is longer than the URL we're checking, the two
	// can't possibly match.
	if (blen > alen)
		return -1;

	return 0;  // both equal
}

int ListContainer::greaterThanEW(const char *a, const char *b)
{
	int alen = strlen(a);
	int blen = strlen(b);
	int maxlen = alen < blen ? alen : blen;
	char *apos = (char *) a + alen - 1;
	char *bpos = (char *) b + blen - 1;
	unsigned char achar;
	unsigned char bchar;
	while (maxlen > 0) {
		achar = apos[0];
		bchar = bpos[0];
		if (achar > bchar) {
			return 1;
		}
		if (achar < bchar) {
			return -1;
		}
		maxlen--;
		apos--;
		bpos--;
	}
	if (blen > alen)
		return -1;
	return 0;  // both equal
}

bool ListContainer::isCacheFileNewer(const char *filename)
{
	int len = getFileLength(filename);
	if (len < 0) {
		if (!is_daemonised) {
			std::cerr << "Error reading file: " << filename << std::endl;
		}
		syslog(LOG_ERR, "%s", "Error reading file:");
		syslog(LOG_ERR, "%s", filename);
		return false;
	}
	int bannedlistdate = getFileDate(filename);
	std::string linebuffer = filename;
	linebuffer += ".processed";
	int cachedate = getFileDate(linebuffer.c_str());
	if (cachedate < bannedlistdate) {
		return false;  // cache file is older than list file
	}
	return true;
}

void ListContainer::increaseMemoryBy(int bytes)
{
	if (data_memory > 0) {
		char *temp = new char[data_memory + bytes];  // replacement store
		memcpy(temp, data, data_length);
		delete[]data;
		data = temp;
		data_memory = data_memory + bytes;
	} else {
		delete[]data;
		data = new char[bytes];
		data_memory = bytes;
	}
}

std::string ListContainer::toLower(std::string s)
{
	int l = s.length();
	for (int i = 0; i < l; i++) {
		if ((s[i] >= 'A') && (s[i] <= 'Z')) {
			s[i] = 'a' + s[i] - 'A';
		}
	}
	return s;
}

int ListContainer::getFileLength(const char *filename)
{
	int len;
	FILE *file = NULL;
	file = fopen(filename, "r");
	if (file) {
		if (!fseek(file, 0, SEEK_END)) {
			len = ftell(file);
		} else {
			len = -1;
		}
		fclose(file);
		return len;
	}
	return -1;
}

int ListContainer::getFileDate(const char *filename)
{
	struct stat status;
	int rc = stat(filename, &status);
	if (rc != 0) {
		return -1;
	}
	struct tm *tmnow = localtime(&status.st_mtime);

	int date = (tmnow->tm_year - 100) * 31536000;
	date += tmnow->tm_mon * 2628000;
	date += tmnow->tm_mday * 86400;
	date += tmnow->tm_hour * 3600;
	date += tmnow->tm_min * 60;
	date += tmnow->tm_sec;
	return date;  // a nice int rather than a horrid struct
}

bool ListContainer::upToDate()
{
	if (getFileDate(sourcefile.toCharArray()) > filedate) {
		return false;
	}
	for (unsigned int i = 0; i < morelists.size(); i++) {
		if (!(*o.lm.l[morelists[i]]).upToDate()) {
			return false;
		}
	}
	return true;
}

bool ListContainer::readTimeTag(String * tag)
{
#ifdef DGDEBUG
	std::cout << "Found a time tag" << std::endl;
#endif
	unsigned int tsthour, tstmin, tendhour, tendmin;
	String temp = (*tag).after("#time: ");
	tsthour = temp.before(" ").toInteger();
	temp = temp.after(" ");
	tstmin = temp.before(" ").toInteger();
	temp = temp.after(" ");
	tendhour = temp.before(" ").toInteger();
	temp = temp.after(" ");
	tendmin = temp.before(" ").toInteger();
	String tdays = temp.after(" ");
	tdays.removeWhiteSpace();
	if (tsthour > 23) {
		if (!is_daemonised) {
			std::cerr << "Time Tag Start Hour over bounds." << std::endl;
		}
		syslog(LOG_ERR, "%s", "Time Tag Start Hour over bounds.");
		return false;
	}
	if (tendhour > 23) {
		if (!is_daemonised) {
			std::cerr << "Time Tag End Hour over bounds." << std::endl;
		}
		syslog(LOG_ERR, "%s", "Time Tag End Hour over bounds.");
		return false;
	}
	if (tstmin > 59) {
		if (!is_daemonised) {
			std::cerr << "Time Tag Start Min over bounds." << std::endl;
		}
		syslog(LOG_ERR, "%s", "Time Tag Start Min over bounds.");
		return false;
	}
	if (tendmin > 59) {
		if (!is_daemonised) {
			std::cerr << "Time Tag End Min over bounds." << std::endl;
		}
		syslog(LOG_ERR, "%s", "Time Tag End Min over bounds.");
		return false;
	}
	if (tdays.length() > 7) {
		if (!is_daemonised) {
			std::cerr << "Time Tag Days over bounds." << std::endl;
		}
		syslog(LOG_ERR, "%s", "Time Tag Days over bounds.");
		return false;
	}
	istimelimited = true;
	sthour = tsthour;
	stmin = tstmin;
	endhour = tendhour;
	endmin = tendmin;
	days = tdays;
	timetag = (*tag);
	return true;
}

bool ListContainer::isNow()
{
	// returns true if the current time is within the limits specified on this list
	if (!istimelimited) {
		return true;
	}
	time_t tnow;  // to hold the result from time()
	struct tm *tmnow;  // to hold the result from localtime()
	unsigned int hour, min, wday;
	time(&tnow);  // get the time after the lock so all entries in order
	tmnow = localtime(&tnow);  // convert to local time (BST, etc)
	hour = tmnow->tm_hour;
	min = tmnow->tm_min;
	wday = tmnow->tm_wday;
	// wrap week to start on Monday
	if (wday == 0) {
		wday = 7;
	}
	wday--;
	unsigned char cday = '0' + wday;
	bool matchday = false;
	for (int i = 0; i < days.length(); i++) {
		if (days[i] == cday) {
			matchday = true;
			break;
		}
	}
	if (!matchday) {
		return false;
	}
	if (hour < sthour) {
		return false;
	}
	if (hour > endhour) {
		return false;
	}
	if (hour == sthour) {
		if (min < stmin) {
			return false;
		}
	}
	if (hour == endhour) {
		if (min > endmin) {
			return false;
		}
	}
#ifdef DGDEBUG
	std::cout << "time match " << sthour << ":" << stmin << "-" << endhour << ":" << endmin << " " << hour << ":" << min << " " << sourcefile << std::endl;
#endif
	return true;
}

int ListContainer::getCategoryIndex(String * lcat)
{
	// where in the category list is our category? if nowhere, add it.
	if ((*lcat).length() < 2) {
#ifdef DGDEBUG
		std::cout << "blank entry index" << std::endl;
#endif
		return 0;  // blank entry index
	}
	int l = (signed) listcategory.size();
	int i;
	for (i = 0; i < l; i++) {
		if ((*lcat) == listcategory[i]) {
			return i;
		}
	}
	listcategory.push_back((*lcat));
	return l;
}

String ListContainer::getListCategoryAt(int index, int *catindex)
{
	//category index of -1 indicates uncategorised list
	if ((index >= categoryindex.size()) || (categoryindex[index] < 0)) {
		return "";
	}
	//return category index (allows naughtyfilter to do numeric comparison between categories
	//when doing duplicate search; much faster than string comparison)
	if (catindex != NULL) {
		(*catindex) = categoryindex[index];
	}
	return listcategory[categoryindex[index]];
}

String ListContainer::getListCategoryAtD(int index)
{
	//category index of -1 indicates uncategorised list
	if ((index < 0) || (index >= listcategory.size())) {
		return "";
	}
	return listcategory[index];
}
