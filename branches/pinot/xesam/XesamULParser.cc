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
using std::map;
using std::exception;
using namespace boost::spirit;

using namespace Dijon;

struct skip_grammar : public grammar<skip_grammar>
{
	template <typename ScannerT>
	struct definition
	{
		definition(skip_grammar const &self)
		{
			// Skip all spaces and comments, starting with a #
			// FIXME: make sure comments start at the beginning of the line !
			skip = space_p | (ch_p('#') >> *(anychar_p - ch_p('\n')) >> ch_p('\n'));
		}
	
		rule<ScannerT> skip;
	
		rule<ScannerT> const& start() const
		{
			return skip;
		}
	};
};

void set_collector_action(char const *first, char const *last)
{
	string str(first, last);

	cout << "Found collector " << str << endl;
}

void on_selection_action(char const *first, char const *last)
{
	string str(first, last);

	cout << "Found selection " << str << endl;
}

void on_phrase_action(char const *first, char const *last)
{
	string str(first, last);

	cout << "Found phrase " << str << endl;
}

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
			ul_query = statement >> *(collector[&set_collector_action] >> statement);

			// Statement
			plus = ch_p('+');

			minus = ch_p('-');

			plus_or_minus = plus |
				minus;

			field_name = *(range_p('a','z') | range_p('A','Z'));

			relation = ch_p('=') |
				ch_p(':') |
				str_p("<=") |
				str_p(">=") |
				ch_p('<') |
				ch_p('>');

			quoted_value = lexeme_d[*(anychar_p - chset<>("\"\n"))];

			value = lexeme_d[*(anychar_p - chset<>("\" \n"))];

			value_end = lexeme_d[*(anychar_p - chset<>("\"\n"))];

			field_value = ch_p('"') >> quoted_value >> ch_p('"') | value;

			selection = field_name >> relation >> field_value;

			modifiers = value;

			quoted_phrase = ch_p('"') >> quoted_value >> ch_p('"') >> modifiers;

			phrase = quoted_phrase | value_end;

			collector = as_lower_d[str_p("or")] | str_p("||") | as_lower_d[str_p("and")] | str_p("&&") | lexeme_d[space_p];

			statement = *(plus_or_minus) >> *(selection[&on_selection_action]) >> phrase[&on_phrase_action];
		}

		rule<ScannerT> ul_query, plus, minus, plus_or_minus, field_name, relation, quoted_value, value, value_end, field_value, selection;
		rule<ScannerT> modifiers, quoted_phrase, phrase, collector, statement;

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
			parse_info<> parseInfo = boost::spirit::parse(xesam_query.c_str(), query, skip);
			fullParsing = parseInfo.full;
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
		cout << "XesamULParser::parse: status is " << fullParsing << endl;
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

