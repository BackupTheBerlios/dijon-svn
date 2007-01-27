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

#include <unistd.h>
#include <fileref.h>
#include <tfile.h>
#include <tag.h>

#include "TagLibFilter.h"

using std::string;
using namespace Dijon;

#ifdef _DYNAMIC_DIJON_FILTERS
bool get_filter_types(std::set<std::string> &mime_types)
{
	mime_types.clear();
	mime_types.insert("audio/mpeg");
	mime_types.insert("audio/x-mp3");
	mime_types.insert("application/ogg");
	mime_types.insert("audio/x-flac+ogg");
	mime_types.insert("audio/x-flac");

	return true;
}

bool check_filter_data_input(int data_input)
{
	Filter::DataInput input = (Filter::DataInput)data_input;

	if ((input == Filter::DOCUMENT_DATA) ||
		(input == Filter::DOCUMENT_STRING) ||
		(input == Filter::DOCUMENT_FILE_NAME))
	{
		return true;
	}

	return false;
}

Filter *get_filter(const std::string &mime_type)
{
	return new TagLibFilter(mime_type);
}
#endif

TagLibFilter::TagLibFilter(const string &mime_type) :
	Filter(mime_type),
	m_deleteFile(false),
	m_parseDocument(false)
{
}

TagLibFilter::~TagLibFilter()
{
	rewind();
}

bool TagLibFilter::is_data_input_ok(DataInput input) const
{
	if ((input == DOCUMENT_DATA) ||
		(input == DOCUMENT_STRING) ||
		(input == DOCUMENT_FILE_NAME))
	{
		return true;
	}

	return false;
}

bool TagLibFilter::set_property(Properties prop_name, const string &prop_value)
{
	return false;
}

bool TagLibFilter::set_document_data(const char *data_ptr, unsigned int data_length)
{
	char inTemplate[18] = "/tmp/filterXXXXXX";

	if ((data_ptr == NULL) ||
		(data_length == 0))
	{
		return false;
	}

	rewind();

	int inFd = mkstemp(inTemplate);
	if (inFd != -1)
	{
		// Save the data into a temporary file
		if (write(inFd, (const void*)data_ptr, data_length) != -1)
		{
			m_filePath = inTemplate;
			m_deleteFile = true;
			m_parseDocument = true;
		}

		close(inFd);
	}

	return true;
}

bool TagLibFilter::set_document_string(const string &data_str)
{
	if (data_str.empty() == true)
	{
		return false;
	}

	return set_document_data(data_str.c_str(), data_str.length());
}

bool TagLibFilter::set_document_file(const string &file_path)
{
	if (file_path.empty() == true)
	{
		return false;
	}

	rewind();

	m_filePath = file_path;
	m_deleteFile = false;
	m_parseDocument = true;

	return true;
}

bool TagLibFilter::set_document_uri(const string &uri)
{
	return false;
}

bool TagLibFilter::has_documents(void) const
{
	return m_parseDocument;
}

bool TagLibFilter::next_document(void)
{
	if (m_parseDocument == true)
	{
		m_parseDocument = false;

		TagLib::FileRef fileRef(m_filePath.c_str(), false);
		if (fileRef.isNull() == false)
		{
			TagLib::Tag *pTag = fileRef.tag();
			if ((pTag != NULL) &&
				(pTag->isEmpty() == false))
			{
				char yearStr[64];

				string trackTitle(pTag->title().toCString(true));
				trackTitle += " ";
				trackTitle += pTag->artist().toCString(true);

				string pseudoContent(trackTitle);
				pseudoContent += " ";
				pseudoContent += pTag->album().toCString(true);
				pseudoContent += " ";
				pseudoContent += pTag->comment().toCString(true);
				pseudoContent += " ";
				pseudoContent += pTag->genre().toCString(true);
				snprintf(yearStr, 64, " %u", pTag->year());
				pseudoContent += yearStr;

				m_metaData["content"] = pseudoContent;
				m_metaData["title"] = trackTitle;
				m_metaData["ipath"] = "";
				m_metaData["mimetype"] = "text/plain";
				m_metaData["charset"] = "utf-8";
				m_metaData["author"] = pTag->artist().toCString(true);

				return true;
			}
		}
	}

	return false;
}

bool TagLibFilter::skip_to_document(const string &ipath)
{
	if (ipath.empty() == true)
	{
		return next_document();
	}

	return false;
}

string TagLibFilter::get_error(void) const
{
	return "";
}

void TagLibFilter::rewind(void)
{
	if ((m_deleteFile == true) &&
		(m_filePath.empty() == false))
	{
		unlink(m_filePath.c_str());
	}

	m_metaData.clear();
	m_filePath.clear();
	m_deleteFile = false;
	m_parseDocument = false;
}
