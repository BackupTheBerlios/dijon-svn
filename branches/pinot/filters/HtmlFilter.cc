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
#include <libxml/xmlerror.h>
#include <libxml/HTMLparser.h>
#include <iostream>

#include "StringManip.h"
#include "Url.h"
#include "HtmlFilter.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::map;
using std::set;

using namespace std;
using namespace Dijon;

static bool getInBetweenLinksText(HtmlFilter::ParserState *pState,
	unsigned int currentLinkIndex)
{
	if (pState == NULL)
	{
		return false;
	}

	if ((pState->m_links.empty() == true) ||
		(pState->m_currentLink.m_index == 0))
	{
		string abstract(pState->m_text);

		StringManip::trimSpaces(abstract);

		pState->m_abstract = abstract;

		return true;
	}

	// Get the text between the current link and the previous one
	for (set<Link>::const_iterator linkIter = pState->m_links.begin();
		linkIter != pState->m_links.end(); ++linkIter)
	{
		// Is this the previous link ?
		if (linkIter->m_index == currentLinkIndex - 1)
		{
			// Is there text in between ?
			if (linkIter->m_endPos + 1 < pState->m_textPos)
			{
				unsigned int abstractLen = pState->m_textPos - linkIter->m_endPos - 1;
				string abstract(pState->m_text.substr(linkIter->m_endPos, abstractLen));

				StringManip::trimSpaces(abstract);

				// The longer, the better
				if (abstract.length() > pState->m_abstract.length())
				{
					pState->m_abstract = abstract;
#ifdef DEBUG
					cout << "HtmlFilter::getInBetweenLinksText: abstract after link "
						<< linkIter->m_index << endl;
#endif

					return true;
				}
			}

			break;
		}
	}

	return false;
}

static void startHandler(void *pData, const char *pElementName, const char **pAttributes)
{
	if ((pData == NULL) ||
		(pElementName == NULL) ||
		(strlen(pElementName) == 0))
	{
		return;
	}

	HtmlFilter::ParserState *pState = (HtmlFilter::ParserState *)pData;
	if (pState == NULL)
	{
		return;
	}

	// Reset the text hash
	pState->m_lastHash.clear();

	// What tag is this ?
	string tagName(StringManip::toLowerCase(pElementName));
	if ((pState->m_foundHead == false) &&
		(tagName == "head"))
	{
		// Expect to find META tags and a title
		pState->m_inHead = true;
		// One head is enough :-)
		pState->m_foundHead = true;
	}
	else if ((pState->m_inHead == true) &&
		(tagName == "meta") &&
		(pAttributes != NULL))
	{
		string metaName, metaContent;

		// Get the META tag's name and content
		for (unsigned int attrNum = 0;
			(pAttributes[attrNum] != NULL) && (pAttributes[attrNum + 1]); attrNum += 2)
		{
			if (strncasecmp(pAttributes[attrNum], "name", 4) == 0)
			{
				metaName = pAttributes[attrNum + 1];
			}
			else if (strncasecmp(pAttributes[attrNum], "content", 7) == 0)
			{
				metaContent = pAttributes[attrNum + 1];
			}
			else if (strncasecmp(pAttributes[attrNum], "http-equiv", 10) == 0)
			{
				metaName = pAttributes[attrNum + 1];
			}
		}

		if ((metaName.empty() == false) &&
			(metaContent.empty() == false))
		{
			// Store this META tag
			pState->m_metaTags[StringManip::toLowerCase(metaName)] = metaContent;
		}
	}
	else if ((pState->m_inHead == true) &&
		(tagName == "title"))
	{
		// Extract title
		pState->m_appendToTitle = true;
	}
	else if (tagName == "body")
	{
		// Index text
		pState->m_appendToText = true;
	}
	else if ((tagName == "a") &&
		(pAttributes != NULL))
	{
		pState->m_currentLink.m_url.clear();
		pState->m_currentLink.m_name.clear();

		// Get the href
		for (unsigned int attrNum = 0;
			(pAttributes[attrNum] != NULL) && (pAttributes[attrNum + 1]); attrNum += 2)
		{
			if (strncasecmp(pAttributes[attrNum], "href", 4) == 0)
			{
				pState->m_currentLink.m_url = pAttributes[attrNum + 1];
				break;
			}
		}

		if (pState->m_currentLink.m_url.empty() == false)
		{
			// FIXME: get the NodeInfo to find out the position of this link
			pState->m_currentLink.m_startPos = pState->m_textPos;

			// Find abstract ?
			if (pState->m_findAbstract == true)
			{
				getInBetweenLinksText(pState, pState->m_currentLink.m_index);
			}

			// Extract link
			pState->m_appendToLink = true;
		}
	}
	else if ((tagName == "frame") &&
		(pAttributes != NULL))
	{
		Link frame;

		// Get the name and source
		for (unsigned int attrNum = 0;
			(pAttributes[attrNum] != NULL) && (pAttributes[attrNum + 1]); attrNum += 2)
		{
			if (strncasecmp(pAttributes[attrNum], "name", 4) == 0)
			{
				frame.m_name = pAttributes[attrNum + 1];
			}
			else if (strncasecmp(pAttributes[attrNum], "src", 3) == 0)
			{
				frame.m_url = pAttributes[attrNum + 1];
			}
		}

		if (frame.m_url.empty() == false)
		{
			// Store this frame
			pState->m_frames.insert(frame);
		}
	}
	else if ((tagName == "frameset") ||
		(tagName == "script") ||
		(tagName == "style"))
	{
		// Skip
		++pState->m_skip;
	}
}

