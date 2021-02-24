/*
 *  BREADBOARD (2020)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  User interface related functions and data.
 *  Acts as a front for the Atra Engine GUI system.
 *
 *  */

#ifndef UI_H
#define UI_H

#include <cglm/cglm.h>

#include "util/util.h"
#include "util/list.h"
#include "gui.h"

#define FILE_CIRCUIT_DIR "save/circuits"
#define FILE_EXT         "bcf"

enum confirmbox_state {
    CONFIRM_NONE,
    CONFIRM_BUT1,
    CONFIRM_BUT2,
    CONFIRM_BUT3
};

typedef struct FileList {
    uint toprow;
    uint select_enabled;
    uint select_index;
    List *file_elems;
    List *pack_elems;
    List *paths;
} FileList;

struct Package;

void ui_init(void);
void ui_shutdown(void);

void ui_packsave_show(void);
void ui_packsave_hide(void);

void ui_packload_addpack(const char *path);
void ui_packload_show(void);
void ui_packload_hide(void);
uint ui_packload_isopen(void);

void ui_filesave_settext(const char *);
char * ui_filesave_gettext(void);
void ui_filesave_show(void);
void ui_filesave_hide(void);

void ui_fileload_show(void);
void ui_fileload_hide(void);

void ui_tooltip_settext(const char *text);
void ui_tooltip_settextcolor(vec4 color);
void ui_tooltip_configtext(GUIElement *elem);
void ui_tooltip_show(void);
void ui_tooltip_hide(void);

void ui_dialogbox_settext(const char *text);
void ui_dialogbox_show(void);
void ui_dialogbox_hide(void);

void ui_confirmbox_settext(const char *text);
void ui_confirmbox_but1_settext(const char *text);
void ui_confirmbox_but2_settext(const char *text);
void ui_confirmbox_but3_settext(const char *text);
void ui_confirmbox_but3_show(void);
void ui_confirmbox_but3_hide(void);
void ui_confirmbox_but1_setfunc(void (*func)(GUIElement *elem, ...));
void ui_confirmbox_but2_setfunc(void (*func)(GUIElement *elem, ...));
void ui_confirmbox_but3_setfunc(void (*func)(GUIElement *elem, ...));
void ui_confirmbox_show(void);
void ui_confirmbox_hide(void);

void ui_confirmbox_filesave_overwrite(void);
void ui_confirmbox_packsave_overwrite(void);
void ui_confirmbox_fileload_preloadsave(void);
void ui_confirmbox_fileload_deletionconfirm(void);

void ui_iorename_show(void);
void ui_iorename_hide(void);
void ui_settargetpack(struct Package *pack);

FileList * ui_getfilelist(void);

#endif /* UI_H */
