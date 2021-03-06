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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>
#include <utility>
#include <algorithm>

#include <gmime/gmime.h>

#include "GMimeMboxFilter.h"

using std::cout;
using std::endl;
using std::string;
using std::map;
using std::max;
using namespace Dijon;

#ifdef _DYNAMIC_DIJON_FILTERS
bool get_filter_types(std::set<std::string> &mime_types)
{
	mime_types.clear();
	mime_types.insert("application/mbox");
	mime_types.insert("text/x-mail");
	mime_types.insert("text/x-news");

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
	return new GMimeMboxFilter(mime_type);
}
#endif

GMimeMboxFilter::GMimeMboxFilter(const string &mime_type) :
	Filter(mime_type),
	m_returnHeaders(false),
	m_fd(-1),
	m_pGMimeMboxStream(NULL),
	m_pParser(NULL),
	m_pMimeMessage(NULL),
	m_partsCount(-1),
	m_partNum(-1),
	m_messageStart(0),
	m_foundDocument(false)
{
	// Initialize gmime
	g_mime_init(GMIME_INIT_FLAG_UTF8);
}

GMimeMboxFilter::~GMimeMboxFilter()
{
	finalize(true);

	// Shutdown gmime
	g_mime_shutdown();
}

bool GMimeMboxFilter::is_data_input_ok(DataInput input) const
{
	if (input == DOCUMENT_FILE_NAME)
	{
		return true;
	}

	return false;
}

bool GMimeMboxFilter::set_property(Properties prop_name, const string &prop_value)
{
	if (prop_name == DEFAULT_CHARSET)
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
	return false;
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
	m_foundDocument = initialize();

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
	if (sscanf(ipath.c_str(), "o=%u&p=%d", &m_messageStart, &m_partNum) != 2)
	{
		return false;
	}

	finalize(false);
	m_partsCount = -1;
	m_messageDate.clear();
	m_partCharset.clear();
	m_foundDocument = false;

	if (initialize() == true)
	{
		// Extract the first message at the given offset
		m_foundDocument = extractMessage("");
	}

	return m_foundDocument;
}

string GMimeMboxFilter::get_error(void) const
{
	return "";
}

bool GMimeMboxFilter::initialize(void)
{
#ifndef O_NOATIME
	int openFlags = O_RDONLY;
#else
	int openFlags = O_RDONLY|O_NOATIME;
#endif

	// Open the mbox file
	m_fd = open(m_filePath.c_str(), openFlags);
#ifdef O_NOATIME
	if ((m_fd < 0) &&
		(errno == EPERM))
	{
		// Try again
		m_fd = open(m_filePath.c_str(), O_RDONLY);
	}
#endif
	if (m_fd < 0)
	{
#ifdef DEBUG
		cout << "GMimeMboxFilter::initialize: couldn't open " << m_filePath << endl;
#endif
		return false;
	}

	// Create a stream
	if (m_messageStart > 0)
	{
		struct stat fileStat;

		if ((fstat(m_fd, &fileStat) == 0) &&
			(!S_ISREG(fileStat.st_mode)))
		{
			// This is not a file !
			return false;
		}

		if (m_messageStart > fileStat.st_size)
		{
			// This offset doesn't make sense !
			m_messageStart = 0;
		}

		m_pGMimeMboxStream = g_mime_stream_fs_new_with_bounds(m_fd, m_messageStart, fileStat.st_size);
#ifdef DEBUG
		cout << "GMimeMboxFilter::initialize: stream starts at offset " << m_messageStart << endl;
#endif
	}
	else
	{
		m_pGMimeMboxStream = g_mime_stream_fs_new(m_fd);
	}

	// And a parser
	m_pParser = g_mime_parser_new();
	if ((m_pGMimeMboxStream != NULL) &&
		(m_pParser != NULL))
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
		g_mime_object_unref(GMIME_OBJECT(m_pMimeMessage));
		m_pMimeMessage = NULL;
	}
	if (m_pParser != NULL)
	{
		// FIXME: does the parser close the stream ?
		g_object_unref(G_OBJECT(m_pParser));
		m_pParser = NULL;
	}
	else if (m_pGMimeMboxStream != NULL)
	{
		g_object_unref(G_OBJECT(m_pGMimeMboxStream));
		m_pGMimeMboxStream = NULL;
	}
	if (m_fd >= 0)
	{
		close(m_fd);
		m_fd = -1;
	}

	if (fullReset == true)
	{
		rewind();
	}
}