static void endHandler(void *pData, const char *pElementName)
{
	if ((pData == NULL) ||
		(pElementName == NULL) ||
		(strlen(pElementName) == 0))
	{
		return;
	}

	HtmlFilter::ParserState *pState = (HtmlFilter::ParserState *)pData;
	if (pState == NULL)
	{
		return;
	}

	// Reset state
	string tagName(StringManip::toLowerCase(pElementName));
	if (tagName == "head")
	{
		pState->m_inHead = false;
	}
	else if (tagName == "title")
	{
		StringManip::trimSpaces(pState->m_title);
		StringManip::removeCharacters(pState->m_title, "\r\n");
#ifdef DEBUG
		cout << "HtmlFilter::endHandler: title is " << pState->m_title << endl;
#endif
		pState->m_appendToTitle = false;
	}
	else if (tagName == "body")
	{
		pState->m_appendToText = false;
	}
	else if (tagName == "a")
	{
		if (pState->m_currentLink.m_url.empty() == false)
		{
			StringManip::trimSpaces(pState->m_currentLink.m_name);
			StringManip::removeCharacters(pState->m_currentLink.m_name, "\r\n");

			pState->m_currentLink.m_endPos = pState->m_textPos;

			// Store this link
			pState->m_links.insert(pState->m_currentLink);
			++pState->m_currentLink.m_index;
		}

		pState->m_appendToLink = false;
	}
	else if ((tagName == "frameset") ||
		(tagName == "script") ||
		(tagName == "style"))
	{
		--pState->m_skip;
	}
}

static void charactersHandler(void *pData, const char *pText, int textLen)
{
	if ((pData == NULL) ||
		(pText == NULL) ||
		(textLen == 0))
	{
		return;
	}

	HtmlFilter::ParserState *pState = (HtmlFilter::ParserState *)pData;
	if (pState == NULL)
	{
		return;
	}

	if (pState->m_skip > 0)
	{
		// Skip this
		return;
	}

	string text(pText, textLen);

	// For some reason, this handler might be called twice for the same text !
	// See http://mail.gnome.org/archives/xml/2002-September/msg00089.html
	string textHash(StringManip::hashString(text));
	if (pState->m_lastHash == textHash)
	{
		// Ignore this
		return;
	}
	pState->m_lastHash = textHash;

	// Append current text
	// FIXME: convert to UTF-8 or Latin 1 ?
	if (pState->m_appendToTitle == true)
	{
		pState->m_title += text;
	}
	else
	{
		if (pState->m_appendToText == true)
		{
			pState->m_text += text;
			pState->m_textPos += textLen;
		}

		// Appending to text and to link are not mutually exclusive operations
		if (pState->m_appendToLink == true)
		{
			pState->m_currentLink.m_name += text;
		}
	}
}

static void cDataHandler(void *pData, const char *pText, int textLen)
{
	// Nothing to do
}

