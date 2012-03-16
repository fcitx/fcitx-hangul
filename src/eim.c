/***************************************************************************
 *   Copyright (C) 2010~2012 by CSSlayer                                   *
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
#include <libintl.h>
#include <iconv.h>

#include <fcitx/context.h>
#include <fcitx/ime.h>
#include <fcitx-config/hotkey.h>
#include <fcitx-config/xdg.h>
#include <fcitx-utils/log.h>
#include <fcitx-config/fcitx-config.h>
#include <fcitx-utils/utils.h>
#include <fcitx/instance.h>
#include <fcitx/keys.h>
#include <fcitx/hook.h>

#include <hangul.h>

#include "eim.h"

FCITX_EXPORT_API
FcitxIMClass ime = {
    FcitxHangulCreate,
    FcitxHangulDestroy
};
FCITX_EXPORT_API
int ABI_VERSION = FCITX_ABI_VERSION;

CONFIG_DESC_DEFINE(GetHangulConfigDesc, "fcitx-hangul.desc")

boolean LoadHangulConfig(FcitxHangulConfig* fs);
static void SaveHangulConfig(FcitxHangulConfig* fs);
static void ConfigHangul(FcitxHangul* hangul);
static void FcitxHangulUpdatePreedit(FcitxHangul* hangul);
static void FcitxHangulUpdateLookupTable(FcitxHangul* hangul);
static void FcitxHangulFlush(FcitxHangul* hangul);
static void FcitxHangulToggleHanja(void* arg);
static boolean FcitxHangulGetHanja(void* arg);
static void FcitxHangulResetEvent(void* arg);
static char* FcitxHangulUcs4ToUtf8(FcitxHangul* hangul, const ucschar* ucsstr, int length);

size_t ucs4_strlen(const ucschar* str)
{
    size_t len = 0;
    while(*str) {
        len ++;
        str ++;
    }
    return len;
}

char* FcitxHangulUcs4ToUtf8(FcitxHangul* hangul, const ucschar* ucsstr, int length)
{
    if (!ucsstr)
        return NULL;
    
    size_t ucslen;
    if (length < 0)
        ucslen = ucs4_strlen(ucsstr);
    else
        ucslen = length;
    size_t len = UTF8_MAX_LENGTH * ucslen;
    char* str = (char*) fcitx_utils_malloc0(sizeof(char) * len + 1);
    len *= sizeof(char);
    ucslen *= sizeof(ucschar);
    char* p = str;
    iconv(hangul->conv, (char**) &ucsstr, &ucslen, &p, &len);
    return str;
}

/**
 * @brief Reset the status.
 *
 **/
