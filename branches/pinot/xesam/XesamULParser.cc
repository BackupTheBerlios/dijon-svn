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
#include <fstream>
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
using std::ifstream;
using std::ios;
using namespace boost::spirit;

using namespace Dijon;

class ULActions
{
    public:
	// Initialization.
	static void initialize(XesamQueryBuilder *pQueryBuilder);

	/// Semantic action for the collector rule.
	static void set_collector_action(char const *first, char const *last);

	/// Semantic action for statements.
	static void on_statement(char const *first, char const *last);

	/// Semantic action for the plus_or_minus rule.
	static void on_pom_action(char const *first, char const *last);

	/// Semantic action for field values.
	static void on_field_name_action(char const *first, char const *last);

	/// Semantic action for field relations.
	static void on_relation_action(char const *first, char const *last);

	/// Semantic action for field values.
	static void on_field_value_action(char const *first, char const *last);

	/// Semantic action for the phrase rule.
	static void on_phrase_action(char const *first, char const *last);

	static XesamQueryBuilder *m_pQueryBuilder;
	static bool m_foundCollector;
	static bool m_foundPOM;
	static bool m_negate;
	static string m_fieldName;
	static SelectionType m_fieldSelectionType;
};

XesamQueryBuilder *ULActions::m_pQueryBuilder = NULL;
bool ULActions::m_foundCollector = false;
bool ULActions::m_foundPOM = false;
bool ULActions::m_negate = false;
string ULActions::m_fieldName;
SelectionType ULActions::m_fieldSelectionType = None;

void ULActions::initialize(XesamQueryBuilder *pQueryBuilder)
{
	m_pQueryBuilder = pQueryBuilder;
	m_foundCollector = false;
	m_foundPOM = false;
	m_negate = false;
	m_fieldName.clear();
	m_fieldSelectionType = None;

	// Signal the query builder we are starting
	m_pQueryBuilder->on_query(NULL);
}

void ULActions::set_collector_action(char const *first, char const *last)
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

	m_pQueryBuilder->set_collector(collector);

	m_foundCollector = true;
	m_foundPOM = false;
	m_negate = false;
}

void ULActions::on_statement(char const *first, char const *last)
{
#ifdef DEBUG
	cout << "on_statement: called" << endl;
#endif
	if (m_foundCollector == true)
	{
		m_foundCollector = false;
	}
	else
	{
		Collector collector(And, false, 0.0);

		// No collector was found between this and the previous statement 
		// Revert to And, the default collector
		m_pQueryBuilder->set_collector(collector);
	}

	if (m_foundPOM == true)
	{
		m_foundPOM = false;
	}
	else
	{
		// Revert to +
		m_negate = false;
	}
}

void ULActions::on_pom_action(char const *first, char const *last)
{
	string str(first, last);

#ifdef DEBUG
	cout << "on_pom_action: found " << str << endl;
#endif
	if (str == "-")
	{
		m_negate = true;
	}
	else
	{
		m_negate = false;
	}
	m_foundPOM = true;
}

void ULActions::on_field_name_action(char const *first, char const *last)
{
	string str(first, last);

#ifdef DEBUG
	cout << "on_field_name_action: found " << str << endl;
#endif
	if (str.empty() == true)
	{
		return;
	}

	// FIXME: determine SimpleType based on fieldName
	m_fieldName = str;
}

void ULActions::on_relation_action(char const *first, char const *last)
{
	string str(first, last);

#ifdef DEBUG
	cout << "on_relation_action: found " << str << endl;
#endif
	if ((str.empty() == true) ||
		(m_fieldName.empty() == true))
	{
		return;
	}

	m_fieldSelectionType = None;
	if (str == ":")
	{
		m_fieldSelectionType = Equals;
	}
	else if (str == "<=")
	{
		m_fieldSelectionType = LessThanEquals;
	}
	else if (str == ">=")
	{
		m_fieldSelectionType = GreaterThanEquals;
	}
	else if (str == "=")
	{
		m_fieldSelectionType = Equals;
	}
	else if (str == "<")
	{
		m_fieldSelectionType = LessThan;
	}
	else if (str == ">")
	{
		m_fieldSelectionType = GreaterThan;
	}
}

