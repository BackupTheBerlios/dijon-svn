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

#ifndef _DIJON_XESAMULPARSER_H
#define _DIJON_XESAMULPARSER_H

#include <pthread.h>
#include <string>

#include "XesamParser.h"

namespace Dijon
{
    /// Xesam User Language parser.
    class XesamULParser : public XesamParser
    {
    public:
	/// Builds a parser for the Xesam User Language.
	XesamULParser();
	virtual ~XesamULParser();

	virtual bool parse(const std::string &xesam_query,
		XesamQueryBuilder &query_builder);

	virtual bool parse_file(const std::string &xesam_query_file,
		XesamQueryBuilder &query_builder);

    protected:
	static pthread_mutex_t m_mutex;

    private:
	/// XesamULParser objects cannot be copied.
	XesamULParser(const XesamULParser &other);
	/// XesamULParser objects cannot be copied.
	XesamULParser& operator=(const XesamULParser &other);

    };
}

#endif // _DIJON_XESAMULPARSER_H
