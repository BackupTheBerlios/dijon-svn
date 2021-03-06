/*
 *  Copyright 2007-2008 Fabrice Colin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <unistd.h>

#include "XmlFilter.h"

using std::string;

using namespace Dijon;

#ifdef _DYNAMIC_DIJON_XMLFILTER
DIJON_FILTER_EXPORT bool get_filter_types(std::set<std::string> &mime_types)
{
	mime_types.clear();
	mime_types.insert("text/xml");
	mime_types.insert("application/xml");

	return true;
}

DIJON_FILTER_EXPORT bool check_filter_data_input(int data_input)
{
	Filter::DataInput input = (Filter::DataInput)data_input;

	if ((input == Filter::DOCUMENT_DATA) ||
		(input == Filter::DOCUMENT_STRING))
	{
		return true;
	}

	return false;
}

DIJON_FILTER_EXPORT Filter *get_filter(const std::string &mime_type)
{
	return new XmlFilter(mime_type);
}
#endif

static string replaceEntities(const string &str)
{
	// FIXME: replace all
	static const char *escapedChars[] = { "quot", "amp", "lt", "gt", "nbsp", "eacute", "egrave", "agrave", "ccedil"};
	static const char *unescapedChars[] = { "\"", "&", "<", ">", " ", "e", "e", "a", "c"};
	static const unsigned int escapedCharsCount = 9;
	string unescapedStr;
	string::size_type startPos = 0;

	string::size_type pos = str.find("&");
	while (pos != string::npos)
	{
		unescapedStr += str.substr(startPos, pos - startPos);

		startPos = pos + 1;
		pos = str.find(";", startPos);
		if ((pos != string::npos) &&
			(pos < startPos + 10))
		{
			string escapedChar(str.substr(startPos, pos - startPos));
			bool replacedChar = false;

			// See if we can replace this with an actual character
			for (unsigned int count = 0; count < escapedCharsCount; ++count)
			{
				if (escapedChar == escapedChars[count])
				{
					unescapedStr += unescapedChars[count];
					replacedChar = true;
					break;
				}
			}

			if (replacedChar == false)
			{
				// This couldn't be replaced, leave it as it is...
				unescapedStr += "&";
				unescapedStr += escapedChar;
				unescapedStr += ";";
			}

			startPos = pos + 1;
		}

		// Next
		pos = str.find("&", startPos);
	}
	if (startPos < str.length())
	{
		unescapedStr  += str.substr(startPos);
	}

	return unescapedStr;
}

XmlFilter::XmlFilter(const string &mime_type) :
	Filter(mime_type),
	m_doneWithDocument(false)
{
}

XmlFilter::~XmlFilter()
{
	rewind();
}

bool XmlFilter::is_data_input_ok(DataInput input) const
{
	if ((input == DOCUMENT_DATA) ||
		(input == DOCUMENT_STRING))
	{
		return true;
	}

	return false;
}

bool XmlFilter::set_property(Properties prop_name, const string &prop_value)
{
	return true;
}

bool XmlFilter::set_document_data(const char *data_ptr, unsigned int data_length)
{
	if ((data_ptr == NULL) ||
		(data_length == 0))
	{
		return false;
	}

	string xml_doc(data_ptr, data_length);
	return set_document_string(xml_doc);
}

bool XmlFilter::set_document_string(const string &data_str)
{
	if (data_str.empty() == true)
	{
		return false;
	}

	rewind();

	if (parse_xml(data_str) == true)
	{
		m_doneWithDocument = false;
		return true;
	}

	return false;
}

bool XmlFilter::set_document_file(const string &file_path, bool unlink_when_done)
{
	return false;
}

bool XmlFilter::set_document_uri(const string &uri)
{
	return false;
}

bool XmlFilter::has_documents(void) const
{
	if (m_doneWithDocument == false)
	{
		return true;
	}

	return false;
}

bool XmlFilter::next_document(void)
{
	if (m_doneWithDocument == false)
	{
		m_doneWithDocument = true;

		return true;
	}

	rewind();

	return false;
}

bool XmlFilter::skip_to_document(const string &ipath)
{
	if (ipath.empty() == true)
	{
		return next_document();
	}

	return false;
}

string XmlFilter::get_error(void) const
{
	return "";
}

void XmlFilter::rewind(void)
{
	Filter::rewind();

	m_doneWithDocument = false;
}

bool XmlFilter::parse_xml(const string &xml_doc)
{
	if (xml_doc.empty() == true)
	{
		return false;
	}

	string stripped(replaceEntities(xml_doc));

	// Tag start
	string::size_type startPos = stripped.find("<");
	while (startPos != string::npos)
	{
		string::size_type endPos = stripped.find(">", startPos);
		if (endPos == string::npos)
		{
			break;
		}

		stripped.replace(startPos, endPos - startPos + 1, " ");

		// Next
		startPos = stripped.find("<");
	}

	// The input may contain partial tags, eg "a>...</a><b>...</b>...<c"
	string::size_type pos = stripped.find(">");
	if (pos != string::npos)
	{
		stripped.erase(0, pos + 1);
	}
	pos = stripped.find("<");
	if (pos != string::npos)
	{
		stripped.erase(pos);
	}

	m_metaData["content"] = stripped;
	m_metaData["ipath"] = "";
	m_metaData["mimetype"] = "text/plain";

	return true;
}
