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
#include "keyboard.h"

#define MAX_LENGTH 40

FCITX_EXPORT_API
FcitxIMClass ime = {
    FcitxHangulCreate,
    FcitxHangulDestroy
};
FCITX_EXPORT_API
int ABI_VERSION = FCITX_ABI_VERSION;

static const FcitxHotkey FCITX_HANGUL_GRAVE[2] = {
    {NULL, FcitxKey_grave, FcitxKeyState_None},
    {NULL, 0, 0},
};

CONFIG_DESC_DEFINE(GetHangulConfigDesc, "fcitx-hangul.desc")

boolean LoadHangulConfig(FcitxHangulConfig* fs);
static void SaveHangulConfig(FcitxHangulConfig* fs);
static void ConfigHangul(FcitxHangul* hangul);
static void FcitxHangulUpdatePreedit(FcitxHangul* hangul);
static void FcitxHangulCleanLookupTable(FcitxHangul* hangul);
static void FcitxHangulUpdateLookupTable(FcitxHangul* hangul, boolean checkSurrounding);
static void FcitxHangulFlush(FcitxHangul* hangul);
static void FcitxHangulToggleHanja(void* arg);
static boolean FcitxHangulGetHanja(void* arg);
static void FcitxHangulResetEvent(void* arg);
static char* FcitxHangulUcs4ToUtf8(FcitxHangul* hangul, const ucschar* ucsstr, int length);
static void FcitxHangulUpdateHanjaStatus(FcitxHangul* hangul);

static inline void FcitxHangulFreeHanjaList(FcitxHangul* hangul) {
    if (hangul->hanjaList) {
        hanja_list_delete (hangul->hanjaList);
        hangul->hanjaList = NULL;
    }
}

static inline size_t ucs4_strlen(const ucschar* str)
{
    size_t len = 0;
    while(*str) {
        len ++;
        str ++;
    }
    return len;
}

