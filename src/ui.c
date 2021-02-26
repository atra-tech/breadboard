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

#include "ui.h"
#include "gui.h"
#include "input.h"
#include "engine.h"
#include "render.h"
#include "core.h"
#include "logic.h"
#include "world.h"
#include "file.h"
#include "log.h"

#include <string.h>

/* General callbacks */
static void ui_button_mouseup(GUIElement *elem, ...);
static void ui_button_mouseover(GUIElement *elem, ...);
static void ui_button_mouseleave(GUIElement *elem, ...);

static void ui_updatefunc_centertext(GUIElement *elem, float dt, ...);
static void ui_updatefunc_heightfittext(GUIElement *elem, float dt, ...);
static void ui_textfield_gainfocus(GUIElement *elem, ...);
static void ui_textfield_losefocus(GUIElement *elem, ...);

/* User-defined pack saving functions */
static void ui_packsave_init(void);
static void ui_packsave_save_mousedown(GUIElement *elem, ...);
static void ui_packsave_cancel_mousedown(GUIElement *elem, ...);

/* User-defined pack menu functions */
static void ui_packload_init(void);
static GUIElement * ui_packload_entry_init(const char *name, GUIElement *parent);
static void ui_packload_entry_mousedown(GUIElement *elem, ...);

/* File saving interface functions */
static void ui_filesave_init(void);
static void ui_filesave_save_mousedown(GUIElement *elem, ...);
static void ui_filesave_cancel_mousedown(GUIElement *elem, ...);

/* File loading interface functions */
static void ui_fileload_init(void);
static GUIElement * ui_fileload_entry_init(const char *name, GUIElement *parent);
static void ui_fileload_entry_mousedown(GUIElement *elem, ...);
static void ui_fileload_entry_mouseup(GUIElement *elem, ...);
static void ui_fileload_entry_mouseover(GUIElement *elem, ...);
static void ui_fileload_entry_mouseleave(GUIElement *elem, ...);
static void ui_fileload_open_mousedown(GUIElement *elem, ...);
static void ui_fileload_delete_mousedown(GUIElement *elem, ...);
static void ui_fileload_cancel_mousedown(GUIElement *elem, ...);

/* Tooltip functions */
static void ui_tooltip_init(void);
static void ui_tooltip_update(GUIElement *elem, float dt, ...);

/* Dialog box functions */
static void ui_dialogbox_init(void);
static void ui_dialogbox_mousedown(GUIElement *elem, ...);

/* Confirm box functions */
static void ui_confirmbox_init(void);

static void ui_confirmbox_filesave_overwrite_save(GUIElement *, ...);
static void ui_confirmbox_filesave_overwrite_cancel(GUIElement *, ...);

static void ui_confirmbox_packsave_overwrite_save(GUIElement *, ...);
static void ui_confirmbox_packsave_overwrite_cancel(GUIElement *, ...);

static void ui_confirmbox_fileload_preloadsave_yes(GUIElement *, ...);
static void ui_confirmbox_fileload_preloadsave_no(GUIElement *, ...);
static void ui_confirmbox_fileload_preloadsave_cancel(GUIElement *, ...);

static void ui_confirmbox_fileload_deletionconfirm_delete(GUIElement *, ...);
static void ui_confirmbox_fileload_deletionconfirm_cancel(GUIElement *, ...);

/* Input/Output pack renaming functions */
static void ui_iorename_init(void);
static void ui_iorename_confirm_mousedown(GUIElement *, ...);
static void ui_iorename_cancel_mousedown(GUIElement *, ...);

/* Hide/Show element functions */
static void ui_elem_show(const char *elem_id);
static void ui_elem_hide(const char *elem_id);

/* File list functions */
static void ui_filelist_init(void);
static void ui_filelist_populate(void);
static void ui_filelist_clear(void);
static void ui_filelist_deletefile(uint index);
static void ui_filelist_openfile(uint index);
static void ui_filelist_openpack(uint index);
static void ui_filelist_scrollup(GUIElement *elem, ...);
static void ui_filelist_scrolldown(GUIElement *elem, ...);
static void ui_filelist_elems_calcpos(void);
static void ui_filelist_delete(void);

static void ui_settargetpath(const char *file);

static Camera *uicam;
static FileList files;
static SelectBox *selbox;
static int confirmbox_result;
static int packload_isopen;
static int newfile_request;
static char *target_path;
static Package *target_pack;

void
ui_init(void)
{
    uicam = guigetcamera();
    selbox = selboxget();
    confirmbox_result = 0;
    packload_isopen = 0;
    newfile_request = 0;

    ui_packsave_init();
    ui_packload_init();
    ui_filesave_init();
    ui_fileload_init();
    ui_tooltip_init();
    ui_dialogbox_init();
    ui_confirmbox_init();
    ui_iorename_init();
    
    ui_filelist_init();
}

void
ui_shutdown(void)
{
    if (target_path) {
        free(target_path);
    }
    ui_filelist_delete();
}

/* 
 *  User-defined package selection menu
 *
 * */

void
ui_packsave_show(void)
{
    ui_elem_show("ui_packsave_base");
}

void
ui_packsave_hide(void)
{
    ui_elem_hide("ui_packsave_base");
}

/* 
 *  User-defined package selection menu
 *
 *  */

void
ui_packload_show(void)
{
    ui_elem_show("ui_packload_base");
    ui_filelist_elems_calcpos();
    packload_isopen = 1;
}

void
ui_packload_hide(void)
{
    ui_elem_hide("ui_packload_base");
    packload_isopen = 0;
}

uint
ui_packload_isopen(void)
{
    return packload_isopen;
}

/*
 *  File saving interface
 *
 * */

void
ui_filesave_show(void)
{
    /* TODO: Load text box with current world's name, if it has one */

    GUIElement *textbox;
    World *world;

    world = worldtop();
    textbox = guigetelem("ui_filesave_text");

    if (world->name) {
        guisetgridtext(textbox, world->name);
    }

    ui_elem_show("ui_filesave_base");
}

void
ui_filesave_hide(void)
{
    ui_elem_hide("ui_filesave_base");
}


/*
 *  File loading interface
 *
 * */

void
ui_fileload_show(void)
{
    ui_elem_show("ui_fileload_base");
    ui_filelist_elems_calcpos();
}

void
ui_fileload_hide(void)
{
    ui_elem_hide("ui_fileload_base");
}


/*
 *  Mouse-bound info tooltip
 *
 * */

void
ui_tooltip_settext(const char *text)
{
    guisetgridtext(guigetelem("ui_tooltip_text"), text);
}

void
ui_tooltip_settextcolor(vec4 color)
{
    uint i;
    GUIElement *elem;
    GUIGlyph *glyph;

    elem = guigetelem("ui_tooltip_text");
    
    for (i = 0; i < elem->textgrid->glyphs->size; i++) {
        glyph = listat(elem->textgrid->glyphs, i);
        memcpy(glyph->color, color, sizeof(vec4));
    }
}

void
ui_tooltip_configtext(GUIElement *elem)
{
    const char *elem_ids[] = {
        "toolbar_not",
        "toolbar_or",
        "toolbar_and",
        "toolbar_xor",
        "toolbar_red",
        "toolbar_green",
        "toolbar_blue",
        "toolbar_input",
        "toolbar_output",
        "toolbar_clock",
        "toolbar_memory",
        "toolbar_connector",
        "filebar_new",
        "filebar_open",
        "filebar_save",
        "packbar_open",
        "packbar_save"
    };

    const char *elem_text[] = {
        "NOT Gate",
        "OR Gate",
        "AND Gate",
        "XOR Gate",
        "Red wire",
        "Green wire",
        "Blue wire",
        "Input node",
        "Output node",
        "Clock",
        "Memory (8-bit)",
        "Connector",
        "New file",
        "Open file",
        "Save file",
        "Open package",
        "Save package"
    };

    const uint size = 17;

    uint i;
    GUIElement *elem_tmp;

    for (i = 0; i < size; i++) {
        elem_tmp = guigetelem(elem_ids[i]);

        if (elem == elem_tmp) {
            ui_tooltip_settext(elem_text[i]);
            break;
        }
    }
}

