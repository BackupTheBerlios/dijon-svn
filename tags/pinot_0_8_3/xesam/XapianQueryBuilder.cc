/*
 *  Copyright 2007,2008 Fabrice Colin
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
#include <sstream>
#include <algorithm>

#include "XesamLog.h"
#ifdef HAVE_BOOST_SPIRIT
#include "XesamULParser.h"
#endif
#include "XapianQueryBuilder.h"

using std::string;
using std::stringstream;
using std::map;
using std::set;
using std::vector;
using std::for_each;

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

/// Trims spaces at the start and end of a string.
unsigned int trimSpaces(string &str)
{
	string::size_type pos = 0;
	unsigned int count = 0;

	while ((str.empty() == false) && (pos < str.length()))
	{
		if (isspace(str[pos]) == 0)
		{
			++pos;
			break;
		}

		str.erase(pos, 1);
		++count;
	}

	for (pos = str.length() - 1;
		(str.empty() == false) && (pos >= 0); --pos)
	{
		if (isspace(str[pos]) == 0)
		{
			break;
		}

		str.erase(pos, 1);
		++count;
	}

	return count;
}

// Converts to a size range
static string sizeToSizeRange(const string &size, SelectionType selection,
	const string &min, const string &max, const string &suffix = string(""))
{
	string sizeRange;

	if (size.empty() == true)
	{
		return "";
	}

	if ((selection == LessThan) ||
		(selection == LessThanEquals))
	{
		sizeRange = min;
		sizeRange += "..";
		sizeRange += size;
	}
	else if ((selection == GreaterThan) ||
		(selection == GreaterThanEquals))
	{
		sizeRange = size;
		sizeRange += "..";
		sizeRange += max;
	}
	sizeRange += suffix;
	XESAM_LOG_DEBUG("XapianQueryBuilder::sizeToSizeRange", sizeRange);

	return sizeRange;
}

// Converts from a ISO 8601 date/time
// Of the formats defined at http://www.w3.org/TR/NOTE-datetime, we only support
// the formats YYYY-MM-DD, YYY-MM-DDThh:mm:ss, and YYY-MM-DDThh:mm:ssTZD
// with a timezone or a time zone name
static string dateToDateAndTimeRanges(const string &timestamp, SelectionType selection)
{
	string ranges;
	char timeStr[64];
	struct tm timeTm;
	time_t gmTime = 0;
	bool inGMTime = false, hasTime = true;

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

			// No time specified
			hasTime = false;
		}
	}

	// Year, month and day make a date range
	if (snprintf(timeStr, 63, "%04d%02d%02d", timeTm.tm_year + 1900, timeTm.tm_mon + 1, timeTm.tm_mday) > 0)
	{
		ranges += sizeToSizeRange(timeStr, selection, "19700101", "20991231", "");
		ranges += " ";
	}
	// And hour, minute and second make a time range
	if ((hasTime == true) &&
		(snprintf(timeStr, 63, "%02d%02d%02d", timeTm.tm_hour, timeTm.tm_min, timeTm.tm_sec) > 0))
	{
		//  205111..235959
		ranges += sizeToSizeRange(timeStr, selection, "000000", "235959", "");
		ranges += " ";
	}


	return ranges;
}

static void extractClasses(const string &category, set<string> &xesamClasses)
{
	// It may be a single class or a comma separated list
	string::size_type categoryLength = category.length();
	string::size_type startPos = 0;

	string::size_type commaPos = category.find(",", startPos);
	while (commaPos != string::npos)
	{
		xesamClasses.insert(category.substr(startPos, commaPos - startPos));
		string::size_type prevStartPos = commaPos + 1;

		if (commaPos < categoryLength - 1)
		{
			startPos = prevStartPos;
			commaPos = category.find(",", startPos);
		}
		else
		{
			startPos = categoryLength;
			commaPos = string::npos;
		}
	}

	if (startPos < categoryLength)
	{
		xesamClasses.insert(category.substr(startPos));
	}
}

// Converts Xesam classes to class filters
static string classesToFilters(const set<string> &xesamClasses)
{
	string filters;

	for (set<string>::const_iterator classIter = xesamClasses.begin();
		classIter != xesamClasses.end(); ++classIter)
	{
		string className(*classIter);

		for_each(className.begin(),className.end(), ToLower());
		trimSpaces(className);

		XESAM_LOG_DEBUG("XapianQueryBuilder::classesToFilters", className);
		/* Only the most obvious classes are supported for the time being. For reference, the full list is
		 * Alarm
		 * Archive
		 * ArchiveItem
		 * Audio
		 * AudioList
		 * Contact
		 * Content
		 * DataObject
		 * DeletedFile
		 * Document
		 * Email
		 * EmailAttachment
		 * Event
		 * File
		 * FileSystem
		 * Folder
		 * FreeBusy
		 * Image
		 * IMMessage
		 * Journal
		 * MailboxItem
		 * Media
		 * MediaList
		 * Message
		 * Partition
		 * Photo
		 * PIM
		 * Presentation
		 * Project
		 * Software
		 * Source
		 * SourceCode
		 * Spreadsheet
		 * Task
		 * TextDocument
		 * Video
		 * Visual
		 */
		if (className == "xesam:audio")
		{
			filters += "class:audio ";
		}
		else if ((className == "xesam:email") ||
			(className == "xesam:message"))
		{
			filters += "(type:application/mbox or type:text/x-mail) ";
		}
		else if (className == "xesam:folder")
		{
			filters += "type:x-directory/normal ";
		}
		else if (className == "xesam:video")
		{
			filters += "class:video ";
		}
	}

	return filters;
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

