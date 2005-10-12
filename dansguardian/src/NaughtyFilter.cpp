//Please refer to http://dansguardian.org/?page=copyright2
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

#include "NaughtyFilter.hpp"
#include "RegExp.hpp"
#include "ListContainer.hpp"

#include <syslog.h>
#include <algorithm>
#include <deque>


// GLOBALS 

extern OptionContainer o;

//using namespace std;


// IMPLEMENTATION

// constructor - set up defaults
NaughtyFilter::NaughtyFilter()
:	isItNaughty(false), isException(false), filtergroup(0), whatIsNaughty(""), whatIsNaughtyLog(""), whatIsNaughtyCategories("")
{
}

// check the given document body for banned, weighted, and exception phrases (and PICS, and regexes, &c.)
void NaughtyFilter::checkme(DataBuffer * body)
{
	int bodylen = (*body).buffer_length;
	char *rawbody = (*body).data;
#ifdef DGDEBUG
	std::cout << "body len:" << bodylen << std::endl;
#endif

	char *bodylc = new char[bodylen + 128];
	// The extra 128 is used for various speed tricks to
	// squeeze as much speed as possible.
	char *bodynohtml;
	bodynohtml = NULL;  // to avoid compiler warnings

	try {			// the last thing we need is an exception causing a memory leak

		int i, j, bodynohtmllen;
		unsigned char c;
		// make a copy of the document lowercase char by char
		if (o.preserve_case == 1) {
			for (i = 0; i < bodylen; i++) {
				c = rawbody[i];
				if (c == 13 || c == 9 || c == 10) {
					c = 32;  // convert all whitespace to a space
				}
				bodylc[i] = c;
			}
		} else {
			for (i = 0; i < bodylen; i++) {
				c = rawbody[i];
				if (c >= 'A' && c <= 'Z') {
					c = 'a' + c - 'A';
				}
				else if (c >= 192 && c <= 221) {	// for accented chars
					c += 32;  // 224 + c - 192
				} else {
					if (c == 13 || c == 9 || c == 10) {
						c = 32;  // convert all whitespace to a space
					}
				}
				bodylc[i] = c;
			}
		}

		if (o.hex_decode_content == 1) {	// Mod suggested by
			// AFN Tue 8th April 2003
			char *hexdecoded = new char[bodylen + 128 + 1];
			unsigned char c1;
			unsigned char c2;
			unsigned char c3;
			char hexval[5] = "0x";  // Initializes a "hexadecimal string"
			hexval[4] = '\0';
			char *ptr;  // pointer required by strtol

			// make a copy of the escaped document char by char
			i = 0;
			j = 0;
			while (i < bodylen - 3) {	// we lose 3 bytes but what the hell..
				c1 = bodylc[i];
				c2 = bodylc[i + 1];
				c3 = bodylc[i + 2];
				if (c1 == '%' && (((c2 >= '0') && (c2 <= '9')) || ((c2 >= 'a') && (c2 <= 'f')))
					&& (((c3 >= '0') && (c3 <= '9')) || ((c3 >= 'a') && (c3 <= 'f'))))
				{
					hexval[2] = c2;
					hexval[3] = c3;
					c = (unsigned char) strtol(hexval, &ptr, 0);
					i += 3;
				} else {
					c = c1;
					i++;
				}
				hexdecoded[j] = c;
				j++;
			}
			if (bodylen > 3) {
				hexdecoded[bodylen - 3] = bodylc[bodylen - 3];
				hexdecoded[bodylen - 2] = bodylc[bodylen - 2];
				hexdecoded[bodylen - 1] = bodylc[bodylen - 1];
				hexdecoded[bodylen] = '\0';
			}
			delete[]bodylc;
			bodylc = hexdecoded;

		}

		if ((*o.fg[filtergroup]).enable_PICS == 1) {
			checkPICS(bodylc, bodylen);
			if (isItNaughty) {
				delete[]bodylc;
				return;  // Well there is no point in continuing is there?
			}
		}

		if (o.phrase_filter_mode == 0 || o.phrase_filter_mode == 2) {
			checkphrase(bodylc, bodylen);  // check raw
			if (isItNaughty || isException) {
				delete[]bodylc;
				return;  // Well there is no point in continuing is there?
			}
		}

		if (o.phrase_filter_mode == 0) {
			delete[]bodylc;
			return;  // only doing raw mode filtering
		}

		bodynohtml = new char[bodylen + 128 + 1];
		// we need this extra byte *
		bool inhtml = false;  // to flag if our pointer is within a html <>
		bool addit;  // flag if we should copy this char to filtered version
		j = 1;
		bodynohtml[0] = 32;  // * for this
		for (i = 0; i < bodylen; i++) {
			addit = true;
			c = bodylc[i];
			if (c == '<') {
				inhtml = true;  // flag we are inside a html <>
			}
			if (c == '>') {	// flag we have just left a html <>
				inhtml = false;
				c = 32;
			}
			if (inhtml) {
				addit = false;
			}
			if (c == 32) {
				if (bodynohtml[j - 1] == 32) {	// * and this
					addit = false;
				}
			}
			if (addit) {	// if it passed the filters
				bodynohtml[j++] = c;  // copy it to the filtered copy
			}
		}
		bodynohtmllen = j;
		checkphrase(bodynohtml, bodynohtmllen);
	}
	catch(exception & e) {
	}
	delete[]bodynohtml;
	delete[]bodylc;
}