void
ui_tooltip_show(void)
{
    ui_elem_show("ui_tooltip_base");
}

void
ui_tooltip_hide(void)
{
    ui_elem_hide("ui_tooltip_base");
}


/*
 *  Dialog box for urgent messages
 *
 * */

void
ui_dialogbox_settext(const char *text)
{
    guisetgridtext(guigetelem("ui_dialog_text"), text);
}

void
ui_dialogbox_show(void)
{
    ui_elem_show("ui_dialog_base");
}

void
ui_dialogbox_hide(void)
{
    ui_elem_hide("ui_dialog_base");
}

void
ui_confirmbox_settext(const char *text)
{
    guisetgridtext(guigetelem("ui_confirmbox_text"), text);
}

/* Confirm Box functions */

void
ui_confirmbox_but1_settext(const char *text)
{
    guisetgridtext(guigetelem("ui_confirmbox_but1_text"), text);
}

void
ui_confirmbox_but2_settext(const char *text)
{
    guisetgridtext(guigetelem("ui_confirmbox_but2_text"), text);
}

void
ui_confirmbox_but3_settext(const char *text)
{
    guisetgridtext(guigetelem("ui_confirmbox_but3_text"), text);
}

void
ui_confirmbox_but3_show(void)
{
    GUIElement *but1, *but2;

    but1 = guigetelem("ui_confirmbox_but1");
    but2 = guigetelem("ui_confirmbox_but2");
    
    but1->x = 0.1f;
    but2->x = 0.4f;

    ui_elem_show("ui_confirmbox_but3");
}

void
ui_confirmbox_but3_hide(void)
{
    GUIElement *but1, *but2;

    but1 = guigetelem("ui_confirmbox_but1");
    but2 = guigetelem("ui_confirmbox_but2");

    but1->x = 0.2f;
    but2->x = 0.6f;

    ui_elem_hide("ui_confirmbox_but3");
}

void
ui_confirmbox_but1_setfunc(void (*func)(GUIElement *elem, ...))
{
    guigetelem("ui_confirmbox_but1")->callbacks[GUI_EVENT_MOUSEDOWN] = func;
}

void
ui_confirmbox_but2_setfunc(void (*func)(GUIElement *elem, ...))
{
    guigetelem("ui_confirmbox_but2")->callbacks[GUI_EVENT_MOUSEDOWN] = func;
}

void
ui_confirmbox_but3_setfunc(void (*func)(GUIElement *elem, ...))
{
    guigetelem("ui_confirmbox_but3")->callbacks[GUI_EVENT_MOUSEDOWN] = func;
}

void
ui_confirmbox_show(void)
{
    ui_elem_show("ui_confirmbox_base");
}

void
ui_confirmbox_hide(void)
{
    ui_elem_hide("ui_confirmbox_base");
}

void
ui_iorename_show(void)
{
    GUIElement *textbox;

    textbox = guigetelem("ui_iorename_text");
    guisetgridtext(textbox, target_pack->name);

    ui_elem_show("ui_iorename_base");
}

void
ui_iorename_hide(void)
{
    ui_elem_hide("ui_iorename_base");
}

/*
 *  Initial setup functions for UI layers and callback bindings
 *
 * */

