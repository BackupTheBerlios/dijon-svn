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

#ifndef _DIJON_XAPIANQUERYBUILDER_H
#define _DIJON_XAPIANQUERYBUILDER_H

#include <string>
#include <set>
#include <xapian.h>

#include "XesamQueryBuilder.h"

namespace Dijon
{
    /// A query builder for Xapian::Query.
    class XapianQueryBuilder : public XesamQueryBuilder
    {
    public:
	/// Builds a query builder for Xapian::Query.
	XapianQueryBuilder(Xapian::QueryParser &queryParser);
	virtual ~XapianQueryBuilder();

	virtual void on_query(const char *type);

	virtual void on_selection(SelectionType selection,
		const std::set<std::string> &field_names,
		const std::set<std::string> &field_values,
		SimpleType field_type,
		const Modifiers &modifiers);

	Xapian::Query get_query(void) const;

    public:
	Xapian::QueryParser &m_queryParser;
	Xapian::Query m_fullQuery;
	bool m_firstSelection;

    private:
	/// XapianQueryBuilder objects cannot be copied.
	XapianQueryBuilder(const XapianQueryBuilder &other);
	/// XapianQueryBuilder objects cannot be copied.
	XapianQueryBuilder& operator=(const XapianQueryBuilder &other);

    };
}

#endif // _DIJON_XAPIANQUERYBUILDER_H
