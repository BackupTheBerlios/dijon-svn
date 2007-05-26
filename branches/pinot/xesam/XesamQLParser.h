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

#ifndef _DIJON_XESAMQLPARSER_H
#define _DIJON_XESAMQLPARSER_H

#include <libxml/xmlreader.h>
#include <string>
#include <set>
#include <map>

#include "XesamQueryBuilder.h"

namespace Dijon
{
    /// Xesam Query Language parser.
    class XesamQLParser
    {
    public:
	/// Builds a parser for the Xesam Query Language.
	XesamQLParser();
	virtual ~XesamQLParser();

	bool parse(const std::string &xesam_query,
		XesamQueryBuilder &query_builder);

	bool parse_file(const std::string &xesam_query_file,
		XesamQueryBuilder &query_builder);

    protected:
	int m_depth;
	std::map<int, Collector> m_collectorsByDepth;
	SelectionType m_selection;
	std::set<std::string> m_fieldNames;
	std::set<std::string> m_fieldValues;
	SimpleType m_fieldType;
	Modifiers m_modifiers;

	bool parse_input(xmlParserInputBufferPtr,
		XesamQueryBuilder &query_builder);

	bool process_node(xmlTextReaderPtr reader,
		XesamQueryBuilder &query_builder);

	bool process_text_node(xmlTextReaderPtr reader,
		std::string &value);

	bool is_collector_type(xmlChar *local_name,
		xmlTextReaderPtr reader,
		XesamQueryBuilder &query_builder);

	bool is_selection_type(xmlChar *local_name,
		xmlTextReaderPtr reader);

	void get_modifiers(xmlTextReaderPtr reader);

    private:
	/// XesamQLParser objects cannot be copied.
	XesamQLParser(const XesamQLParser &other);
	/// XesamQLParser objects cannot be copied.
	XesamQLParser& operator=(const XesamQLParser &other);

    };
}

#endif // _DIJON_XESAMQLPARSER_H
