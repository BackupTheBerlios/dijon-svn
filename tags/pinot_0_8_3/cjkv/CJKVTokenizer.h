/*
 *  Copyright 2007,2008 林永忠 Yung-Chung Lin
 *  Copyright 2008 Fabrice Colin
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

#ifndef _DIJON_CJKVTOKENIZER_H
#define _DIJON_CJKVTOKENIZER_H

#include <string>
#include <vector>
#ifdef HAVE_UNICODE_H
#include <unicode.h>
#else
#include <glib/gunicode.h>
#define unicode_char_t gunichar
#endif

namespace Dijon
{
	class CJKVTokenizer
	{
		public:
			CJKVTokenizer();
			~CJKVTokenizer();

			void set_ngram_size(unsigned int ngram_size);

			unsigned int get_ngram_size(void) const;

			void set_max_token_count(unsigned int max_token_count);

			unsigned int get_max_token_count(void) const;

			void tokenize(const std::string &str,
				std::vector<std::string> &token_list);

			void split(const std::string &str,
				std::vector<std::string> &token_list);

			void split(const std::string &str,
				std::vector<unicode_char_t> &token_list);

			bool has_cjkv(const std::string &str);

			bool has_cjkv_only(const std::string &str);

		protected:
			unsigned int m_nGramSize;
			unsigned int m_maxTokenCount;

	};
};

#endif // _DIJON_CJKVTOKENIZER_H
