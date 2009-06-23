/*
 *  Copyright 2007-2009 Fabrice Colin
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>
#include <utility>
#include <algorithm>

#include <gmime/gmime.h>

#include "GMimeMboxFilter.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::map;
using std::max;
using namespace Dijon;

#ifdef _DYNAMIC_DIJON_FILTERS
DIJON_FILTER_EXPORT bool get_filter_types(std::set<std::string> &mime_types)
{
	mime_types.clear();
	mime_types.insert("application/mbox");
	mime_types.insert("text/x-mail");
	mime_types.insert("text/x-news");

	return true;
}

DIJON_FILTER_EXPORT bool check_filter_data_input(int data_input)
{
	Filter::DataInput input = (Filter::DataInput)data_input;

	if ((input == Filter::DOCUMENT_DATA) ||
		(input == Filter::DOCUMENT_FILE_NAME))
	{
		return true;
	}

	return false;
}

DIJON_FILTER_EXPORT Filter *get_filter(const std::string &mime_type)
{
	return new GMimeMboxFilter(mime_type);
}

DIJON_FILTER_INITIALIZE void initialize_gmime(void)
{
	// Initialize gmime
#ifdef GMIME_ENABLE_RFC2047_WORKAROUNDS
	g_mime_init(GMIME_ENABLE_RFC2047_WORKAROUNDS);
#else
	g_mime_init(GMIME_INIT_FLAG_UTF8);
#endif
#ifdef DEBUG
	cout << "GMimeMboxFilter: initialized" << endl;
#endif
}

DIJON_FILTER_SHUTDOWN void shutdown_gmime(void)
{
	// Shutdown gmime
	g_mime_shutdown();
}
#endif

static bool read_stream(GMimeStream *memStream, dstring &fileBuffer)
{
	char readBuffer[4096];
	ssize_t totalSize = 0, bytesRead = 0;
	bool gotOutput = true;

	do
	{
		bytesRead = g_mime_stream_read(memStream, readBuffer, 4096);
		if (bytesRead > 0)
		{
			fileBuffer.append(readBuffer, bytesRead);
			totalSize += bytesRead;
		}
		else if (bytesRead == -1)
		{
			// An error occured
			if (errno != EINTR)
			{
				gotOutput = false;
				break;
			}

			// Try again
			bytesRead = 1;
		}
	} while (bytesRead > 0);
#ifdef DEBUG
	cout << "GMimeMboxFilter::extractPart: read " << totalSize
		<< "/" << fileBuffer.size() << " bytes" << endl;
#endif

	return gotOutput;
}

GMimeMboxFilter::GMimeMboxFilter(const string &mime_type) :
	Filter(mime_type),
	m_returnHeaders(false),
	m_pData(NULL),
	m_dataLength(0),
	m_fd(-1),
	m_pGMimeMboxStream(NULL),
	m_pParser(NULL),
	m_pMimeMessage(NULL),
	m_partsCount(-1),
	m_partNum(-1),
	m_messageStart(0),
	m_foundDocument(false)
{
}

GMimeMboxFilter::~GMimeMboxFilter()
{
	finalize(true);
}

bool GMimeMboxFilter::is_data_input_ok(DataInput input) const
{
	if ((input == DOCUMENT_DATA) ||
		(input == DOCUMENT_FILE_NAME))
	{
		return true;
	}

	return false;
}

bool GMimeMboxFilter::set_property(Properties prop_name, const string &prop_value)
{
	if (prop_name == PREFERRED_CHARSET)
	{
		m_defaultCharset = prop_value;

		return true;
	}
	else if (prop_name == OPERATING_MODE)
	{
		if (prop_value == "view")
		{
			m_returnHeaders = true;
		}
		else
		{
			m_returnHeaders = false;
		}

		return true;
	}

	return false;
}

bool GMimeMboxFilter::set_document_data(const char *data_ptr, unsigned int data_length)
{
	// Close/free whatever was opened/allocated on a previous call to set_document()
	finalize(true);
	m_partsCount = m_partNum = -1;
	m_messageStart = 0;
	m_messageDate.clear();
	m_partCharset.clear();
	m_foundDocument = false;

	m_pData = data_ptr;
	m_dataLength = data_length;

	// Assume there are documents if initialization is successful
	// but don't actually retrieve anything, until next or skip is called
	if (initializeData() == true)
	{
		m_foundDocument = initialize();
	}

	return m_foundDocument;
}

bool GMimeMboxFilter::set_document_string(const string &data_str)
{
	return false;
}

bool GMimeMboxFilter::set_document_file(const string &file_path, bool unlink_when_done)
{
	// Close/free whatever was opened/allocated on a previous call to set_document()
	finalize(true);
	m_partsCount = m_partNum = -1;
	m_messageStart = 0;
	m_messageDate.clear();
	m_partCharset.clear();
	m_foundDocument = false;

	Filter::set_document_file(file_path, unlink_when_done);

	// Assume there are documents if initialization is successful
	// but don't actually retrieve anything, until next or skip is called
	if (initializeFile() == true)
	{
		m_foundDocument = initialize();
	}

	return m_foundDocument;
}

bool GMimeMboxFilter::set_document_uri(const string &uri)
{
	return false;
}

bool GMimeMboxFilter::has_documents(void) const
{
	// As long as a document was found, chances are another one is available
	return m_foundDocument;
}

bool GMimeMboxFilter::next_document(void)
{
	string subject;

	map<string, string>::const_iterator titleIter = m_metaData.find("title");
	if (titleIter != m_metaData.end())
	{
		subject = titleIter->second;
	}

	return extractMessage(subject);
}

bool GMimeMboxFilter::skip_to_document(const string &ipath)
{
	if (ipath.empty() == true)
	{
		if (m_messageStart > 0)
		{
			// Reset
			return set_document_file(m_filePath);
		}

		return true;
	}

	// ipath's format is "o=offset&p=part_number"
	if (sscanf(ipath.c_str(), "o=%u&p=%d", (unsigned int*)&m_messageStart, &m_partNum) != 2)
	{
		return false;
	}

	finalize(false);
	m_partsCount = -1;
	m_messageDate.clear();
	m_partCharset.clear();
	m_foundDocument = false;

	if (((m_filePath.empty() == false) && (initializeFile() == true)) ||
		(initializeData() == true))
	{
		if (initialize() == true)
		{
			// Extract the first message at the given offset
			m_foundDocument = extractMessage("");
		}
	}

	return m_foundDocument;
}

string GMimeMboxFilter::get_error(void) const
{
	return "";
}

bool GMimeMboxFilter::initializeData(void)
{
	// Create a stream
	m_pGMimeMboxStream = g_mime_stream_mem_new_with_buffer(m_pData, m_dataLength);
	if (m_pGMimeMboxStream == NULL)
	{
		return false;
	}

	if (m_messageStart > 0)
	{
		ssize_t streamLength = g_mime_stream_length(m_pGMimeMboxStream);

#ifdef DEBUG
		cout << "GMimeMboxFilter::initializeData: from offset " << m_messageStart
			<< " to " << streamLength << endl;
#endif
		if (m_messageStart > (GMIME_OFFSET_TYPE)streamLength)
		{
			// This offset doesn't make sense !
			m_messageStart = 0;
		}

		g_mime_stream_set_bounds(m_pGMimeMboxStream, m_messageStart, (GMIME_OFFSET_TYPE)streamLength);
	}

	return true;
}

bool GMimeMboxFilter::initializeFile(void)
{
	int openFlags = O_RDONLY;
#ifdef O_CLOEXEC
	openFlags |= O_CLOEXEC;
#endif

	// Open the mbox file
#ifdef O_NOATIME
	m_fd = open(m_filePath.c_str(), openFlags|O_NOATIME);
#else
	m_fd = open(m_filePath.c_str(), openFlags);
#endif
#ifdef O_NOATIME
	if ((m_fd < 0) &&
		(errno == EPERM))
	{
		// Try again
		m_fd = open(m_filePath.c_str(), openFlags);
	}
#endif
	if (m_fd < 0)
	{
#ifdef DEBUG
		cout << "GMimeMboxFilter::initializeFile: couldn't open " << m_filePath << endl;
#endif
		return false;
	}
#ifndef O_CLOEXEC
	int fdFlags = fcntl(m_fd, F_GETFD);
	fcntl(m_fd, F_SETFD, fdFlags|FD_CLOEXEC);
#endif

	// Create a stream
	if (m_messageStart > 0)
	{
		ssize_t streamLength = g_mime_stream_length(m_pGMimeMboxStream);

		if (m_messageStart > (GMIME_OFFSET_TYPE)streamLength)
		{
			// This offset doesn't make sense !
			m_messageStart = 0;
		}

#ifdef HAVE_MMAP
		m_pGMimeMboxStream = g_mime_stream_mmap_new_with_bounds(m_fd, PROT_READ, MAP_PRIVATE, m_messageStart, (GMIME_OFFSET_TYPE)streamLength);
#else
		m_pGMimeMboxStream = g_mime_stream_fs_new_with_bounds(m_fd, m_messageStart, (GMIME_OFFSET_TYPE)streamLength);
#endif
#ifdef DEBUG
		cout << "GMimeMboxFilter::initializeFile: stream starts at offset " << m_messageStart << endl;
#endif
	}
	else
	{
#ifdef HAVE_MMAP
		m_pGMimeMboxStream = g_mime_stream_mmap_new(m_fd, PROT_READ, MAP_PRIVATE);
#else
		m_pGMimeMboxStream = g_mime_stream_fs_new(m_fd);
#endif
	}

	return true;
}

bool GMimeMboxFilter::initialize(void)
{
	if (m_pGMimeMboxStream == NULL)
	{
		return false;
	}

	// And a parser
	m_pParser = g_mime_parser_new();
	if (m_pParser != NULL)
	{
		g_mime_parser_init_with_stream(m_pParser, m_pGMimeMboxStream);
		g_mime_parser_set_respect_content_length(m_pParser, TRUE);
		// Scan for mbox From-lines
		g_mime_parser_set_scan_from(m_pParser, TRUE);

		return true;
	}
#ifdef DEBUG
	cout << "GMimeMboxFilter::initialize: couldn't create new parser" << endl;
#endif

	return false;
}

void GMimeMboxFilter::finalize(bool fullReset)
{
	if (m_pMimeMessage != NULL)
	{
#ifdef GMIME_ENABLE_RFC2047_WORKAROUNDS
		g_object_unref(m_pMimeMessage);
#else
		g_mime_object_unref(GMIME_OBJECT(m_pMimeMessage));
#endif
		m_pMimeMessage = NULL;
	}
	if (m_pParser != NULL)
	{
		// FIXME: does the parser close the stream ?
		g_object_unref(m_pParser);
		m_pParser = NULL;
	}
	if (m_pGMimeMboxStream != NULL)
	{
		g_object_unref(m_pGMimeMboxStream);
		m_pGMimeMboxStream = NULL;
	}

	// initializeFile() will always reopen the file
	if (m_fd >= 0)
	{
		close(m_fd);
		m_fd = -1;
	}
	if (fullReset == true)
	{
		// ...but those data fields will only be reinit'ed on a full reset
		m_pData = NULL;
		m_dataLength = 0;

		rewind();
	}
}

bool GMimeMboxFilter::nextPart(const string &subject)
{
	if (m_pMimeMessage != NULL)
	{
		// Get the top-level MIME part in the message
		GMimeObject *pMimePart = g_mime_message_get_mime_part(m_pMimeMessage);
		if (pMimePart != NULL)
		{
			string partTitle(subject), partContentType;

			// Extract the part's text
			m_content.clear();
			if (extractPart(pMimePart, partTitle, partContentType, m_content) == true)
			{
				char posStr[128];

				// New document
				m_metaData.clear();
				m_metaData["title"] = partTitle;
				m_metaData["mimetype"] = partContentType;
				m_metaData["date"] = m_messageDate;
				m_metaData["charset"] = m_partCharset;
				snprintf(posStr, 128, "%u", m_content.length());
				m_metaData["size"] = posStr;
				// FIXME: use the same scheme as Mozilla
				snprintf(posStr, 128, "o=%u&p=%d", m_messageStart, max(m_partNum - 1, 0));
				m_metaData["ipath"] = posStr;
#ifdef DEBUG
				cout << "GMimeMboxFilter::extractMessage: message location is " << posStr << endl; 
#endif

#ifndef GMIME_ENABLE_RFC2047_WORKAROUNDS
				g_mime_object_unref(pMimePart);
#endif

				return true;
			}

#ifndef GMIME_ENABLE_RFC2047_WORKAROUNDS
			g_mime_object_unref(pMimePart);
#endif
		}

#ifdef GMIME_ENABLE_RFC2047_WORKAROUNDS
		g_object_unref(m_pMimeMessage);
#else
		g_mime_object_unref(GMIME_OBJECT(m_pMimeMessage));
#endif
		m_pMimeMessage = NULL;
	}

	// If we get there, no suitable parts were found
	m_partsCount = m_partNum = -1;

	return false;
}

bool GMimeMboxFilter::extractPart(GMimeObject *part, string &subject, string &contentType, dstring &partBuffer)
{
	if (part == NULL)
	{
		return false;
	}

	// Message parts may be nested
	while (GMIME_IS_MESSAGE_PART(part))
	{
#ifdef DEBUG
		cout << "GMimeMboxFilter::extractPart: nested message part" << endl;
#endif
		GMimeMessage *partMessage = g_mime_message_part_get_message(GMIME_MESSAGE_PART(part));
		part = g_mime_message_get_mime_part(partMessage);
#ifdef GMIME_ENABLE_RFC2047_WORKAROUNDS
		g_object_unref(partMessage);
#else
		g_mime_object_unref(GMIME_OBJECT(partMessage));
#endif
	}

	// Is this a multipart ?
	if (GMIME_IS_MULTIPART(part))
	{
#ifdef GMIME_ENABLE_RFC2047_WORKAROUNDS
		m_partsCount = g_mime_multipart_get_count(GMIME_MULTIPART(part));
#else
		m_partsCount = g_mime_multipart_get_number(GMIME_MULTIPART(part));
#endif
#ifdef DEBUG
		cout << "GMimeMboxFilter::extractPart: message has " << m_partsCount << " parts" << endl;
#endif
		for (int partNum = max(m_partNum, 0); partNum < m_partsCount; ++partNum)
		{
#ifdef DEBUG
			cout << "GMimeMboxFilter::extractPart: extracting part " << partNum << endl;
#endif
			
			GMimeObject *multiMimePart = g_mime_multipart_get_part(GMIME_MULTIPART(part), partNum);
			if (multiMimePart == NULL)
			{
				continue;
			}

			bool gotPart = extractPart(multiMimePart, subject, contentType, partBuffer);
#ifndef GMIME_ENABLE_RFC2047_WORKAROUNDS
			g_mime_object_unref(multiMimePart);
#endif

			if (gotPart == true)
			{
				m_partNum = ++partNum;
				return true;
			}
		}

		// None of the parts were suitable
		m_partsCount = m_partNum = -1;
	}

	if (!GMIME_IS_PART(part))
	{
#ifdef DEBUG
		cout << "GMimeMboxFilter::extractPart: not a part" << endl;
#endif
		return false;
	}
	GMimePart *mimePart = GMIME_PART(part);

	// Check the content type
#ifdef GMIME_ENABLE_RFC2047_WORKAROUNDS
	GMimeContentType *mimeType = g_mime_object_get_content_type(GMIME_OBJECT(mimePart));
#else
	const GMimeContentType *mimeType = g_mime_object_get_content_type(GMIME_OBJECT(mimePart));
#endif
	// Set this for caller
	char *partType = g_mime_content_type_to_string(mimeType);
	if (partType != NULL)
	{
#ifdef DEBUG
		cout << "GMimeMboxFilter::extractPart: type is " << partType << endl;
#endif
		contentType = partType;
		g_free(partType);
	}

#ifdef GMIME_ENABLE_RFC2047_WORKAROUNDS
	GMimeContentEncoding encodingType = g_mime_part_get_content_encoding(mimePart);
#else
	GMimePartEncodingType encodingType = g_mime_part_get_encoding(mimePart);
#endif
#ifdef DEBUG
	cout << "GMimeMboxFilter::extractPart: encoding is " << encodingType << endl;
#endif
#ifdef GMIME_ENABLE_RFC2047_WORKAROUNDS
	g_mime_part_set_content_encoding(mimePart, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
#else
	g_mime_part_set_encoding(mimePart, GMIME_PART_ENCODING_QUOTEDPRINTABLE);
#endif
	const char *fileName = g_mime_part_get_filename(mimePart);
	if (fileName != NULL)
	{
#ifdef DEBUG
		cout << "GMimeMboxFilter::extractPart: file name is " << fileName << endl;
#endif
		subject = fileName;
	}

	// Create a in-memory output stream
	GMimeStream *memStream = g_mime_stream_mem_new();

	const char *charset = g_mime_content_type_get_parameter(mimeType, "charset");
	if (charset != NULL)
	{
		m_partCharset = charset;
#if 0
		// Install a charset filter
		if (strncasecmp(charset, "UTF-8", 5) != 0)
		{
			GMimeFilter *charsetFilter = g_mime_filter_charset_new(charset, "UTF-8");
			if (charsetFilter != NULL)
			{
#ifdef DEBUG
				cout << "GMimeMboxFilter::extractPart: converting from charset " << charset << endl;
#endif
				g_mime_stream_filter_add(GMIME_STREAM_FILTER(memStream), charsetFilter);
				g_object_unref(charsetFilter);
			}
		}
#endif
	}

	// Write the part to the stream
	GMimeDataWrapper *dataWrapper = g_mime_part_get_content_object(mimePart);
	if (dataWrapper != NULL)
	{
		ssize_t writeLen = g_mime_data_wrapper_write_to_stream(dataWrapper, memStream);
#ifdef DEBUG
		cout << "GMimeMboxFilter::extractPart: wrote " << writeLen << " bytes" << endl;
#endif
		g_object_unref(dataWrapper);
	}
	g_mime_stream_flush(memStream);
	ssize_t partLen = g_mime_stream_length(memStream);
#ifdef DEBUG
	cout << "GMimeMboxFilter::extractPart: part is " << partLen << " bytes long" << endl;
#endif

	if ((m_returnHeaders == true) &&
		(contentType.length() >= 10) &&
		(strncasecmp(contentType.c_str(), "text/plain", 10) == 0))
	{
		char *pHeaders = g_mime_object_get_headers(GMIME_OBJECT(m_pMimeMessage));

		if (pHeaders != NULL)
		{
			partBuffer = pHeaders;
			partBuffer += "\n";
			free(pHeaders);
		}
	}

	g_mime_stream_reset(memStream);
	partBuffer.reserve(partLen);
	read_stream(memStream, partBuffer);
	g_object_unref(memStream);

	return true;
}

bool GMimeMboxFilter::extractMessage(const string &subject)
{
	string msgSubject(subject);

	while (g_mime_stream_eos(m_pGMimeMboxStream) == FALSE)
	{
		// Does the previous message have parts left to parse ?
		if (m_partsCount == -1)
		{
			// No, it doesn't
			if (m_pMimeMessage != NULL)
			{
#ifdef GMIME_ENABLE_RFC2047_WORKAROUNDS
				g_object_unref(m_pMimeMessage);
#else
				g_mime_object_unref(GMIME_OBJECT(m_pMimeMessage));
#endif
				m_pMimeMessage = NULL;
			}

			// Get the next message
			m_pMimeMessage = g_mime_parser_construct_message(m_pParser);
			if (m_pMimeMessage == NULL)
			{
				cerr << "Couldn't construct new MIME message" << endl;
				break;
			}

			m_messageStart = g_mime_parser_get_from_offset(m_pParser);
#ifdef GMIME_ENABLE_RFC2047_WORKAROUNDS
			gint64 messageEnd = g_mime_parser_tell(m_pParser);
#else
			off_t messageEnd = g_mime_parser_tell(m_pParser);
#endif
#ifdef DEBUG
			cout << "GMimeMboxFilter::extractMessage: message between offsets " << m_messageStart
				<< " and " << messageEnd << endl;
#endif
			if (messageEnd > m_messageStart)
			{
				// This only applies to Mozilla
				const char *pMozStatus = g_mime_object_get_header(GMIME_OBJECT(m_pMimeMessage), "X-Mozilla-Status");
				if (pMozStatus != NULL)
				{
					long int mozFlags = strtol(pMozStatus, NULL, 16);

					// Watch out for Mozilla specific flags :
					// MSG_FLAG_EXPUNGED, MSG_FLAG_EXPIRED
					// They are defined in mailnews/MailNewsTypes.h and msgbase/nsMsgMessageFlags.h
					if ((mozFlags & 0x0008) ||
						(mozFlags & 0x0040))
					{
#ifdef DEBUG
						cout << "GMimeMboxFilter::extractMessage: flagged by Mozilla" << endl;
#endif
						continue;
					}
				}
				// This only applies to Evolution
				const char *pEvoStatus = g_mime_object_get_header(GMIME_OBJECT(m_pMimeMessage), "X-Evolution");
				if (pEvoStatus != NULL)
				{
					string evoStatus(pEvoStatus);
					string::size_type flagsPos = evoStatus.find('-');

					if (flagsPos != string::npos)
					{
						long int evoFlags = strtol(evoStatus.substr(flagsPos + 1).c_str(), NULL, 16);

						// Watch out for Evolution specific flags :
						// CAMEL_MESSAGE_DELETED
						// It's defined in camel/camel-folder-summary.h
						if (evoFlags & 0x0002)
						{
#ifdef DEBUG
							cout << "GMimeMboxFilter::extractMessage: flagged by Evolution" << endl;
#endif
							continue;
						}
					}
				}

				// How old is this message ?
				const char *pDate = g_mime_object_get_header(GMIME_OBJECT(m_pMimeMessage), "Date");
				if (pDate != NULL)
				{
					m_messageDate = pDate;
				}
				else
				{
					time_t timeNow = time(NULL);
					struct tm *pTimeTm = new struct tm;

#ifdef HAVE_LOCALTIME_R
					if (localtime_r(&timeNow, pTimeTm) != NULL)
#else
					pTimeTm = localtime(&timeNow);
					if (pTimeTm != NULL)
#endif
					{
						char timeStr[64];

						if (strftime(timeStr, 64, "%a, %d %b %Y %H:%M:%S %Z", pTimeTm) > 0)
						{
							m_messageDate = timeStr;
						}
					}

					delete pTimeTm;
				}
#ifdef DEBUG
				cout << "GMimeMboxFilter::extractMessage: message date is " << m_messageDate << endl;
#endif

				// Extract the subject
				const char *pSubject = g_mime_message_get_subject(m_pMimeMessage);
				if (pSubject != NULL)
				{
					msgSubject = pSubject;
				}
			}
		}
#ifdef DEBUG
		cout << "GMimeMboxFilter::extractMessage: message subject is " << msgSubject << endl;
#endif

		return nextPart(msgSubject);
	}

	// The last message may have parts left
	if (m_partsCount != -1)
	{
		return nextPart(msgSubject);
	}

	return false;
}
