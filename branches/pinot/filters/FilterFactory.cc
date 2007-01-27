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
#include "TextFilter.h"
#include "FilterFactory.h"

#ifdef __CYGWIN__
#define DLOPEN_FLAGS RTLD_LAZY
#else
#define DLOPEN_FLAGS (RTLD_LAZY|RTLD_LOCAL)
#endif

#define GETFILTERTYPESFUNC	"_Z16get_filter_typesRSt3setISsSt4lessISsESaISsEE"
#define GETFILTERFUNC		"_Z10get_filterRKSs"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::set;
using std::map;
using std::copy;
using std::inserter;
using namespace Dijon;

set<string> FilterFactory::m_externalTypes;
map<string, string> FilterFactory::m_types;
map<string, void *> FilterFactory::m_handles;

FilterFactory::FilterFactory()
{
}

FilterFactory::~FilterFactory()
{
}

unsigned int FilterFactory::loadFilters(const string &dir_name,
	const string &externalfilters_config_file)
{
	struct stat fileStat;
	unsigned int count = 0;

	if (dir_name.empty() == true)
	{
		return 0;
	}

	// Is it a directory ?
	if ((stat(dir_name.c_str(), &fileStat) == -1) ||
		(!S_ISDIR(fileStat.st_mode)))
	{
		cerr << "FilterFactory::loadFilters: " << dir_name << " is not a directory" << endl;
		return 0;
	}

	// Scan it
	DIR *pDir = opendir(dir_name.c_str());
	if (pDir == NULL)
	{
		return 0;
	}

	// Iterate through this directory's entries
	struct dirent *pDirEntry = readdir(pDir);
	while (pDirEntry != NULL)
	{
		char *pEntryName = pDirEntry->d_name;
		if (pEntryName != NULL)
		{
			string fileName = pEntryName;
			string::size_type extPos = fileName.find_last_of(".");

			if ((extPos == string::npos) ||
				(fileName.substr(extPos) != ".so"))
			{
				// Next entry
				pDirEntry = readdir(pDir);
				continue;
			}

			fileName = dir_name;
			fileName += "/";
			fileName += pEntryName;

			// Check this entry
			if ((stat(fileName.c_str(), &fileStat) == 0) &&
				(S_ISREG(fileStat.st_mode)))
			{
				void *pHandle = dlopen(fileName.c_str(), DLOPEN_FLAGS);
				if (pHandle != NULL)
				{
					// What type(s) does this support ?
					get_filter_types_func *pTypesFunc = (get_filter_types_func *)dlsym(pHandle,
							GETFILTERTYPESFUNC);
					if (pTypesFunc != NULL)
					{
						set<string> types;
						bool filterOkay = (*pTypesFunc)(types);

						if (filterOkay == true)
						{
							for (set<string>::iterator typeIter = types.begin();
								typeIter != types.end(); ++typeIter)
							{
								// Add a record for this filter
								m_types[*typeIter] = fileName;
#ifdef DEBUG
								cout << "FilterFactory::loadFilters: type " << *typeIter
									<< " is supported by " << pEntryName << endl;
#endif
							}

							m_handles[fileName] = pHandle;
						}
						cerr << "FilterFactory::loadFilters: couldn't get types from " << fileName << endl;
					}
					else cerr << "FilterFactory::loadFilters: " << dlerror() << endl;
				}
				else cerr << "FilterFactory::loadFilters: " << dlerror() << endl;
			}
#ifdef DEBUG
			else cout << "FilterFactory::loadFilters: "
				<< pEntryName << " is not a file" << endl;
#endif
		}

		// Next entry
		pDirEntry = readdir(pDir);
	}
	closedir(pDir);

	return count;
}

Filter *FilterFactory::getLibraryFilter(const string &mime_type)
{
	void *pHandle = NULL;

	if (m_handles.empty() == true)
	{
#ifdef DEBUG
		cout << "FilterFactory::getLibraryFilter: no libraries" << endl;
#endif
		return NULL;
	}

	map<string, string>::iterator typeIter = m_types.find(mime_type);
	if (typeIter == m_types.end())
	{
		// We don't know about this type
		return NULL;
	}
	map<string, void *>::iterator handleIter = m_handles.find(typeIter->second);
	if (handleIter == m_handles.end())
	{
		// We don't know about this library
		return NULL;
	}
	pHandle = handleIter->second;
	if (pHandle == NULL)
	{
		return NULL;
	}

	// Get a filter object then
	get_filter_func *pFunc = (get_filter_func *)dlsym(pHandle,
		GETFILTERFUNC);
	if (pFunc != NULL)
	{
		return (*pFunc)(mime_type);
	}
#ifdef DEBUG
	cout << "FilterFactory::getLibraryFilter: couldn't find export getFilter" << endl;
#endif

	return NULL;
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

	if ((typeOnly == "text/plain") ||
		(m_externalTypes.find(typeOnly) != m_externalTypes.end()))
	{
		return new TextFilter(typeOnly);
	}

	Filter *pFilter = getLibraryFilter(typeOnly);
	if (pFilter == NULL)
	{
		if (strncasecmp(typeOnly.c_str(), "text", 4) == 0)
		{
			// Use this by default for text documents
			return new TextFilter(typeOnly);
		}
	}

	return pFilter;
}

void FilterFactory::getSupportedTypes(set<string> &mime_types)
{
	mime_types.clear();

	// List supported types
	mime_types.insert("text/plain");
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
	for (map<string, void*>::iterator iter = m_handles.begin(); iter != m_handles.end(); ++iter)
	{
		if (dlclose(iter->second) != 0)
		{
#ifdef DEBUG
			cout << "FilterFactory::unloadFilters: failed on " << iter->first << endl;
#endif
		}
	}

	m_types.clear();
	m_handles.clear();
}
