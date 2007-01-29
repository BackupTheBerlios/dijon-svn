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

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <libxml/xmlreader.h>

#include "StringManip.h"
#include "Url.h"
#include "ExternalFilter.h"

using std::string;
using std::set;
using std::map;
using std::cout;
using std::endl;

using namespace Dijon;

#ifdef _DYNAMIC_DIJON_FILTERS
bool get_filter_types(std::set<std::string> &mime_types)
{
#ifdef _DIJON_EXTERNALFILTER_CONFFILE
	ExternalFilter::initialize(_DIJON_EXTERNALFILTER_CONFFILE, mime_types);
#else
	ExternalFilter::initialize("/etc/dijon/external-filters.xml", mime_types);
#endif

	return true;
}

bool check_filter_data_input(int data_input)
{
	Filter::DataInput input = (Filter::DataInput)data_input;

	if (input == Filter::DOCUMENT_FILE_NAME)
	{
		return true;
	}

	return false;
}

Filter *get_filter(const std::string &mime_type)
{
	return new ExternalFilter(mime_type);
}
#endif

map<string, string> ExternalFilter::m_commandsByType;
map<string, string> ExternalFilter::m_outputsByType;

ExternalFilter::ExternalFilter(const string &mime_type) :
	Filter(mime_type),
	m_doneWithDocument(false)
{
}

ExternalFilter::~ExternalFilter()
{
	rewind();
}

bool ExternalFilter::is_data_input_ok(DataInput input) const
{
	if (input == DOCUMENT_FILE_NAME)
	{
		return true;
	}

	return false;
}

bool ExternalFilter::set_property(Properties prop_name, const string &prop_value)
{
	return true;
}

bool ExternalFilter::set_document_data(const char *data_ptr, unsigned int data_length)
{
	return false;
}

bool ExternalFilter::set_document_string(const string &data_str)
{
	return false;
}

bool ExternalFilter::set_document_file(const string &file_path)
{
	if (file_path.empty() == true)
	{
		return false;
	}

	rewind();
	m_filePath = file_path;

	return true;
}

bool ExternalFilter::set_document_uri(const string &uri)
{
	return false;
}

bool ExternalFilter::has_documents(void) const
{
	if ((m_doneWithDocument == false) &&
		(m_filePath.empty() == false))
	{
		return true;
	}

	return false;
}

bool ExternalFilter::next_document(void)
{
	if ((m_doneWithDocument == false) &&
		(m_mimeType.empty() == false) &&
		(m_filePath.empty() == false) &&
		(m_commandsByType.empty() == false))
	{
		bool gotOutput = false;

		m_doneWithDocument = true;

		// Is this type supported ?
		map<string, string>::const_iterator commandIter = m_commandsByType.find(m_mimeType);
		if ((commandIter == m_commandsByType.end()) ||
			(commandIter->second.empty() == true))
		{
			return false;
		}

		string commandLine(commandIter->second);
		string::size_type argPos = commandLine.find("%s");
		char outTemplate[18] = "/tmp/filterXXXXXX";

		// Create a temporary file for the program's output
		int outFd = mkstemp(outTemplate);
		if (outFd == -1)
		{
			return false;
		}

		if (argPos == string::npos)
		{
			commandLine += " \'";
			commandLine += escapeQuotes(m_filePath);
			commandLine += "\'";
		}
		else
		{
			string quotedFilePath("\'");

			quotedFilePath += escapeQuotes(m_filePath);
			quotedFilePath += "\'";
			commandLine.replace(argPos, 2, quotedFilePath);
		}
		commandLine += ">";
		commandLine += outTemplate;
#ifdef DEBUG
		cout << "ExternalFilter::next_document: running " << commandLine << endl;
#endif

		// Run the command
		if (system(commandLine.c_str()) != -1)
		{
			struct stat outStats;

			if (fstat(outFd, &outStats) == 0)
			{
				// Grab the output
				char *fileBuffer = new char[outStats.st_size + 1];
				if (fileBuffer != NULL)
				{
					int bytesRead = read(outFd, (void*)fileBuffer, outStats.st_size);
					if (bytesRead > 0)
					{
						char numStr[64];

						fileBuffer[bytesRead] = '\0';

						m_metaData["uri"] = "file://" + m_filePath;
						// What's the output type ?
						map<string, string>::const_iterator outputIter = m_outputsByType.find(m_mimeType);
						if (outputIter == m_outputsByType.end())
						{
							// Assume it's plain text if undefined
							m_metaData["mimetype"] = "text/plain";
						}
						else
						{
							m_metaData["mimetype"] = outputIter->second;
						}
						m_metaData["content"] = string(fileBuffer, (unsigned int)bytesRead);
						snprintf(numStr, 64, "%u", outStats.st_size);
						m_metaData["size"] = numStr;
						gotOutput = true;
					}
#ifdef DEBUG
					else cout << "ExternalFilter::next_document: couldn't read output" << endl;
#endif

					delete[] fileBuffer;
				}
#ifdef DEBUG
				else cout << "ExternalFilter::next_document: couldn't allocate output" << endl;
#endif
			}
#ifdef DEBUG
			else cout << "ExternalFilter::next_document: couldn't stat output" << endl;
#endif
		}
#ifdef DEBUG
		else cout << "ExternalFilter::next_document: couldn't run command line" << endl;
#endif

		// Close and delete the temporary file
		close(outFd);
		unlink(outTemplate);

		return gotOutput;
	}

	rewind();

	return false;
}