void
ui_packsave_init(void)
{
    GUILayer *layer;
    GUIElement *base, *textfield, *msg, 
               *savebutton, *savebutton_text,
               *cancelbutton, *cancelbutton_text;

    layer = guilayercreate(
            "res/shader/guivert.glsl",
            "res/shader/guifrag.glsl",
            "res/texture/gui.png");

    base = guielemcreate(
            0.2f, 0.5f, 0.6f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER |
            GUI_ATTR_BLOCKING,
            "ui_packsave_base");

    msg = guielemcreate(
            0.1f, 0.8f, 0.8f, 0.1f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT,
            "ui_packsave_msg");

    textfield = guielemcreate(
            0.1f, 0.5f, 0.8f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER |
            GUI_ATTR_HASTEXT |
            GUI_ATTR_TEXTINPUT,
            "ui_packsave_text");

    savebutton = guielemcreate(
            0.2f, 0.1f, 0.2f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_CLICKABLE |
            GUI_ATTR_HASTEXTURE,
            "ui_packsave_button_save");

    savebutton_text = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_packsave_button_save_text");

    cancelbutton = guielemcreate(
            0.6f, 0.1f, 0.2f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_CLICKABLE |
            GUI_ATTR_HASTEXTURE,
            "ui_packsave_button_cancel");

    cancelbutton_text = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_packsave_button_cancel_text");

    guilinklayer(base, layer);
    guienableratioresizing(base, 0.3);
    guienabletextlayer(layer,
            "res/shader/fontvert.glsl",
            "res/shader/fontfrag.glsl",
            "res/font/maledict_12x12.png",
            12, 12);

    guitextgridadd(guitextgridcreate(12, 12, 0, 0), msg);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), textfield);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), savebutton_text);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), cancelbutton_text);
    guisetgridtext(msg, "Circuit package name");
    guisetgridtext(textfield, "");
    guisetgridtext(savebutton_text, "SAVE");
    guisetgridtext(cancelbutton_text, "CANCEL");
    
    guiaddelem(base, msg);
    guiaddelem(base, textfield);
    guiaddelem(base, savebutton);
    guiaddelem(base, cancelbutton);
    guiaddelem(savebutton, savebutton_text);
    guiaddelem(cancelbutton, cancelbutton_text);

    guisetbordersize(base, 4);
    guisetbacktexture(base, 0, 8, 1, 1);
    guisetedgetexture(base, 0, 2, 1, 1);
    guisetcornertexture(base, 0, 2, 1, 1);
    
    guisetbordersize(textfield, 8);
    guisetbacktexture(textfield, 0, 8, 1, 1);
    guisetedgetexture(textfield, 32, 0, 8, 8);
    guisetcornertexture(textfield, 40, 0, 8, 8);

    guisetbacktexture(savebutton, 0, 3, 1, 1);
    guisetbacktexture(cancelbutton, 0, 3, 1, 1);

    guisetcursortexture(textfield, 0, 0, 12, 12);

    guiseteventcallback(savebutton, GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(savebutton, GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(savebutton, GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(savebutton, GUI_EVENT_MOUSEDOWN, ui_packsave_save_mousedown);

    guiseteventcallback(cancelbutton, GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(cancelbutton, GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(cancelbutton, GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(cancelbutton, GUI_EVENT_MOUSEDOWN, ui_packsave_cancel_mousedown);

    guiseteventcallback(textfield, GUI_EVENT_GAINFOCUS, ui_textfield_gainfocus);
    guiseteventcallback(textfield, GUI_EVENT_LOSEFOCUS, ui_textfield_losefocus);

    guisetupdatefunc(msg, ui_updatefunc_heightfittext);
    guisetupdatefunc(textfield, ui_updatefunc_heightfittext);
    guisetupdatefunc(savebutton_text, ui_updatefunc_centertext);
    guisetupdatefunc(cancelbutton_text, ui_updatefunc_centertext);

    guihideelem(base);
}

void
ui_packload_init(void)
{
    GUILayer *layer;
    GUIElement *base, *filelist;

    layer = guilayercreate(
            "res/shader/guivert.glsl",
            "res/shader/guifrag.glsl",
            "res/texture/gui.png");

    base = guielemcreate(
            0.8f, 0.5f, 0.2f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER,
            "ui_packload_base");

    filelist = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            0,
            "ui_packload_filelist");

    guilinklayer(base, layer);
    guienableratioresizing(base, 1.5f);
    guienabletextlayer(layer,
            "res/shader/fontvert.glsl",
            "res/shader/fontfrag.glsl",
            "res/font/maledict_12x12.png",
            12, 12);

    /* Run file list populating subroutine */

    /* Add update function which adjusts y to fit to top bar */
    guiaddelem(base, filelist);

    guisetbordersize(base, 4);
    guisetbacktexture(base, 0, 8, 1, 1);
    guisetedgetexture(base, 0, 2, 1, 1);
    guisetcornertexture(base, 0, 2, 1, 1);
    
    guiseteventcallback(filelist, GUI_EVENT_SCROLLUP, ui_filelist_scrollup);
    guiseteventcallback(filelist, GUI_EVENT_SCROLLDN, ui_filelist_scrolldown);
    
    guihideelem(base);
}

GUIElement *
ui_packload_entry_init(const char *name, GUIElement *parent)
{
    const char prefix[] = "ui_packload_entry_";
    const char base_suffix[] = "_base";
    const char text_suffix[] = "_text";
    
    GUIElement *entry, *text;
    char *elemname;
    uint len;

    len = strlen(name) + 18 + 5;
    elemname = malloc((len + 1) * sizeof(char));
    sprintf(elemname, "%s%s%s", prefix, name, base_suffix);

    entry = guielemcreate(
            0.0f, 0.0f, 1.0f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            elemname);
    
    sprintf(elemname, "%s%s%s", prefix, name, text_suffix);

    text = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            elemname);

    guiaddelem(parent, entry);
    guiaddelem(entry, text);
    
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), text);
    guisetgridtext(text, name);

    guisetbacktexture(entry, 0, 3, 1, 1);

    guiseteventcallback(entry, GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(entry, GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(entry, GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(entry, GUI_EVENT_MOUSEDOWN, ui_packload_entry_mousedown);
    guiseteventcallback(entry, GUI_EVENT_SCROLLUP, ui_filelist_scrollup);
    guiseteventcallback(entry, GUI_EVENT_SCROLLDN, ui_filelist_scrolldown);
    
    guisetupdatefunc(text, ui_updatefunc_centertext);

    free(elemname);

    return entry;
}

void
ui_filesave_init(void)
{
    GUILayer *layer;
    GUIElement *base, *textfield, *msg, 
               *savebutton, *savebutton_text,
               *cancelbutton, *cancelbutton_text;

    layer = guilayercreate(
            "res/shader/guivert.glsl",
            "res/shader/guifrag.glsl",
            "res/texture/gui.png");

    base = guielemcreate(
            0.2f, 0.5f, 0.6f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER |
            GUI_ATTR_BLOCKING,
            "ui_filesave_base");

    msg = guielemcreate(
            0.1f, 0.8f, 0.8f, 0.1f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT,
            "ui_filesave_msg");

    textfield = guielemcreate(
            0.1f, 0.5f, 0.8f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER |
            GUI_ATTR_HASTEXT |
            GUI_ATTR_TEXTINPUT,
            "ui_filesave_text");

    savebutton = guielemcreate(
            0.2f, 0.1f, 0.2f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_CLICKABLE |
            GUI_ATTR_HASTEXTURE,
            "ui_filesave_button_save");

    savebutton_text = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_filesave_button_save_text");

    cancelbutton = guielemcreate(
            0.6f, 0.1f, 0.2f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_CLICKABLE |
            GUI_ATTR_HASTEXTURE,
            "ui_filesave_button_cancel");

    cancelbutton_text = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_filesave_button_cancel_text");

    guilinklayer(base, layer);
    guienableratioresizing(base, 0.3);
    guienabletextlayer(layer,
            "res/shader/fontvert.glsl",
            "res/shader/fontfrag.glsl",
            "res/font/maledict_12x12.png",
            12, 12);

    guitextgridadd(guitextgridcreate(12, 12, 0, 0), msg);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), textfield);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), savebutton_text);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), cancelbutton_text);
    guisetgridtext(msg, "File name");
    guisetgridtext(textfield, "");
    guisetgridtext(savebutton_text, "SAVE");
    guisetgridtext(cancelbutton_text, "CANCEL");
    
    guiaddelem(base, msg);
    guiaddelem(base, textfield);
    guiaddelem(base, savebutton);
    guiaddelem(base, cancelbutton);
    guiaddelem(savebutton, savebutton_text);
    guiaddelem(cancelbutton, cancelbutton_text);

    guisetbordersize(base, 4);
    guisetbacktexture(base, 0, 8, 1, 1);
    guisetedgetexture(base, 0, 2, 1, 1);
    guisetcornertexture(base, 0, 2, 1, 1);
    
    guisetbordersize(textfield, 8);
    guisetbacktexture(textfield, 0, 8, 1, 1);
    guisetedgetexture(textfield, 32, 0, 8, 8);
    guisetcornertexture(textfield, 40, 0, 8, 8);

    guisetbacktexture(savebutton, 0, 3, 1, 1);
    guisetbacktexture(cancelbutton, 0, 3, 1, 1);

    guisetcursortexture(textfield, 0, 0, 12, 12);

    guiseteventcallback(savebutton, GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(savebutton, GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(savebutton, GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(savebutton, GUI_EVENT_MOUSEDOWN, ui_filesave_save_mousedown);

    guiseteventcallback(cancelbutton, GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(cancelbutton, GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(cancelbutton, GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(cancelbutton, GUI_EVENT_MOUSEDOWN, ui_filesave_cancel_mousedown);

    guiseteventcallback(textfield, GUI_EVENT_GAINFOCUS, ui_textfield_gainfocus);
    guiseteventcallback(textfield, GUI_EVENT_LOSEFOCUS, ui_textfield_losefocus);

    guisetupdatefunc(msg, ui_updatefunc_heightfittext);
    guisetupdatefunc(textfield, ui_updatefunc_heightfittext);
    guisetupdatefunc(savebutton_text, ui_updatefunc_centertext);
    guisetupdatefunc(cancelbutton_text, ui_updatefunc_centertext);

    guihideelem(base);
}

void
ui_fileload_init(void)
{
    GUILayer *layer;
    GUIElement *base, *filelist;
    GUIElement *open, *delete, *cancel;
    GUIElement *open_text, *delete_text, *cancel_text;

    layer = guilayercreate(
            "res/shader/guivert.glsl",
            "res/shader/guifrag.glsl",
            "res/texture/gui.png");

    base = guielemcreate(
            0.25f, 0.5f, 0.5f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER |
            GUI_ATTR_BLOCKING,
            "ui_fileload_base");
    
    filelist = guielemcreate(
            0.0f, 0.2f, 1.0f, 0.8f,
            GUI_CUSTOM,
            0,
            "ui_fileload_filelist");

    open = guielemcreate(
            0.05f, 0.05f, 0.25f, 0.1f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "ui_fileload_open");

    delete = guielemcreate(
            0.375f, 0.05f, 0.25f, 0.1f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "ui_fileload_delete");

    cancel = guielemcreate(
            0.7f, 0.05f, 0.25f, 0.1f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "ui_fileload_cancel");

    open_text = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_fileload_opentext");

    delete_text = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_fileload_deletetext");

    cancel_text = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_fileload_canceltext");
    
    guilinklayer(base, layer);
    guienableratioresizing(base, 0.5f);
    guienabletextlayer(layer,
            "res/shader/fontvert.glsl",
            "res/shader/fontfrag.glsl",
            "res/font/maledict_12x12.png",
            12, 12);

    guitextgridadd(guitextgridcreate(12, 12, 0, 0), open_text);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), delete_text);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), cancel_text);
    guisetgridtext(open_text, "OPEN");
    guisetgridtext(delete_text, "DELETE");
    guisetgridtext(cancel_text, "CANCEL");

    guiaddelem(base, filelist);
    guiaddelem(base, open);
    guiaddelem(base, delete);
    guiaddelem(base, cancel);
    guiaddelem(open, open_text);
    guiaddelem(delete, delete_text);
    guiaddelem(cancel, cancel_text);
    
    guisetbordersize(base, 4);
    guisetbacktexture(base, 0, 8, 1, 1);
    guisetedgetexture(base, 0, 2, 1, 1);
    guisetcornertexture(base, 0, 2, 1, 1);

    guisetbacktexture(open, 0, 3, 1, 1);
    guisetbacktexture(delete, 0, 3, 1, 1);
    guisetbacktexture(cancel, 0, 3, 1, 1);

    guiseteventcallback(open, GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(open, GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(open, GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(open, GUI_EVENT_MOUSEDOWN, ui_fileload_open_mousedown);

    guiseteventcallback(delete, GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(delete, GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(delete, GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(delete, GUI_EVENT_MOUSEDOWN, ui_fileload_delete_mousedown);

    guiseteventcallback(cancel, GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(cancel, GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(cancel, GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(cancel, GUI_EVENT_MOUSEDOWN, ui_fileload_cancel_mousedown);

    guiseteventcallback(filelist, GUI_EVENT_SCROLLUP, ui_filelist_scrollup);
    guiseteventcallback(filelist, GUI_EVENT_SCROLLDN, ui_filelist_scrolldown);

    guisetupdatefunc(open_text, ui_updatefunc_centertext);
    guisetupdatefunc(delete_text, ui_updatefunc_centertext);
    guisetupdatefunc(cancel_text, ui_updatefunc_centertext);

    guihideelem(base);
}

GUIElement *
ui_fileload_entry_init(const char *name, GUIElement *parent)
{
    const char prefix[] = "ui_fileload_entry_";
    const char base_suffix[] = "_base";
    const char text_suffix[] = "_text";
    
    GUIElement *entry, *text;
    char *elemname;
    uint len;

    len = strlen(name) + 18 + 5;
    elemname = malloc((len + 1) * sizeof(char));
    sprintf(elemname, "%s%s%s", prefix, name, base_suffix);

    entry = guielemcreate(
            0.0f, 0.0f, 1.0f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            elemname);
    
    sprintf(elemname, "%s%s%s", prefix, name, text_suffix);

    text = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            elemname);

    guiaddelem(parent, entry);
    guiaddelem(entry, text);
    
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), text);
    guisetgridtext(text, name);

    guisetbacktexture(entry, 0, 3, 1, 1);

    guiseteventcallback(entry, GUI_EVENT_MOUSEOVER, ui_fileload_entry_mouseover);
    guiseteventcallback(entry, GUI_EVENT_MOUSELEAVE, ui_fileload_entry_mouseleave);
    guiseteventcallback(entry, GUI_EVENT_MOUSEUP, ui_fileload_entry_mouseup);
    guiseteventcallback(entry, GUI_EVENT_MOUSEDOWN, ui_fileload_entry_mousedown);
    guiseteventcallback(entry, GUI_EVENT_SCROLLUP, ui_filelist_scrollup);
    guiseteventcallback(entry, GUI_EVENT_SCROLLDN, ui_filelist_scrolldown);
    
    guisetupdatefunc(text, ui_updatefunc_centertext);

    free(elemname);

    return entry;
}

void
ui_tooltip_init(void)
{
    GUILayer *layer;
    GUIElement *base, *textbox;

    layer = guilayercreate(
            "res/shader/guivert.glsl",
            "res/shader/guifrag.glsl",
            "res/texture/gui.png");

    base = guielemcreate(
            0.0f, 0.0f, 0.05f, 0.1f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER |
            GUI_ATTR_NOCAPTURE,
            "ui_tooltip_base");

    textbox = guielemcreate(
            0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_tooltip_text");

    guilinklayer(base, layer);
    guienabletextlayer(layer,
            "res/shader/fontvert.glsl",
            "res/shader/fontfrag.glsl",
            "res/font/maledict_12x12.png",
            12, 12);

    guitextgridadd(guitextgridcreate(12, 12, 0, 2), textbox);
    guisetgridtext(textbox, "Tooltip");

    guiaddelem(base, textbox);

    guisetbordersize(base, 2);
    guisetbacktexture(base, 0, 8, 1, 1);
    guisetedgetexture(base, 0, 2, 1, 1);
    guisetcornertexture(base, 0, 2, 1, 1);

    guisetupdatefunc(base, ui_tooltip_update);

    guihideelem(base);
}

void
ui_dialogbox_init(void)
{
    GUILayer *layer;
    GUIElement *base, *textbox, *button, *button_text;

    layer = guilayercreate(
            "res/shader/guivert.glsl",
            "res/shader/guifrag.glsl",
            "res/texture/gui.png");

    base = guielemcreate(
            0.2f, 0.5f, 0.6f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER |
            GUI_ATTR_BLOCKING,
            "ui_dialog_base");

    textbox = guielemcreate(
            0.05f, 0.35f, 0.9f, 0.55f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT  |
            GUI_ATTR_WRAPTEXT |
            GUI_ATTR_MULTILINE |
            GUI_ATTR_NOCAPTURE,
            "ui_dialog_text");

    button = guielemcreate(
            0.4f, 0.1f, 0.2f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_CLICKABLE |
            GUI_ATTR_HASTEXTURE,
            "ui_dialog_button");

    button_text = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_dialog_button_text");

    guilinklayer(base, layer);
    guienableratioresizing(base, 0.3);
    guienabletextlayer(layer,
            "res/shader/fontvert.glsl",
            "res/shader/fontfrag.glsl",
            "res/font/maledict_12x12.png",
            12, 12);

    guitextgridadd(guitextgridcreate(12, 12, 0, 2), textbox);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), button_text);
    guisetgridtext(textbox, "");
    guisetgridtext(button_text, "OK");
    
    guiaddelem(base, textbox);
    guiaddelem(base, button);
    guiaddelem(button, button_text);

    guisetbordersize(base, 4);
    guisetbacktexture(base, 0, 8, 1, 1);
    guisetedgetexture(base, 0, 2, 1, 1);
    guisetcornertexture(base, 0, 2, 1, 1);

    guisetbacktexture(button, 0, 3, 1, 1);

    guiseteventcallback(button, GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(button, GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(button, GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(button, GUI_EVENT_MOUSEDOWN, ui_dialogbox_mousedown);

    guisetupdatefunc(button_text, ui_updatefunc_centertext);
    
    guihideelem(base);
}

void
ui_confirmbox_init()
{
    GUILayer *layer;
    GUIElement *base, *textbox, *button[3], *button_text[3];

    layer = guilayercreate(
            "res/shader/guivert.glsl",
            "res/shader/guifrag.glsl",
            "res/texture/gui.png");

    base = guielemcreate(
            0.2f, 0.5f, 0.6f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER |
            GUI_ATTR_BLOCKING,
            "ui_confirmbox_base");

    textbox = guielemcreate(
            0.05f, 0.35f, 0.9f, 0.55f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT  |
            GUI_ATTR_WRAPTEXT |
            GUI_ATTR_MULTILINE |
            GUI_ATTR_NOCAPTURE,
            "ui_confirmbox_text");

    button[0] = guielemcreate(
            0.1f, 0.1f, 0.2f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_CLICKABLE |
            GUI_ATTR_HASTEXTURE,
            "ui_confirmbox_but1");

    button_text[0] = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_confirmbox_but1_text");

    button[1] = guielemcreate(
            0.4f, 0.1f, 0.2f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_CLICKABLE |
            GUI_ATTR_HASTEXTURE,
            "ui_confirmbox_but2");

    button_text[1] = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_confirmbox_but2_text");

    button[2] = guielemcreate(
            0.7f, 0.1f, 0.2f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_CLICKABLE |
            GUI_ATTR_HASTEXTURE,
            "ui_confirmbox_but3");

    button_text[2] = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_confirmbox_but3_text");

    guilinklayer(base, layer);
    guienableratioresizing(base, 0.3);
    guienabletextlayer(layer,
            "res/shader/fontvert.glsl",
            "res/shader/fontfrag.glsl",
            "res/font/maledict_12x12.png",
            12, 12);

    guitextgridadd(guitextgridcreate(12, 12, 0, 2), textbox);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), button_text[0]);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), button_text[1]);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), button_text[2]);
    guisetgridtext(textbox, "");
    guisetgridtext(button_text[0], "YES");
    guisetgridtext(button_text[1], "NO");
    guisetgridtext(button_text[2], "CANCEL");
    
    guiaddelem(base, textbox);
    guiaddelem(base, button[0]);
    guiaddelem(base, button[1]);
    guiaddelem(base, button[2]);
    guiaddelem(button[0], button_text[0]);
    guiaddelem(button[1], button_text[1]);
    guiaddelem(button[2], button_text[2]);

    guisetbordersize(base, 4);
    guisetbacktexture(base, 0, 8, 1, 1);
    guisetedgetexture(base, 0, 2, 1, 1);
    guisetcornertexture(base, 0, 2, 1, 1);

    guisetbacktexture(button[0], 0, 3, 1, 1);
    guisetbacktexture(button[1], 0, 3, 1, 1);
    guisetbacktexture(button[2], 0, 3, 1, 1);

    guiseteventcallback(button[0], GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(button[0], GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(button[0], GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(button[0], GUI_EVENT_MOUSEDOWN, NULL);

    guiseteventcallback(button[1], GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(button[1], GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(button[1], GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(button[1], GUI_EVENT_MOUSEDOWN, NULL);

    guiseteventcallback(button[2], GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(button[2], GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(button[2], GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(button[2], GUI_EVENT_MOUSEDOWN, NULL);

    guisetupdatefunc(button_text[0], ui_updatefunc_centertext);
    guisetupdatefunc(button_text[1], ui_updatefunc_centertext);
    guisetupdatefunc(button_text[2], ui_updatefunc_centertext);
    
    guihideelem(base);
}

void
ui_iorename_init(void)
{
    GUILayer *layer;
    GUIElement *base, *textfield, *msg, 
               *confirmbutton, *confirmbutton_text,
               *cancelbutton, *cancelbutton_text;

    layer = guilayercreate(
            "res/shader/guivert.glsl",
            "res/shader/guifrag.glsl",
            "res/texture/gui.png");

    base = guielemcreate(
            0.2f, 0.5f, 0.6f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER |
            GUI_ATTR_BLOCKING,
            "ui_iorename_base");

    msg = guielemcreate(
            0.1f, 0.8f, 0.8f, 0.1f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT,
            "ui_iorename_msg");

    textfield = guielemcreate(
            0.1f, 0.5f, 0.8f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER |
            GUI_ATTR_HASTEXT |
            GUI_ATTR_TEXTINPUT,
            "ui_iorename_text");

    confirmbutton = guielemcreate(
            0.2f, 0.1f, 0.2f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_CLICKABLE |
            GUI_ATTR_HASTEXTURE,
            "ui_iorename_button_confirm");

    confirmbutton_text = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_iorename_button_confirm_text");

    cancelbutton = guielemcreate(
            0.6f, 0.1f, 0.2f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_CLICKABLE |
            GUI_ATTR_HASTEXTURE,
            "ui_iorename_button_cancel");

    cancelbutton_text = guielemcreate(
            0.0f, 0.0f, 1.0f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXT |
            GUI_ATTR_NOCAPTURE,
            "ui_iorename_button_cancel_text");

    guilinklayer(base, layer);
    guienableratioresizing(base, 0.3);
    guienabletextlayer(layer,
            "res/shader/fontvert.glsl",
            "res/shader/fontfrag.glsl",
            "res/font/maledict_12x12.png",
            12, 12);

    guitextgridadd(guitextgridcreate(12, 12, 0, 0), msg);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), textfield);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), confirmbutton_text);
    guitextgridadd(guitextgridcreate(12, 12, 0, 0), cancelbutton_text);
    guisetgridtext(msg, "Rename input/output node");
    guisetgridtext(textfield, "");
    guisetgridtext(confirmbutton_text, "CONFIRM");
    guisetgridtext(cancelbutton_text, "CANCEL");
    
    guiaddelem(base, msg);
    guiaddelem(base, textfield);
    guiaddelem(base, confirmbutton);
    guiaddelem(base, cancelbutton);
    guiaddelem(confirmbutton, confirmbutton_text);
    guiaddelem(cancelbutton, cancelbutton_text);

    guisetbordersize(base, 4);
    guisetbacktexture(base, 0, 8, 1, 1);
    guisetedgetexture(base, 0, 2, 1, 1);
    guisetcornertexture(base, 0, 2, 1, 1);
    
    guisetbordersize(textfield, 8);
    guisetbacktexture(textfield, 0, 8, 1, 1);
    guisetedgetexture(textfield, 32, 0, 8, 8);
    guisetcornertexture(textfield, 40, 0, 8, 8);

    guisetbacktexture(confirmbutton, 0, 3, 1, 1);
    guisetbacktexture(cancelbutton, 0, 3, 1, 1);

    guisetcursortexture(textfield, 0, 0, 12, 12);

    guiseteventcallback(confirmbutton, GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(confirmbutton, GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(confirmbutton, GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(confirmbutton, GUI_EVENT_MOUSEDOWN, ui_iorename_confirm_mousedown);

    guiseteventcallback(cancelbutton, GUI_EVENT_MOUSEOVER, ui_button_mouseover);
    guiseteventcallback(cancelbutton, GUI_EVENT_MOUSELEAVE, ui_button_mouseleave);
    guiseteventcallback(cancelbutton, GUI_EVENT_MOUSEUP, ui_button_mouseup);
    guiseteventcallback(cancelbutton, GUI_EVENT_MOUSEDOWN, ui_iorename_cancel_mousedown);

    guiseteventcallback(textfield, GUI_EVENT_GAINFOCUS, ui_textfield_gainfocus);
    guiseteventcallback(textfield, GUI_EVENT_LOSEFOCUS, ui_textfield_losefocus);

    guisetupdatefunc(msg, ui_updatefunc_heightfittext);
    guisetupdatefunc(textfield, ui_updatefunc_heightfittext);
    guisetupdatefunc(confirmbutton_text, ui_updatefunc_centertext);
    guisetupdatefunc(cancelbutton_text, ui_updatefunc_centertext);

    guihideelem(base);
}

/*  
 *  UI element callbacks
 *
 *  */
    
void
ui_tooltip_update(GUIElement *elem, float dt, ...)
{
    int mx, my;
    int len;
    float pix_w, pix_h;
    GUIElement *tbox;

    UNUSED(dt);
    guishowelem(elem);

    /* Calculate required width of base element */
    tbox = guigetelem("ui_tooltip_text");
    pix_w = 1.0f / uicam->width;
    pix_h = 1.0f / uicam->height;
    len = strlen(tbox->textgrid->text);
    len *= tbox->textgrid->fontwidth;

    elem->w = (len + tbox->textgrid->fontspacing) * pix_w;
    elem->w += 2 * (elem->w * 0.13f);

    elem->h = tbox->textgrid->fontheight * pix_h;
    elem->h += 4 * (elem->h * 0.13f);

    /* Translate to be positioned at mouse cursor */
    mousegetstate(&mx, &my);
    my = uicam->height - my;
    glm_mat4_identity(elem->layer->modelmat);

    if (mx + elem->w * uicam->width > uicam->width) {
        mx -= (mx + elem->w * uicam->width) - uicam->width;
        glm_translate_x(elem->layer->modelmat, mx);
    } else {
        glm_translate_x(elem->layer->modelmat, mx + 8);
    }
    glm_translate_y(elem->layer->modelmat, my + 8);
}

/* Centers the given text box in its parent element */
void
ui_updatefunc_centertext(GUIElement *elem, float dt, ...)
{
    int len;
    float pix_w, pix_h;
    float rel_w, rel_h;
    GUIElement *next, *rel_elem;

    UNUSED(dt);
    rel_elem = elem->parent;
    next = rel_elem->parent;

    len = strlen(elem->textgrid->text);

    rel_w = 1.0f;
    rel_h = 1.0f;

    while (next) {
        rel_w *= rel_elem->w;

        if (rel_elem->attr & GUI_ATTR_WH_RESIZE) {
            rel_h *= rel_elem->w * rel_elem->ratio_h;
        } else {
            rel_h *= rel_elem->h;
        }
        rel_elem = next;
        next = rel_elem->parent;
    }
    rel_w *= rel_elem->w * uicam->width;

    if (rel_elem->attr & GUI_ATTR_WH_RESIZE) {
        rel_h *= rel_elem->w * rel_elem->ratio_h * uicam->width;
    } else {
        rel_h *= rel_elem->h * uicam->height;
    }

    pix_w = (1.0f / rel_w);
    pix_h = (1.0f / rel_h);

    elem->w = (elem->textgrid->fontwidth + (elem->textgrid->fontspacing)) * pix_w * len;
    elem->h = (elem->textgrid->fontheight + 0.001) * pix_h;
    elem->x = 0.5f - elem->w * 0.5f;
    elem->y = 0.5f - elem->h * 0.5f;
}

void
ui_updatefunc_heightfittext(GUIElement *elem, float dt, ...)
{
    float rel_h, pix_h;
    GUIElement *next, *rel_elem;

    UNUSED(dt);
    rel_elem = elem->parent;
    next = rel_elem->parent;

    rel_h = 1.0f;

    while (next) {
        if (rel_elem->attr & GUI_ATTR_WH_RESIZE) {
            rel_h *= rel_elem->w * rel_elem->ratio_h;
        } else {
            rel_h *= rel_elem->h;
        }
        rel_elem = next;
        next = rel_elem->parent;
    }

    if (rel_elem->attr & GUI_ATTR_WH_RESIZE) {
        rel_h *= rel_elem->w * rel_elem->ratio_h * uicam->width;
    } else {
        rel_h *= rel_elem->h * uicam->height;
    }

    pix_h = (1.0f / rel_h) + 0.001;

    elem->h = (elem->textgrid->fontheight + elem->textgrid->linespacing) * pix_h;
}

void
ui_textfield_gainfocus(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 3, 0, 1, 1);
    guisetedgetexture(elem, 48, 0, 8, 8);
    guisetcornertexture(elem, 56, 0, 8, 8);
}

void
ui_textfield_losefocus(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 0, 8, 1, 1);
    guisetedgetexture(elem, 32, 0, 8, 8);
    guisetcornertexture(elem, 40, 0, 8, 8);
}

void
ui_button_mouseup(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 0, 3, 1, 1);
    elem->layer->dirty = 1;
}

void
ui_button_mouseover(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 0, 2, 1, 1);
    elem->layer->dirty = 1;
}

void
ui_button_mouseleave(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 0, 3, 1, 1);
    elem->layer->dirty = 1;
}

void
ui_dialogbox_mouseup(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 0, 3, 1, 1);
    elem->layer->dirty = 1;
}

void
ui_dialogbox_mousedown(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 0, 3, 1, 1);
    ui_dialogbox_hide();
    elem->layer->dirty = 1;
}

void
ui_elem_show(const char *elem_id)
{
    GUIElement *elem;

    elem = guigetelem(elem_id);
    guishowelem(elem);
    elem->layer->dirty = 1;
}
    
void
ui_elem_hide(const char *elem_id)
{
    GUIElement *elem;

    elem = guigetelem(elem_id);
    guihideelem(elem);
    elem->layer->dirty = 1;
}

void
ui_packsave_save_mousedown(GUIElement *elem, ...)
{
    World *world;
    Circuit *circuit;
    Package *pack;
    FreeSegment *seg;
    GUIElement *textfield;
    char *fname;
    uint i, len;

    UNUSED(elem);

    if (selbox->enabled) {
        textfield = guigetelem("ui_packsave_text");
        fname = textfield->textgrid->text;
        len = strlen(fname);

        if (len == 0) {
            ui_dialogbox_settext("Cannot save a file with a blank name.");
            ui_dialogbox_show();
            return;
        } else if (len >= 64) {
            ui_dialogbox_settext("File name must be less than 64 characters long.");
            ui_dialogbox_show();
            return;
        }
        ui_settargetpath(fname);

        /* Ask for confirmation if file already exists */
        for (i = 0; i < files.paths->size; i++) {
            if (!strcmp(listat(files.paths, i), fname)) {
                ui_confirmbox_packsave_overwrite();
                return;
            }
        }
        /* Save package */
        world = worldcreatefake();
        worldbuildfromselbox(world, worldtop(), 1);
        filesaveworld(target_path, world);
        ui_filelist_populate();

        /* Build package as mouseseg */
        circuit = circuitbuild(world, 1, 1);
        pack = packcreate(PACK_CIRCUIT, target_path);
        pack->name = strdup(fname); 
        packbindcircuit(pack, circuit);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, 0, 0, pack->w, pack->h);
        listpushback(seg->packs, pack);
        freesegmesh(seg);
        setmouseseg(seg);
        selbox->enabled = 0;
        worlddeletefake(world);
    } else {
        ui_dialogbox_settext("Unable to create package. Please make a "
                "selection with the drag-drop box of the circuitry you wish "
                "to save as a package.");
        ui_dialogbox_show();
    }
    ui_packsave_hide();
}

void 
ui_packsave_cancel_mousedown(GUIElement *elem, ...)
{
    /* Close package save dialog and reset text field to be empty */
    guisetbacktexture(elem, 0, 3, 1, 1);
    ui_packsave_hide();
    elem->layer->dirty = 1;
}

void
ui_packload_entry_mousedown(GUIElement *elem, ...)
{
    uint i;
    
    /* Spawn a new package as part of the mouse segment */
    for (i = 0; i < files.pack_elems->size; i++) {
        if (elem == listat(files.pack_elems, i)) {
            ui_filelist_openpack(i);
            break;
        }
    }
}

void
ui_filesave_save_mousedown(GUIElement *elem, ...)
{
    GUIElement *textfield;
    char *fname;
    uint i, len;
    World *world;

    UNUSED(elem);

    textfield = guigetelem("ui_filesave_text");
    fname = textfield->textgrid->text;
    len = strlen(fname);

    if (len == 0) {
        ui_dialogbox_settext("Cannot save a file with a blank name.");
        ui_dialogbox_show();
        return;
    } else if (len >= 64) {
        ui_dialogbox_settext("File name must be less than 64 characters long.");
        ui_dialogbox_show();
        return;
    }
    world = worldtop();

    if (world->name) {
        free(world->name);
    }
    world->name = strdup(fname);
    ui_settargetpath(fname);

    for (i = 0; i < files.paths->size; i++) {
        if (!strcmp(listat(files.paths, i), fname)) {
            ui_confirmbox_filesave_overwrite();
            return;
        }
    }
    filesaveworld(target_path, world);

    if (files.select_enabled) {
        ui_filelist_openfile(files.select_index);
        files.select_enabled = 0;
    }

    if (newfile_request) {
        world = worldcreate();
        worldswitch(world);
        newfile_request = 0;
    }

    ui_filesave_hide();
    ui_filelist_populate();
}

void
ui_filesave_cancel_mousedown(GUIElement *elem, ...)
{
    World *world;
    GUIElement *textfield;

    UNUSED(elem);

    world = worldtop();
    textfield = guigetelem("ui_filesave_text");

    ui_filesave_hide();
    
    if (world->name) {
        guisetgridtext(textfield, world->name); 
    }
    files.select_enabled = 0;
    files.select_index = 0;
}

void
ui_fileload_entry_mousedown(GUIElement *elem, ...)
{
    uint i;
    GUIElement *entry;

    if (files.select_enabled) {
        if (!(elem == listat(files.file_elems, files.select_index))) {
            for (i = 0; i < files.file_elems->size; i++) {
                entry = listat(files.file_elems, i);
                
                if (entry == elem) {
                    files.select_index = i;
                } else {
                    guisetbacktexture(entry, 4, 0, 1, 1);
                }
            }
        }
    } else {
        for (i = 0; i < files.file_elems->size; i++) {
            entry = listat(files.file_elems, i);

            if (entry == elem) {
                files.select_enabled = 1;
                files.select_index = i;
            }
        }
    }
    guisetbacktexture(elem, 6, 0, 1, 1);
    elem->layer->dirty = 1;
}
    
void
ui_fileload_entry_mouseup(GUIElement *elem, ...)
{
    if (files.select_enabled &&
            elem == listat(files.file_elems, files.select_index)) {
        guisetbacktexture(elem, 5, 0, 1, 1);
    } else {
        guisetbacktexture(elem, 4, 0, 1, 1);
    }
    elem->layer->dirty = 1;
}

void
ui_fileload_entry_mouseover(GUIElement *elem, ...)
{
    if (files.select_enabled &&
            elem == listat(files.file_elems, files.select_index)) {
        guisetbacktexture(elem, 5, 0, 1, 1);
    } else {
        guisetbacktexture(elem, 5, 0, 1, 1);
    }
    elem->layer->dirty = 1;
}

void
ui_fileload_entry_mouseleave(GUIElement *elem, ...)
{
    if (files.select_enabled &&
            elem == listat(files.file_elems, files.select_index)) {
        guisetbacktexture(elem, 5, 0, 1, 1);
    } else {
        guisetbacktexture(elem, 4, 0, 1, 1);
    }
    elem->layer->dirty = 1;
}

void
ui_fileload_open_mousedown(GUIElement *elem, ...)
{
    UNUSED(elem);

    if (files.select_enabled) {
        ui_confirmbox_fileload_preloadsave();
        ui_fileload_hide();
    } else {
        ui_dialogbox_settext("No file selected. Please select a file to open.");
        ui_dialogbox_show();
    }
}

void
ui_fileload_delete_mousedown(GUIElement *elem, ...)
{
    /* Remove entry from filelist and rebuild list */
    UNUSED(elem);

    if (files.select_enabled) {
        ui_confirmbox_fileload_deletionconfirm();
    } else {
        ui_dialogbox_settext("No file selected. Please select a file to delete.");
        ui_dialogbox_show();
    }
}

void
ui_fileload_cancel_mousedown(GUIElement *elem, ...)
{
    /* Hide fileload base elem */
    UNUSED(elem);
    files.select_enabled = 0;
    files.select_index = 0;
    ui_fileload_hide();
}


/* 
 *  File list functions 
 *
 * */

void
ui_filelist_init(void)
{
    files.toprow = 0;
    files.select_enabled = 0;
    files.select_index = 0;
    files.file_elems = listcreate();
    files.pack_elems = listcreate();
    files.paths = listcreate();

    ui_filelist_populate();
}

/* Scan circuit folder for a list of files with .bcf extention
 * and construct packages representing those files */
void
ui_filelist_populate(void)
{
    uint i;
    List *filelist;
    char *fname, *p;
    GUIElement *entry, *file_elem, *pack_elem;

    if (files.paths->size) {
        ui_filelist_clear();
    }

    filelist = listfilesat(FILE_CIRCUIT_DIR, "bcf");
    file_elem = guigetelem("ui_fileload_filelist");
    pack_elem = guigetelem("ui_packload_filelist");

    if (!filelist) {
        logprint(LOG_ERROR, "ui: Unable to populate file list");
        return; 
        /* TODO: Create directory and negotiate around error
         *       Reduce to warning. */
    }
 
    if (files.paths->size) {
        ui_filelist_clear();
    }

    for (i = 0; i < filelist->size; i++) {
        fname = listat(filelist, i);
        
        if ((p = strrchr(fname, '.'))) {
            *p = '\0';
        }
        entry = ui_fileload_entry_init(fname, file_elem);
        listpushback(files.file_elems, entry);
        entry = ui_packload_entry_init(fname, pack_elem);
        listpushback(files.pack_elems, entry);
        listpushback(files.paths, fname);
    }
    listdelete(filelist);

    ui_filelist_elems_calcpos();
}

/* Clear file list of all files and GUI elements */
void
ui_filelist_clear(void)
{
    uint i;

    if (files.paths->size == 0) {
        return;
    }

    files.toprow = 0;

    listclear(guigetelem("ui_fileload_filelist")->children);
    listclear(guigetelem("ui_packload_filelist")->children);

    for (i = 0; i < files.paths->size; i++) {
        guideleteelem(listat(files.file_elems, i), 1);
        guideleteelem(listat(files.pack_elems, i), 1);
    }
    listclear(files.file_elems);
    listclear(files.pack_elems);
    listdealloc(files.paths);
    listclear(files.paths);

    guisignalinterrupt(guigetelem("ui_fileload_filelist"));
    guisignalinterrupt(guigetelem("ui_packload_filelist"));
}

/* Delete file and related elements at the given index */
void
ui_filelist_deletefile(uint index)
{
    ui_settargetpath(listat(files.paths, index));
    removefile(target_path);

    if (index < files.toprow) {
        files.toprow--;
    }
    listremove(guigetelem("ui_fileload_filelist")->children, index);
    listremove(guigetelem("ui_packload_filelist")->children, index);

    guideleteelem(listat(files.file_elems, index), 1);
    guideleteelem(listat(files.pack_elems, index), 1);
    listremove(files.file_elems, index);
    listremove(files.pack_elems, index);
    listfreemove(files.paths, index);
    
    guisignalinterrupt(guigetelem("ui_fileload_filelist"));
    guisignalinterrupt(guigetelem("ui_packload_filelist"));

    ui_filelist_elems_calcpos();
}

/* Call to load a file as a world */
void
ui_filelist_openfile(uint index)
{
    World *world;
    char *fname;

    fname = listat(files.paths, index);
    ui_settargetpath(fname);
    world = worldcreate();
    world->name = strdup(fname);
    filegetworld(target_path, world);
    circuitrebuild(world, &world->circuit);
    worldswitch(world);
}

/* Call to load a file as a circuit package */
void
ui_filelist_openpack(uint index)
{
    World *world;
    Circuit *circuit;
    Package *pack;
    FreeSegment *seg;
    char *fname;

    /* Load the package */
    fname = listat(files.paths, index);
    ui_settargetpath(fname);
    world = worldcreatefake();
    filegetworld(target_path, world);

    /* Build package as mouseseg */
    circuit = circuitbuild(world, 1, 1);
    pack = packcreate(PACK_CIRCUIT, target_path);
    pack->name = strdup(fname); 
    packbindcircuit(pack, circuit);
    seg = malloc(sizeof(FreeSegment));
    freeseginit(seg, 0, 0, pack->w, pack->h);
    listpushback(seg->packs, pack);
    freesegmesh(seg);
    setmouseseg(seg);
    selbox->enabled = 0;
    worlddeletefake(world);
}

void
ui_filelist_scrollup(GUIElement *elem, ...)
{
    UNUSED(elem);

    if (files.toprow > 0) {
        files.toprow--;
    }
    ui_filelist_elems_calcpos();
}

void
ui_filelist_scrolldown(GUIElement *elem, ...)
{
    UNUSED(elem);

    if (files.toprow < files.paths->size - 1) {
        files.toprow++;
    }
    ui_filelist_elems_calcpos();
}

void
ui_filelist_elems_calcpos(void)
{
    uint i;
    float y;
    GUIElement *file, *pack;

    for (i = 0; i < files.paths->size; i++) {
        file = listat(files.file_elems, i);
        pack = listat(files.pack_elems, i);

        if (i >= files.toprow && i < files.toprow + 5) {
            y = 0.8f - 0.2f * (i - files.toprow);
            file->y = y;
            pack->y = y;

            if (!(file->parent->state & GUI_STATE_INVISIBLE)) {
                guishowelem(file);
            }

            if (!(pack->parent->state & GUI_STATE_INVISIBLE)) {
                guishowelem(pack);
            }
        } else {
            guihideelem(file);
            guihideelem(pack);
        }
        file->layer->dirty = 1;
        pack->layer->dirty = 1;
    }
}

void
ui_filelist_delete(void)
{
    ui_filelist_clear();
    listdelete(files.file_elems);
    listdelete(files.pack_elems);
    listdelete(files.paths);
}

FileList *
ui_getfilelist(void)
{
    return &files;
}

void
ui_confirmbox_filesave_overwrite(void)
{
    ui_confirmbox_show();
    ui_confirmbox_settext("A file with that name already exists. " 
            "Overwrite existing file?");
    ui_confirmbox_but1_settext("SAVE");
    ui_confirmbox_but2_settext("CANCEL");
    ui_confirmbox_but3_hide();
    ui_confirmbox_but1_setfunc(ui_confirmbox_filesave_overwrite_save);
    ui_confirmbox_but2_setfunc(ui_confirmbox_filesave_overwrite_cancel);
}

void 
ui_confirmbox_packsave_overwrite(void)
{
    ui_confirmbox_show();
    ui_confirmbox_settext("A file with that name already exists. " 
            "Overwrite existing file?");
    ui_confirmbox_but1_settext("SAVE");
    ui_confirmbox_but2_settext("CANCEL");
    ui_confirmbox_but3_hide();
    ui_confirmbox_but1_setfunc(ui_confirmbox_packsave_overwrite_save);
    ui_confirmbox_but2_setfunc(ui_confirmbox_packsave_overwrite_cancel);
}

void
ui_confirmbox_fileload_preloadsave(void)
{
    ui_fileload_hide();
    ui_confirmbox_show();
    ui_confirmbox_settext("You are loading a file. "
            "Any unsaved changes will be lost. Save current progress?");
    ui_confirmbox_but1_settext("YES");
    ui_confirmbox_but2_settext("NO");
    ui_confirmbox_but3_settext("CANCEL");
    ui_confirmbox_but3_show();
    ui_confirmbox_but1_setfunc(ui_confirmbox_fileload_preloadsave_yes);
    ui_confirmbox_but2_setfunc(ui_confirmbox_fileload_preloadsave_no);
    ui_confirmbox_but3_setfunc(ui_confirmbox_fileload_preloadsave_cancel);
}

void
ui_confirmbox_fileload_deletionconfirm(void)
{
    ui_confirmbox_show();
    ui_confirmbox_settext("Are you sure you want to permanently delete this file?");
    ui_confirmbox_but1_settext("DELETE");
    ui_confirmbox_but2_settext("CANCEL");
    ui_confirmbox_but3_hide();
    ui_confirmbox_but1_setfunc(ui_confirmbox_fileload_deletionconfirm_delete);
    ui_confirmbox_but2_setfunc(ui_confirmbox_fileload_deletionconfirm_cancel);
}

void
ui_confirmbox_filesave_overwrite_save(GUIElement *elem, ...)
{
    UNUSED(elem);

    filesaveworld(target_path, worldtop());

    if (files.select_enabled) {
        ui_filelist_openfile(files.select_index);
        files.select_enabled = 0;
    }

    if (newfile_request) {
        worldswitch(worldcreate());
        newfile_request = 0;
    }
    ui_filesave_hide();
    ui_confirmbox_hide();
    ui_filelist_populate();
}

void
ui_confirmbox_filesave_overwrite_cancel(GUIElement *elem, ...)
{
    UNUSED(elem);

    ui_confirmbox_hide();
}

void
ui_confirmbox_packsave_overwrite_save(GUIElement *elem, ...)
{
    GUIElement *textfield;
    World *world;
    Circuit *circuit;
    FreeSegment *seg;
    Package *pack;
    char *fname;

    UNUSED(elem);

    /* Save package */
    textfield = guigetelem("ui_packsave_text");
    fname = textfield->textgrid->text;

    world = worldcreatefake();
    worldbuildfromselbox(world, worldtop(), 1);
    filesaveworld(target_path, world);
    ui_filelist_populate();

    /* Build package as mouseseg */
    circuit = circuitbuild(world, 1, 1);
    pack = packcreate(PACK_CIRCUIT, target_path);
    pack->name = strdup(fname);
    packbindcircuit(pack, circuit);
    seg = malloc(sizeof(FreeSegment));
    freeseginit(seg, 0, 0, pack->w, pack->h);
    listpushback(seg->packs, pack);
    freesegmesh(seg);
    setmouseseg(seg);
    selbox->enabled = 0;
    worlddeletefake(world);

    ui_confirmbox_hide();
    ui_packsave_hide();
}

void
ui_confirmbox_packsave_overwrite_cancel(GUIElement *elem, ...)
{
    UNUSED(elem);

    ui_confirmbox_hide();
}

void
ui_confirmbox_fileload_preloadsave_yes(GUIElement *elem, ...)
{
    UNUSED(elem);

    ui_filesave_show();
    ui_fileload_hide();
    ui_confirmbox_hide();
}

void
ui_confirmbox_fileload_preloadsave_no(GUIElement *elem, ...)
{
    UNUSED(elem);

    World *world;

    if (files.select_enabled) {
        ui_filelist_openfile(files.select_index);
        files.select_enabled = 0;
    } else {
        world = worldcreate();
        worldswitch(world);
        newfile_request = 0;
    }
    ui_fileload_hide();
    ui_confirmbox_hide();
}

void
ui_confirmbox_fileload_preloadsave_cancel(GUIElement *elem, ...)
{
    UNUSED(elem);

    files.select_enabled = 0;
    newfile_request = 0;
    ui_confirmbox_hide();
}

void
ui_confirmbox_fileload_deletionconfirm_delete(GUIElement *elem, ...)
{
    UNUSED(elem);

    ui_filelist_deletefile(files.select_index);
    ui_confirmbox_hide();
}

void
ui_confirmbox_fileload_deletionconfirm_cancel(GUIElement *elem, ...)
{
    UNUSED(elem);

    ui_confirmbox_hide();
}

void
ui_iorename_confirm_mousedown(GUIElement *elem, ...)
{
    GUIElement *textbox;

    UNUSED(elem);

    textbox = guigetelem("ui_iorename_text");

    if (target_pack->name) {
        free(target_pack->name);
    }
    target_pack->name = strdup(textbox->textgrid->text);
    target_pack->seg->dirty = 1;

    ui_iorename_hide();
}

void
ui_iorename_cancel_mousedown(GUIElement *elem, ...)
{
    UNUSED(elem);

    ui_iorename_hide();
}

/* Copies the given file name into the target_path global with the directory
 * and file extentions added. */
void
ui_settargetpath(const char *file)
{
    size_t len;

    if (target_path) {
        free(target_path);
    }
    len = strlen(FILE_CIRCUIT_DIR) + strlen(file) + strlen(FILE_EXT) + 3;
    target_path = malloc(len * sizeof(char));
    sprintf(target_path, "%s/%s.%s", FILE_CIRCUIT_DIR, file, FILE_EXT);
}

void
ui_settargetpack(Package *pack)
{
    target_pack = pack;
}

void
ui_requestnewfile(void)
{
    newfile_request = 1;
}
