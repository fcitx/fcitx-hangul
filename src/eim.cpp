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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcitx/ime.h>
#include <fcitx-config/hotkey.h>
#include <fcitx-config/xdg.h>
#include <fcitx-utils/log.h>
#include <fcitx-config/fcitx-config.h>
#include <fcitx-utils/utils.h>
#include <fcitx/instance.h>
#include <fcitx/keys.h>
#include <string>
#include <libintl.h>

#include <hangul.h>

#include "eim.h"

#ifdef __cplusplus
extern "C" {
#endif
FCITX_EXPORT_API
FcitxIMClass ime = {
    FcitxHangulCreate,
    FcitxHangulDestroy
};
FCITX_EXPORT_API
int ABI_VERSION = FCITX_ABI_VERSION;
#ifdef __cplusplus
}
#endif

CONFIG_DESC_DEFINE(GetHangulConfigDesc, "fcitx-hangul.desc")

boolean LoadHangulConfig(FcitxHangulConfig* fs);
static void SaveHangulConfig(FcitxHangulConfig* fs);
static void ConfigHangul(FcitxHangul* hangul);

/**
 * @brief Reset the status.
 *
 **/
__EXPORT_API
void FcitxHangulReset (void* arg)
{
    FcitxHangul* hangul = (FcitxHangul*) arg;
    hangul_ic_reset(hangul->ic);
}

/**
 * @brief Process Key Input and return the status
 *
 * @param keycode keycode from XKeyEvent
 * @param state state from XKeyEvent
 * @param count count from XKeyEvent
 * @return INPUT_RETURN_VALUE
 **/
__EXPORT_API
INPUT_RETURN_VALUE FcitxHangulDoInput(void* arg, FcitxKeySym sym, unsigned int state)
{
    FcitxHangul* hangul = (FcitxHangul*) arg;

    return IRV_TO_PROCESS;
}

boolean FcitxHangulInit(void* arg)
{
    return true;
}


/**
 * @brief function DoInput has done everything for us.
 *
 * @param searchMode
 * @return INPUT_RETURN_VALUE
 **/
__EXPORT_API
INPUT_RETURN_VALUE FcitxHangulGetCandWords(void* arg)
{
    FcitxHangul* hangul = (FcitxHangul* )arg;
    return IRV_DISPLAY_CANDWORDS;
}

/**
 * @brief get the candidate word by index
 *
 * @param iIndex index of candidate word
 * @return the string of canidate word
 **/
__EXPORT_API
INPUT_RETURN_VALUE FcitxHangulGetCandWord (void* arg, CandidateWord* candWord)
{
    FcitxHangul* hangul = (FcitxHangul* )arg;    
    return IRV_DO_NOTHING;
}

/**
 * @brief initialize the extra input method
 *
 * @param arg
 * @return successful or not
 **/
__EXPORT_API
void* FcitxHangulCreate (FcitxInstance* instance)
{
    FcitxHangul* hangul = (FcitxHangul*) fcitx_malloc0(sizeof(FcitxHangul));
    bindtextdomain("fcitx-hangul", LOCALEDIR);
    hangul->owner = instance;
    
    if (LoadHangulConfig(&hangul->fh))
    {
        free(hangul);
        return NULL;
    }
    ConfigHangul(hangul);
    
    hangul->table = hanja_table_load(NULL);
    hangul->ic = hangul_ic_new(hangul->fh.keyboaryLayout);
    
    FcitxRegisterIM(instance,
                    hangul,
                    _("Hangul"),
                    "hangul",
                    FcitxHangulInit,
                    FcitxHangulReset,
                    FcitxHangulDoInput,
                    FcitxHangulGetCandWords,
                    NULL,
                    NULL,
                    ReloadConfigFcitxHangul,
                    NULL,
                    hangul->fh.iHangulPriority
                   );
    return hangul;
}

/**
 * @brief Destroy the input method while unload it.
 *
 * @return int
 **/
__EXPORT_API
void FcitxHangulDestroy (void* arg)
{
    FcitxHangul* hangul = (FcitxHangul*) arg;
    hanja_table_delete(hangul->table);
    free(arg);
}

/**
 * @brief Load the config file for fcitx-hangul
 *
 * @param Bool is reload or not
 **/
boolean LoadHangulConfig(FcitxHangulConfig* fs)
{
    ConfigFileDesc *configDesc = GetHangulConfigDesc();
    if (!configDesc)
        return false;

    FILE *fp = GetXDGFileUserWithPrefix("conf", "fcitx-hangul.config", "r", NULL);

    if (!fp)
    {
        if (errno == ENOENT)
            SaveHangulConfig(fs);
    }
    ConfigFile *cfile = ParseConfigFileFp(fp, configDesc);

    FcitxHangulConfigConfigBind(fs, cfile, configDesc);
    ConfigBindSync(&fs->gconfig);
    
    if (fp)
        fclose(fp);    
    return true;
}

void ConfigHangul(FcitxHangul* hangul)
{
}

__EXPORT_API void ReloadConfigFcitxHangul(void* arg)
{
    FcitxHangul* hangul = (FcitxHangul*) arg;
    LoadHangulConfig(&hangul->fh);
    ConfigHangul(hangul);
}

/**
 * @brief Save the config
 *
 * @return void
 **/
void SaveHangulConfig(FcitxHangulConfig* fa)
{
    ConfigFileDesc *configDesc = GetHangulConfigDesc();
    FILE *fp = GetXDGFileUserWithPrefix("conf", "fcitx-hangul.config", "w", NULL);
    SaveConfigFileFp(fp, &fa->gconfig, configDesc);
    if (fp)
        fclose(fp);
}