static boolean
FcitxHangulOnTransition (HangulInputContext     *hic,
                                  ucschar                 c,
                                  const ucschar          *preedit,
                                  void                   *data)
{
    FcitxHangul* hangul = (FcitxHangul*) data;
    if (!hangul->fh.autoReorder) {
        if (hangul_is_choseong (c)) {
            if (hangul_ic_has_jungseong (hic) || hangul_ic_has_jongseong (hic))
                return false;
        }

        if (hangul_is_jungseong (c)) {
            if (hangul_ic_has_jongseong (hic))
               return false;
        }
    }

    return true;
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
    if (hangul->hanjaList) {
        FcitxHangulCleanLookupTable(hangul);
    }
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
    FcitxInstance* instance = hangul->owner;

    if (FcitxHotkeyIsHotKey(sym, state, hangul->fh.hkHanjaMode)) {
        if (hangul->hanjaList == NULL) {
            FcitxHangulUpdateLookupTable(hangul, true);
        }
        else {
            FcitxHangulCleanLookupTable(hangul);
        }
        return IRV_DISPLAY_MESSAGE;
    }

    if (sym == FcitxKey_Shift_L || sym == FcitxKey_Shift_R)
        return IRV_TO_PROCESS;

    FcitxGlobalConfig* config = FcitxInstanceGetGlobalConfig(hangul->owner);
    const FcitxHotkey* prevPage = FcitxConfigPrevPageKey(instance, config);
    const FcitxHotkey* nextPage = FcitxConfigNextPageKey(instance, config);
    int s = hangul->fh.hkHanjaMode[0].state | hangul->fh.hkHanjaMode[1].state
          | config->prevWord[0].state | config->prevWord[1].state
          | config->nextWord[0].state | config->nextWord[1].state
          | prevPage[0].state | prevPage[1].state
          | nextPage[0].state | nextPage[1].state;

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

    FcitxInputState* input = FcitxInstanceGetInputState(hangul->owner);
    FcitxCandidateWordList* candList = FcitxInputStateGetCandidateList(input);
    int candSize = FcitxCandidateWordGetListSize(candList);

    if (candSize > 0) {
        if (FcitxHotkeyIsHotKey(sym, state, prevPage)) {
            if (FcitxCandidateWordHasPrev(candList)) {
                FcitxCandidateWordGetFocus(candList, true);
            }
            if (FcitxCandidateWordGoPrevPage(candList)) {
                FcitxCandidateWordSetType(FcitxCandidateWordGetByIndex(candList, 0), MSG_CANDIATE_CURSOR);
                return IRV_FLAG_UPDATE_INPUT_WINDOW;
            } else {
                return IRV_DO_NOTHING;
            }
        } else if (FcitxHotkeyIsHotKey(sym, state, nextPage)) {
            if (FcitxCandidateWordHasNext(candList)) {
                FcitxCandidateWordGetFocus(candList, true);
            }
            if (FcitxCandidateWordGoNextPage(candList)) {
                FcitxCandidateWordSetType(FcitxCandidateWordGetByIndex(candList, 0), MSG_CANDIATE_CURSOR);
                return IRV_FLAG_UPDATE_INPUT_WINDOW;
            } else {
                return IRV_DO_NOTHING;
            }
        }

        FcitxCandidateWord* candWord = NULL;
        if (FcitxHotkeyIsHotKey(sym, state, config->nextWord)) {
            candWord = FcitxCandidateWordGetFocus(candList, true);
            candWord = FcitxCandidateWordGetNext(candList, candWord);
            if (!candWord) {
                FcitxCandidateWordSetPage(candList, 0);
                candWord = FcitxCandidateWordGetCurrentWindow(candList);
            } else {
                FcitxCandidateWordSetFocus(
                    candList, FcitxCandidateWordGetIndex(candList,
                                                        candWord));
            }
        } else if (FcitxHotkeyIsHotKey(sym, state, config->prevWord)) {
            candWord = FcitxCandidateWordGetFocus(candList, true);
            candWord = FcitxCandidateWordGetPrev(candList, candWord);
            if (!candWord) {
                candWord = FcitxCandidateWordGetLast(candList);
            }
            FcitxCandidateWordSetFocus(
                candList, FcitxCandidateWordGetIndex(candList,
                                                     candWord));
        }
        if (candWord) {
            FcitxCandidateWordSetType(candWord, MSG_CANDIATE_CURSOR);
            return IRV_FLAG_UPDATE_INPUT_WINDOW;
        }

        if (FcitxHotkeyIsHotKeyDigit(sym, state))
            return IRV_TO_PROCESS;

        if (FcitxHotkeyIsHotKey(sym, state , FCITX_ENTER)) {
            do {
                candWord = FcitxCandidateWordGetFocus(candList, true);
                if (!candWord) {
                    break;
                }
                // FcitxLog(INFO, "%d", FcitxCandidateWordGetIndex(candList, candWord));
                return FcitxCandidateWordChooseByTotalIndex(candList,
                                                            FcitxCandidateWordGetIndex(candList, candWord));
            } while(0);
            return FcitxCandidateWordChooseByIndex(candList, 0);
        }

        if (!hangul->fh.hanjaMode) {
            FcitxHangulCleanLookupTable(hangul);
        }
    }

    s = FcitxKeyState_Ctrl | FcitxKeyState_Alt | FcitxKeyState_Shift | FcitxKeyState_Super | FcitxKeyState_Hyper;
    if (state & s) {
        FcitxHangulFlush (hangul);
        FcitxHangulUpdatePreedit(hangul);
        FcitxUIUpdateInputWindow(hangul->owner);
        return IRV_TO_PROCESS;
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
        if (ustring_length(hangul->preedit) >= MAX_LENGTH) {
            FcitxHangulFlush(hangul);
        }

        keyUsed = hangul_ic_process(hangul->ic, sym);
        boolean notFlush = false;

        const ucschar* str = hangul_ic_get_commit_string (hangul->ic);
        if (hangul->fh.wordCommit || hangul->fh.hanjaMode) {
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
                        size_t len = fcitx_utf8_strlen(commit);
                        if (len > 0) {
                            char* p = fcitx_utf8_get_nth_char(commit, len - 1);
                            if ((strcmp(p, "`") == 0 && FcitxHotkeyIsHotKey(sym, state, FCITX_HANGUL_GRAVE))
                            || (strcmp(p, ";") == 0 && FcitxHotkeyIsHotKey(sym, state, FCITX_SEMICOLON))) {
                                keyUsed = false;
                                notFlush = true;
                                *p = 0;
                            }
                        }
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
                    if ((strcmp(commit, "`") == 0 && FcitxHotkeyIsHotKey(sym, state, FCITX_HANGUL_GRAVE))
                     || (strcmp(commit, ";") == 0 && FcitxHotkeyIsHotKey(sym, state, FCITX_SEMICOLON))) {
                        keyUsed = false;
                        notFlush = true;
                    }
                    else {
                        FcitxInstanceCommitString(hangul->owner, FcitxInstanceGetCurrentIC(hangul->owner), commit);
                    }
                    free(commit);
                }
            }
        }

        FcitxHangulGetCandWords(hangul);
        FcitxUIUpdateInputWindow(hangul->owner);
        if (!keyUsed && !notFlush)
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
    FcitxInputStateSetShowCursor(input, true);
    const ucschar *hic_preedit = hangul_ic_get_preedit_string (hangul->ic);

    char* pre1 = FcitxHangulUcs4ToUtf8(hangul, ustring_begin(hangul->preedit), ustring_length(hangul->preedit));
    char* pre2 = FcitxHangulUcs4ToUtf8(hangul, hic_preedit, -1);
    FcitxInputContext* ic = FcitxInstanceGetCurrentIC(hangul->owner);
    FcitxProfile* profile = FcitxInstanceGetProfile(hangul->owner);

    size_t preeditLen = 0;

    boolean clientPreeditNotAvail = (ic && ((ic->contextCaps & CAPACITY_PREEDIT) == 0 || !profile->bUsePreedit));

    if (pre1 && pre1[0] != 0) {
        size_t len1 = strlen(pre1);
        if (clientPreeditNotAvail)
            FcitxMessagesAddMessageAtLast(preedit, MSG_INPUT, "%s", pre1);
        FcitxMessagesAddMessageAtLast(clientPreedit, MSG_INPUT, "%s", pre1);
        preeditLen += len1;
    }

    if (pre2 && pre2[0] != '\0') {
        size_t len2 = strlen(pre2);
        if (clientPreeditNotAvail)
            FcitxMessagesAddMessageAtLast(preedit, MSG_INPUT | MSG_HIGHLIGHT, "%s", pre2);
        FcitxMessagesAddMessageAtLast(clientPreedit, MSG_INPUT | MSG_HIGHLIGHT, "%s", pre2);
        preeditLen += len2;
    }

    FcitxInputStateSetCursorPos(input, clientPreeditNotAvail ? preeditLen : 0);
    FcitxInputStateSetClientCursorPos(input, preeditLen);

    if (pre1)
        free(pre1);

    if (pre2)
        free(pre2);
}

