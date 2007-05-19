/*
 *  Copyright 2007 Fabrice Colin
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <iostream>

#include "XesamQLParser.h"

using std::string;
using std::set;
using std::map;
using std::cout;
using std::cerr;
using std::endl;

using namespace Dijon;

XesamQLParser::XesamQLParser() :
	m_depth(0),
	m_selection(None)
{
}

XesamQLParser::~XesamQLParser()
{
}

bool XesamQLParser::parse(const string &xesam_query, bool is_file,
	XesamQueryBuilder &query_builder)
{
	xmlParserInputBufferPtr pBuffer = NULL;
	bool parsedQuery = true;

	// Initialize the library and check for potential ABI mismatches
	LIBXML_TEST_VERSION

	// FIXME: encoding may not be UTF-8
	if (is_file == true)
	{
		pBuffer = xmlParserInputBufferCreateFilename(xesam_query.c_str(),
			XML_CHAR_ENCODING_UTF8);
	}
	else
	{
		pBuffer = xmlParserInputBufferCreateMem(xesam_query.c_str(),
			xesam_query.length(), XML_CHAR_ENCODING_UTF8);
	}
	if (pBuffer == NULL)
	{
		cerr << "XesamQLParser: couldn't create input buffer" << endl;
		return false;
	}

	xmlTextReaderPtr pReader = xmlNewTextReader(pBuffer, NULL);
	if (pReader != NULL)
	{
		// Reset everything
		m_depth = 0;
		m_collectorsByDepth.clear();
		m_selection = None;
		m_propertyName.clear();
		m_propertyValues.clear();
		m_propertyType = String;

		// First node
        	int readNode = xmlTextReaderRead(pReader);
		while (readNode == 1)
		{
			if (process_node(pReader, query_builder) == false)
			{
				parsedQuery = false;
				break;
			}

			// Next node
			readNode = xmlTextReaderRead(pReader);
		}

		xmlFreeTextReader(pReader);

		if (parsedQuery == false)
		{
			cerr << "XesamQLParser: failed to parse input" << endl;
		}
	}

	xmlFreeParserInputBuffer(pBuffer);

	return parsedQuery;
}

bool XesamQLParser::process_node(xmlTextReaderPtr reader,
	XesamQueryBuilder &query_builder)
{
	int type = xmlTextReaderNodeType(reader);
	int depth = xmlTextReaderDepth(reader);

#ifdef DEBUG
	cout << "XesamQLParser::process_node: " << depth << " " << type << endl;
#endif
	if (type == XML_READER_TYPE_END_ELEMENT)
	{
		if (m_depth > depth)
		{
			// Are we coming out of a selection type ?
			if (m_selection != None)
			{
				// Fire up a selection event
				query_builder.on_selection(m_selection, m_propertyName,
					m_propertyType, m_propertyValues);

				m_selection = None;

#ifdef DEBUG
				cout << "XesamQLParser::process_node: transitioning out of a selection type" << endl;
#endif
			}
			// ...or a collector type ?
			else
			{
				map<int, Collector>::iterator collIter = m_collectorsByDepth.find(depth);
				if (collIter != m_collectorsByDepth.end())
				{
					m_collectorsByDepth.erase(collIter);

					// Was this nested inside another collector ?
					collIter = m_collectorsByDepth.find(depth - 1);
					if (collIter != m_collectorsByDepth.end())
					{
						query_builder.set_collector(collIter->second);
					}

#ifdef DEBUG
					cout << "XesamQLParser::process_node: transitioning out of a collector type" << endl;
#endif
				}
			}
		}

		m_depth = depth;

		return true;
	}

	m_depth = depth;

	xmlChar *pLocalName = xmlTextReaderLocalName(reader);

	if ((type != XML_READER_TYPE_ELEMENT) ||
		(pLocalName == NULL))
	{
		return true;
	}

#ifdef DEBUG
	cout << "XesamQLParser::process_node: " << pLocalName << " " << xmlTextReaderHasValue(reader) << endl;
#endif
	if (depth == 0)
	{
		if (xmlStrncmp(pLocalName, BAD_CAST"request", 7) != 0)
		{
			cerr << "XesamQLParser: expected request, found " << pLocalName << endl;
			return false;
		}

		m_collectorsByDepth.clear();
	}
	else if (depth == 1)
	{
		m_selection = None;

		if (xmlStrncmp(pLocalName, BAD_CAST"userQuery", 9) == 0)
		{
			string userQueryValue;

			if (process_text_node(reader, userQueryValue) == false)
			{
				return false;
			}

			query_builder.on_userQuery(userQueryValue.c_str());
		}
		else if (xmlStrncmp(pLocalName, BAD_CAST"query", 5) == 0)
		{
			query_builder.on_query((const char *)xmlTextReaderGetAttribute(reader, BAD_CAST"type"));
		}
	}
	else if (depth == 2)
	{
		// Collectible types
		// Collector type, with at least two children
		// Selection types
		if ((is_collector_type(pLocalName, reader, query_builder) == false) &&
			(is_selection_type(pLocalName) == false))
		{
			cerr << "XesamQLParser: expected a collector or a selection type, found " << pLocalName << endl;
			return false;
		}
	}
	// Depth 3, or deeper
	else
	{
		if (m_selection != None)
		{
			string propertyValue;
			SimpleType propType = m_propertyType;

			// Property
			if (xmlStrncmp(pLocalName, BAD_CAST"property", 8) == 0)
			{
				xmlChar *pAttrValue = xmlTextReaderGetAttribute(reader, BAD_CAST"name");

				if (pAttrValue != NULL)
				{
					m_propertyName = (const char *)pAttrValue;
				}

				// Nothing else to do here
				return true;
			}
			// Simple type
			else if (xmlStrncmp(pLocalName, BAD_CAST"string", 6) == 0)
			{
				xmlChar *pAttrValue = xmlTextReaderGetAttribute(reader, BAD_CAST"phrase");

				// FIXME: support "caseSensitive", "diacriticSensitive", "slack", "ordered"
				// "enableStemming", "language", and "fuzzy"
				if ((pAttrValue != NULL) &&
					(xmlStrncmp(pAttrValue, BAD_CAST"true", 4) == 0))
				{
					m_propertyType = Phrase;
				}
				else
				{
					m_propertyType = String;
				}
			}
			else if (xmlStrncmp(pLocalName, BAD_CAST"integer", 7) == 0)
			{
				m_propertyType = Integer;
			}
			else if (xmlStrncmp(pLocalName, BAD_CAST"date", 4) == 0)
			{
				m_propertyType = Date;
			}
			else if (xmlStrncmp(pLocalName, BAD_CAST"boolean", 7) == 0)
			{
				m_propertyType = Boolean;
			}
			else if (xmlStrncmp(pLocalName, BAD_CAST"float", 5) == 0)
			{
				m_propertyType = Float;
			}
			else
			{
				cerr << "XesamQLParser: expected a property or simple type, found " << pLocalName << endl;
				return false;
			}

			// More than one value is only possible with InSet selection
			// and all values should be of the same type
			if ((m_selection != InSet) &&
				(m_propertyValues.empty() == false))
			{
				cerr << "XesamQLParser: a simple type was already provided" << endl;
				return false;
			}
			if ((m_selection == InSet) &&
				(propType != m_propertyType))
			{
				cerr << "XesamQLParser: the same simple type should be used throughout a set" << endl;
				return false;
			}

			if (process_text_node(reader, propertyValue) == false)
			{
				return false;
			}

			if (propertyValue.empty() == false)
			{
				m_propertyValues.insert(propertyValue);
			}
#ifdef DEBUG
			else cout << "XesamQLParser::process_node: simple type has no value" << endl;
#endif
		}
		else
		{
			if ((is_collector_type(pLocalName, reader, query_builder) == false) &&
				(is_selection_type(pLocalName) == false))
			{
				cerr << "XesamQLParser: expected a collector or a selection type, found " << pLocalName << endl;
				return false;
			}
		}
	}

	return true;
}

bool XesamQLParser::process_text_node(xmlTextReaderPtr reader, string &value)
{
	// Read the next node
	xmlTextReaderRead(reader);

	// Is it a text node ?
	int type = xmlTextReaderNodeType(reader);
	if (type == XML_READER_TYPE_TEXT)
	{
		const xmlChar *pValue = xmlTextReaderConstValue(reader);

		if (pValue != NULL)
		{
			value = (const char *)pValue;
			return true;
		}
	}

	cerr << "XesamQLParser: expected a text node, found a node of type " << type << endl;

	return false;
}

bool XesamQLParser::is_collector_type(xmlChar *local_name,
	xmlTextReaderPtr reader,
	XesamQueryBuilder &query_builder)
{
	Collector collector;

	collector.m_collector = And;
	collector.m_boost = 0.0;
	collector.m_negate = false;

	m_selection = None;

	// Collector type, with at least two children
	if (xmlStrncmp(local_name, BAD_CAST"and", 3) == 0)
	{
		collector.m_collector = And;
	}
	else if (xmlStrncmp(local_name, BAD_CAST"or", 2) == 0)
	{
		collector.m_collector = Or;
	}
	else
	{
		return false;
	}

	if (xmlTextReaderHasAttributes(reader) == 1)
	{
		const xmlChar *pBoost = xmlTextReaderGetAttribute(reader, BAD_CAST"boost");
		if (pBoost != NULL)
		{
			collector.m_boost = (float)atof((const char *)pBoost);
		}
		
		const xmlChar *pNegate = xmlTextReaderGetAttribute(reader, BAD_CAST"negate");
		if ((pNegate != NULL) &&
			(xmlStrncmp(pNegate, BAD_CAST"true", 4) == 0))
		{
			collector.m_negate = true;
		}
	}

	m_collectorsByDepth[m_depth] = collector;
	query_builder.set_collector(collector);

	return true;
}

bool XesamQLParser::is_selection_type(xmlChar *local_name)
{
	m_propertyValues.clear();

	// Selection types
	if (xmlStrncmp(local_name, BAD_CAST"equals", 6) == 0)
	{
		m_selection = Equals;
	}
	else if (xmlStrncmp(local_name, BAD_CAST"contains", 8) == 0)
	{
		m_selection = Contains;
	}
	else if (xmlStrncmp(local_name, BAD_CAST"lessThan", 8) == 0)
	{
		m_selection = LessThan;
	}
	else if (xmlStrncmp(local_name, BAD_CAST"lessThanEquals", 14) == 0)
	{
		m_selection = LessThanEquals;
	}
	else if (xmlStrncmp(local_name, BAD_CAST"greaterThan", 11) == 0)
	{
		m_selection = GreaterThan;
	}
	else if (xmlStrncmp(local_name, BAD_CAST"greaterThanEquals", 17) == 0)
	{
		m_selection = GreaterThanEquals;
	}
	else if (xmlStrncmp(local_name, BAD_CAST"startsWith", 10) == 0)
	{
		m_selection = StartsWith; 
	}
	else if (xmlStrncmp(local_name, BAD_CAST"inSet", 5) == 0)
	{
		m_selection = InSet; 
	}
	else if (xmlStrncmp(local_name, BAD_CAST"fullText", 8) == 0)
	{
		m_selection = FullText; 
	}
	// Extended selection types
	else if (xmlStrncmp(local_name, BAD_CAST"regExp", 6) == 0)
	{
		m_selection = RegExp; 
	}
	else if (xmlStrncmp(local_name, BAD_CAST"proximity", 9) == 0)
	{
		m_selection = Proximity; 
	}
	else
	{
		return false;
	}

	// FIXME: check this is nested in a collector type
	return true;
}

