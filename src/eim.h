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

#ifdef __cplusplus
#define __EXPORT_API extern "C"
#else
#define __EXPORT_API
#endif

#define _(x) gettext(x)

class FcitxWindowHandler;
struct FcitxHangulConfig
{
    GenericConfig gconfig;
    int iHangulPriority;
    char* keyboaryLayout;
};

CONFIG_BINDING_DECLARE(FcitxHangulConfig);
__EXPORT_API void* FcitxHangulCreate(FcitxInstance* instance);
__EXPORT_API void FcitxHangulDestroy(void* arg);
__EXPORT_API INPUT_RETURN_VALUE FcitxHangulDoInput(void* arg, FcitxKeySym sym, unsigned int state);
__EXPORT_API INPUT_RETURN_VALUE FcitxHangulGetCandWords (void *arg);
__EXPORT_API INPUT_RETURN_VALUE FcitxHangulGetCandWord (void *arg, CandidateWord* candWord);
__EXPORT_API boolean FcitxHangulInit(void*);
__EXPORT_API void ReloadConfigFcitxHangul(void*);

typedef struct _FcitxHangul
{
    FcitxHangulConfig fh;
    FcitxInstance* owner;
    HanjaTable* table;
    HangulInputContext* ic;
} FcitxHangul;

#endif
