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

#include "ustring.h"
#include <fcitx-utils/utarray.h>
#include <fcitx-utils/utf8.h>

static const UT_icd ucs_icd = { sizeof(ucschar), NULL, NULL, NULL};

UString*
ustring_new()
{
    UString* str;
    utarray_new(str, &ucs_icd);
    return str;
}

UString*
ustring_dup(const UString* str)
{
    UString* dup;
    dup = ustring_new();
    ustring_append(dup, str);
    return dup;
}

void
ustring_delete(UString* str)
{
    utarray_free(str);
}

void
ustring_clear(UString* str)
{
    utarray_clear(str);
}

UString*
ustring_erase(UString* str, size_t pos, size_t len)
{
    if (len > 0)
        utarray_erase(str, pos, len);
    return str;
}

ucschar*
ustring_begin(UString* str)
{
    return (ucschar*) utarray_front(str);
}

ucschar*
ustring_end(UString* str)
{
    return (ucschar*) utarray_eltptr(str, utarray_len(str));
}

unsigned
ustring_length(const UString* str)
{
    return utarray_len(str);
}

UString*
ustring_append(UString* str, const UString* s)
{
    utarray_concat(str, s);
    return str;
}

UString*
ustring_append_ucs4(UString* str, const ucschar* s)
{
    const ucschar*p = s;
    while (*p != 0) {
        utarray_push_back(str, p);
        p++;
    }

    return str;
}

UString*
ustring_append_utf8(UString* str, const char* utf8)
{
    while (*utf8 != '\0') {
        ucschar c;
        utf8 = fcitx_utf8_get_char(utf8, (int*) &c);
        utarray_push_back(str, &c);
    }
    return str;
}