bool ExternalFilter::skip_to_document(const string &ipath)
{
	if (ipath.empty() == true)
	{
		return next_document();
	}

	return false;
}

string ExternalFilter::get_error(void) const
{
	return "";
}

void ExternalFilter::initialize(const std::string &config_file, set<std::string> &types)
{
	xmlDoc *pDoc = NULL;
	xmlNode *pRootElement = NULL;

	types.clear();

	// Initialize the library and check for potential ABI mismatches
	LIBXML_TEST_VERSION

	// Parse the file and get the document
#if LIBXML_VERSION < 20600
	pDoc = xmlParseFile(config_file.c_str());
#else
	pDoc = xmlReadFile(config_file.c_str(), NULL, 0);
#endif
	if (pDoc == NULL)
	{
		return;
	}

	// Iterate through the root element's nodes
	pRootElement = xmlDocGetRootElement(pDoc);
	for (xmlNode *pCurrentNode = pRootElement->children; pCurrentNode != NULL;
		pCurrentNode = pCurrentNode->next)
	{
		// What type of tag is it ?
		if (pCurrentNode->type != XML_ELEMENT_NODE)
		{
			continue;
		}

		// Get all filter elements
		if (xmlStrncmp(pCurrentNode->name, BAD_CAST"filter", 6) == 0)
		{
			string mimeType, command, arguments, output;

			for (xmlNode *pCurrentCodecNode = pCurrentNode->children;
				pCurrentCodecNode != NULL; pCurrentCodecNode = pCurrentCodecNode->next)
			{
				if (pCurrentCodecNode->type != XML_ELEMENT_NODE)
				{
					continue;
				}

				char *pChildContent = (char*)xmlNodeGetContent(pCurrentCodecNode);
				if (pChildContent == NULL)
				{
					continue;
				}

				// Filters are keyed by their MIME type, "extension" is ignored
				if (xmlStrncmp(pCurrentCodecNode->name, BAD_CAST"mimetype", 8) == 0)
				{
					mimeType = pChildContent;
				}
				else if (xmlStrncmp(pCurrentCodecNode->name, BAD_CAST"command", 7) == 0)
				{
					command = pChildContent;
				}
				if (xmlStrncmp(pCurrentCodecNode->name, BAD_CAST"arguments", 9) == 0)
				{
					arguments = pChildContent;
				}
				else if (xmlStrncmp(pCurrentCodecNode->name, BAD_CAST"output", 6) == 0)
				{
					output = pChildContent;
				}

				// Free
				xmlFree(pChildContent);
			}

			if ((mimeType.empty() == false) &&
				(command.empty() == false) &&
				(arguments.empty() == false))
			{
#ifdef DEBUG
				cout << "ExternalFilter::initialize: " << mimeType << "="
					<< command << " " << arguments << endl;
#endif

				// Command to run
				m_commandsByType[mimeType] = command + " " + arguments;
				// Output
				if (output.empty() == false)
				{
					m_outputsByType[mimeType] = output;
				}

				types.insert(mimeType);
			}
		}
	}

	// Free the document
	xmlFreeDoc(pDoc);

	// Cleanup
	xmlCleanupParser();
}

void ExternalFilter::rewind(void)
{
	m_metaData.clear();
	m_doneWithDocument = false;
}

string ExternalFilter::escapeQuotes(const string &file_name)
{
	string quoted(file_name);

	string::size_type pos = quoted.find('\'');
	while (pos != string::npos)
	{
		quoted.replace(pos, 1, "\\\'");
		pos = quoted.find('\'', pos + 2);
	}

	return quoted;
}