// check the phrase lists
void NaughtyFilter::checkphrase(char *file, int l)
{
	std::string bannedphrase = "";
	std::string weightedphrase = "";
	std::string exceptionphrase = "";
	int weighting = 0;
	int numfound;
	int i, j, cat;

	// this line here searches for phrases contained in the list - the rest of the code is all sorting
	// through it to find the categories, weightings, types etc. of what has actually been found.
	std::deque<unsigned int> found = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).graphSearch(file, l);
	int combisize = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).combilist.size();

	std::deque<int> listedcategories;
	numfound = found.size();
	int type, index, weight;
	bool allcmatched = true;
	bool isfound, wasbefore, catfound;
	std::string s1;

	// look for combinations first

	//if banned must wait for exception later

	bool bannedcombifound = false;
	std::string combifound = "";
	std::string combisofar = "";
	std::string categories = "";
	std::string noncombicategories = "";

	for (i = 0; i < combisize; i++) {
		index = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).combilist[i];
		if (index == -2) {
			if (allcmatched) {
				type = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).combilist[i + 1];
				if (type == -1) {	// combination exception
					isItNaughty = false;
					isException = true;
					whatIsNaughtyLog = o.language_list.getTranslation(605);
					// Combination exception phrase found:
					whatIsNaughtyLog += combisofar;
					whatIsNaughty = "";
					return;
				}
				else if (type == 1) {	// combination weighting
					weight = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).combilist[i + 2];
					weighting += weight;
					if (weight > 0) {
						catfound = false;
						cat = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).combilist[i + 3];
						//category index -1 indicates an uncategorised list
						if (cat >= 0) {
							//don't output duplicate categories
							for (j = 0; j < listedcategories.size(); j++) {
								if (listedcategories[j] == cat) {
									catfound = true;
									break;
								}
							}
							if (!catfound) {
								s1 = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getListCategoryAtD(cat).toCharArray();
								if (s1.length() > 0) {
									if (categories.length() > 0) {
										categories += ", ";
									}
									categories += s1;
								}
								listedcategories.push_back(cat);
							}
						}
					}
					if (weightedphrase.length() > 0) {
						weightedphrase += "+";
					}
					weightedphrase += "(";
					if (weight < 0) {
						weightedphrase += "-" + combisofar;
					} else {
						weightedphrase += combisofar;
					}

					weightedphrase += ")";
					combisofar = "";
				}
				else if (type == 0) {	// combination banned
					bannedcombifound = true;
					combifound = combisofar;
					catfound = false;
					cat = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).combilist[i + 3];
					if (cat >= 0) {
						for (j = 0; j < listedcategories.size(); j++) {
							if (listedcategories[j] == cat) {
								catfound = true;
								break;
							}
						}
						if (!catfound) {
							s1 = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getListCategoryAtD(cat).toCharArray();
							if (s1.length() > 0) {
								if (categories.length() > 0) {
									categories += ", ";
								}
								categories += s1;
							}
							listedcategories.push_back(cat);
						}
					}
				}
				i += 3;
			} else {
				allcmatched = true;
				i += 3;
			}
		} else {
			if (allcmatched) {
				isfound = false;
				s1 = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getItemAtInt(index);
				for (j = 0; j < numfound; j++) {
					if (s1 == (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getItemAtInt(found[j])) {
						isfound = true;
						break;
					}
				}
				if (!isfound) {
					allcmatched = false;
					combisofar = "";
				} else {
					if (combisofar.length() > 0) {
						combisofar += ", ";
					}
					combisofar += s1;
				}
			}
		}
	}

	for (i = 0; i < numfound; i++) {

		type = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getTypeAt(found[i]);
		// 0=banned, 1=weighted, -1=exception, 2=combi, 3=weightedcombi
		if (type == 0) {
			isItNaughty = true;
			bannedphrase = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getItemAtInt(found[i]);
			catfound = false;
			s1 = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getListCategoryAt(found[i], &cat).toCharArray();
			if (cat >= 0) {
				for (j = 0; j < listedcategories.size(); j++) {
					if (listedcategories[j] == cat) {
						catfound = true;
						break;
					}
				}
				if (!catfound) {
					if (s1.length() > 0) {
						if (noncombicategories.length() > 0) {
							noncombicategories += ", ";
						}
						noncombicategories += s1;
					}
					listedcategories.push_back(cat);
				}
			}
		}
		else if (type == 1) {
			if (o.weighted_phrase_mode == 1) {
				//normal mode - count all instances; i.e., multiple instances of word on one page all get counted.
				weight = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getWeightAt(found[i]);
				weighting += weight;
				catfound = false;
				s1 = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getListCategoryAt(found[i], &cat).toCharArray();
				if (cat >= 0) {
					for (j = 0; j < listedcategories.size(); j++) {
						if (listedcategories[j] == cat) {
							catfound = true;
							break;
						}
					}
					if (!catfound) {
						if (s1.length() > 0) {
							if (noncombicategories.length() > 0) {
								noncombicategories += ", ";
							}
							noncombicategories += s1;
						}
						listedcategories.push_back(cat);
					}
				}

				if (o.show_weighted_found == 1) {
					if (weightedphrase.length() > 0) {
						weightedphrase += "+";
					}
					if (weight < 0) {
						weightedphrase += "-";
					}

					weightedphrase += (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getItemAtInt(found[i]);
				}
#ifdef DGDEBUG
				std::cout << "found weighted phrase (1):" << (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getItemAtInt(found[i]) << std::endl;
#endif
			}
			else if ((*o.fg[filtergroup]).weighted_phrase_mode == 2) {
				//singular mode - each phrase counts once per page, so perform a search to see if we have found a phrase once before.
				wasbefore = false;
				for (j = 0; j < i; j++) {
					if (found[i] == found[j]) {
						wasbefore = true;
						break;
					}
				}
				if (!wasbefore) {
					weight = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getWeightAt(found[i]);
					weighting += weight;

					if (o.show_weighted_found == 1) {
						if (weightedphrase.length() > 0) {
							weightedphrase += "+";
						}
						if (weight < 0) {
							weightedphrase += "-";
						}

						weightedphrase += (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getItemAtInt(found[i]);
					}

					if (weight > 0) {
						catfound = false;
						s1 = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getListCategoryAt(found[i], &cat).toCharArray();
						if (cat >= 0) {
							for (j = 0; j < listedcategories.size(); j++) {
								if (listedcategories[j] == cat) {
									catfound = true;
									break;
								}
							}
							if (!catfound) {
								if (s1.length() > 0) {
									if (noncombicategories.length() > 0) {
										noncombicategories += ", ";
									}
									noncombicategories += s1;
								}
								listedcategories.push_back(cat);
							}
						}
					}
#ifdef DGDEBUG
					std::cout << "found weighted phrase (2):" << (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getItemAtInt(found[i]) << std::endl;
#endif
				}
			}
		}
		else if (type == -1) {
			exceptionphrase = (*o.lm.l[(*o.fg[filtergroup]).banned_phrase_list]).getItemAtInt(found[i]);
			isException = true;
			break;  // no point in going further
		}
	}
#ifdef DGDEBUG
	std::cout << "WEIGHTING: " << weighting << std::endl;
#endif
	if (isException) {
		isItNaughty = false;
		whatIsNaughtyLog = o.language_list.getTranslation(604);
		// Exception phrase found:
		whatIsNaughtyLog += exceptionphrase;
		whatIsNaughty = "";
		return;
	}

	if (isItNaughty) {
		whatIsNaughtyLog = o.language_list.getTranslation(300);
		// Banned Phrase found:
		whatIsNaughtyLog += bannedphrase;
		whatIsNaughty = o.language_list.getTranslation(301);
		// Banned phrase found.
		whatIsNaughtyCategories = noncombicategories;
		return;
	}

	if (weighting > (*o.fg[filtergroup]).naughtyness_limit) {
		isItNaughty = true;
		whatIsNaughtyLog = o.language_list.getTranslation(402);
		// Weighted phrase limit of
		whatIsNaughtyLog += String((*o.fg[filtergroup]).naughtyness_limit).toCharArray();
		whatIsNaughtyLog += " : ";
		whatIsNaughtyLog += String(weighting).toCharArray();
		if (o.show_weighted_found == 1) {
			whatIsNaughtyLog += " (";
			whatIsNaughtyLog += weightedphrase;
			whatIsNaughtyLog += ")";
		}
		whatIsNaughty = o.language_list.getTranslation(403);
		// Weighted phrase limit exceeded.
		if (categories.length() > 0) {
			whatIsNaughtyCategories = categories;
			if (noncombicategories.length() > 0) {
				whatIsNaughtyCategories += ", ";
			}
		}
		whatIsNaughtyCategories += noncombicategories;
		return;
	}
	if (bannedcombifound) {
		isItNaughty = true;
		whatIsNaughtyLog = o.language_list.getTranslation(400);
		// Banned combination phrase found:
		whatIsNaughtyLog += combifound;
		whatIsNaughty = o.language_list.getTranslation(401);
		// Banned combination phrase found.
		whatIsNaughtyCategories = categories;
		return;
	}
	// whatIsNaughty is what is displayed in the browser
	// whatIsNaughtyLog is what is logged in the log file if at all
}



// *
// *
// * PICS code
// *
// *



// check the document's PICS rating
// when checkPICS is called we assume checkphrase has made the document lower case.
void NaughtyFilter::checkPICS(char *file, int l)
{
	file[l] = 0;  // to ensure end of c-string marker is there
	(*o.fg[filtergroup]).pics1.match(file);
	if (!(*o.fg[filtergroup]).pics1.matched()) {
		return;
	}			// exit if not found
	for (int i = 0; i < (*o.fg[filtergroup]).pics1.numberOfMatches(); i++) {
		checkPICSrating((*o.fg[filtergroup]).pics1.result(i));  // pass on result for further
		// tests
	}
}

// the meat of the process 
void NaughtyFilter::checkPICSrating(std::string label)
{
	(*o.fg[filtergroup]).pics2.match(label.c_str());
	if (!(*o.fg[filtergroup]).pics2.matched()) {
		return;
	}			// exit if not found
	String lab = label.c_str();  // convert to a String for easy manip
	String r;
	String service;
	for (int i = 0; i < (*o.fg[filtergroup]).pics2.numberOfMatches(); i++) {
		r = (*o.fg[filtergroup]).pics2.result(i).c_str();  // ditto
		r = r.after("(");
		r = r.before(")");  // remove the brackets

		// Only check the substring of lab that is between
		// the start of lab (or the end of the previous match)
		// and the start of this rating.
		// It is possible to have multiple ratings in one pics-label.
		// This is done on e.g. http://www.jesusfilm.org/
		if (i == 0) {
			service = lab.subString(0, (*o.fg[filtergroup]).pics2.offset(i));
		} else {
			service = lab.subString((*o.fg[filtergroup]).pics2.offset(i - 1) + (*o.fg[filtergroup]).pics2.length(i - 1), (*o.fg[filtergroup]).pics2.offset(i));
		}

		if (service.contains("safesurf")) {
			checkPICSratingSafeSurf(r);
		}
		if (isItNaughty) {
			return;
		}
		if (service.contains("evaluweb")) {
			checkPICSratingevaluWEB(r);
		}
		if (isItNaughty) {
			return;
		}
		if (service.contains("microsys")) {
			checkPICSratingCyberNOT(r);
		}
		if (isItNaughty) {
			return;
		}
		if (service.contains("icra")) {
			checkPICSratingICRA(r);
		}
		if (isItNaughty) {
			return;
		}
		if (service.contains("rsac")) {
			checkPICSratingRSAC(r);
		}
		if (isItNaughty) {
			return;
		}
		if (service.contains("weburbia")) {
			checkPICSratingWeburbia(r);
		}
		if (isItNaughty) {
			return;
		}
		if (service.contains("vancouver")) {
			checkPICSratingVancouver(r);
		}
		// check label for word denoting rating system then pass on to the
		// appropriate function the rating String.
	}
}

void NaughtyFilter::checkPICSagainstoption(String s, char *l, int opt, std::string m)
{
	if (s.indexOf(l) != -1) {
		// if the rating contains the label then:
		int i = 0;
		// get the rating label value
		s = s.after(l);
		if (s.indexOf(" ") != -1) {
			//remove anything after it
			s = s.before(" ");
		}
		// sanity checking
		if (s.length() > 0) {
			i = s.toInteger();  // convert the value in a String to an integer
			if (opt < i) {	// check its value against the option in config file
				isItNaughty = true;  // must be over limit
				whatIsNaughty = m + " ";
				whatIsNaughty += o.language_list.getTranslation(1000);
				// PICS labeling level exceeded on the above site.
				whatIsNaughtyCategories = "PICS";
				whatIsNaughtyLog = whatIsNaughty;
			}
		}
	}
}

// The next few functions are flippin' obvious so no explanation...
void NaughtyFilter::checkPICSratingevaluWEB(String r)
{
	checkPICSagainstoption(r, "rating ", (*o.fg[filtergroup]).pics_evaluweb_rating, "evaluWEB age range");
}

void NaughtyFilter::checkPICSratingWeburbia(String r)
{
	checkPICSagainstoption(r, "s ", (*o.fg[filtergroup]).pics_weburbia_rating, "Weburbia rating");
}

void NaughtyFilter::checkPICSratingCyberNOT(String r)
{
	checkPICSagainstoption(r, "sex ", (*o.fg[filtergroup]).pics_cybernot_sex, "CyberNOT sex rating");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "other ", (*o.fg[filtergroup]).pics_cybernot_sex, "CyberNOT other rating");
}

void NaughtyFilter::checkPICSratingRSAC(String r)
{
	checkPICSagainstoption(r, "v ", (*o.fg[filtergroup]).pics_rsac_violence, "RSAC violence");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "s ", (*o.fg[filtergroup]).pics_rsac_sex, "RSAC sex");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "n ", (*o.fg[filtergroup]).pics_rsac_nudity, "RSAC nudity");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "l ", (*o.fg[filtergroup]).pics_rsac_language, "RSAC language");
}

