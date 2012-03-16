/***************************************************************************
 *   Copyright (C) 2012~2012 by CSSlayer                                   *
 *   wengxt@gmail.com                                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

#ifndef USTRING_H
#define USTRING_H

#include <hangul.h>
#include <fcitx-utils/utarray.h>

typedef UT_array UString;

UString* ustring_new();
UString* ustring_dup(const UString* str);
void     ustring_delete(UString* str);

void     ustring_clear(UString* str);
UString* ustring_erase(UString* str, size_t pos, size_t len);

ucschar* ustring_begin(UString* str);
ucschar* ustring_end(UString* str);
unsigned ustring_length(const UString* str);

UString* ustring_append(UString* str, const UString* s);
UString* ustring_append_ucs4(UString* str, const ucschar* s);
UString* ustring_append_utf8(UString* str, const char* utf8);

#endif