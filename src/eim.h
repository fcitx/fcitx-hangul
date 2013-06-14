/***************************************************************************
 *   Copyright (C) 2010~2010 by CSSlayer                                   *
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

#ifndef EIM_H
#define EIM_H

#include <fcitx/ime.h>
#include <fcitx-config/fcitx-config.h>
#include <fcitx/instance.h>
#include <fcitx/candidate.h>
#include <hangul.h>
#include <iconv.h>
#include "ustring.h"
#include "keyboard.h"

#define _(x) dgettext("fcitx-hangul", x)

typedef struct _FcitxHangulConfig
{
    FcitxGenericConfig gconfig;
    FcitxHangulKeyboard  keyboardLayout;
    boolean hanjaMode;
    boolean autoReorder;
    boolean wordCommit;
    FcitxHotkey hkHanjaMode[2];
} FcitxHangulConfig;

CONFIG_BINDING_DECLARE(FcitxHangulConfig);
void* FcitxHangulCreate(FcitxInstance* instance);
void FcitxHangulDestroy(void* arg);
INPUT_RETURN_VALUE FcitxHangulDoInput(void* arg, FcitxKeySym sym, unsigned int state);
INPUT_RETURN_VALUE FcitxHangulGetCandWords (void *arg);
INPUT_RETURN_VALUE FcitxHangulGetCandWord (void *arg, FcitxCandidateWord* candWord);
void FcitxHangulOnClose(void* arg, FcitxIMCloseEventType event);
boolean FcitxHangulInit(void*);
void ReloadConfigFcitxHangul(void*);

typedef enum _LookupMethod
{
    LOOKUP_METHOD_PREFIX,
    LOOKUP_METHOD_EXACT,
    LOOKUP_METHOD_SUFFIX
} LookupMethod;

typedef struct _FcitxHangul
{
    FcitxHangulConfig fh;
    FcitxInstance* owner;
    HanjaTable* table;
    HangulInputContext* ic;
    HanjaTable* symbolTable;
    UString* preedit;
    iconv_t conv;
    HanjaList* hanjaList;
    LookupMethod lastLookupMethod;
} FcitxHangul;

#endif
