/*
 *  Copyright 2008 Fabrice Colin
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
#include <stdarg.h>
#include <iostream>
#include <libexif/exif-data.h>
#include <libexif/exif-ifd.h>
#include <libexif/exif-loader.h>
#include <libexif/exif-utils.h>

#include "ExifImageFilter.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;
using namespace Dijon;

#ifdef _DYNAMIC_DIJON_FILTERS
bool get_filter_types(std::set<std::string> &mime_types)
{
	mime_types.clear();
	mime_types.insert("image/jpeg");

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
	return new ExifImageFilter(mime_type);
}
#endif

ExifImageFilter::ExifImageFilter(const string &mime_type) :
	Filter(mime_type),
	m_parseDocument(false)
{
}

ExifImageFilter::~ExifImageFilter()
{
	rewind();
}

bool ExifImageFilter::is_data_input_ok(DataInput input) const
{
	if (input == DOCUMENT_FILE_NAME)
	{
		return true;
	}

	return false;
}

bool ExifImageFilter::set_property(Properties prop_name, const string &prop_value)
{
	return false;
}

bool ExifImageFilter::set_document_data(const char *data_ptr, unsigned int data_length)
{
	return false;
}

bool ExifImageFilter::set_document_string(const string &data_str)
{
	return false;
}

bool ExifImageFilter::set_document_file(const string &file_path, bool unlink_when_done)
{
	if (Filter::set_document_file(file_path, unlink_when_done) == true)
	{
		m_parseDocument = true;

		return true;
	}

	return false;
}

bool ExifImageFilter::set_document_uri(const string &uri)
{
	return false;
}

bool ExifImageFilter::has_documents(void) const
{
	return m_parseDocument;
}

bool ExifImageFilter::next_document(void)
{
	if (m_parseDocument == true)
	{
		ExifData *pData = exif_data_new_from_file(m_filePath.c_str());

#ifdef DEBUG
		cout << "ExifImageFilter::next_document: " << m_filePath << endl;
#endif
		m_parseDocument = false;

		m_metaData["mimetype"] = "text/plain";
		m_metaData["charset"] = "utf-8";

		if (pData == NULL)
		{
			cerr << "No EXIF data in " << m_filePath.c_str() << endl;
		}
		else
		{
			ExifEntry *pEntry = NULL;
			string pseudoContent;
			char value[1024];

			// Content
			pEntry = exif_data_get_entry(pData, EXIF_TAG_USER_COMMENT);
			if (pEntry != NULL)
			{
				exif_entry_get_value(pEntry, value, 1024);
				pseudoContent += value;
			}
			pEntry = exif_data_get_entry(pData, EXIF_TAG_IMAGE_DESCRIPTION);
			if (pEntry != NULL)
			{
				exif_entry_get_value(pEntry, value, 1024);
				if (pseudoContent.empty() == false)
				{
					pseudoContent += "\n";
				}
				pseudoContent += value;
			}
			m_metaData["content"] = pseudoContent;

			// Title
			pEntry = exif_data_get_entry(pData, EXIF_TAG_DOCUMENT_NAME);
			if (pEntry != NULL)
			{
				exif_entry_get_value(pEntry, value, 1024);
				m_metaData["title"] = value;
			}

			// Author
			pEntry = exif_data_get_entry(pData, EXIF_TAG_ARTIST);
			if (pEntry != NULL)
			{
				exif_entry_get_value(pEntry, value, 1024);
				m_metaData["author"] = value;
			}
		}

		exif_data_unref(pData);

		return true;
	}

	return false;
}

bool ExifImageFilter::skip_to_document(const string &ipath)
{
	if (ipath.empty() == true)
	{
		return next_document();
	}

	return false;
}

string ExifImageFilter::get_error(void) const
{
	return "";
}

void ExifImageFilter::rewind(void)
{
	Filter::rewind();

	m_parseDocument = false;
}