void NaughtyFilter::checkPICSratingVancouver(String r)
{
	checkPICSagainstoption(r, "MC ", (*o.fg[filtergroup]).pics_vancouver_multiculturalism, "Vancouvermulticulturalism");
	checkPICSagainstoption(r, "Edu ", (*o.fg[filtergroup]).pics_vancouver_educationalcontent, "Vancouvereducationalcontent");
	checkPICSagainstoption(r, "Env ", (*o.fg[filtergroup]).pics_vancouver_environmentalawareness, "Vancouverenvironmentalawareness");
	checkPICSagainstoption(r, "Tol ", (*o.fg[filtergroup]).pics_vancouver_tolerance, "Vancouvertolerance");
	checkPICSagainstoption(r, "V ", (*o.fg[filtergroup]).pics_vancouver_violence, "Vancouverviolence");
	checkPICSagainstoption(r, "S ", (*o.fg[filtergroup]).pics_vancouver_sex, "Vancouversex");
	checkPICSagainstoption(r, "P ", (*o.fg[filtergroup]).pics_vancouver_profanity, "Vancouverprofanity");
	checkPICSagainstoption(r, "SF ", (*o.fg[filtergroup]).pics_vancouver_safety, "Vancouversafety");
	checkPICSagainstoption(r, "Can ", (*o.fg[filtergroup]).pics_vancouver_canadiancontent, "Vancouvercanadiancontent");
	checkPICSagainstoption(r, "Com ", (*o.fg[filtergroup]).pics_vancouver_commercialcontent, "Vancouvercommercialcontent");
	checkPICSagainstoption(r, "Gam ", (*o.fg[filtergroup]).pics_vancouver_gambling, "Vancouvergambling");
}

