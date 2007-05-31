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

#include "XapianQueryBuilder.h"

using std::string;
using std::set;
using std::cout;
using std::cerr;
using std::endl;

using namespace Dijon;

XapianQueryBuilder::XapianQueryBuilder(Xapian::QueryParser &queryParser) :
	XesamQueryBuilder(),
	m_queryParser(queryParser),
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
	cout << "XapianQueryBuilder::on_query: called";
	if (type != NULL)
	{
		cout << " with " << type;
	}
	cout << endl;
#endif
}

void XapianQueryBuilder::on_selection(SelectionType selection,
	const set<string> &field_names,
	const set<string> &field_values,
	SimpleType field_type,
	const Modifiers &modifiers)
{
#ifdef DEBUG
	cout << "XapianQueryBuilder::on_selection: called on "
		<< field_names.size() << " fields" << endl;
#endif

	if ((selection == None) ||
		(selection == RegExp))
	{
		// Ignore those selection types
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
	else
	{
		// The rest deals with numerical values
		// FIXME: handle them
#ifdef DEBUG
		cout << "XapianQueryBuilder::on_selection: no support for numerical values yet" << endl;
#endif
		return;
	}

#ifdef DEBUG
	cout << "XapianQueryBuilder::on_selection: field values are " << endl;
#endif
	Xapian::Query parsedQuery;
	bool firstValue = true;

	for (set<string>::iterator valueIter = field_values.begin();
		valueIter != field_values.end(); ++valueIter)
	{
		string fieldValue(*valueIter);

		// Let the QueryParser do the heavy lifting
		// FIXME: we don't actually need to activate all these flags
		Xapian::Query thisQuery = m_queryParser.parse_query(fieldValue,
			Xapian::QueryParser::FLAG_BOOLEAN|Xapian::QueryParser::FLAG_PHRASE|
			Xapian::QueryParser::FLAG_LOVEHATE|Xapian::QueryParser::FLAG_BOOLEAN_ANY_CASE|
#if XAPIAN_MAJOR_VERSION==0
			Xapian::QueryParser::FLAG_WILDCARD
#else
			Xapian::QueryParser::FLAG_WILDCARD|Xapian::QueryParser::FLAG_PURE_NOT
#endif
			);
		if (firstValue == true)
		{
			parsedQuery = thisQuery;
			firstValue = false;
		}
		else
		{
			// Only InSet will have multiple values, and they should be OR'ed
			parsedQuery = Xapian::Query(Xapian::Query::OP_OR,
				parsedQuery, thisQuery);
		}
#ifdef DEBUG
		cout << " " << *valueIter;
#endif
	}
#ifdef DEBUG
 	cout << endl;
#endif

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
	cout << "XapianQueryBuilder::on_selection: query now " << m_fullQuery.get_description() << endl;
#endif
}

Xapian::Query XapianQueryBuilder::get_query(void) const
{
	return m_fullQuery;
}