HanjaList* FcitxHangulLookupTable(FcitxHangul* hangul, const char* key, int method)
{
    HanjaList* list;

    if (key == NULL)
        return NULL;

    switch (method) {
    case LOOKUP_METHOD_EXACT:
        if (hangul->symbolTable != NULL)
            list = hanja_table_match_exact (hangul->symbolTable, key);

        if (list == NULL)
            list = hanja_table_match_exact (hangul->table, key);

        break;
    case LOOKUP_METHOD_PREFIX:
        if (hangul->symbolTable != NULL)
            list = hanja_table_match_prefix (hangul->symbolTable, key);

        if (list == NULL)
            list = hanja_table_match_prefix (hangul->table, key);

        break;
    case LOOKUP_METHOD_SUFFIX:
        if (hangul->symbolTable != NULL)
            list = hanja_table_match_suffix (hangul->symbolTable, key);

        if (list == NULL)
            list = hanja_table_match_suffix (hangul->table, key);

        break;
    }

    return list;
}

#define FCITX_HANGUL_MAX(a, b) ((a) > (b)? (a) : (b))
#define FCITX_HANGUL_MIN(a, b) ((a) < (b)? (a) : (b))
#define FCITX_HANGUL_ABS(a) ((a) >= (0)? (a) : -(a))

static char*
GetSubstring (const char* str, long p1, long p2)
{
    const char* begin;
    const char* end;
    char* substring;
    long limit;
    long pos;
    long n;

    if (str == NULL || str[0] == '\0')
        return NULL;

    limit = strlen(str) + 1;

    p1 = FCITX_HANGUL_MAX(0, p1);
    p2 = FCITX_HANGUL_MAX(0, p2);

    pos = FCITX_HANGUL_MIN(p1, p2);
    n = FCITX_HANGUL_ABS(p2 - p1);

    if (pos + n > limit)
        n = limit - pos;

    begin = fcitx_utf8_get_nth_char ((char*)str, pos);
    end = fcitx_utf8_get_nth_char ((char*)begin, n);

    substring = strndup (begin, end - begin);
    return substring;
}

void FcitxHangulCleanLookupTable(FcitxHangul* hangul) {
    FcitxInstanceCleanInputWindowDown(hangul->owner);
    // FcitxUIUpdateInputWindow(hangul->owner);
    FcitxHangulFreeHanjaList(hangul);
}