static void whitespaceHandler(void *pData, const xmlChar *pText, int txtLen)
{
	if (pData == NULL)
	{
		return;
	}

	HtmlFilter::ParserState *pState = (HtmlFilter::ParserState *)pData;
	if (pState == NULL)
	{
		return;
	}

	if (pState->m_skip > 0)
	{
		// Skip this
		return;
	}

	// Append a single space
	if (pState->m_appendToTitle == true)
	{
		pState->m_title += " ";
	}
	else
	{
		if (pState->m_appendToText == true)
		{
			pState->m_text += " ";
		}

		// Appending to text and to link are not mutually exclusive operations
		if (pState->m_appendToLink == true)
		{
			pState->m_currentLink.m_name += " ";
		}
	}
}

static void commentHandler(void *pData, const char *pText)
{
	// FIXME: take comments into account, eg on terms position ?
}

static void errorHandler(void *pData, const char *pMsg, ...)
{
	if (pData == NULL)
	{
		return;
	}

	HtmlFilter::ParserState *pState = (HtmlFilter::ParserState *)pData;
	if (pState == NULL)
	{
		return;
	}

	va_list args;
	char pErr[1000];

	va_start(args, pMsg);
	vsnprintf(pErr, 1000, pMsg, args );
	va_end(args);

	cerr << "HtmlFilter::errorHandler: " << pErr << endl;

	// Be lenient as much as possible
	xmlResetLastError();
	// ...but remember the document had errors
	pState->m_isValid = false;
}

static void warningHandler(void *pData, const char *pMsg, ...)
{
	va_list args;
	char pErr[1000];

	va_start(args, pMsg);
	vsnprintf(pErr, 1000, pMsg, args );
	va_end(args);

	cerr << "HtmlFilter::warningHandler: " << pErr << endl;
}

Link::Link() :
	m_index(0),
	m_startPos(0),
	m_endPos(0)
{
}

Link::Link(const Link &other) :
	m_url(other.m_url),
	m_name(other.m_name),
	m_index(other.m_index),
	m_startPos(other.m_startPos),
	m_endPos(other.m_endPos)
{
}

Link::~Link()
{
}

Link& Link::operator=(const Link& other)
{
	if (this != &other)
	{
		m_url = other.m_url;
		m_name = other.m_name;
		m_index = other.m_index;
		m_startPos = other.m_startPos;
		m_endPos = other.m_endPos;
	}

	return *this;
}

bool Link::operator==(const Link &other) const
{
	return m_url == other.m_url;
}

bool Link::operator<(const Link &other) const
{
	return m_index < other.m_index;
}

HtmlFilter::ParserState::ParserState() :
	m_isValid(true),
	m_findAbstract(false),
	m_textPos(0),
	m_inHead(false),
	m_foundHead(false),
	m_appendToTitle(false),
	m_appendToText(false),
	m_appendToLink(false),
	m_skip(0)
{
}

HtmlFilter::ParserState::~ParserState()
{
}

unsigned int HtmlFilter::m_initialized = 0;

HtmlFilter::HtmlFilter(const string &mime_type) :
	Filter(mime_type),
	m_pState(NULL)
{
	if (m_initialized == 0)
	{
		xmlInitParser();
		++m_initialized;
	}
}

HtmlFilter::~HtmlFilter()
{
	rewind();

	--m_initialized;
	if (m_initialized == 0)
	{
		xmlCleanupParser();
	}
}

void HtmlFilter::rewind(void)
{
	if (m_pState != NULL)
	{
		delete m_pState;
		m_pState = NULL;
	}

	m_metaData.clear();
}

