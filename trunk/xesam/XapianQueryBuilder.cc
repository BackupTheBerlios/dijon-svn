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

#ifdef HAVE_TIMEGM
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#include <time.h>
#undef _XOPEN_SOURCE
#else
#include <time.h>
#endif
#else
#include <time.h>
/* This comment and function are taken from Wget's mktime_from_utc()
   Converts struct tm to time_t, assuming the data in tm is UTC rather
   than local timezone.

   mktime is similar but assumes struct tm, also known as the
   "broken-down" form of time, is in local time zone.  mktime_from_utc
   uses mktime to make the conversion understanding that an offset
   will be introduced by the local time assumption.

   mktime_from_utc then measures the introduced offset by applying
   gmtime to the initial result and applying mktime to the resulting
   "broken-down" form.  The difference between the two mktime results
   is the measured offset which is then subtracted from the initial
   mktime result to yield a calendar time which is the value returned.

   tm_isdst in struct tm is set to 0 to force mktime to introduce a
   consistent offset (the non DST offset) since tm and tm+o might be
   on opposite sides of a DST change.

   Some implementations of mktime return -1 for the nonexistent
   localtime hour at the beginning of DST.  In this event, use
   mktime(tm - 1hr) + 3600.

   Schematically
     mktime(tm)   --> t+o
     gmtime(t+o)  --> tm+o
     mktime(tm+o) --> t+2o
     t+o - (t+2o - t+o) = t

   Note that glibc contains a function of the same purpose named
   `timegm' (reverse of gmtime).  But obviously, it is not universally
   available, and unfortunately it is not straightforwardly
   extractable for use here.  Perhaps configure should detect timegm
   and use it where available.

   Contributed by Roger Beeman <beeman@cisco.com>, with the help of
   Mark Baushke <mdb@cisco.com> and the rest of the Gurus at CISCO.
   Further improved by Roger with assistance from Edward J. Sabol
   based on input by Jamie Zawinski.  */
static time_t mktime_from_utc (struct tm *t)
{
  time_t tl, tb;
  struct tm *tg;

  tl = mktime (t);
  if (tl == -1)
    {
      t->tm_hour--;
      tl = mktime (t);
      if (tl == -1)
	return -1; /* can't deal with output from strptime */
      tl += 3600;
    }
  tg = gmtime (&tl);
  tg->tm_isdst = 0;
  tb = mktime (tg);
  if (tb == -1)
    {
      tg->tm_hour--;
      tb = mktime (tg);
      if (tb == -1)
	return -1; /* can't deal with output from gmtime */
      tb += 3600;
    }
  return (tl - (tb - tl));
}
#define timegm(T) mktime_from_utc(T)
#endif
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

// Converts from a ISO 8601 date/time
// Of the formats defined at http://www.w3.org/TR/NOTE-datetime, we only support
// the formats YYYY-MM-DD, YYY-MM-DDThh:mm:ss, and YYY-MM-DDThh:mm:ssTZD
// with a timezone or a time zone name
static string dateTimeToDateRange(const string &timestamp, SelectionType selection)
{
	char timeStr[64];
	struct tm timeTm;
	time_t gmTime = 0;
	bool inGMTime = false;

	if (timestamp.empty() == true)
	{
		return "";
	}

	// Initialize the structure
	timeTm.tm_sec = timeTm.tm_min = timeTm.tm_hour = timeTm.tm_mday = 0;
	timeTm.tm_mon = timeTm.tm_year = timeTm.tm_wday = timeTm.tm_yday = timeTm.tm_isdst = 0;

	char *remainder = strptime(timestamp.c_str(), "%Y-%m-%dT%H:%M:%S%z", &timeTm);
	if (remainder == NULL)
	{
		inGMTime = true;

		remainder = strptime(timestamp.c_str(), "%Y-%m-%dT%H:%M:%S", &timeTm);
		if (remainder == NULL)
		{
			remainder = strptime(timestamp.c_str(), "%Y-%m-%d", &timeTm);
			if (remainder == NULL)
			{
				return "";
			}
		}
	}

	// FIXME: only year, month and day can be used as date ranges
	if (snprintf(timeStr, 63, "%04d%02d%02d", timeTm.tm_year + 1900, timeTm.tm_mon + 1, timeTm.tm_mday) > 0)
	{
		string dateRange;

		if ((selection == LessThan) ||
			(selection == LessThanEquals))
		{
			dateRange = "19700101..";
			dateRange += timeStr;
		}
		else if ((selection == GreaterThan) ||
			(selection == GreaterThanEquals))
		{
			dateRange = timeStr;
			dateRange += "..20991231";
		}

		return dateRange;
	}

	return "";
}

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

void XapianQueryBuilder::on_query(const char *content, const char *storedAs)
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
	else if ((selection == LessThan) ||
		(selection == LessThanEquals) ||
		(selection == GreaterThan) ||
		(selection == GreaterThanEquals))
	{
		if (field_type != Date)
		{
#ifdef DEBUG
			cout << "XapianQueryBuilder::on_selection: numerical operators only work on dates" << endl;
#endif
			return;
		}
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
		bool ignoreFieldNames = false;

		if (field_type == Date)
		{
			pseudoQueryString = dateTimeToDateRange(*valueIter, selection);
			// FIXME: allow applying to specific fields
			ignoreFieldNames = true;
		}

		// Does this apply to known field(s) ?
		for (set<string>::iterator nameIter = field_names.begin();
			(ignoreFieldNames == false) && (nameIter != field_names.end()); ++nameIter)
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