char *GMimeMboxFilter::extractPart(GMimeObject *part, string &contentType, ssize_t &partLen)
{
	char *pBuffer = NULL;

	if (part == NULL)
	{
		return NULL;
	}

	// Message parts may be nested
	while (GMIME_IS_MESSAGE_PART(part))
	{
#ifdef DEBUG
		cout << "GMimeMboxFilter::extractPart: nested message part" << endl;
#endif
		GMimeMessage *partMessage = g_mime_message_part_get_message(GMIME_MESSAGE_PART(part));
		part = g_mime_message_get_mime_part(partMessage);
		g_mime_object_unref(GMIME_OBJECT(partMessage));
	}

	// Is this a multipart ?
	if (GMIME_IS_MULTIPART(part))
	{
		m_partsCount = g_mime_multipart_get_number(GMIME_MULTIPART(part));
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

			char *pPart = extractPart(multiMimePart, contentType, partLen);
			g_mime_object_unref(multiMimePart);
			if (pPart != NULL)
			{
				m_partNum = ++partNum;
				return pPart;
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
		return NULL;
	}
	GMimePart *mimePart = GMIME_PART(part);
	GMimeFilter *charsetFilter = NULL;

	// Check the content type
	const GMimeContentType *mimeType = g_mime_part_get_content_type(mimePart);
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

	GMimePartEncodingType encodingType = g_mime_part_get_encoding(mimePart);
#ifdef DEBUG
	cout << "GMimeMboxFilter::extractPart: encoding is " << encodingType << endl;
#endif
	g_mime_part_set_encoding(mimePart, GMIME_PART_ENCODING_QUOTEDPRINTABLE);

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
			charsetFilter = g_mime_filter_charset_new(charset, "UTF-8");
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
	partLen = g_mime_stream_length(memStream);
#ifdef DEBUG
	cout << "GMimeMboxFilter::extractPart: part is " << partLen << " bytes long" << endl;
#endif

	pBuffer = (char*)malloc(partLen + 1);
	pBuffer[partLen] = '\0';
	g_mime_stream_reset(memStream);
	ssize_t readLen = g_mime_stream_read(memStream, pBuffer, partLen);
#ifdef DEBUG
	cout << "GMimeMboxFilter::extractPart: read " << readLen << " bytes" << endl;
#endif
	g_mime_stream_unref(memStream);

	return pBuffer;
}

bool GMimeMboxFilter::extractMessage(const string &subject)
{
	string msgSubject(subject), contentType;
	char *pPart = NULL;
	ssize_t partLength = 0;

	while (g_mime_stream_eos(m_pGMimeMboxStream) == FALSE)
	{
		// Does the previous message have parts left to parse ?
		if (m_partsCount == -1)
		{
			// No, it doesn't
			if (m_pMimeMessage != NULL)
			{
				g_mime_object_unref(GMIME_OBJECT(m_pMimeMessage));
				m_pMimeMessage = NULL;
			}

			// Get the next message
			m_pMimeMessage = g_mime_parser_construct_message(m_pParser);

			m_messageStart = g_mime_parser_get_from_offset(m_pParser);
			off_t messageEnd = g_mime_parser_tell(m_pParser);
#ifdef DEBUG
			cout << "GMimeMboxFilter::extractMessage: message between offsets " << m_messageStart
				<< " and " << messageEnd << endl;
#endif
			if (messageEnd > m_messageStart)
			{
				// FIXME: this only applies to Mozilla
				const char *pMozStatus = g_mime_message_get_header(m_pMimeMessage, "X-Mozilla-Status");
				if (pMozStatus != NULL)
				{
					long int mozStatus = strtol(pMozStatus, NULL, 16);
					// Watch out for Mozilla specific flags :
					// MSG_FLAG_EXPUNGED, MSG_FLAG_EXPIRED
					// They are defined in mailnews/MailNewsTypes.h and msgbase/nsMsgMessageFlags.h
					if ((mozStatus & 0x0008) ||
						(mozStatus & 0x0040))
					{
#ifdef DEBUG
						cout << "GMimeMboxFilter::extractMessage: flagged by Mozilla" << endl;
#endif
						continue;
					}
				}

				// How old is this message ?
				const char *pDate = g_mime_message_get_header(m_pMimeMessage, "Date");
				if (pDate != NULL)
				{
					m_messageDate = pDate;
				}
				else
				{
					time_t timeNow = time(NULL);
					struct tm timeTm;

					if (localtime_r(&timeNow, &timeTm) != NULL)
					{
						char timeStr[64];

						if (strftime(timeStr, 64, "%a, %d %b %Y %H:%M:%S %Z", &timeTm) > 0)
						{
							m_messageDate = timeStr;
						}
					}
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

		if (m_pMimeMessage != NULL)
		{
			// Get the top-level MIME part in the message
			GMimeObject *pMimePart = g_mime_message_get_mime_part(m_pMimeMessage);
			if (pMimePart != NULL)
			{
				// Extract the part's text
				pPart = extractPart(pMimePart, contentType, partLength);
				if (pPart != NULL)
				{
					string content;
					string location("mailbox://");
					char posStr[128];

					if ((m_returnHeaders == true) &&
						(contentType.length() >= 10) &&
						(strncasecmp(contentType.c_str(), "text/plain", 10) == 0))
					{
						char *pHeaders = g_mime_message_get_headers(m_pMimeMessage);

						if (pHeaders != NULL)
						{
							content = pHeaders;
							content += "\n";
							free(pHeaders);
						}
					}
					content += string(pPart, (unsigned int)partLength);

					// FIXME: use the same scheme as Mozilla
					location += m_filePath;

					// New document
					m_metaData.clear();
					m_metaData["title"] = msgSubject;
					m_metaData["uri"] = location;
					m_metaData["mimetype"] = contentType;
					m_metaData["content"] = content;
					m_metaData["date"] = m_messageDate;
					m_metaData["charset"] = m_partCharset;
					snprintf(posStr, 128, "%u", partLength);
					m_metaData["size"] = posStr;
					snprintf(posStr, 128, "o=%u&p=%d", m_messageStart, max(m_partNum - 1, 0));
					m_metaData["ipath"] = posStr;
#ifdef DEBUG
					cout << "GMimeMboxFilter::extractMessage: message location is " << location
						<< "?" << posStr << endl;
#endif

					free(pPart);
					g_mime_object_unref(pMimePart);

					return true;
				}

				g_mime_object_unref(pMimePart);
			}

			g_mime_object_unref(GMIME_OBJECT(m_pMimeMessage));
			m_pMimeMessage = NULL;
		}

		// If we get there, no suitable parts were found
		m_partsCount = m_partNum = -1;
	}

	return false;
}