void FcitxHangulUpdateLookupTable(FcitxHangul* hangul, boolean checkSurrounding)
{
    char* surroundingStr = NULL;
    char* utf8;
    char* hanjaKey = NULL;
    LookupMethod lookupMethod = LOOKUP_METHOD_PREFIX;
    const ucschar* hic_preedit;
    UString* preedit;
    unsigned int cursorPos;
    unsigned int anchorPos;

    FcitxHangulFreeHanjaList(hangul);

    hic_preedit = hangul_ic_get_preedit_string (hangul->ic);

    preedit = ustring_dup (hangul->preedit);
    ustring_append_ucs4 (preedit, hic_preedit);
    if (ustring_length(preedit) > 0) {
        utf8 = FcitxHangulUcs4ToUtf8 (hangul, ustring_begin(preedit), ustring_length(preedit));
        if (hangul->fh.wordCommit || hangul->fh.hanjaMode) {
            hanjaKey = utf8;
            lookupMethod = LOOKUP_METHOD_PREFIX;
        } else {
            char* substr;
            FcitxInstanceGetSurroundingText(hangul->owner, FcitxInstanceGetCurrentIC(hangul->owner), &surroundingStr, &cursorPos, &anchorPos);

            substr = GetSubstring (surroundingStr, (long) cursorPos - 64, cursorPos);

            if (substr != NULL) {
                asprintf(&hanjaKey, "%s%s", substr, utf8);
                free (utf8);
                free (substr);
            } else {
                hanjaKey = utf8;
            }
            lookupMethod = LOOKUP_METHOD_SUFFIX;
        }
    } else {
        if (checkSurrounding) {
            FcitxInstanceGetSurroundingText(hangul->owner, FcitxInstanceGetCurrentIC(hangul->owner), &surroundingStr, &cursorPos, &anchorPos);
            if (cursorPos != anchorPos) {
                // If we have selection in surrounding text, we use that.
                hanjaKey = GetSubstring(surroundingStr, cursorPos, anchorPos);
                lookupMethod = LOOKUP_METHOD_EXACT;
            } else {
                hanjaKey = GetSubstring (surroundingStr, (long) cursorPos - 64, cursorPos);
                lookupMethod = LOOKUP_METHOD_SUFFIX;
            }
        }
    }

    if (hanjaKey != NULL) {
        hangul->hanjaList = FcitxHangulLookupTable (hangul, hanjaKey, lookupMethod);
        hangul->lastLookupMethod = lookupMethod;
        free (hanjaKey);
    }
    ustring_delete (preedit);

    if (surroundingStr)
        free(surroundingStr);

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
                word.wordType = (i == 0) ? MSG_CANDIATE_CURSOR : MSG_INPUT;
                word.strExtra = NULL;
                word.extraType = MSG_INPUT;
                word.priv = idx;
                word.owner = hangul;
                word.callback = FcitxHangulGetCandWord;
                FcitxCandidateWordAppend(candList, &word);
            }

            FcitxCandidateWordSetFocus(candList, 0);
        }
    }
}