void NaughtyFilter::checkPICSratingSafeSurf(String r)
{
	checkPICSagainstoption(r, "000 ", (*o.fg[filtergroup]).pics_safesurf_agerange, "Safesurf age range");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "001 ", (*o.fg[filtergroup]).pics_safesurf_profanity, "Safesurf profanity");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "002 ", (*o.fg[filtergroup]).pics_safesurf_heterosexualthemes, "Safesurf heterosexualthemes");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "003 ", (*o.fg[filtergroup]).pics_safesurf_homosexualthemes, "Safesurf ");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "004 ", (*o.fg[filtergroup]).pics_safesurf_nudity, "Safesurf nudity");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "005 ", (*o.fg[filtergroup]).pics_safesurf_violence, "Safesurf violence");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "006 ", (*o.fg[filtergroup]).pics_safesurf_sexviolenceandprofanity, "Safesurf sexviolenceandprofanity");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "007 ", (*o.fg[filtergroup]).pics_safesurf_intolerance, "Safesurf intolerance");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "008 ", (*o.fg[filtergroup]).pics_safesurf_druguse, "Safesurf druguse");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "009 ", (*o.fg[filtergroup]).pics_safesurf_otheradultthemes, "Safesurf otheradultthemes");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "00A ", (*o.fg[filtergroup]).pics_safesurf_gambling, "Safesurf gambling");
	if (isItNaughty) {
		return;
	}
}