void FcitxHangulReset (void* arg)
{
    FcitxHangul* hangul = (FcitxHangul*) arg;
    ustring_clear(hangul->preedit);
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
INPUT_RETURN_VALUE FcitxHangulDoInput(void* arg, FcitxKeySym sym, unsigned int state)
{
    FcitxHangul* hangul = (FcitxHangul*) arg;
    
    if (FcitxHotkeyIsHotKey(sym, state, hangul->fh.hkHanjaMode)) {
        FcitxUIUpdateStatus(hangul->owner, "hanja");
        if (hangul->fh.hkHanjaMode)
            return IRV_DISPLAY_CANDWORDS;
        else
            return IRV_DO_NOTHING;
    }
    
    if (sym == FcitxKey_Shift_L || sym == FcitxKey_Shift_R)
        return IRV_TO_PROCESS;
    
    int s = hangul->fh.hkHanjaMode[0].state | hangul->fh.hkHanjaMode[1].state;
    if (s & FcitxKeyState_Ctrl) {
        if (sym == FcitxKey_Control_L || sym == FcitxKey_Control_R)
            return IRV_TO_PROCESS;
    }
    if (s & FcitxKeyState_Alt) {
        if (sym == FcitxKey_Alt_L || sym == FcitxKey_Alt_R)
            return IRV_TO_PROCESS;
    }
    if (s & FcitxKeyState_Shift) {
        if (sym == FcitxKey_Shift_L || sym == FcitxKey_Shift_R)
            return IRV_TO_PROCESS;
    }
    if (s & FcitxKeyState_Super) {
        if (sym == FcitxKey_Super_L || sym == FcitxKey_Super_R)
            return IRV_TO_PROCESS;
    }
    if (s & FcitxKeyState_Hyper) {
        if (sym == FcitxKey_Hyper_L || sym == FcitxKey_Hyper_R)
            return IRV_TO_PROCESS;
    }
    
    s = FcitxKeyState_Ctrl | FcitxKeyState_Alt | FcitxKeyState_Shift | FcitxKeyState_Super | FcitxKeyState_Hyper;
    if (state & s) {
        FcitxHangulFlush (hangul);
        return IRV_TO_PROCESS;
    }
    
    FcitxInputState* input = FcitxInstanceGetInputState(hangul->owner);
    FcitxCandidateWordList* candList = FcitxInputStateGetCandidateList(input);
    int candSize = FcitxCandidateWordGetListSize(candList);
    
    if (candSize > 0) {
        FcitxGlobalConfig* config = FcitxInstanceGetGlobalConfig(hangul->owner);
        
        if (FcitxHotkeyIsHotKey(sym, state, config->hkPrevPage) || FcitxHotkeyIsHotKey(sym, state, config->hkNextPage))
            return IRV_TO_PROCESS;
        
        if (FcitxHotkeyIsHotKeyDigit(sym, state))
            return IRV_TO_PROCESS;
        
        if (FcitxHotkeyIsHotKey(sym, state , FCITX_ENTER)) {
            return FcitxCandidateWordChooseByIndex(FcitxInputStateGetCandidateList(input), 0);
        }
         
         if (!hangul->fh.hanjaMode) {
            return IRV_DONOT_PROCESS;
        }
    }
    
    bool keyUsed = false;
    if (FcitxHotkeyIsHotKey(sym, state, FCITX_BACKSPACE)) {
        keyUsed = hangul_ic_backspace (hangul->ic);
        if (!keyUsed) {
            unsigned int preedit_len = ustring_length (hangul->preedit);
            if (preedit_len > 0) {
                ustring_erase (hangul->preedit, preedit_len - 1, 1);
                keyUsed = true;
            }
        }
    } else {
        keyUsed = hangul_ic_process(hangul->ic, sym);
        
        const ucschar* str = hangul_ic_get_commit_string (hangul->ic);
        if (hangul->word_commit || hangul->fh.hanjaMode) {
            const ucschar* hic_preedit;

            hic_preedit = hangul_ic_get_preedit_string (hangul->ic);
            if (hic_preedit != NULL && hic_preedit[0] != 0) {
                ustring_append_ucs4 (hangul->preedit, str);
            } else {
                ustring_append_ucs4 (hangul->preedit, str);
                if (ustring_length (hangul->preedit) > 0) {
                    char* commit = FcitxHangulUcs4ToUtf8(hangul, ustring_begin(hangul->preedit), ustring_length(hangul->preedit));
                    if (commit) {
                        FcitxInstanceCleanInputWindowUp(hangul->owner);
                        FcitxInstanceCommitString(hangul->owner, FcitxInstanceGetCurrentIC(hangul->owner), commit);
                        free(commit);
                    }
                }
                ustring_clear (hangul->preedit);
            }
        } else {
            if (str != NULL && str[0] != 0) {
                char* commit = FcitxHangulUcs4ToUtf8(hangul, str, -1);
                if (commit) {
                    FcitxInstanceCleanInputWindowUp(hangul->owner);
                    FcitxInstanceCommitString(hangul->owner, FcitxInstanceGetCurrentIC(hangul->owner), commit);
                    free(commit);
                }
            }
        }

        FcitxHangulGetCandWords(hangul);
        FcitxUIUpdateInputWindow(hangul->owner);
        if (!keyUsed)
            FcitxHangulFlush (hangul);
    }
    
    if (!keyUsed)
        return IRV_TO_PROCESS;
    else
        return IRV_DISPLAY_CANDWORDS;
}

void FcitxHangulUpdatePreedit(FcitxHangul* hangul)
{
    FcitxInputState* input = FcitxInstanceGetInputState(hangul->owner);
    FcitxMessages* preedit = FcitxInputStateGetPreedit(input);
    FcitxMessages* clientPreedit = FcitxInputStateGetClientPreedit(input);
    FcitxInstanceCleanInputWindowUp(hangul->owner);
    const ucschar *hic_preedit = hangul_ic_get_preedit_string (hangul->ic);

    char* pre1 = FcitxHangulUcs4ToUtf8(hangul, ustring_begin(hangul->preedit), ustring_length(hangul->preedit));
    char* pre2 = FcitxHangulUcs4ToUtf8(hangul, hic_preedit, -1);
    FcitxInputContext* ic = FcitxInstanceGetCurrentIC(hangul->owner);
    FcitxProfile* profile = FcitxInstanceGetProfile(hangul->owner);
    
    if (pre1 && pre1[0] != 0) {
        if (ic && ((ic->contextCaps & CAPACITY_PREEDIT) == 0 || !profile->bUsePreedit))
            FcitxMessagesAddMessageAtLast(preedit, MSG_INPUT, "%s", pre1);
        FcitxMessagesAddMessageAtLast(clientPreedit, MSG_INPUT, "%s", pre1);
    }
    
    if (pre2 && pre2[0] != '\0') {
        if (ic && ((ic->contextCaps & CAPACITY_PREEDIT) == 0 || !profile->bUsePreedit))
            FcitxMessagesAddMessageAtLast(preedit, MSG_INPUT | MSG_HIGHLIGHT, "%s", pre2);
        FcitxMessagesAddMessageAtLast(clientPreedit, MSG_INPUT | MSG_HIGHLIGHT, "%s", pre2);
    }
    
    if (pre1)
        free(pre1);
    
    if (pre2)
        free(pre2);
}

void FcitxHangulUpdateLookupTable(FcitxHangul* hangul)
{
    char* utf8;
    const ucschar* hic_preedit;
    UString* preedit;

    if (hangul->hanjaList != NULL) {
        hanja_list_delete (hangul->hanjaList);
        hangul->hanjaList = NULL;
    }

    hic_preedit = hangul_ic_get_preedit_string (hangul->ic);

    preedit = ustring_dup (hangul->preedit);
    ustring_append_ucs4 (preedit, hic_preedit);
    if (ustring_length(preedit) > 0) {
        utf8 = FcitxHangulUcs4ToUtf8 (hangul, ustring_begin(preedit), ustring_length(preedit));
        if (utf8 != NULL) {
            if (hangul->symbolTable != NULL)
                hangul->hanjaList = hanja_table_match_prefix (hangul->symbolTable, utf8);
            if (hangul->hanjaList == NULL)
                hangul->hanjaList = hanja_table_match_prefix (hangul->table, utf8);
            free (utf8);
        }
    }

    ustring_delete (preedit);
    
    if (hangul->hanjaList) {
        HanjaList* list = hangul->hanjaList;
        if (list != NULL) {
            int i, n;
            n = hanja_list_get_size (list);

            FcitxInputState* input = FcitxInstanceGetInputState(hangul->owner);
            FcitxCandidateWordList* candList = FcitxInputStateGetCandidateList(input);
            FcitxGlobalConfig* config = FcitxInstanceGetGlobalConfig(hangul->owner);
            FcitxCandidateWordSetPageSize(candList, config->iMaxCandWord);
            FcitxCandidateWordSetChoose(candList, "1234567890");
            FcitxCandidateWord word;
            FcitxCandidateWordReset(candList);
            for (i = 0; i < n; i++) {
                const char* value = hanja_list_get_nth_value (list, i);
                unsigned int* idx = fcitx_utils_malloc0(sizeof(unsigned int));
                *idx = i;
                word.strWord = strdup(value);
                word.wordType = MSG_INPUT;
                word.strExtra = NULL;
                word.extraType = MSG_INPUT;
                word.priv = idx;
                word.owner = hangul;
                word.callback = FcitxHangulGetCandWord;
                FcitxCandidateWordAppend(candList, &word);
            }
        }
    }
}

void FcitxHangulFlush(FcitxHangul* hangul)
{
    const ucschar *str;

    FcitxInstanceCleanInputWindowDown(hangul->owner);

    str = hangul_ic_flush (hangul->ic);

    ustring_append_ucs4 (hangul->preedit, str);

    if (ustring_length (hangul->preedit) == 0)
        return;

    str = ustring_begin (hangul->preedit);
    char* utf8 = FcitxHangulUcs4ToUtf8(hangul, str, ustring_length(hangul->preedit));
    if (utf8) {
        FcitxInstanceCommitString(hangul->owner, FcitxInstanceGetCurrentIC(hangul->owner), utf8);
        free(utf8);
    }

    ustring_clear(hangul->preedit);
}

boolean FcitxHangulInit(void* arg)
{
    FcitxHangul* hangul = (FcitxHangul*) arg;
    FcitxInstanceSetContext(hangul->owner, CONTEXT_IM_KEYBOARD_LAYOUT, "us");
    return true;
}


/**
 * @brief function DoInput has done everything for us.
 *
 * @param searchMode
 * @return INPUT_RETURN_VALUE
 **/
INPUT_RETURN_VALUE FcitxHangulGetCandWords(void* arg)
{
    FcitxHangul* hangul = (FcitxHangul* )arg;
    FcitxHangulUpdatePreedit(hangul);

    if (hangul->fh.hanjaMode) {
        FcitxHangulUpdateLookupTable(hangul);
    }

    return IRV_DISPLAY_CANDWORDS;
}

/**
 * @brief get the candidate word by index
 *
 * @param iIndex index of candidate word
 * @return the string of canidate word
 **/
INPUT_RETURN_VALUE FcitxHangulGetCandWord (void* arg, FcitxCandidateWord* candWord)
{
    FcitxHangul* hangul = (FcitxHangul* )arg;
    unsigned int pos = *(unsigned int*) candWord->priv;
    const char* key;
    const char* value;
    int key_len;
    int preedit_len;
    int len;

    key = hanja_list_get_nth_key (hangul->hanjaList, pos);
    value = hanja_list_get_nth_value (hangul->hanjaList, pos);

    key_len = fcitx_utf8_strlen(key);
    preedit_len = ustring_length(hangul->preedit);

    len = (key_len < preedit_len) ? key_len : preedit_len;
    ustring_erase (hangul->preedit, 0, len);
    if (key_len > preedit_len)
        hangul_ic_reset (hangul->ic);

    FcitxInstanceCommitString(hangul->owner, FcitxInstanceGetCurrentIC(hangul->owner), value);
    return IRV_DISPLAY_CANDWORDS;
}

/**
 * @brief initialize the extra input method
 *
 * @param arg
 * @return successful or not
 **/
void* FcitxHangulCreate (FcitxInstance* instance)
{
    FcitxHangul* hangul = (FcitxHangul*) fcitx_utils_malloc0(sizeof(FcitxHangul));
    bindtextdomain("fcitx-hangul", LOCALEDIR);
    hangul->owner = instance;
    if (!LoadHangulConfig(&hangul->fh))
    {
        free(hangul);
        return NULL;
    }
    
    hangul->conv = iconv_open("UTF-8", "UCS-4LE");
    hangul->preedit = ustring_new();
    
    ConfigHangul(hangul);
    
    hangul->table = hanja_table_load(NULL);
    char* path;
    FILE* fp = FcitxXDGGetFileWithPrefix("hangul", "symbol.txt", "r", &path);
    if (fp)
        fclose(fp);
    hangul->symbolTable = hanja_table_load ( path );
    
    free(path);


    hangul->ic = hangul_ic_new(hangul->fh.keyboaryLayout);
    
    char* retFile;
    fp = FcitxXDGGetFileWithPrefix("hangul", "hangul.png", "r", &retFile);
    if (fp)
        fclose(fp);
    if (!retFile)
        retFile = strdup("hangul");
    
    FcitxInstanceRegisterIM(instance,
                    hangul,
                    "hangul",
                    _("Hangul"),
                    retFile,
                    FcitxHangulInit,
                    FcitxHangulReset,
                    FcitxHangulDoInput,
                    FcitxHangulGetCandWords,
                    NULL,
                    NULL,
                    ReloadConfigFcitxHangul,
                    NULL,
                    5,
                    "ko_KR"
                   );
    
    free(retFile);
    
    FcitxIMEventHook hk;
    hk.arg = hangul;
    hk.func = FcitxHangulResetEvent;
    
    FcitxInstanceRegisterResetInputHook(instance, hk);
    
    FcitxUIRegisterStatus(
        instance,
        hangul,
        "hanja",
        "Hanja Lock",
        "Hanja Lock",
        FcitxHangulToggleHanja,
        FcitxHangulGetHanja
    );
    
    return hangul;
}

void FcitxHangulToggleHanja(void* arg)
{
    FcitxHangul* hangul = (FcitxHangul*) arg;
    hangul->fh.hanjaMode = !hangul->fh.hanjaMode;
    SaveHangulConfig(&hangul->fh);
}

boolean FcitxHangulGetHanja(void* arg)
{
    FcitxHangul* hangul = (FcitxHangul*) arg;
    return hangul->fh.hanjaMode;
}

/**
 * @brief Destroy the input method while unload it.
 *
 * @return int
 **/
void FcitxHangulDestroy (void* arg)
{
    FcitxHangul* hangul = (FcitxHangul*) arg;
    hanja_table_delete(hangul->table);
    
    hanja_table_delete(hangul->symbolTable);
    free(arg);
}

/**
 * @brief Load the config file for fcitx-hangul
 *
 * @param Bool is reload or not
 **/
boolean LoadHangulConfig(FcitxHangulConfig* fs)
{
    FcitxConfigFileDesc *configDesc = GetHangulConfigDesc();
    if (!configDesc)
        return false;

    FILE *fp = FcitxXDGGetFileUserWithPrefix("conf", "fcitx-hangul.config", "r", NULL);

    if (!fp)
    {
        if (errno == ENOENT)
            SaveHangulConfig(fs);
    }
    FcitxConfigFile *cfile = FcitxConfigParseConfigFileFp(fp, configDesc);

    FcitxHangulConfigConfigBind(fs, cfile, configDesc);
    FcitxConfigBindSync(&fs->gconfig);
    
    if (fp)
        fclose(fp);    
    return true;
}

void ConfigHangul(FcitxHangul* hangul)
{
}

void ReloadConfigFcitxHangul(void* arg)
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
    FcitxConfigFileDesc *configDesc = GetHangulConfigDesc();
    FILE *fp = FcitxXDGGetFileUserWithPrefix("conf", "fcitx-hangul.config", "w", NULL);
    FcitxConfigSaveConfigFileFp(fp, &fa->gconfig, configDesc);
    if (fp)
        fclose(fp);
}

void FcitxHangulResetEvent(void* arg)
{
    FcitxHangul* hangul = (FcitxHangul*) arg;
    FcitxIM* im = FcitxInstanceGetCurrentIM(hangul->owner);
    if (!im || strcmp(im->uniqueName, "hangul") != 0) {
        FcitxUISetStatusVisable(hangul->owner, "hanja", false);
    }
    else {
        FcitxUISetStatusVisable(hangul->owner, "hanja", true);
    }
}
