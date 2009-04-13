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
	mime_types.insert("application/x-bzip-compressed-tar");
	mime_types.insert("application/x-compressed-tar");
	mime_types.insert("application/x-cd-image");
	mime_types.insert("application/x-iso9660-image");
	mime_types.insert("application/x-tarz");

	return true;
}

DIJON_FILTER_EXPORT bool check_filter_data_input(int data_input)
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

DIJON_FILTER_EXPORT Filter *get_filter(const std::string &mime_type)
{
	return new ArchiveFilter(mime_type);
}
#endif

ArchiveFilter::ArchiveFilter(const string &mime_type) :
	Filter(mime_type),
	m_parseDocument(false),
	m_isBig(false),
	m_pMem(NULL),
	m_fd(-1),
	m_pHandle(NULL)
{
	if ((mime_type == "application/x-cd-image") ||
		(mime_type == "application/x-iso9660-image"))
	{
		m_isBig = true;
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
		return !m_isBig;
	} 
	else if (input == DOCUMENT_FILE_NAME)
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
	initialize();
	if ((m_pHandle == NULL) ||
		(m_isBig == true))
	{
		return false;
	}

	// archive_read_open_memory() expects a non-const pointer
	// so we'd better make a copy
	m_pMem = (char *)malloc(sizeof(char) * (data_length + 1));
	if (m_pMem == NULL)
	{
		return false;
	}

	void *pVoidMem = static_cast<void*>(m_pMem);
	memcpy(pVoidMem, static_cast<const void*>(data_ptr), data_length);
	m_pMem[data_length] = '\0';

	if (archive_read_open_memory(m_pHandle, pVoidMem, (size_t)data_length) == ARCHIVE_OK)
	{
		m_parseDocument = true;
#ifdef DEBUG
		cout << "ArchiveFilter::set_document_data: " << m_mimeType 
			<< ", format " << archive_format(m_pHandle) << endl;
#endif

		return true;
	}

	free(m_pMem);
	m_pMem = NULL;

	return false;
}

bool ArchiveFilter::set_document_string(const string &data_str)
{
	return set_document_data(data_str.c_str(), data_str.length());
}

bool ArchiveFilter::set_document_file(const string &file_path, bool unlink_when_done)
{
	if (Filter::set_document_file(file_path, unlink_when_done) == true)
	{
#ifndef O_NOATIME
		int openFlags = O_RDONLY;
#else
		int openFlags = O_RDONLY|O_NOATIME;
#endif

		initialize();
		if (m_pHandle == NULL)
		{
			return false;
		}

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
			cout << "ArchiveFilter::set_document_file: couldn't open " << file_path << endl;
#endif
			return false;
		}

		if (archive_read_open_fd(m_pHandle, m_fd, ARCHIVE_DEFAULT_BYTES_PER_BLOCK) == ARCHIVE_OK)
		{
			m_parseDocument = true;
#ifdef DEBUG
			cout << "ArchiveFilter::set_document_file: " << file_path
				<< ", " << m_mimeType << ", format " << archive_format(m_pHandle) << endl;
#endif

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

void ArchiveFilter::initialize(void)
{
	m_pHandle = archive_read_new();
	if (m_pHandle != NULL)
	{
		// Enable what we need for the given type
		if (m_mimeType == "application/x-tar")
		{
			archive_read_support_format_tar(m_pHandle);
			archive_read_support_format_gnutar(m_pHandle);
		}
		else if (m_mimeType == "application/x-bzip-compressed-tar")
		{
			archive_read_support_compression_bzip2(m_pHandle);
			archive_read_support_format_tar(m_pHandle);
			archive_read_support_format_gnutar(m_pHandle);
		}
		else if (m_mimeType == "application/x-compressed-tar")
		{
			archive_read_support_compression_gzip(m_pHandle);
			archive_read_support_format_tar(m_pHandle);
			archive_read_support_format_gnutar(m_pHandle);
		}
		else if ((m_mimeType == "application/x-cd-image") ||
			(m_mimeType == "application/x-iso9660-image"))
		{
			archive_read_support_format_iso9660(m_pHandle);
		}
		else if (m_mimeType == "application/x-tarz")
		{
			archive_read_support_compression_compress(m_pHandle);
			archive_read_support_format_tar(m_pHandle);
			archive_read_support_format_gnutar(m_pHandle);
		}
	}
}

bool ArchiveFilter::next_document(const std::string &ipath)
{
	struct archive_entry *pEntry = NULL;
	const char *pFileName = NULL;
	bool foundFile = false;

	if ((m_parseDocument == false) ||
		(m_pHandle == NULL))
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
	m_metaData["ipath"] = string("f=") + pFileName;
	sizeStream << size;
	m_metaData["size"] = sizeStream.str();
#ifdef DEBUG
	cout << "ArchiveFilter::next_document: found " << pFileName << ", size " << size << " bytes" << endl;
#endif

	if (S_ISDIR(pEntryStats->st_mode))
	{
		m_metaData["mimetype"] = "x-directory/normal";
	}
	else if (S_ISLNK(pEntryStats->st_mode))
	{
		m_metaData["mimetype"] = "inode/symlink";
	}
	else if (S_ISREG(pEntryStats->st_mode))
	{
		m_content.reserve(size);

#if 1
		const void *pBuffer = NULL;
		size_t readSize = 0, totalSize = 0;
		off_t offset = 0;

		while (archive_read_data_block(m_pHandle,
			&pBuffer, &readSize, &offset) == ARCHIVE_OK)
		{
			totalSize += readSize;
#ifdef DEBUG
			cout << "ArchiveFilter::next_document: read " << readSize << " bytes" << endl;
#endif
			m_content.append(static_cast<const char*>(pBuffer), readSize);
		}
#ifdef DEBUG
		cout << "ArchiveFilter::next_document: read " << totalSize
			<< "/" << m_content.size() << " bytes" << endl;
#endif
#else
		char *pBuffer = (char *)malloc(sizeof(char) * (size + 1));

		if (pBuffer != NULL)
		{
			if (archive_read_data_into_buffer(m_pHandle, (void*)pBuffer, size) == ARCHIVE_OK)
			{
				m_content.append(pBuffer, size);
			}
#ifdef DEBUG
			else
			{
				cout << "ArchiveFilter::next_document: failed to read " << size << " bytes" << endl;
			}
#endif

			free(pBuffer);
		}
#endif

		m_metaData["mimetype"] = "SCAN";

		return true;
	}

	return true;
}

bool ArchiveFilter::skip_to_document(const string &ipath)
{
	string::size_type fPos = ipath.find("f=");

	if (fPos != 0)
	{
		return false;
	}

	return next_document(ipath.substr(2));
}

string ArchiveFilter::get_error(void) const
{
	return "";
}

void ArchiveFilter::rewind(void)
{
	Filter::rewind();

	m_parseDocument = m_isBig = false;
	if (m_pHandle != NULL)
	{
		archive_read_close(m_pHandle);
		archive_read_finish(m_pHandle);
		m_pHandle = NULL;
	}
	if (m_pMem != NULL)
	{
		free(m_pMem);
		m_pMem = NULL;
	}
	if (m_fd >= 0)
	{
		close(m_fd);
		m_fd = -1;
	}
}
