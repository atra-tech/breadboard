/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Library for the simplified creation and management of general-purpose
 *  graphical user interfaces.
 *  The library works with the frame and layer system to capture input and
 *  handle window focus and stacking.
 *
 *  */

#ifndef ATRA_GUI_H
#define ATRA_GUI_H

#include "entity.h"
#include "render.h"

enum GUIType {
    GUI_CUSTOM,
    GUI_FRAME,
    GUI_WINDOW,
    GUI_BUTTON,
    GUI_TEXTBOX,
    GUI_TEXTFIELD
};

enum GUIEvent {
    GUI_EVENT_MOUSEOVER,
    GUI_EVENT_MOUSELEAVE,
    GUI_EVENT_MOUSEDOWN,
    GUI_EVENT_MOUSEUP,
    GUI_EVENT_SCROLLUP,
    GUI_EVENT_SCROLLDN,
    GUI_EVENT_GAINFOCUS,
    GUI_EVENT_LOSEFOCUS,
    GUI_EVENT_KEYDOWN,
    GUI_EVENT_KEYUP,
    GUI_EVENT_CLOSE,
    GUI_EVENT_TOTAL
};

enum GUIState {
    GUI_STATE_MOUSEOVER = BIT(0),
    GUI_STATE_MOUSE_LB  = BIT(1),
    GUI_STATE_MOUSE_RB  = BIT(2),
    GUI_STATE_MOUSE_MB  = BIT(3),
    GUI_STATE_HASFOCUS  = BIT(4),
    GUI_STATE_KEYPRESS  = BIT(5),
    GUI_STATE_INVISIBLE = BIT(6),
    GUI_STATE_DISABLED  = BIT(7),
    GUI_STATE_DIRTY     = BIT(8)
};

enum GUIAttrib {
    GUI_ATTR_RESIZABLE  = BIT(0),
    GUI_ATTR_MOVABLE    = BIT(1),
    GUI_ATTR_TEXTINPUT  = BIT(2),
    GUI_ATTR_MULTILINE  = BIT(3),
    GUI_ATTR_SCROLLABLE = BIT(4),
    GUI_ATTR_WRAPTEXT   = BIT(5),
    GUI_ATTR_HASTEXT    = BIT(6),
    GUI_ATTR_SCROLLBAR  = BIT(7),
    GUI_ATTR_HASTEXTURE = BIT(8),
    GUI_ATTR_HASBORDER  = BIT(9),
    GUI_ATTR_CLICKABLE  = BIT(10),
    GUI_ATTR_RESIZETAB  = BIT(11),
    GUI_ATTR_NOCAPTURE  = BIT(12),
    GUI_ATTR_WH_RESIZE  = BIT(13),
    GUI_ATTR_BLOCKING   = BIT(14)
};

enum GUIResizeTabType {
    GUI_RESIZE_T,
    GUI_RESIZE_B,
    GUI_RESIZE_L,
    GUI_RESIZE_R,
    GUI_RESIZE_TL,
    GUI_RESIZE_TR,
    GUI_RESIZE_BL,
    GUI_RESIZE_BR
};

//TODO: Add textdirty for only redrawing fonts
typedef struct {
    VBuffer vbo;
    VBuffer fontvbo;
    Shader shader;
    Shader fontshader;
    Texture texture;
    FontTex fonttex;
    VecList fontcolors;
    mat4 modelmat;
    void *baseelem;
    int dirty;
    int textenabled;
} GUILayer;

typedef struct {
    uint pos;
    uint textpos;
    uint blink;
    uint ticks;
    uint enabled;
    float uvs[8];
} GUITextCursor;

typedef struct {
    uint width;
    uint h_extra;
    uint bar_w_offset;
    uint x_offset;
    float backtex[8];
    float bartex[8];
    vec3 verts[4];
} GUIScrollBar;

typedef struct {
    vec4 color;
    char c;
} GUIGlyph;

