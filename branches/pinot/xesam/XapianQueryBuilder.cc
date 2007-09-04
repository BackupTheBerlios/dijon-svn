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
#include <algorithm>

#include "XapianQueryBuilder.h"

using std::string;
using std::map;
using std::set;
using std::vector;
using std::for_each;
using std::cout;
using std::cerr;
using std::endl;

using namespace Dijon;

// A function object to lower case strings with for_each()
struct ToLower
{
	public:
		void operator()(char &c)
		{
			c = (char)tolower((int)c);
		}
};

XapianQueryBuilder::XapianQueryBuilder(Xapian::QueryParser &query_parser,
	const map<string, string> &field_to_prefix_mapping) :
	XesamQueryBuilder(),
	m_queryParser(query_parser),
	m_fieldMapping(field_to_prefix_mapping),
	m_fullQuery(),
	m_firstSelection(true)
{
}

XapianQueryBuilder::~XapianQueryBuilder()
{
}

void XapianQueryBuilder::on_query(const char *type)
{
#ifdef DEBUG
	cout << "XapianQueryBuilder::on_query: called" << endl;
#endif
}

void XapianQueryBuilder::on_selection(SelectionType selection,
	const set<string> &field_names,
	const vector<string> &field_values,
	SimpleType field_type,
	const Modifiers &modifiers)
{
#ifdef DEBUG
	cout << "XapianQueryBuilder::on_selection: called on "
		<< field_names.size() << " field(s)" << endl;
#endif

	if ((selection == None) ||
		(selection == RegExp))
	{
		// Ignore those selection types
#ifdef DEBUG
		cout << "XapianQueryBuilder::on_selection: ignoring selection type " << selection << endl;
#endif
		return;
	}

	if ((selection == Equals) ||
		(selection == Contains) ||
		(selection == FullText) ||
		(selection == InSet) ||
		(selection == Proximity))
	{
#ifdef DEBUG
		cout << "XapianQueryBuilder::on_selection: performing full text search" << endl;
#endif
	}
	else if (selection == Type)
	{
		// FIXME: handle this
#ifdef DEBUG
		cout << "XapianQueryBuilder::on_selection: no support for type yet" << endl;
#endif
		return;
	}
	else
	{
		// The rest deals with numerical values
		// FIXME: handle them
#ifdef DEBUG
		cout << "XapianQueryBuilder::on_selection: no support for numerical values yet" << endl;
#endif
		return;
	}

	Xapian::Query parsedQuery;
	// FIXME: we may not actually need all these flags
	unsigned int flags = Xapian::QueryParser::FLAG_BOOLEAN|Xapian::QueryParser::FLAG_PHRASE|
		Xapian::QueryParser::FLAG_LOVEHATE|Xapian::QueryParser::FLAG_BOOLEAN_ANY_CASE|
#if XAPIAN_MAJOR_VERSION==0
		Xapian::QueryParser::FLAG_WILDCARD;
#else
		Xapian::QueryParser::FLAG_WILDCARD|Xapian::QueryParser::FLAG_PURE_NOT;
#endif
	unsigned int valueCount = 0;

	for (vector<string>::const_iterator valueIter = field_values.begin();
		valueIter != field_values.end(); ++valueIter)
	{
		Xapian::Query thisQuery;
		string pseudoQueryString(*valueIter);

		// Does this apply to known field(s) ?
		for (set<string>::iterator nameIter = field_names.begin();
			nameIter != field_names.end(); ++nameIter)
		{
			string fieldName(*nameIter);

			// Lower-case field names
			for_each(fieldName.begin(), fieldName.end(), ToLower());

			if (fieldName == "mime")
			{
				// Special consideration apply : the QueryParser will split
				// application/pdf into two terms which we don't want
				thisQuery = m_queryParser.parse_query(string("type:") + pseudoQueryString, flags);
			}
			else
			{
				map<string, string>::const_iterator mapIter = m_fieldMapping.find(fieldName);
				if (mapIter != m_fieldMapping.end())
				{
					// Use this prefix as default
					thisQuery = m_queryParser.parse_query(pseudoQueryString, flags, mapIter->second);
				}
			}
		}

		// Was the query applied to a field ?
		if (thisQuery.empty() == true)
		{
			// No, parse as is
			thisQuery = m_queryParser.parse_query(pseudoQueryString, flags);
		}
#ifdef DEBUG
		cout << "XapianQueryBuilder::on_selection: query for this field value is " << thisQuery.get_description() << endl;
#endif

		// Are there multiple values ?
		if (valueCount == 0)
		{
			parsedQuery = thisQuery;
		}
		else
		{
			Xapian::Query::op valuesOp = Xapian::Query::OP_OR;

			// Is this proximity search ? Use the NEAR operator
			// If InSet, OR the values together
			if ((selection == Proximity) &&
				(valueCount < 2))
			{
				// FIXME: more than two values with OP_NEAR will throw an UnimplementedError
				// "Can't use NEAR/PHRASE with a subexpression containing NEAR or PHRASE"
				valuesOp = Xapian::Query::OP_NEAR;
			}
			parsedQuery = Xapian::Query(valuesOp, parsedQuery, thisQuery);
		}
#ifdef DEBUG
		cout << "XapianQueryBuilder::on_selection: query for this block is " << parsedQuery.get_description() << endl;
#endif
		++valueCount;
	}

#ifdef DEBUG
	cout << "XapianQueryBuilder::on_selection: collector is " << m_collector.m_collector << endl;
#endif
	if (m_firstSelection == true)
	{
		m_fullQuery = parsedQuery;
		m_firstSelection = false;
	}
	else
	{
		Xapian::Query::op queryOp = Xapian::Query::OP_AND;

		if (m_collector.m_collector == And)
		{
			if (m_collector.m_negate == true)
			{
				queryOp = Xapian::Query::OP_AND_NOT;
			}
		}
		else
		{
			queryOp = Xapian::Query::OP_OR;
		}

		Xapian::Query fullerQuery = Xapian::Query(queryOp, m_fullQuery, parsedQuery);
		m_fullQuery = fullerQuery;
	}
#ifdef DEBUG
	cout << "XapianQueryBuilder::on_selection: full query now " << m_fullQuery.get_description() << endl;
#endif
}

Xapian::Query XapianQueryBuilder::get_query(void) const
{
	return m_fullQuery;
}