void NaughtyFilter::checkPICSratingICRA(String r)
{
	checkPICSagainstoption(r, "la ", (*o.fg[filtergroup]).pics_icra_languagesexual, "ICRA languagesexual");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "ca ", (*o.fg[filtergroup]).pics_icra_chat, "ICRA chat");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "cb ", (*o.fg[filtergroup]).pics_icra_moderatedchat, "ICRA moderatedchat");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "lb ", (*o.fg[filtergroup]).pics_icra_languageprofanity, "ICRA languageprofanity");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "lc ", (*o.fg[filtergroup]).pics_icra_languagemildexpletives, "ICRA languagemildexpletives");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "na ", (*o.fg[filtergroup]).pics_icra_nuditygraphic, "ICRA nuditygraphic");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "nb ", (*o.fg[filtergroup]).pics_icra_nuditymalegraphic, "ICRA nuditymalegraphic");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "nc ", (*o.fg[filtergroup]).pics_icra_nudityfemalegraphic, "ICRA nudityfemalegraphic");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "nd ", (*o.fg[filtergroup]).pics_icra_nuditytopless, "ICRA nuditytopless");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "ne ", (*o.fg[filtergroup]).pics_icra_nuditybottoms, "ICRA nuditybottoms");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "nf ", (*o.fg[filtergroup]).pics_icra_nuditysexualacts, "ICRA nuditysexualacts");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "ng ", (*o.fg[filtergroup]).pics_icra_nudityobscuredsexualacts, "ICRA nudityobscuredsexualacts");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "nh ", (*o.fg[filtergroup]).pics_icra_nuditysexualtouching, "ICRA nuditysexualtouching");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "ni ", (*o.fg[filtergroup]).pics_icra_nuditykissing, "ICRA nuditykissing");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "nr ", (*o.fg[filtergroup]).pics_icra_nudityartistic, "ICRA nudityartistic");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "ns ", (*o.fg[filtergroup]).pics_icra_nudityeducational, "ICRA nudityeducational");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "nt ", (*o.fg[filtergroup]).pics_icra_nuditymedical, "ICRA nuditymedical");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "oa ", (*o.fg[filtergroup]).pics_icra_drugstobacco, "ICRA drugstobacco");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "ob ", (*o.fg[filtergroup]).pics_icra_drugsalcohol, "ICRA drugsalcohol");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "oc ", (*o.fg[filtergroup]).pics_icra_drugsuse, "ICRA drugsuse");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "od ", (*o.fg[filtergroup]).pics_icra_gambling, "ICRA gambling");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "oe ", (*o.fg[filtergroup]).pics_icra_weaponuse, "ICRA weaponuse");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "of ", (*o.fg[filtergroup]).pics_icra_intolerance, "ICRA intolerance");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "og ", (*o.fg[filtergroup]).pics_icra_badexample, "ICRA badexample");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "oh ", (*o.fg[filtergroup]).pics_icra_pgmaterial, "ICRA pgmaterial");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "va ", (*o.fg[filtergroup]).pics_icra_violencerape, "ICRA violencerape");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "vb ", (*o.fg[filtergroup]).pics_icra_violencetohumans, "ICRA violencetohumans");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "vc ", (*o.fg[filtergroup]).pics_icra_violencetoanimals, "ICRA violencetoanimals");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "vd ", (*o.fg[filtergroup]).pics_icra_violencetofantasy, "ICRA violencetofantasy");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "ve ", (*o.fg[filtergroup]).pics_icra_violencekillinghumans, "ICRA violencekillinghumans");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "vf ", (*o.fg[filtergroup]).pics_icra_violencekillinganimals, "ICRA violencekillinganimals");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "vg ", (*o.fg[filtergroup]).pics_icra_violencekillingfantasy, "ICRA violencekillingfantasy");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "vh ", (*o.fg[filtergroup]).pics_icra_violenceinjuryhumans, "ICRA violenceinjuryhumans");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "vi ", (*o.fg[filtergroup]).pics_icra_violenceinjuryanimals, "ICRA violenceinjuryanimals");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "vj ", (*o.fg[filtergroup]).pics_icra_violenceinjuryfantasy, "ICRA violenceinjuryfantasy");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "vr ", (*o.fg[filtergroup]).pics_icra_violenceartisitic, "ICRA violenceartisitic");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "vs ", (*o.fg[filtergroup]).pics_icra_violenceeducational, "ICRA violenceeducational");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "vt ", (*o.fg[filtergroup]).pics_icra_violencemedical, "ICRA violencemedical");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "vu ", (*o.fg[filtergroup]).pics_icra_violencesports, "ICRA violencesports");
	if (isItNaughty) {
		return;
	}
	checkPICSagainstoption(r, "vk ", (*o.fg[filtergroup]).pics_icra_violenceobjects, "ICRA violenceobjects");

}
