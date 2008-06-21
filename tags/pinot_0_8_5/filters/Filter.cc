/*
 *  Copyright 2007 Fabrice Colin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

#include "Filter.h"

using std::string;
using std::set;
using std::map;
using std::cout;
using std::endl;

using namespace Dijon;

Filter::Filter(const string &mime_type) :
	m_mimeType(mime_type),
	m_convertToUTF8Func(NULL),
	m_deleteInputFile(false)
{
}

Filter::~Filter()
{
	deleteInputFile();
}

bool Filter::set_document_file(const string &file_path, bool unlink_when_done)
{
	if (file_path.empty() == true)
	{
		return false;
	}

	rewind();
	m_filePath = file_path;
	m_deleteInputFile = unlink_when_done;

	return true;
}

string Filter::get_mime_type(void) const
{
	return m_mimeType;
}

bool Filter::set_utf8_converter(convert_to_utf8_func *func)
{
	m_convertToUTF8Func = func;
}

const map<string, string> &Filter::get_meta_data(void) const
{
	return m_metaData;
}

void Filter::rewind(void)
{
	m_metaData.clear();
	deleteInputFile();
	m_filePath.clear();
	m_deleteInputFile = false;
}

void Filter::deleteInputFile(void)
{
	if ((m_deleteInputFile == true) &&
		(m_filePath.empty() == false))
	{
		unlink(m_filePath.c_str());
	}
}
