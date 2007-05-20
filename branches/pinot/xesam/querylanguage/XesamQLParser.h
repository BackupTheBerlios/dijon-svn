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

namespace Dijon
{
    typedef enum { And, Or } CollectorType;

    struct Collector
    {
	CollectorType m_collector;
	bool m_negate;
	float m_boost;
    };

    typedef enum { None, Equals, Contains, LessThan, LessThanEquals, GreaterThan,
	GreaterThanEquals, StartsWith, InSet, FullText, RegExp, Proximity } SelectionType;

    typedef enum { String, Integer, Date, Boolean, Float } SimpleType;

    struct Modifiers
    {
	bool m_phrase;
	bool m_caseSensitive;
	bool m_diacriticSensitive;
	int m_slack;
	bool m_ordered;
	bool m_enableStemming;
	std::string m_language;
	float m_fuzzy;
    };

    /// Interface implemented by all query builders.
    class XesamQueryBuilder
    {
    public:
	/// Builds a query builder.
	XesamQueryBuilder()
	{
		m_collector.m_collector = And;
		m_collector.m_negate = false;
		m_collector.m_boost = 0.0;
	};

	virtual ~XesamQueryBuilder()
	{
	};

	void set_collector(const Collector &collector)
	{
		m_collector.m_collector = collector.m_collector;
		m_collector.m_negate = collector.m_negate;
		m_collector.m_boost = collector.m_boost;
	};

	virtual void on_user_query(const char *value) = 0;

	virtual void on_query(const char *type) = 0;

	virtual void on_selection(SelectionType selection,
		const std::set<std::string> &property_name,
		const std::set<std::string> &property_values,
		SimpleType property_type,
		const Modifiers &modifiers) = 0;

    protected:
	Collector m_collector;

    private:
	/// XesamQueryBuilder objects cannot be copied.
	XesamQueryBuilder(const XesamQueryBuilder &other);
	/// XesamQueryBuilder objects cannot be copied.
	XesamQueryBuilder& operator=(const XesamQueryBuilder &other);

    };

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
	std::set<std::string> m_propertyNames;
	std::set<std::string> m_propertyValues;
	SimpleType m_propertyType;
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

	bool is_selection_type(xmlChar *local_name);

	void get_modifiers(xmlTextReaderPtr reader);

    private:
	/// XesamQLParser objects cannot be copied.
	XesamQLParser(const XesamQLParser &other);
	/// XesamQLParser objects cannot be copied.
	XesamQLParser& operator=(const XesamQLParser &other);

    };
}

#endif // _DIJON_XESAMQLPARSER_H
