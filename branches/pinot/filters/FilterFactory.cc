/*
 *  Copyright 2007 Fabrice Colin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <dlfcn.h>
#include <algorithm>
#include <iostream>

#include "Filter.h"
#include "ExternalFilter.h"
#include "HtmlFilter.h"
#include "MboxFilter.h"
#include "TagLibFilter.h"
#include "TextFilter.h"
#include "XmlFilter.h"
#include "FilterFactory.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::set;
using std::copy;
using std::inserter;
using namespace Dijon;

set<string> FilterFactory::m_externalTypes;

FilterFactory::FilterFactory()
{
}

FilterFactory::~FilterFactory()
{
}

unsigned int FilterFactory::loadFilters(const string &dir_name,
	const string &externalfilters_config_file)
{
	ExternalFilter::initialize(externalfilters_config_file, m_externalTypes);

	return 0;
}

Filter *FilterFactory::getFilter(const string &mime_type)
{
	string typeOnly(mime_type);
	string::size_type semiColonPos = mime_type.find(";");

	// Remove the charset, if any
	if (semiColonPos != string::npos)
	{
		typeOnly = mime_type.substr(0, semiColonPos);
	}
#ifdef DEBUG
	cout << "FilterFactory::getFilter: file type is " << typeOnly << endl;
#endif

	if (typeOnly == "text/html")
	{
		return new HtmlFilter(typeOnly);
	}
	else if (typeOnly == "text/plain")
	{
		return new TextFilter(typeOnly);
	}
	else if (typeOnly == "application/mbox")
	{
		return new MboxFilter(typeOnly);
	}
	else if ((typeOnly == "text/xml") ||
		(typeOnly == "application/xml"))
	{
		return new XmlFilter(typeOnly);
	}
	else if ((typeOnly == "audio/mpeg") ||
		(typeOnly == "audio/x-mp3") ||
		(typeOnly == "application/ogg") ||
		(typeOnly == "audio/x-flac+ogg") ||
		(typeOnly == "audio/x-flac"))
	{
		return new TagLibFilter(typeOnly);
	}
	else if (m_externalTypes.find(typeOnly) != m_externalTypes.end())
	{
		return new ExternalFilter(typeOnly);
	}
	else if (strncasecmp(typeOnly.c_str(), "text", 4) == 0)
	{
		return new TextFilter(typeOnly);
	}

	return NULL;
}

void FilterFactory::getSupportedTypes(set<string> &mime_types)
{
	mime_types.clear();

	// List supported types
	mime_types.insert("text/plain");
	mime_types.insert("text/html");
	mime_types.insert("application/mbox");
	mime_types.insert("text/xml");
	mime_types.insert("application/xml");
	mime_types.insert("audio/mpeg");
	mime_types.insert("audio/x-mp3");
	mime_types.insert("application/ogg");
	mime_types.insert("audio/x-flac+ogg");
	mime_types.insert("audio/x-flac");
	copy(mime_types.begin(), mime_types.end(),
		 inserter(mime_types, m_externalTypes.begin()));
}

bool FilterFactory::isSupportedType(const string &mime_type)
{
	string typeOnly(mime_type);
	string::size_type semiColonPos = mime_type.find(";");

	// Remove the charset, if any
	if (semiColonPos != string::npos)
	{
		typeOnly = mime_type.substr(0, semiColonPos);
	}

	// Is it a built-in type ?
	if ((typeOnly == "text/html") ||
		(typeOnly == "application/mbox") ||
		(typeOnly == "text/xml") ||
		(typeOnly == "application/xml") ||
		(typeOnly == "audio/mpeg") ||
		(typeOnly == "audio/x-mp3") ||
		(typeOnly == "application/ogg") ||
		(typeOnly == "audio/x-flac+ogg") ||
		(typeOnly == "audio/x-flac") ||
		(strncasecmp(typeOnly.c_str(), "text", 4) == 0) ||
		(m_externalTypes.find(typeOnly) != m_externalTypes.end()))
	{
		return true;
	}

	return false;
}

void FilterFactory::unloadFilters(void)
{
}
