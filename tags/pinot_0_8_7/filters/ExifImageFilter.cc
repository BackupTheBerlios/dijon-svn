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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <unistd.h>
#include <stdarg.h>
#include <time.h>
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

struct ExifMetaData
{
	string m_title;
	string m_date;
	string m_content;
};

static void entryCallback(ExifEntry *pEntry, void *pData)
{
	if ((pEntry == NULL) ||
		(pData == NULL))
	{
		return;
	}

	ExifMetaData *pMetaData = (ExifMetaData *)pData;
	struct tm timeTm;
	char value[1024];

	// Initialize the structure
	timeTm.tm_sec = timeTm.tm_min = timeTm.tm_hour = timeTm.tm_mday = 0;
	timeTm.tm_mon = timeTm.tm_year = timeTm.tm_wday = timeTm.tm_yday = timeTm.tm_isdst = 0;

	exif_entry_get_value(pEntry, value, 1024);
	switch (pEntry->tag)
	{
		case EXIF_TAG_DOCUMENT_NAME:
			pMetaData->m_title = value;
			break;
		case EXIF_TAG_DATE_TIME:
			if (strptime(value, "%Y:%m:%d %H:%M:%S", &timeTm) != NULL)
			{
				char timeStr[64];

				if (strftime(timeStr, 64, "%a, %d %b %Y %H:%M:%S %z", &timeTm) > 0)
				{
					pMetaData->m_date = timeStr;
				}
			}
			break;
		default:
			pMetaData->m_content += " ";
			pMetaData->m_content += value;
			break;
	}
#ifdef DEBUG
	cout << "ExifImageFilter: tag " << exif_tag_get_name(pEntry->tag) << ": " << value << endl;
#endif
}

static void contentCallback(ExifContent *pContent, void *pData)
{
	exif_content_foreach_entry(pContent, entryCallback, pData);
}

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
#ifdef DEBUG
		cout << "ExifImageFilter::next_document: " << m_filePath << endl;
#endif
		m_parseDocument = false;

		m_metaData["mimetype"] = "text/plain";
		m_metaData["charset"] = "utf-8";

		ExifData *pData = exif_data_new_from_file(m_filePath.c_str());
		if (pData == NULL)
		{
			cerr << "No EXIF data in " << m_filePath.c_str() << endl;
		}
		else
		{
			ExifMetaData *pMetaData = new ExifMetaData();

			// Get it all
			exif_data_foreach_content(pData, contentCallback, pMetaData);

			m_metaData["title"] = pMetaData->m_title;
			if (pMetaData->m_date.empty() == false)
			{
				m_metaData["date"] = pMetaData->m_date;
			}
			m_metaData["content"] = pMetaData->m_content;

			delete pMetaData;
			exif_data_unref(pData);
		}

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
