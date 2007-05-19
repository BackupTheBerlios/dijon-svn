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

XapianQueryBuilder::XapianQueryBuilder() :
	XesamQueryBuilder()
{
}

XapianQueryBuilder::~XapianQueryBuilder()
{
}

void XapianQueryBuilder::on_userQuery(const char *value)
{
	cout << "XapianQueryBuilder::on_userQuery: called";
	if (value != NULL)
	{
		cout << " with " << value;
	}
	cout << endl;
}

void XapianQueryBuilder::on_query(const char *type)
{
	cout << "XapianQueryBuilder::on_query: called";
	if (type != NULL)
	{
		cout << " with " << type;
	}
	cout << endl;
}

void XapianQueryBuilder::on_selection(SelectionType selection, const string &property_name,
	SimpleType property_type, const set<string> &property_values)
{
	cout << "XapianQueryBuilder::on_selection: called ";
	if (property_name.empty() == true)
	{
		cout << "on all properties";
	}
	else cout << "on property " << property_name;
	for (set<string>::iterator valueIter = property_values.begin(); valueIter != property_values.end(); ++valueIter)
	{
		cout << " " << *valueIter;
	}
 	cout << endl;
	cout << "XapianQueryBuilder::on_selection: collector is " << m_collector.m_collector << endl;
}

