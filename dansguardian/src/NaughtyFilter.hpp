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

#ifndef __HPP_NAUGHTYFILTER
#define __HPP_NAUGHTYFILTER
#include "String.hpp"
#include <string>
#include "OptionContainer.hpp"
#include "DataBuffer.hpp"


class NaughtyFilter {

public:
    NaughtyFilter();
    void checkme(DataBuffer* body);
    void contentRegExp(DataBuffer* body);
    bool isItNaughty;
    bool isException;
    int filtergroup;
    std::string whatIsNaughty;
    std::string whatIsNaughtyLog;

private:
    void checkphrase(char* file, int l);
    void checkPICS(char* file, int l);
    void checkPICSrating(std::string label);
    void checkPICSratingSafeSurf(String r);
    void checkPICSratingevaluWEB(String r);
    void checkPICSratingCyberNOT(String r);
    void checkPICSratingRSAC(String r);
    void checkPICSratingICRA(String r);
    void checkPICSratingWeburbia(String r);
    void checkPICSratingVancouver(String r);
    void checkPICSagainstoption(String s, char* l, int opt, std::string m);
    };

#endif