void ULActions::on_field_value_action(char const *first, char const *last)
{
	set<string> fieldNames, fieldValues;
	string str(first, last);
	string::size_type pos;
	SimpleType type = String; 
	Modifiers modifiers;

#ifdef DEBUG
	cout << "on_selection_action: found " << str << endl;
#endif
	if ((str.empty() == true) ||
		(m_fieldName.empty() == true))
	{
		return;
	}

	modifiers.m_negate = m_negate;

	fieldNames.insert(m_fieldName);
	fieldValues.insert(str);

	m_pQueryBuilder->on_selection(m_fieldSelectionType,
		fieldNames, fieldValues, type, modifiers);

	m_fieldName.clear();
	m_fieldSelectionType = None;
}

void ULActions::on_phrase_action(char const *first, char const *last)
{
	set<string> fieldNames, fieldValues;
	string str(first, last);
	SelectionType selectionType = FullText;
	SimpleType type = String; 
	Modifiers modifiers;

#ifdef DEBUG
	cout << "on_phrase_action: found " << str << endl;
#endif
	if (str.empty() == true)
	{
		return;
	}

	modifiers.m_negate = m_negate;

	if (str[0] == '"')
	{
		string::size_type closingQuote = str.find_last_of("\"");
		if (closingQuote == 0)
		{
			return;
		}
		fieldValues.insert(str.substr(1, closingQuote - 1));

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
	else
	{
		fieldValues.insert(str);
	}

	m_pQueryBuilder->on_selection(selectionType,
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
			ul_query = statement >> *(collector[&ULActions::set_collector_action] >> statement);

			// Positive or negative ?
			plus = ch_p('+');

			minus = ch_p('-');

			plus_or_minus = plus |
				minus;

			// Field names are alphabetical characters only
			field_name = *(range_p('a','z') | range_p('A','Z'));

			// Valid relations
			relation = ch_p(':') |
				str_p("<=") |
				str_p(">=") |
				ch_p('=') |
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
			selection = field_name[&ULActions::on_field_name_action] >> relation[&ULActions::on_relation_action]
				>> field_value[&ULActions::on_field_value_action];

			// Quoted phrases end with modifiers directly after the closing double-quote
			quoted_phrase = ch_p('"') >> quoted_value >> lexeme_d[ch_p('"') >> *(anychar_p - chset<>("\" \n"))];

			// Pharses
			phrase = quoted_phrase | value_end;

			// Collectors
			collector = as_lower_d[str_p("or")] | str_p("||") | as_lower_d[str_p("and")] | str_p("&&");

			// Statements
			statement = (*(plus_or_minus)[&ULActions::on_pom_action] >> *(selection)
				>> phrase[&ULActions::on_phrase_action])[&ULActions::on_statement];
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

XesamULParser::XesamULParser() :
	XesamParser()
{
}

XesamULParser::~XesamULParser()
{
}

bool XesamULParser::parse(const string &xesam_query,
	XesamQueryBuilder &query_builder)
{
	string::size_type parsedLength = 0;
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
			// Initialize
			ULActions::initialize(&query_builder);

			// If the whole string couldn't be parsed, try again starting where parsing stopped
			while ((fullParsing == false) &&
				(parsedLength < xesam_query.length()))
			{
				parse_info<> parseInfo = boost::spirit::parse(xesam_query.c_str() + parsedLength, query, skip);
				fullParsing = parseInfo.full;
				parsedLength += parseInfo.length;
#ifdef DEBUG
				cout << "XesamULParser::parse: status is " << fullParsing << ", length " << parseInfo.length << endl;
#endif
			}
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
		cout << "XesamULParser::parse: final status is " << fullParsing << ", length " << parsedLength << endl;
#endif

		pthread_mutex_unlock(&m_mutex);
	}

	return fullParsing;
}

bool XesamULParser::parse_file(const string &xesam_query_file,
	XesamQueryBuilder &query_builder)
{
	ifstream inputFile;
	bool readFile = false;

	inputFile.open(xesam_query_file.c_str());
	if (inputFile.good() == true)
	{
		inputFile.seekg(0, ios::end);
		int length = inputFile.tellg();
		inputFile.seekg(0, ios::beg);

		char *pXmlBuffer = new char[length + 1];
		inputFile.read(pXmlBuffer, length);
		if (inputFile.fail() == false)
		{
			pXmlBuffer[length] = '\0';
			string fileContents(pXmlBuffer, length);

			readFile = parse(fileContents, query_builder);
		}
		delete[] pXmlBuffer;
	}
	inputFile.close();

	return readFile;
}