bool HtmlFilter::parse_html(const string &html_doc)
{
	htmlSAXHandler saxHandler;

	// Setup the SAX handler
	memset((void*)&saxHandler, 0, sizeof(htmlSAXHandler));
	saxHandler.startElement = (startElementSAXFunc)&startHandler;
	saxHandler.endElement = (endElementSAXFunc)&endHandler;
	saxHandler.characters = (charactersSAXFunc)&charactersHandler;
	saxHandler.cdataBlock = (charactersSAXFunc)&cDataHandler;
	saxHandler.ignorableWhitespace = (ignorableWhitespaceSAXFunc)&whitespaceHandler;
	saxHandler.comment = (commentSAXFunc)&commentHandler;
	saxHandler.fatalError = (fatalErrorSAXFunc)&errorHandler;
	saxHandler.error = (errorSAXFunc)&errorHandler;
	saxHandler.warning = (warningSAXFunc)&warningHandler;

	m_pState = new ParserState();

	htmlParserCtxtPtr pContext = htmlCreatePushParserCtxt(&saxHandler, (void*)m_pState,
		html_doc.c_str(), (int)html_doc.length(), "", XML_CHAR_ENCODING_NONE);
	if (pContext != NULL)
	{
		xmlCtxtUseOptions(pContext, 0);

		// Parse
		htmlParseChunk(pContext, html_doc.c_str(), (int)html_doc.length(), 0);

		// Free
		htmlParseChunk(pContext, html_doc.c_str(), 0, 1);
		xmlDocPtr pDoc = pContext->myDoc;
		int ret = pContext->wellFormed;
		xmlFreeParserCtxt(pContext);
		if (!ret)
		{
#ifdef DEBUG
			cout << "HtmlFilter::parse_html: freeing document" << endl;
#endif
			xmlFreeDoc(pDoc);
		}
	}
	else
	{
		cerr << "HtmlFilter::parse_html: couldn't create parser context" << endl;
	}

	// The text after the last link might make a good abstract
	if (m_pState->m_findAbstract == true)
	{
		getInBetweenLinksText(m_pState, m_pState->m_currentLink.m_index);
	}

	// Append META keywords, if any were found
	m_pState->m_text += m_pState->m_metaTags["keywords"];

	return true;
}

bool HtmlFilter::is_data_input_ok(DataInput input) const
{
	if ((input == DOCUMENT_DATA) ||
		(input == DOCUMENT_STRING))
	{
		return true;
	}

	return false;
}

bool HtmlFilter::set_property(Properties prop_name, const string &prop_value)
{
	if (prop_name == OPERATING_MODE)
	{
		if (prop_value == "view")
		{
			// This will ensure text is skipped
			++m_pState->m_skip;
		}
		else
		{
			// Attempt to find an abstract
			m_pState->m_findAbstract = true;
		}

		return true;
	}

	return false;
}

bool HtmlFilter::set_document_data(const char *data_ptr, unsigned int data_length)
{
	if ((data_ptr == NULL) ||
		(data_length == 0))
	{
		return false;
	}

	string html_doc(data_ptr, data_length);
	return set_document_string(html_doc);
}

bool HtmlFilter::set_document_string(const string &data_str)
{
	if (data_str.empty() == true)
	{
		return false;
	}

	rewind();

	// Try to cope with pages that have scripts or other rubbish prepended
	string::size_type htmlPos = data_str.find("<!DOCTYPE");
	if (htmlPos == string::npos)
	{
		htmlPos = data_str.find("<!doctype");
	}
	if ((htmlPos != string::npos) &&
		(htmlPos > 0))
	{
#ifdef DEBUG
		cout << "HtmlFilter::set_document_string: removed " << htmlPos << " characters" << endl;
#endif
		return parse_html(data_str.substr(htmlPos));
	}

	return parse_html(data_str);
}

bool HtmlFilter::set_document_file(const string &file_path)
{
	return false;
}

bool HtmlFilter::set_document_uri(const string &uri)
{
	return false;
}

bool HtmlFilter::has_documents(void) const
{
	if (m_pState != NULL)
	{
		return true;
	}

	return false;
}

bool HtmlFilter::next_document(void)
{
	if (m_pState != NULL)
	{
		bool foundCharset = false;

		m_metaData["title"] = m_pState->m_title;
		m_metaData["content"] = m_pState->m_text;
		m_metaData["abstract"] = m_pState->m_abstract;
		m_metaData["ipath"] = "";
		m_metaData["mimetype"] = "text/plain";
		for (map<string, string>::const_iterator iter = m_pState->m_metaTags.begin();
			iter != m_pState->m_metaTags.end(); ++iter)
		{
			if (iter->first == "charset")
			{
				foundCharset = true;
			}
			m_metaData[iter->first] = iter->second;
		}
		if (foundCharset == false)
		{
			m_metaData["charset"] = "utf-8";
		}
		// FIXME: shove the links in there somehow !

		delete m_pState;
		m_pState = NULL;

		return true;
	}

	return false;
}

bool HtmlFilter::skip_to_document(const string &ipath)
{
	if (ipath.empty() == true)
	{
		return next_document();
	}

	return false;
}

string HtmlFilter::get_error(void) const
{
	return m_error;
}
