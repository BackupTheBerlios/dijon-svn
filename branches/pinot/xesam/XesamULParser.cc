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
#include <boost/spirit/core.hpp>
#include <boost/spirit/actor/push_back_actor.hpp>
#include <boost/spirit/actor/insert_at_actor.hpp>
#include <boost/spirit/utility/chset.hpp>
#include <boost/spirit/utility/confix.hpp>

#include "XesamULParser.h"

using std::cout;
using std::endl;
using std::string;
using std::set;
using std::map;
using std::exception;
using namespace boost::spirit;

using namespace Dijon;

static XesamQueryBuilder *g_pQueryBuilder = NULL;
static bool g_foundCollector = false;
static bool g_foundPOM = false;
static bool g_negate = false;

/// Semantic action for the collector rule.
void set_collector_action(char const *first, char const *last)
{
	string str(first, last);
	Collector collector(And, false, 0.0);

#ifdef DEBUG
	cout << "set_collector_action: found " << str << endl;
#endif
	if ((str == "or") ||
		(str == "Or") ||
		(str == "oR") ||
		(str == "OR") ||
		(str == "||"))
	{
		collector.m_collector = Or;
	}

	g_pQueryBuilder->set_collector(collector);

	g_foundCollector = true;
	g_foundPOM = false;
	g_negate = false;
}

/// Semantic action for the plus_or_minus rule.
void on_pom_action(char const *first, char const *last)
{
	string str(first, last);

#ifdef DEBUG
	cout << "on_pom_action: found " << str << endl;
#endif
	if (str == "-")
	{
		g_negate = true;
	}
	else
	{
		g_negate = false;
	}
	g_foundPOM = true;
}

/// Semantic action for the selection rule.
void on_selection_action(char const *first, char const *last)
{
	string str(first, last);

#ifdef DEBUG
	cout << "on_selection_action: found " << str << endl;
#endif
	// FIXME: parse str, find the SelectionType and SimpleType, and call on_selection()
}

/// Semantic action for the phrase rule.
void on_phrase_action(char const *first, char const *last)
{
	string str(first, last);
	SelectionType selectionType = FullText;
	SimpleType type = String; 
	Modifiers modifiers;

#ifdef DEBUG
	cout << "on_phrase_action: found " << str << endl;
#endif
	if (g_foundCollector == true)
	{
		g_foundCollector = false;
	}
	else
	{
		Collector collector(And, false, 0.0);

		// No collector was found between this and the previous phrase
		// so use And, the default collector
		g_pQueryBuilder->set_collector(collector);
	}
	if (g_foundPOM == true)
	{
		g_foundPOM = false;
	}
	else
	{
		// No POM was found, revert to +
		g_negate = false;
	}
	// FIXME: parse the modifiers, find the SelectionType and call on_selection()

	if (str.empty() == true)
	{
		return;
	}

	// These only apply to String
	modifiers.m_negate = g_negate;
	modifiers.m_boost = 0.0;
	modifiers.m_phrase = true;
	modifiers.m_caseSensitive = false;
	modifiers.m_diacriticSensitive = true;
	modifiers.m_slack = 0;
	modifiers.m_ordered = false;
	modifiers.m_enableStemming = true;
	modifiers.m_language.clear();
	modifiers.m_fuzzy = 0.0;
	modifiers.m_distance = 0;

	if (str[0] == '"')
	{
		string::size_type closingQuote = str.find_last_of("\"");
		if (closingQuote == 0)
		{
			return;
		}

		// Are there modifiers ?
		if (closingQuote < str.length() - 1)
		{
			string modiferString(str.substr(closingQuote + 1));

			/*
			c phrase Case sensitive
			C phrase Case insensitive
			d phrase Diacritic sensitive
			D phrase Diacritic insensitive
			l phrase Don't do stemming
			L phrase Do stemming
			e phrase Exact match. Short for cdl
			f phrase Fuzzy search
			b phrase Boost. Any match on the phrase should boost the score of the hit significantly
			p words Proximity search. The words in the string should appear close to each other (suggested default: 10)
			s words Sloppy search. Not all words need to match (suggested default slack: floor(sqrt(num_words)))
			w words Word based matching. Match words inside other strings if there is some meaning full word separation. Fx "case"w matches CamelCase
			o words Ordered words. The words in the string should appear in order, but not necessarily next to each other
			r special The phrase is a regular expression 
			*/
			for (unsigned int mod = 0; mod < modiferString.length(); ++mod)
			{
				switch (modiferString[mod])
				{
					case 'c':
						modifiers.m_phrase = true;
						modifiers.m_caseSensitive = true;
						break;
					case 'C':
						modifiers.m_phrase = true;
						modifiers.m_caseSensitive = false;
						break;
					case 'd':
						modifiers.m_phrase = true;
						modifiers.m_diacriticSensitive = true;
						break;
					case 'D':
						modifiers.m_phrase = true;
						modifiers.m_diacriticSensitive = false;
						break;
					case 'l':
						modifiers.m_phrase = true;
						modifiers.m_enableStemming = false;
						break;
					case 'L':
						modifiers.m_phrase = true;
						modifiers.m_enableStemming = true;
						break;
					case 'e':
						modifiers.m_phrase = true;
						modifiers.m_caseSensitive = true;
						modifiers.m_diacriticSensitive = true;
						modifiers.m_enableStemming = false;
						break;
					case 'f':
						modifiers.m_phrase = true;
						modifiers.m_fuzzy = 1.0;
						break;
					case 'b':
						modifiers.m_phrase = true;
						modifiers.m_boost = 1.0;
						break;
					case 'p':
						selectionType = Proximity;
						modifiers.m_phrase = false;
						modifiers.m_distance = 10;
						break;
					case 's':
						modifiers.m_phrase = false;
						// FIXME: default to floor(sqrt(num_words)
						modifiers.m_slack = 1;
						break;
					case 'w':
						selectionType = Contains;
						modifiers.m_phrase = false;
						break;
					case 'o':
						modifiers.m_phrase = false;
						modifiers.m_ordered = true;
						break;
					case 'r':
						selectionType = RegExp;
						modifiers.m_phrase = false;
						break;
					default:
						break;
				}
			}
		}
	}

	set<string> fieldNames, fieldValues;

	fieldValues.insert(str);
	g_pQueryBuilder->on_selection(selectionType,
		fieldNames, fieldValues, type, modifiers);
}