void FcitxHangulFlush(FcitxHangul* hangul)
{
    const ucschar *str;

    FcitxHangulCleanLookupTable(hangul);

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
    boolean flag = true;
    FcitxInstanceSetContext(hangul->owner, CONTEXT_IM_KEYBOARD_LAYOUT, "us");
    FcitxInstanceSetContext(hangul->owner, CONTEXT_DISABLE_AUTO_FIRST_CANDIDATE_HIGHTLIGHT, &flag);
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
        FcitxHangulUpdateLookupTable(hangul, false);
    } else {
        FcitxHangulCleanLookupTable(hangul);
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
    const ucschar* hic_preedit;
    int key_len;
    int preedit_len;
    int hic_preedit_len;

    key = hanja_list_get_nth_key (hangul->hanjaList, pos);
    value = hanja_list_get_nth_value (hangul->hanjaList, pos);
    hic_preedit = hangul_ic_get_preedit_string (hangul->ic);

    if (!key || !value || !hic_preedit)
        return IRV_CLEAN;

    // FcitxLog(INFO, "%s", key);
    key_len = fcitx_utf8_strlen(key);
    preedit_len = ustring_length(hangul->preedit);
    hic_preedit_len = ucs4_strlen (hic_preedit);

    boolean surrounding = false;
    if (hangul->lastLookupMethod == LOOKUP_METHOD_PREFIX) {
        if (preedit_len == 0 && hic_preedit_len == 0) {
            /* remove surrounding_text */
            if (key_len > 0) {
                FcitxInstanceDeleteSurroundingText (hangul->owner, FcitxInstanceGetCurrentIC(hangul->owner), -key_len , key_len);
                surrounding = true;
            }
        } else {
            /* remove preedit text */
            if (key_len > 0) {
                long n = FCITX_HANGUL_MIN(key_len, preedit_len);
                ustring_erase (hangul->preedit, 0, n);
                key_len -= preedit_len;
            }

            /* remove hic preedit text */
            if (key_len > 0) {
                hangul_ic_reset (hangul->ic);
                key_len -= hic_preedit_len;
            }
        }
    } else {
        /* remove hic preedit text */
        if (hic_preedit_len > 0) {
            hangul_ic_reset (hangul->ic);
            key_len -= hic_preedit_len;
        }

        /* remove ibus preedit text */
        if (key_len > preedit_len) {
            ustring_erase (hangul->preedit, 0, preedit_len);
            key_len -= preedit_len;
        } else if (key_len > 0) {
            ustring_erase (hangul->preedit, 0, key_len);
            key_len = 0;
        }

        /* remove surrounding_text */
        if (LOOKUP_METHOD_EXACT != hangul->lastLookupMethod && key_len > 0) {
            FcitxInstanceDeleteSurroundingText (hangul->owner, FcitxInstanceGetCurrentIC(hangul->owner), -key_len , key_len);
            surrounding = true;
        }
    }

    FcitxInstanceCommitString(hangul->owner, FcitxInstanceGetCurrentIC(hangul->owner), value);
    if (surrounding) {
        FcitxInstanceCleanInputWindowUp(hangul->owner);
        FcitxHangulCleanLookupTable(hangul);
        return IRV_DISPLAY_MESSAGE;
    }
    else {
        return IRV_DISPLAY_CANDWORDS;
    }
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
    hangul->lastLookupMethod = LOOKUP_METHOD_PREFIX;
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

    hangul->ic = hangul_ic_new(keyboard[hangul->fh.keyboardLayout]);
    hangul_ic_connect_callback (hangul->ic, "transition",
                                FcitxHangulOnTransition, hangul);

    FcitxIMIFace iface;
    memset(&iface, 0, sizeof(FcitxIMIFace));
    iface.Init = FcitxHangulInit;
    iface.ResetIM = FcitxHangulReset;
    iface.DoInput = FcitxHangulDoInput;
    iface.GetCandWords = FcitxHangulGetCandWords;
    iface.ReloadConfig = ReloadConfigFcitxHangul;
    iface.OnClose = FcitxHangulOnClose;

    FcitxInstanceRegisterIMv2(instance,
                    hangul,
                    "hangul",
                    _("Hangul"),
                    "hangul",
                    iface,
                    5,
                    "ko"
                   );

    FcitxIMEventHook hk;
    hk.arg = hangul;
    hk.func = FcitxHangulResetEvent;

    FcitxInstanceRegisterResetInputHook(instance, hk);

    FcitxUIRegisterStatus(
        instance,
        hangul,
        "hanja",
        "",
        "",
        FcitxHangulToggleHanja,
        FcitxHangulGetHanja
    );

    FcitxHangulUpdateHanjaStatus(hangul);

    return hangul;
}

void FcitxHangulOnClose(void* arg, FcitxIMCloseEventType event)
{
    FcitxHangul* hangul = arg;
    if (event == CET_LostFocus) {
    } else if (event == CET_ChangeByInactivate) {
        FcitxHangulFlush(hangul);
    } else if (event == CET_ChangeByUser) {
        FcitxHangulFlush(hangul);
    }
}


void FcitxHangulToggleHanja(void* arg)
{
    FcitxHangul* hangul = (FcitxHangul*) arg;
    hangul->fh.hanjaMode = !hangul->fh.hanjaMode;
    FcitxHangulUpdateHanjaStatus(hangul);
    SaveHangulConfig(&hangul->fh);
}

void FcitxHangulUpdateHanjaStatus(FcitxHangul* hangul)
{
    if (hangul->fh.hanjaMode) {
        FcitxUISetStatusString(hangul->owner, "hanja", "\xe9\x9f\x93", _("Use Hanja"));
    }
    else {
        FcitxUISetStatusString(hangul->owner, "hanja", "\xed\x95\x9c", _("Use Hangul"));
    }
    FcitxHangulFlush(hangul);
    FcitxHangulUpdatePreedit(hangul);
    FcitxUIUpdateInputWindow(hangul->owner);
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
    FcitxLog(DEBUG, "Hangul Layout: %s", keyboard[hangul->fh.keyboardLayout]);
    hangul_ic_select_keyboard(hangul->ic, keyboard[hangul->fh.keyboardLayout]);
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