typedef struct {
    GUIGlyph *glyph;
    vec3 verts[4];
} GUIGlyphBox;

typedef struct {
    uint fontwidth;
    uint fontheight;
    uint fontspacing;
    uint linespacing;
    uint rowcount;
    uint colcount;
    uint startrow;
    uint dirty;
    uint resized;   //TODO: Implement
    uint textset;   //TODO: Implement
    GUITextCursor cursor;
    GUIGlyphBox *view;
    List *glyphs;
    char *text;
} GUITextGrid;

typedef struct GUIElem {
    float x, y;
    float w, h, ratio_h;
    uint attr;
    uint state, prevstate;
    uint event;
    struct GUIElem *parent;
    List *children;
    GUILayer *layer;
    GUITextGrid *textgrid;
    GUIScrollBar *scrollbar;
    float backtex[8];
    float cornertex[8];
    float edgetex[8];
    uint bordersize;
    void (*update)(struct GUIElem *self, float dt, ...);
    void (*callbacks[GUI_EVENT_TOTAL])(struct GUIElem *self, ...);
} GUIElement;


void guiinit(Camera *gamecam);
GUIElement *guielemcreate(float x, float y, float w, float h,
        int type, int attr, const char *id);
GUILayer *guilayercreate(const char *vshader, const char *fshader,
        const char *texpath);
GUITextGrid *guitextgridcreate(uint font_w, uint font_h,
        int fspacing, int lspacing);
GUIScrollBar *guiscrollbarcreate(uint width, uint h_extra,
        uint bar_w_offset, uint x_offset);
void guilinklayer(GUIElement *elem, GUILayer *layer);
void guiaddelem(GUIElement *dest, GUIElement *elem);
GUIElement *guigetelem(const char *id);
GUIElement *guigetfocus(void);
GUIElement *guigetkbdfocus(void);
Camera *guigetcamera(void);
void guisetbordersize(GUIElement *elem, uint size);
void guisetbacktexture(GUIElement *elem, int x, int y, int w, int h);
void guisetedgetexture(GUIElement *elem, int x, int y, int w, int h);
void guisetcornertexture(GUIElement *elem, int x, int y, int w, int h);
void guisetscrollbarbacktex(GUIElement *elem, int x, int y, int w, int h);
void guisetscrollbarbartex(GUIElement *elem, int x, int y, int w, int h);
void guisetcursortexture(GUIElement *elem, int x, int y, int w, int h);
void guishowelem(GUIElement *elem);
void guihideelem(GUIElement *elem);
void guisignalinterrupt(GUIElement *elem);
void guihandleinterrupt(void);

void guisetupdatefunc(GUIElement *elem,
        void (*func)(GUIElement *elem, float dt, ...));
void guiseteventcallback(GUIElement *elem, int eventtype,
        void (*func)(GUIElement *self, ...));

int guihandle(Event *e);
void guiupdate(float dt);
void guirender(void);
void guiredraw(void);
void guiupdateelem(GUIElement *elem, float dt);
void guideleteelem(GUIElement *elem, int cascade);
void guideletelayer(GUILayer *layer);
void guishutdown(void);

int guimeshfont(GUITextGrid *grid, GUIElement *elem);
void guicalcfontgridvertices(GUITextGrid *grid, int x_min, int y_min,
        int x_max, int y_max, float depth);
void guisetgridtext(GUIElement *elem, const char *text);
void guitextscrollenable(GUIElement *elem);
void guitextgridadd(GUITextGrid *grid, GUIElement *parent);
void guiscrollbaradd(GUIScrollBar *bar, GUIElement *parent);
void guienableratioresizing(GUIElement *elem, float ratio_h);
void guienabletextlayer(GUILayer *layer, const char *vshader,
        const char *fshader, const char *fontpath,
        int font_w, int font_h);


#endif /* ATRA_GUI_H */
