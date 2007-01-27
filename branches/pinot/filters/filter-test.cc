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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <map>

#include "MIMEScanner.h"
#include "Tokenizer.h"
#include "FilterFactory.h"

using namespace std;
using namespace Dijon;

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		cerr << "Usage: " << argv[0] << " <file name> RAWDATA|LISTTOKENS [TYPE=<type>]" << endl;
		return EXIT_FAILURE;
	}

	FilterFactory::loadFilters("/usr/lib64/pinot/filters", "/home/fabrice/Projects/MetaSE/pinot/Tokenize/filters/external-filters.xml");
	string mimeType;

	// How shall we get the filter ?
	if ((argc >= 4) &&
		(strncmp(argv[3], "TYPE=", 5) == 0))
	{
		mimeType = argv[3] + 5;
	}
	else
	{
		mimeType = MIMEScanner::scanFile(argv[1]);
	}

	Filter *pFilter = FilterFactory::getFilter(mimeType);
	if (pFilter == NULL)
	{
		cerr << "Couldn't obtain filter for " << argv[1] << " !" << endl;
		return EXIT_FAILURE;
	}

	if (pFilter->is_data_input_ok(Filter::DOCUMENT_FILE_NAME) == true)
	{
		pFilter->set_document_file(argv[1]);
	}
	else if (pFilter->is_data_input_ok(Filter::DOCUMENT_DATA) == true)
	{
		Document doc;
		if (doc.setDataFromFile(argv[1]) == false)
		{
			cerr << "Couldn't load " << argv[1] << " !" << endl;
			delete pFilter;
			return EXIT_FAILURE;
		}

		unsigned int dataLength = 0;
		const char *pData = doc.getData(dataLength);

		pFilter->set_document_data(pData, dataLength);
	}

	unsigned int count = 0;
	while (pFilter->has_documents() == true)
	{
		Document doc(argv[1], string("file://") + argv[1], mimeType, "");
		string uri, ipath;
		unsigned int dataLength = 0;

		if (pFilter->next_document() == false)
		{
			break;
		}
		cout << "##### Document " << ++count << endl;

		const map<string, string> &metaData = pFilter->get_meta_data();
		for (map<string, string>::const_iterator metaIter = metaData.begin();
			metaIter != metaData.end(); ++metaIter)
		{
			if (metaIter->first == "content")
			{
				doc.setData(metaIter->second.c_str(), metaIter->second.length());
			}
			else if (metaIter->first == "ipath")
			{
				ipath = metaIter->second;
			}
			else if (metaIter->first == "language")
			{
				doc.setLanguage(metaIter->second);
			}
			else if (metaIter->first == "mimetype")
			{
				doc.setType(metaIter->second);
			}
			else if (metaIter->first == "size")
			{
				doc.setSize((off_t)atoi(metaIter->second.c_str()));
			}
			else if (metaIter->first == "title")
			{
				doc.setTitle(metaIter->second);
			}
			else if (metaIter->first == "uri")
			{
				uri = metaIter->second;
			}
		}

		if (uri.empty() == false)
		{
			doc.setLocation(uri);
		}
		if (ipath.empty() == false)
		{
			doc.setLocation(doc.getLocation() + "?" + ipath);
		}
		cout << "Location: " << doc.getLocation() << endl;

		Tokenizer tokens(&doc);

		if (strncmp(argv[2], "RAWDATA", 5) == 0)
		{
			// Call the base class's method
			const Document *pDoc = tokens.Tokenizer::getDocument();
			if (pDoc != NULL)
			{
				cout << "Raw text is: " << endl;
				cout << pDoc->getData(dataLength) << endl;
			}
		}
		else if (strncmp(argv[2], "LISTTOKENS", 10) == 0)
		{
			string token;

			cout << "Tokens are: " << endl;
			while (tokens.nextToken(token) == true)
			{
				cout << token << endl;
			}
		}
	}

	delete pFilter;

	return EXIT_SUCCESS;
}