void XapianQueryBuilder::on_user_query(const string &user_query)
{
	XESAM_LOG_DEBUG("XapianQueryBuilder::on_user_query", "called");
	if (user_query.empty() == false)
	{
#ifdef HAVE_BOOST_SPIRIT
		XesamULParser ulParser;
		string xesamULQuery(user_query);

		ulParser.parse(xesamULQuery, *this);
#else
	XESAM_LOG_ERROR("XapianQueryBuilder::on_user_query", "no support for Xesam User Language queries");
#endif
	}
}

void XapianQueryBuilder::on_query(const string &content, const string &source)
{
	XESAM_LOG_DEBUG("XapianQueryBuilder::on_query", "called");
	m_firstSelection = true;

	if (content.empty() == false)
	{
		set<string> xesamClasses;

		extractClasses(content, xesamClasses);
		m_contentClassFilter = classesToFilters(xesamClasses);
	}
	if (source.empty() == false)
	{
		// FIXME: handle this
		XESAM_LOG_DEBUG("XapianQueryBuilder::on_query", "no support for source yet");
	}
}

void XapianQueryBuilder::on_selection(SelectionType selection,
	const set<string> &field_names,
	const vector<string> &field_values,
	SimpleType field_type,
	const Modifiers &modifiers)
{
	Xapian::Query parsedQuery;
	stringstream msg;
	// FIXME: we may not actually need all these flags
	unsigned int flags = Xapian::QueryParser::FLAG_BOOLEAN|Xapian::QueryParser::FLAG_PHRASE|
		Xapian::QueryParser::FLAG_LOVEHATE|Xapian::QueryParser::FLAG_BOOLEAN_ANY_CASE|
#if XAPIAN_MAJOR_VERSION==0
		Xapian::QueryParser::FLAG_WILDCARD;
#else
		Xapian::QueryParser::FLAG_WILDCARD|Xapian::QueryParser::FLAG_PURE_NOT;
#endif
	unsigned int valueCount = 0;

	msg << "called on " << field_names.size() << " field(s)";
	XESAM_LOG_DEBUG("XapianQueryBuilder::on_selection", msg.str());
	if ((selection == None) ||
		(selection == RegExp))
	{
		// Ignore those selection types
		msg.str("");
		msg << "ignoring selection type " << selection;
		XESAM_LOG_DEBUG("XapianQueryBuilder::on_selection", msg.str());
		return;
	}

	if ((selection == Equals) ||
		(selection == Contains) ||
		(selection == FullText) ||
		(selection == InSet) ||
		(selection == Proximity))
	{
		XESAM_LOG_DEBUG("XapianQueryBuilder::on_selection", "performing full text search");
	}
	else if (selection == Category)
	{
		set<string> xesamClasses;

		if (modifiers.m_content.empty() == false)
		{
			extractClasses(modifiers.m_content, xesamClasses);

			string classFilters(classesToFilters(xesamClasses));
			if (classFilters.empty() == false)
			{
				parsedQuery = m_queryParser.parse_query(classFilters);
			}
		}

		if (modifiers.m_source.empty() == false)
		{
			// FIXME: handle this
			XESAM_LOG_DEBUG("XapianQueryBuilder::on_selection", "no support for category source yet");
		}
	}
	else if ((selection == LessThan) ||
		(selection == LessThanEquals) ||
		(selection == GreaterThan) ||
		(selection == GreaterThanEquals))
	{
		if ((field_type != Integer) &&
			(field_type != Date))
		{
			XESAM_LOG_DEBUG("XapianQueryBuilder::on_selection", "numerical operators only work on integers and dates");
			return;
		}
	}
	else
	{
		// The rest deals with numerical values
		// FIXME: handle them
		XESAM_LOG_DEBUG("XapianQueryBuilder::on_selection", "no support for numerical values yet");
		return;
	}

	// Is this proximity search ? Use the NEAR operator
	if (selection == Proximity)
	{
		msg.str("");
		msg << "proximity search on " << field_values.size() << " values";
		XESAM_LOG_DEBUG("XapianQueryBuilder::on_selection", msg.str());
		parsedQuery = Xapian::Query(Xapian::Query::OP_NEAR, field_values.begin(), field_values.end());
	}
	else for (vector<string>::const_iterator valueIter = field_values.begin();
		valueIter != field_values.end(); ++valueIter)
	{
		Xapian::Query thisQuery;
		string pseudoQueryString(*valueIter);
		string defaultPrefix;

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
				string withFilter(string("type:") + pseudoQueryString);

				pseudoQueryString = withFilter;
			}
			else if (fieldName == "file:size")
			{
				if (field_type == Integer)
				{
					// FIXME: we can go as far as a double goes
					pseudoQueryString = sizeToSizeRange(*valueIter, selection,
						"0", "1000000000000", "b");
				}
			}
			else if (fieldName.find("date") != string::npos)
			{
				if (field_type == Date)
				{
					pseudoQueryString = dateToDateAndTimeRanges(*valueIter, selection);
				}
			}
			else
			{
				map<string, string>::const_iterator mapIter = m_fieldMapping.find(fieldName);
				if (mapIter != m_fieldMapping.end())
				{
					// Use this prefix as default
					defaultPrefix = mapIter->second;
				}
				else
				{
					continue;
				}
			}

			// FIXME: does each value alway apply to one field only ?
			break;
		}

		if (pseudoQueryString.empty() == true)
		{
			continue;
		}

		thisQuery = m_queryParser.parse_query(pseudoQueryString, flags, defaultPrefix);
		XESAM_LOG_DEBUG("XapianQueryBuilder::on_selection", "query for this field value is "
			+ pseudoQueryString + "=" + thisQuery.get_description());

		// Are there multiple values ?
		if (valueCount == 0)
		{
			parsedQuery = thisQuery;
		}
		else
		{
			parsedQuery = Xapian::Query(Xapian::Query::OP_OR, parsedQuery, thisQuery);
		}

		msg.str("");
		msg << "query for this block is " << parsedQuery.get_description();
		XESAM_LOG_DEBUG("XapianQueryBuilder::on_selection", msg.str());

		++valueCount;
	}

	// Did we manage to parse something ?
	if (parsedQuery.empty() == true)
	{
		XESAM_LOG_DEBUG("XapianQueryBuilder::on_selection", "skipping empty query");
		return;
	}

	msg.str("");
	msg << "collector is " << m_collector.m_collector;
	XESAM_LOG_DEBUG("XapianQueryBuilder::on_selection", msg.str());

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
			if ((m_collector.m_negate == true) ||
				(modifiers.m_negate == true))
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
	XESAM_LOG_DEBUG("XapianQueryBuilder::on_selection", "full query now " + m_fullQuery.get_description());
}

Xapian::Query XapianQueryBuilder::get_query(void)
{
	// Apply a filter ?
	if (m_contentClassFilter.empty() == false)
	{
		Xapian::Query filterQuery = m_queryParser.parse_query(m_contentClassFilter,
			Xapian::QueryParser::FLAG_BOOLEAN|Xapian::QueryParser::FLAG_BOOLEAN_ANY_CASE);
		m_fullQuery = Xapian::Query(Xapian::Query::OP_FILTER, m_fullQuery, filterQuery);
		XESAM_LOG_DEBUG("XapianQueryBuilder::on_selection", "full query now " + m_fullQuery.get_description());

		m_contentClassFilter.clear();
	}

	return m_fullQuery;
}