/// A grammar that skips spaces.
struct skip_grammar : public grammar<skip_grammar>
{
	template <typename ScannerT>
	struct definition
	{
		definition(skip_grammar const &self)
		{
			// Skip all spaces
			skip = space_p;
		}
	
		rule<ScannerT> skip;
	
		rule<ScannerT> const& start() const
		{
			return skip;
		}
	};
};

/// A grammar for the Xesam User Language.
struct xesam_ul_grammar : public grammar<xesam_ul_grammar>
{
	xesam_ul_grammar()
	{
	}

	template <typename ScannerT>
	struct definition
	{
		definition(xesam_ul_grammar const &self)
		{
			// Start
			// Statements are separated by collectors
			ul_query = statement >> *(collector[&set_collector_action] >> statement);

			// Positive or negative ?
			plus = ch_p('+');

			minus = ch_p('-');

			plus_or_minus = plus |
				minus;

			// Field names are alphabetical characters only
			field_name = *(range_p('a','z') | range_p('A','Z'));

			// Valid relations
			relation = ch_p('=') |
				ch_p(':') |
				str_p("<=") |
				str_p(">=") |
				ch_p('<') |
				ch_p('>');

			// A value that's between double-quotes
			quoted_value = lexeme_d[*(anychar_p - chset<>("\"\n"))];

			// A value that's not between double-quotes
			value = lexeme_d[*(anychar_p - chset<>("\" \n"))];

			value_end = lexeme_d[*(anychar_p - chset<>("\"\n"))];

			// Field values
			field_value = ch_p('"') >> quoted_value >> ch_p('"') | value;

			// Selection
			selection = field_name >> relation >> field_value;

			// Quoted phrases end with modifiers directly after the closing double-quote
			quoted_phrase = ch_p('"') >> quoted_value >> lexeme_d[ch_p('"') >> *(anychar_p - chset<>("\" \n"))];

			// Pharses
			phrase = quoted_phrase | value_end;

			// Collectors
			collector = as_lower_d[str_p("or")] | str_p("||") | as_lower_d[str_p("and")] | str_p("&&");

			// Statements
			statement = *(plus_or_minus)[&on_pom_action] >> *(selection)[&on_selection_action] >> phrase[&on_phrase_action];
		}

		rule<ScannerT> ul_query, plus, minus, plus_or_minus, field_name, relation;
		rule<ScannerT> quoted_value, value, value_end, field_value, selection;
		rule<ScannerT> quoted_phrase, phrase, collector, statement;

		rule<ScannerT> const& start() const
		{
			return ul_query;
		}
	};
};

pthread_mutex_t XesamULParser::m_mutex = PTHREAD_MUTEX_INITIALIZER;

XesamULParser::XesamULParser()
{
}

XesamULParser::~XesamULParser()
{
}

bool XesamULParser::parse(const string &xesam_query,
	XesamQueryBuilder &query_builder)
{
	size_t parsedLength = 0;
	bool fullParsing = false;

	if (pthread_mutex_lock(&m_mutex) == 0)
	{
		try
		{
			skip_grammar skip;
			xesam_ul_grammar query;

#ifdef DEBUG
			cout << "XesamULParser::parse: query is " << xesam_query << endl;
#endif
			// Signal the query builder we are starting
			query_builder.on_query(NULL);
			// Initialize these before any semantic action is called
			g_pQueryBuilder = &query_builder;
			g_foundCollector = false;
			g_foundPOM = false;
			g_negate = false;

			parse_info<> parseInfo = boost::spirit::parse(xesam_query.c_str(), query, skip);
			fullParsing = parseInfo.full;
			parsedLength = parseInfo.length;
			// FIXME: try parsing the remainder on its own
		}
		catch (const exception &e)
		{
#ifdef DEBUG
			cout << "XesamULParser::parse: caught exception ! " << e.what() << endl;
#endif
			fullParsing = false;
		}
		catch (...)
		{
#ifdef DEBUG
			cout << "XesamULParser::parse: caught unknown exception !" << endl;
#endif
			fullParsing = false;
		}
#ifdef DEBUG
		cout << "XesamULParser::parse: status is " << fullParsing << ", length " << parsedLength << endl;
#endif

		pthread_mutex_unlock(&m_mutex);
	}

	return fullParsing;
}

bool XesamULParser::parse_file(const string &xesam_query_file,
	XesamQueryBuilder &query_builder)
{
	return false;
}

