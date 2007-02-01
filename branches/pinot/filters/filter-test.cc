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

#include "config.h"
#include "MIMEScanner.h"
#include "Tokenizer.h"
#include "FilterFactory.h"

using namespace std;
using namespace Dijon;

static bool filterDocument(Document &doc, const char *pOption, unsigned int count)
{
	string mimeType(doc.getType());
	Url urlObj(doc.getLocation());
	string fileName;
	unsigned int dataLength = 0;
	const char *pData = doc.getData(dataLength);
	bool fedInput = false;

	Filter *pFilter = FilterFactory::getFilter(mimeType);
	if (pFilter == NULL)
	{
		cerr << "Couldn't obtain filter for " << doc.getLocation() << endl;
		return false;
	}

	if (urlObj.getProtocol() == "file")
	{
		fileName = doc.getLocation().substr(7);
	}

	// Prefer feeding the data
	if (((dataLength > 0) && (pData != NULL)) &&
		(pFilter->is_data_input_ok(Filter::DOCUMENT_DATA) == true))
	{
		cout << "Feeding data" << endl;

		fedInput = pFilter->set_document_data(pData, dataLength);
	}
	// ... to feeding the file
	if ((fedInput == false) &&
		(fileName.empty() == false))
	{ 
		if (pFilter->is_data_input_ok(Filter::DOCUMENT_FILE_NAME) == true)
		{
			cout << "Feeding file " << fileName << endl;

			fedInput = pFilter->set_document_file(fileName);
		}
		// ...and to feeding the file's contents
		if ((fedInput == false) &&
			(pFilter->is_data_input_ok(Filter::DOCUMENT_DATA) == true))
		{
			if (doc.setDataFromFile(fileName) == false)
			{
				cerr << "Couldn't load " << fileName << endl;
				delete pFilter;
				return false;
			}
			cout << "Feeding contents of file " << fileName << endl;

			pData = doc.getData(dataLength);
			if ((dataLength > 0) &&
				(pData != NULL))
			{
				fedInput = pFilter->set_document_data(pData, dataLength);
			}
		}
	}

	if (fedInput == false)
	{
		cerr << "Couldn't feed filter for " << doc.getLocation() << endl;
		delete pFilter;
		return false;
	}

	while (pFilter->has_documents() == true)
	{
		Document filteredDoc(doc.getTitle(), doc.getLocation(), "text/plain", doc.getLanguage());
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
				filteredDoc.setData(metaIter->second.c_str(), metaIter->second.length());

				cout << "Set " << metaIter->second.length() << " characters of data" << endl;
			}
			else if (metaIter->first == "ipath")
			{
				ipath = metaIter->second;
			}
			else if (metaIter->first == "language")
			{
				filteredDoc.setLanguage(metaIter->second);
			}
			else if (metaIter->first == "mimetype")
			{
				filteredDoc.setType(metaIter->second);
			}
			else if (metaIter->first == "size")
			{
				filteredDoc.setSize((off_t)atoi(metaIter->second.c_str()));
			}
			else if (metaIter->first == "title")
			{
				filteredDoc.setTitle(metaIter->second);
			}
			else if (metaIter->first == "uri")
			{
				uri = metaIter->second;
			}
		}

		if (uri.empty() == false)
		{
			filteredDoc.setLocation(uri);
		}
		if (ipath.empty() == false)
		{
			filteredDoc.setLocation(filteredDoc.getLocation() + "?" + ipath);
		}
		cout << "Location: " << filteredDoc.getLocation() << endl;
		cout << "MIME type: " << filteredDoc.getType() << endl;

		// Pass it down to another filter ?
		if (filteredDoc.getType() != "text/plain")
		{
			cout << "Passing down to another filter" << endl;

			filterDocument(filteredDoc, pOption, count);
		}
		else
		{
			Tokenizer tokens(&filteredDoc);

			if (strncmp(pOption, "RAWDATA", 5) == 0)
			{
				// Call the base class's method
				const Document *pDoc = tokens.Tokenizer::getDocument();
				if (pDoc != NULL)
				{
					cout << "Raw text is: " << endl;
					cout << pDoc->getData(dataLength) << endl;
				}
			}
			else if (strncmp(pOption, "LISTTOKENS", 10) == 0)
			{
				string token;

				cout << "Tokens are: " << endl;
				while (tokens.nextToken(token) == true)
				{
					cout << token << endl;
				}
			}
		}
	}

	delete pFilter;

	return true;
}

int main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;

	if (argc < 3)
	{
		cerr << "Usage: " << argv[0] << " <file name> RAWDATA|LISTTOKENS [TYPE=<type>]" << endl;
		return EXIT_FAILURE;
	}

	FilterFactory::loadFilters(string(LIBDIR) + "/pinot/filters");
	string mimeType;
	unsigned int count = 0;

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

	Document baseDoc(argv[1], string("file://") + argv[1], mimeType, "");

	if (filterDocument(baseDoc, argv[2], count) == false)
	{
		status = EXIT_FAILURE;
	}
	FilterFactory::unloadFilters();

	return status;
}
