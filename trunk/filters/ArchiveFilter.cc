/*
 *  Copyright 2009 Fabrice Colin
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <archive_entry.h>
#include <iostream>
#include <sstream>

#include "ArchiveFilter.h"

using std::string;
using std::cout;
using std::endl;
using std::stringstream;
using namespace Dijon;

#ifdef _DYNAMIC_DIJON_FILTERS
DIJON_FILTER_EXPORT bool get_filter_types(std::set<std::string> &mime_types)
{
	mime_types.clear();
	mime_types.insert("application/x-tar");
	mime_types.insert("application/x-bcpio");
	mime_types.insert("application/x-bzip-compressed-tar");
	mime_types.insert("application/x-compressed-tar");
	mime_types.insert("application/x-cpio-compressed");
	mime_types.insert("application/x-cpio");
	mime_types.insert("application/x-sv4cpio");
	mime_types.insert("application/x-cd-image");
	mime_types.insert("application/x-iso9660-image");
	mime_types.insert("application/zip");

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
	return new ArchiveFilter(mime_type);
}
#endif

ArchiveFilter::ArchiveFilter(const string &mime_type) :
	Filter(mime_type),
	m_parseDocument(false),
	m_fd(-1),
	m_pHandle(NULL)
{
	m_pHandle = archive_read_new();
	if (m_pHandle != NULL)
	{
		// Go for the full monty
		archive_read_support_compression_all(m_pHandle);
		archive_read_support_format_all(m_pHandle);
	}
}

ArchiveFilter::~ArchiveFilter()
{
	rewind();
}

bool ArchiveFilter::is_data_input_ok(DataInput input) const
{
	if ((input == DOCUMENT_DATA) ||
		(input == DOCUMENT_STRING))
	{
		return true;
	}

	return false;
}

bool ArchiveFilter::set_property(Properties prop_name, const string &prop_value)
{
	return false;
}

bool ArchiveFilter::set_document_data(const char *data_ptr, unsigned int data_length)
{
	char *pMem = const_cast<char*>(data_ptr);

	if ((m_pHandle != NULL) &&
		(archive_read_open_memory(m_pHandle, static_cast<void*>(pMem), (size_t)data_length) == ARCHIVE_OK))
	{
		m_parseDocument = true;

		return true;
	}

	return false;
}

bool ArchiveFilter::set_document_string(const string &data_str)
{
	return set_document_data(data_str.c_str(), data_str.length());
}

bool ArchiveFilter::set_document_file(const string &file_path, bool unlink_when_done)
{
	if ((m_pHandle != NULL) &&
		(Filter::set_document_file(file_path, unlink_when_done) == true))
	{
#ifndef O_NOATIME
		int openFlags = O_RDONLY;
#else
		int openFlags = O_RDONLY|O_NOATIME;
#endif

		// Open the archive 
		m_fd = open(file_path.c_str(), openFlags);
#ifdef O_NOATIME
		if ((m_fd < 0) &&
			(errno == EPERM))
		{
			// Try again
			m_fd = open(file_path.c_str(), O_RDONLY);
		}
#endif
		if (m_fd < 0)
		{
#ifdef DEBUG
			cout << "ArchiveFilter::initialize: couldn't open " << file_path << endl;
#endif
			return false;
		}

		// FIXME: this segfaults in _archive_check_magic()
		if (archive_read_open_fd(m_pHandle, m_fd, ARCHIVE_DEFAULT_BYTES_PER_BLOCK) == ARCHIVE_OK)
		{
			m_parseDocument = true;

			return true;
		}

		close(m_fd);
		m_fd = -1;
	}

	return false;
}

bool ArchiveFilter::set_document_uri(const string &uri)
{
	return false;
}

bool ArchiveFilter::has_documents(void) const
{
	return m_parseDocument;
}

bool ArchiveFilter::next_document(void)
{
	return next_document("");
}

bool ArchiveFilter::next_document(const std::string &ipath)
{
	struct archive_entry *pEntry = NULL;
	const char *pFileName = NULL;
	bool foundFile = false;

	if (m_parseDocument == false)
	{
		return false;
	}

	do
	{
		if (archive_read_next_header(m_pHandle, &pEntry) != ARCHIVE_OK)
		{
#ifdef DEBUG
			cout << "ArchiveFilter::next_document: no more entries" << endl;
#endif
			m_parseDocument = false;
			return false;
		}

		pFileName = archive_entry_pathname(pEntry);
		if (pFileName == NULL)
		{
			return false;
		}

		if (ipath.empty() == true)
		{
			foundFile = true;
		}
		else if (ipath != pFileName)
		{
			if (archive_read_data_skip(m_pHandle) != ARCHIVE_OK)
			{
				m_parseDocument = false;
				return false;
			}
		}
		else
		{
			foundFile = true;
		}
	} while (foundFile == false);

	stringstream sizeStream;
	const struct stat *pEntryStats = archive_entry_stat(pEntry);
	if (pEntryStats == NULL)
	{
		return false;
	}
	off_t size = pEntryStats->st_size;

	m_content.clear();
	m_metaData.clear();
	m_metaData["title"] = pFileName;
	m_metaData["ipath"] = pFileName;
	sizeStream << size;
	m_metaData["size"] = sizeStream.str();
#ifdef DEBUG
	cout << "ArchiveFilter::next_document: found " << pFileName << ", size " << size << endl;
#endif

	if (S_ISDIR(pEntryStats->st_mode))
	{
		archive_entry_set_perm(pEntry, 0755);

		m_metaData["mimetype"] = "x-directory/normal";
	}
	else if (S_ISLNK(pEntryStats->st_mode))
	{
		m_metaData["mimetype"] = "inode/symlink";
	}
	else if (S_ISREG(pEntryStats->st_mode))
	{
		char pBuffer[ARCHIVE_DEFAULT_BYTES_PER_BLOCK];
		size_t blockNum = size;
		bool readFile = true;

		archive_entry_set_perm(pEntry, 0644);

		m_content.reserve(size);
		while (blockNum > 0)
		{
			int readSize = archive_read_data(m_pHandle, pBuffer, ARCHIVE_DEFAULT_BYTES_PER_BLOCK);
#ifdef DEBUG
			cout << "ArchiveFilter::next_document: read " << readSize << endl;
#endif
			if (readSize <= 0)
			{
				readFile = false;
				break;
			}

			m_content.append(pBuffer, readSize);
			blockNum -= readSize;
		}

		m_metaData["mimetype"] = "SCAN";

		return readFile;
	}

	return true;
}

bool ArchiveFilter::skip_to_document(const string &ipath)
{
	return next_document(ipath);
}

string ArchiveFilter::get_error(void) const
{
	return "";
}

void ArchiveFilter::rewind(void)
{
	Filter::rewind();

	if (m_pHandle != NULL)
	{
		archive_read_finish(m_pHandle);
		m_pHandle = NULL;
	}
	if (m_fd >= 0)
	{
		close(m_fd);
		m_fd = -1;
	}

	m_parseDocument = false;
}
