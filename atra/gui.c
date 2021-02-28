/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Library for the simplified creation and management of general-purpose
 *  graphical user interfaces. 
 *  The library works with the layer system to capture input and
 *  handle window focus, stacking, and input capture.
 *
 *  */

#include "render.h"
#include "util/log.h"
#include "util/htable.h"
#include "util/list.h"
#include "util/utf.h"
#include "gui.h"
#include "input.h"
#include "entity.h"

#include <string.h>

#define LERP(a, b, t) (1.0f - t) * a + b * t
#define CLAMP(v, l, h) (v < l) ? l : ((v > h) ? h : v)

static HTable *elemtable;
static List *elemlist;
static List *layerstack;
static Camera guicam;
static Camera *maincam;
static GUIElement *kbdfocus;
static GUIElement *focus;
static int prev_mx, prev_my;
static int prev_cam_w, prev_cam_h;
static int mcaptured;
static int interrupt;

static int guimouseintersects(GUIElement *elem, int mx, int my);
static void guisetattrtemplate(GUIElement *elem, int type, int attr);
static int guihandlemouse(GUIElement *elem, Event *e);
static int guihandlekeyboard(GUIElement *elem, Event *e);
static void guisetfocus(GUIElement *elem);
static void guimesh(GUIElement *elem);
static void guimeshborders(GUIElement *elem, float x_min, float y_min,
        float x_max, float y_max, float depth);
static void guipushquad(VBuffer *vbo, float x_min, float y_min,
        float x_max, float y_max, float depth, float uvs[8]);
static void guipushquadinverted(VBuffer *vbo, float x_min, float y_min,
        float x_max, float y_max, float depth, float uvs[8]);
static void guicursormouseset(GUIElement *elem, int mx, int my);
static void guihandletextinput(GUIElement *elem, uchar chr);
static void guicursormoveup(GUITextGrid *grid);
static void guicursormovedn(GUITextGrid *grid);
static void guicursormoveright(GUITextGrid *grid);
static void guicursormoveleft(GUITextGrid *grid);
static void guitextreadtext(GUIElement *elem);
static void guitextwritetext(GUIElement *elem);
static void guikbdnewline(GUIElement *elem);
static void guikbdbackspace(GUIElement *elem);
static void guikbdtab(GUIElement *elem);
static void guikbddelete(GUIElement *elem);
static GUIGlyph *guiglyphcreate(void);

/* Initialise the GUI system.  Must be called first. */
void
guiinit(Camera *gamecam)
{
    elemtable = htablecreate(htablestringhash, htablestringequal);
    elemlist = listcreate();
    layerstack = listcreate();
    maincam = gamecam;
    camerainit(&guicam, maincam->width, maincam->height, CAMERA_ORTHOGRAPHIC);
    guicam.ent.ry = PI;
    glm_ortho(0, guicam.width,
              0, guicam.height,
              0.01f, 1000.0f,
              guicam.projmat);
    prev_mx = prev_my = 0;
    prev_cam_w = maincam->width;
    prev_cam_h = maincam->height;
    mcaptured = 0;
}

/* Create a new gui element, must be deleted with guideleteelem() */
GUIElement *
guielemcreate(float x, float y, float w, float h,
        int type, int attr, const char *id)
{
    GUIElement *elem = calloc(1, sizeof(GUIElement));
    char *key;

    if (checkmalloc(elem) < 0) {
        logprint(LOG_ERROR, "gui: Unable to create element:"\
                " Failed to allocate memory");
    }
    elem->x = x;
    elem->y = y;
    elem->w = w;
    elem->h = h;
    elem->children = listcreate();
    guisetattrtemplate(elem, type, attr);

    if (id) {
        key = strdup(id);
        htableadd(elemtable, key, elem);
    }
    listpushback(elemlist, elem);

    return elem;
}

void
guisetattrtemplate(GUIElement *elem, int type, int attr)
{
    switch (type) {
    case GUI_CUSTOM:
        elem->attr = attr;
        break;
    case GUI_FRAME:
        elem->attr = GUI_ATTR_HASTEXTURE |
                     GUI_ATTR_HASBORDER |
                     attr;
        break;
    case GUI_WINDOW:
        elem->attr = GUI_ATTR_HASTEXTURE |
                     GUI_ATTR_HASBORDER |
                     GUI_ATTR_MOVABLE |
                     GUI_ATTR_RESIZABLE |
                     attr;
        break;
    case GUI_BUTTON:
        elem->attr = GUI_ATTR_HASTEXTURE |
                     GUI_ATTR_CLICKABLE |
                     attr;
        break;
    case GUI_TEXTBOX:
        elem->attr = GUI_ATTR_HASTEXT |
                     attr;
        break;
    case GUI_TEXTFIELD:
        elem->attr = GUI_ATTR_HASTEXT |
                     GUI_ATTR_TEXTINPUT |
                     attr;
        break;
    default:
        elem->attr = 0;
    }
}

/* Creates a new empty layer, must be deleted with guideletelayer() */
GUILayer *
guilayercreate(const char *vshader, const char *fshader, const char *texpath)
{
    GUILayer *layer = calloc(1, sizeof(GUILayer));

    vbufinit(&layer->vbo);
    shaderinit(&layer->shader, vshader, fshader);
    texfind(&layer->texture, texpath);
    glm_mat4_identity(layer->modelmat);
    veclistinit(&layer->fontcolors, 4);
    listpushback(layerstack, layer);
    layer->dirty = 1;

    return layer;
}

/* Enables and initialises text rendering in given layer */
void
guienabletextlayer(GUILayer *layer, const char *vshader, const char *fshader,
        const char *fontpath, int font_w, int font_h)
{
    vbufinit(&layer->fontvbo);
    shaderinit(&layer->fontshader, vshader, fshader);
    fontfind(&layer->fonttex, fontpath, font_w, font_h);
    fontcalctexcoords(&layer->fonttex, 255);
    layer->textenabled = 1;
}

/* Sets GUI element to scale in relativity  */
void
guienableratioresizing(GUIElement *elem, float ratio_h)
{
    BIT_SET(elem->attr, GUI_ATTR_WH_RESIZE);
    elem->ratio_h = ratio_h;
}

/* Make an element and all its children visible */
void
guishowelem(GUIElement *elem)
{
    uint i;

    BIT_CLR(elem->state, GUI_STATE_INVISIBLE);

    for (i = 0; i < elem->children->size; i++) {
        guishowelem(listat(elem->children, i));
    }
   
    /* Bring to top of layer stack if this is the base element of the layer */
    if (!elem->parent) {
        guisetfocus(elem);

        for (i = 0; i < layerstack->size; i++) {
            GUILayer *layer = listat(layerstack, i);

            if (layer == elem->layer) {
                listmoveelem(layerstack, i, layerstack->size - 1);
                break;
            }
        }
    }
}

void
guisignalinterrupt(GUIElement *elem)
{
    interrupt = 1;
    elem->layer->dirty = 1;
}

void
guihandleinterrupt(void)
{
    interrupt = 0;
}

/* Make an element and all its children invisible */
void
guihideelem(GUIElement *elem)
{
    uint i;

    BIT_SET(elem->state, GUI_STATE_INVISIBLE);

    if (elem == kbdfocus) {
        kbdfocus = NULL;
    }

    for (i = 0; i < elem->children->size; i++) {
        guihideelem(listat(elem->children, i));
    }
}

/* Set a GUI element as the base element for a layer */
void
guilinklayer(GUIElement *elem, GUILayer *layer)
{
    elem->layer = layer;
    layer->baseelem = elem;
}

/* Set a GUI element as a child node to another element */
void
guiaddelem(GUIElement *dest, GUIElement *elem)
{
    listpushback(dest->children, elem);
    elem->parent = dest;
    elem->layer = dest->layer;

    if (dest->state & GUI_STATE_INVISIBLE) {
        guihideelem(elem);
    }
}

void
guisetupdatefunc(GUIElement *elem,
        void (*func)(GUIElement *elem, float dt, ...))
{
    elem->update = func;
}

void
guiseteventcallback(GUIElement *elem, int eventtype,
        void (*func)(GUIElement *self, ...))
{
    elem->callbacks[eventtype] = func;
}

GUIElement *
guigetelem(const char *id)
{
    return htableget(elemtable, id);
}

Camera *
guigetcamera(void)
{
    return &guicam;
}

int
guihandle(Event *e)
{
    int i;
    GUILayer *layer;
    GUIElement *base;
    int captured = 0;

    // Check all elements for events, if input event is captured, return 1
    // If event falls through entirely, return 0.
    if (e->type == SDL_MOUSEMOTION ||
        e->type == SDL_MOUSEBUTTONUP ||
        e->type == SDL_MOUSEBUTTONDOWN ||
        e->type == SDL_MOUSEWHEEL) {

        for (i = layerstack->size - 1; i >= 0; i--) {
            layer = listat(layerstack, i);
            base = layer->baseelem;
            captured += guihandlemouse(base, e);
            captured += ((base->attr & GUI_ATTR_BLOCKING) && 
                    !(base->state & GUI_STATE_INVISIBLE));

            if (captured > 0) {
                break;
            }
        }
    } else if (e->type == SDL_KEYDOWN ||
               e->type == SDL_KEYUP ||
               e->type == SDL_TEXTINPUT) {
        if (kbdfocus) {
            guihandlekeyboard(kbdfocus, e);
            captured += 1;
        }
    }
    if (kbdfocus) {
        captured += 1;
    }

    return captured;
}

void
guiupdateelem(GUIElement *elem, float dt)
{
    uint i;
    GUIElement *child;

    if (elem->update) {
        elem->update(elem, dt);
    }
    for (i = 0; i < elem->children->size; i++) {
        child = listat(elem->children, i);
        guiupdateelem(child, dt);
    }
}

/* Iterate through all GUI elements and check for movement, resizing, 
 * remeshing.  Calls custom update callback if it exists. */
void
guiupdate(float dt)
{
    uint i;
    int mx, my, diff_mx, diff_my;
    GUIElement *elem;
    GUILayer *layer;
    vec3 transvec = {0};
    float off_x, off_y, max_x, max_y, min_x, min_y;

    mousegetstate(&mx, &my);
    my = guicam.height - my;
    diff_mx = mx - prev_mx;
    diff_my = my - prev_my;

    for (i = 0; i < layerstack->size; i++) {
        layer = listat(layerstack, i);
        elem = layer->baseelem;

        if (elem->state & GUI_STATE_INVISIBLE) {
            continue;
        }
        
        /* Call recursive element update */
        guiupdateelem(elem, dt);

        /* Reposition grabbed movable element */
        if (elem->attr & GUI_ATTR_MOVABLE) {
            if (elem->state & GUI_STATE_MOUSE_LB &&
                elem->prevstate & GUI_STATE_MOUSE_LB) {
                layer = elem->layer;

                transvec[0] = diff_mx;
                transvec[1] = diff_my;
                off_x = transvec[0] + layer->modelmat[3][0];
                off_y = transvec[1] + layer->modelmat[3][1];
                max_x = (elem->x + elem->w) * guicam.width;
                max_y = (elem->y + elem->h) * guicam.height;
                min_x = elem->x * guicam.width;
                min_y = elem->y * guicam.height;
                
                if (max_x + off_x > guicam.width) {
                    transvec[0] = guicam.width - max_x - layer->modelmat[3][0];
                } else if (min_x + off_x < 0) {
                    transvec[0] = -min_x - layer->modelmat[3][0];
                }
                if (max_y + off_y > guicam.height) {
                    transvec[1] = guicam.height - max_y - layer->modelmat[3][1];
                } else if (min_y + off_y < 0) {
                    transvec[1] = -min_y - layer->modelmat[3][1];
                }
                glm_translate(layer->modelmat, transvec);
            }
        }
    }
    if (kbdfocus) {
        kbdfocus->textgrid->cursor.ticks += dt * 1000;
        if (kbdfocus->textgrid->cursor.ticks > 500 &&
                kbdfocus->textgrid->cursor.blink == 1) {
            kbdfocus->textgrid->cursor.blink = 0;
            kbdfocus->layer->dirty = 1;
        } else if (kbdfocus->textgrid->cursor.ticks > 1000 &&
                kbdfocus->textgrid->cursor.blink == 0) {
            kbdfocus->textgrid->cursor.blink = 1;
            kbdfocus->textgrid->cursor.ticks = 0;
            kbdfocus->layer->dirty = 1;
        }
    }
    
    prev_mx = mx;
    prev_my = my;
}

/* Render the entire layer stack */
void
guirender(void)
{
    uint i;
    GUILayer *layer;
    Shader *shader;

    for (i = 0; i < layerstack->size; i++) {
        layer = listat(layerstack, i);
        shader = &layer->shader;

        if (layer->dirty) {
            guimesh(layer->baseelem);
        }
        cameracalcvectors(&guicam);
        cameracalcviewmatrix(&guicam);
        cameracalcprojviewmatrix(&guicam);

        texbind(&layer->texture);
        shaderbind(shader);
        shaderloadprojviewmat(shader, guicam.projviewmat);
        shaderloadviewmat(shader, guicam.viewmat);
        shaderloadmodelmat(shader, layer->modelmat);

        vbufbindvao(&layer->vbo);

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);

        atra_drawtriangles(&layer->vbo);

        shaderunbind();

        /* Font rendering */
        if (layer->textenabled) {
            shader = &layer->fontshader;

            fontbind(&layer->fonttex);
            shaderbind(shader);
            shaderloadprojviewmat(shader, guicam.projviewmat);
            shaderloadviewmat(shader, guicam.viewmat);
            shaderloadmodelmat(shader, layer->modelmat);

            vbufbindvao(&layer->fontvbo);

            atra_drawtriangles(&layer->fontvbo);

            shaderunbind();
        }
    }
}

/* Schedule all layers to redraw on next guirender() call */
void
guiredraw(void)
{
    uint i;
    GUILayer *layer;
    float ratio_w = maincam->width / (float)guicam.width;
    float ratio_h = maincam->height / (float)guicam.height;

    /* Recalculate projection matrix */
    guicam.width = maincam->width;
    guicam.height = maincam->height;
    glm_ortho(0, guicam.width,
              0, guicam.height,
              0.1f, 1000.0f,
              guicam.projmat);

    for (i = 0; i < layerstack->size; i++)
    {
        layer = listat(layerstack, i);
        layer->dirty = 1;
        layer->modelmat[3][0] *= ratio_w;
        layer->modelmat[3][1] *= ratio_h;
    }
}

void
guimeshscrollbar(GUIElement *elem, float totalrows, float x_min, float y_min,
        float x_max, float y_max, float depth)
{
    GUITextGrid *grid = elem->textgrid;
    GUIScrollBar *bar = elem->scrollbar;
    VBuffer *vbo = &elem->layer->vbo;
    vec3 vert;
    vec2 uv;
    vec3 norm = {0, 0, 1};
    ivec3 index;
    float barsize, barpos, disttop;
    uint indexcount = vbo->indexcount - vbo->indexcount / 3;

    x_min = x_max + bar->x_offset;
    x_max += bar->width + bar->x_offset;
    y_min -= bar->h_extra;
    y_max += bar->h_extra;
    totalrows = grid->glyphs->size / grid->colcount;

    /*
    if (totalrows > grid->rowcount &&
            grid->startrow + grid->rowcount > totalrows)
    {
        grid->startrow = totalrows - grid->rowcount;
    }*/

    if (totalrows <= grid->rowcount) {
        barsize = y_max - y_min;
        barpos = y_min;
    } else {
        barsize = (grid->rowcount / totalrows) * (y_max - y_min);
        disttop = (grid->startrow / totalrows) * (y_max - y_min);
        barpos = y_max - barsize - disttop;
    }

    /* Render scroll bar background */
    vert[0] = x_max;
    vert[1] = y_min;
    vert[2] = depth; 
    uv[0] = bar->backtex[0];
    uv[1] = bar->backtex[1];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[0] = x_min;
    vert[1] = y_min;
    uv[0] = bar->backtex[2];
    uv[1] = bar->backtex[3];
    vbufaddvertex(vbo, vert, uv, norm);
    
    vert[0] = x_min;
    vert[1] = y_max;
    uv[0] = bar->backtex[4];
    uv[1] = bar->backtex[5];
    vbufaddvertex(vbo, vert, uv, norm);
    
    vert[0] = x_max;
    vert[1] = y_max;
    uv[0] = bar->backtex[6];
    uv[1] = bar->backtex[7];
    vbufaddvertex(vbo, vert, uv, norm);

    index[0] = indexcount + 0;
    index[1] = indexcount + 1;
    index[2] = indexcount + 2;
    vbufaddindex(vbo, index);
    
    index[0] = indexcount + 2;
    index[1] = indexcount + 3;
    index[2] = indexcount + 0;
    vbufaddindex(vbo, index);

    indexcount += 4;

    /* Render scroll bar */
    vert[0] = x_max - bar->bar_w_offset;
    vert[1] = barpos;
    uv[0] = bar->bartex[0];
    uv[1] = bar->bartex[1];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[0] = x_min + bar->bar_w_offset;
    vert[1] = barpos;
    uv[0] = bar->bartex[2];
    uv[1] = bar->bartex[3];
    vbufaddvertex(vbo, vert, uv, norm);
    
    vert[0] = x_min + bar->bar_w_offset;
    vert[1] = barpos + barsize;
    uv[0] = bar->bartex[4];
    uv[1] = bar->bartex[5];
    vbufaddvertex(vbo, vert, uv, norm);
    
    vert[0] = x_max - bar->bar_w_offset;
    vert[1] = barpos + barsize;
    uv[0] = bar->bartex[6];
    uv[1] = bar->bartex[7];
    vbufaddvertex(vbo, vert, uv, norm);

    index[0] = indexcount + 0;
    index[1] = indexcount + 1;
    index[2] = indexcount + 2;
    vbufaddindex(vbo, index);
    
    index[0] = indexcount + 2;
    index[1] = indexcount + 3;
    index[2] = indexcount + 0;
    vbufaddindex(vbo, index);
}

/* Compose the mesh for the given frame and its children,
 * sets the VBO in the element layer */
void
guimesh(GUIElement *elem)
{
    uint i, totalrows;
    GUILayer *layer = elem->layer;
    GUIElement *next, *rel_elem;
    GUITextGrid *grid = elem->textgrid;
    VBuffer *vbo = &layer->vbo;
    List *agenda;
    int x_min, y_min, x_max, y_max, depth;
    float rel_x, rel_y, rel_w, rel_h, scr_ratio;

    vbufreset(vbo);

    if (layer->textenabled) {
        vbufreset(&layer->fontvbo);
        veclistclear(&layer->fontcolors);
    }

    depth = -950.0f;
    scr_ratio = guicam.height / (float)guicam.width;
    agenda = listcreate();
    listpushback(agenda, elem);

    while (agenda->size > 0) {
        elem = listat(agenda, 0);
        rel_elem = elem;
        next = elem->parent;
        rel_x = 0; rel_y = 0;
        rel_w = 1; rel_h = 1;

        /* Calculate relative positioning based on parents */
        while (next) {
            rel_x = (rel_x + rel_elem->x) * next->w;
            rel_w *= rel_elem->w;


            if (rel_elem->attr & GUI_ATTR_WH_RESIZE) {
                rel_h *= rel_elem->w * rel_elem->ratio_h;
            } else {
                rel_h *= rel_elem->h;
            } 
 
            if (next->attr & GUI_ATTR_WH_RESIZE) {
                rel_y = (rel_y + rel_elem->y) * next->w * next->ratio_h;
            } else {
                rel_y = (rel_y + rel_elem->y) * next->h;
            }

            rel_elem = next;
            next = rel_elem->parent;
        }

        if (elem->children->size > 0) {
            for (i = 0; i < elem->children->size; i++) {
                listpushback(agenda, listat(elem->children, i));
            }
        }
        if (!(elem->state & GUI_STATE_INVISIBLE)) {
            x_min = (rel_x + rel_elem->x) * guicam.width;
            x_max = x_min + rel_w * (rel_elem->w * guicam.width);
            
            if (rel_elem->attr & GUI_ATTR_WH_RESIZE) {
                float height = rel_elem->w * rel_elem->ratio_h;
                float offset = height * rel_elem->y;
                float rel_min = rel_y + rel_elem->y * scr_ratio - offset;
                float rel_max = rel_min + height * rel_h;

                y_min = rel_min * guicam.width;
                y_max = rel_max * guicam.width;
            } else {
                y_min = (rel_y + rel_elem->y) * guicam.height;
                y_max = y_min + rel_h * rel_elem->h * guicam.height;
            }

            if (elem->attr & GUI_ATTR_HASTEXTURE) {
                guipushquad(vbo, x_min, y_min, x_max, y_max,
                        depth, elem->backtex);
                depth += 2.5f;
            }
            if (elem->attr & GUI_ATTR_HASBORDER) {
                guimeshborders(elem, x_min, y_min, x_max, y_max, depth);
                depth += 2.5f;
            }
            if (elem->attr & GUI_ATTR_HASTEXT &&
                    elem->layer->dirty &&
                    elem->textgrid) {
                grid = elem->textgrid;
                
                guicalcfontgridvertices(grid, x_min, y_min,
                        x_max, y_max, depth);
                guitextwritetext(elem);

                if (!grid->textset) {
                    //guitextwritetext(elem);
                } else {
                    grid->textset = 0;
                }
                totalrows = guimeshfont(grid, elem);
                grid->dirty = 0;
                depth += 2.5f;

                if (elem->attr & GUI_ATTR_SCROLLBAR &&
                    elem->attr & GUI_ATTR_SCROLLABLE) {
                    guimeshscrollbar(elem, totalrows, x_min, y_min,
                            x_max, y_max, depth);
                    depth += 2.5f;
                }
            }
        }
        listremove(agenda, 0);
    }
    vbufbind(vbo);

    if (layer->textenabled) {
        vbufbind(&layer->fontvbo);
        vbufbindvbo(&layer->fontvbo, &layer->fontcolors);
    }
    listdelete(agenda);
    layer->dirty = 0;
}

void
guimeshborders(GUIElement *elem, float x_min, float y_min,
        float x_max, float y_max, float depth)
{
    VBuffer *vbo = &elem->layer->vbo;
    float x1, x2, y1, y2;
    int size = elem->bordersize;

    /* Top-right corner */
    x1 = x_max; x2 = x_max + size;
    y1 = y_max; y2 = y_max + size;
    guipushquad(vbo, x1, y1, x2, y2, depth, elem->cornertex);

    /* Top edge */
    x2 = x_min;
    guipushquad(vbo, x2, y1, x1, y2, depth, elem->edgetex);

    /* Top-left corner */
    x1 = x_min - size;
    guipushquadinverted(vbo, y1, x2, y2, x1, depth, elem->cornertex);

    /* Left edge */
    y2 = y_min;
    guipushquadinverted(vbo, y2, x2, y1, x1, depth, elem->edgetex);

    /* Bottom-left corner */
    y1 = y_min - size;
    guipushquad(vbo, x2, y2, x1, y1, depth, elem->cornertex);

    /* Bottom edge */
    x1 = x_max;
    guipushquad(vbo, x1, y2, x2, y1, depth, elem->edgetex);

    /* Bottom-right corner */
    x2 = x_max + size;
    guipushquadinverted(vbo, y2, x1, y1, x2, depth, elem->cornertex);

    /* Right edge */
    y1 = y_max;
    guipushquadinverted(vbo, y1, x1, y2, x2, depth, elem->edgetex);
}

void
guipushquad(VBuffer *vbo, float x_min, float y_min,
        float x_max, float y_max, float depth, float uvs[8])
{
    vec3 vert;
    vec2 uv;
    vec3 norm = {0, 0, 1};
    ivec3 index;
    int indexbase = vbo->indexcount - vbo->indexcount / 3;

    vert[2] = depth;

    vert[0] = x_max;
    vert[1] = y_min;
    uv[0] = uvs[0];
    uv[1] = uvs[1];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[0] = x_min;
    vert[1] = y_min;
    uv[0] = uvs[2];
    uv[1] = uvs[3];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[0] = x_min;
    vert[1] = y_max;
    uv[0] = uvs[4];
    uv[1] = uvs[5];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[0] = x_max;
    vert[1] = y_max;
    uv[0] = uvs[6];
    uv[1] = uvs[7];
    vbufaddvertex(vbo, vert, uv, norm);

    index[0] = indexbase + 0;
    index[1] = indexbase + 1;
    index[2] = indexbase + 2;
    vbufaddindex(vbo, index);

    index[0] = indexbase + 2;
    index[1] = indexbase + 3;
    index[2] = indexbase + 0;
    vbufaddindex(vbo, index);
}

void
guipushquadinverted(VBuffer *vbo, float x_min, float y_min,
        float x_max, float y_max, float depth, float uvs[8])
{
    vec3 vert;
    vec2 uv;
    vec3 norm = {0, 0, 1};
    ivec3 index;
    int indexbase = vbo->indexcount - vbo->indexcount / 3;

    vert[2] = depth;

    vert[1] = x_max;
    vert[0] = y_min;
    uv[0] = uvs[0];
    uv[1] = uvs[1];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[1] = x_min;
    vert[0] = y_min;
    uv[0] = uvs[2];
    uv[1] = uvs[3];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[1] = x_min;
    vert[0] = y_max;
    uv[0] = uvs[4];
    uv[1] = uvs[5];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[1] = x_max;
    vert[0] = y_max;
    uv[0] = uvs[6];
    uv[1] = uvs[7];
    vbufaddvertex(vbo, vert, uv, norm);

    index[0] = indexbase + 0;
    index[1] = indexbase + 1;
    index[2] = indexbase + 2;
    vbufaddindex(vbo, index);

    index[0] = indexbase + 2;
    index[1] = indexbase + 3;
    index[2] = indexbase + 0;
    vbufaddindex(vbo, index);
}

void
guisetbacktexture(GUIElement *elem, int x, int y, int w, int h)
{
    TextureRect rect = {x, y, w, h};

    texgetrect(&elem->layer->texture, &rect, elem->backtex);
}

void
guisetedgetexture(GUIElement *elem, int x, int y, int w, int h)
{
    TextureRect rect = {x, y, w, h};

    texgetrect(&elem->layer->texture, &rect, elem->edgetex);
}

void
guisetcornertexture(GUIElement *elem, int x, int y, int w, int h)
{
    TextureRect rect = {x, y, w, h};

    texgetrect(&elem->layer->texture, &rect, elem->cornertex);
}

void
guisetscrollbarbacktex(GUIElement *elem, int x, int y, int w, int h)
{
    TextureRect rect = {x, y, w, h};

    texgetrect(&elem->layer->texture, &rect, elem->scrollbar->backtex);
}

void
guisetscrollbarbartex(GUIElement *elem, int x, int y, int w, int h)
{
    TextureRect rect = {x, y, w, h};

    texgetrect(&elem->layer->texture, &rect, elem->scrollbar->bartex);
}

void guisetcursortexture(GUIElement *elem, int x, int y, int w, int h)
{
    TextureRect rect = {x, y, w, h};

    UNUSED(rect);

    fontget(&elem->layer->fonttex, 0, elem->textgrid->cursor.uvs);
}

/* Set cursor position via mouse position */
void
guicursormouseset(GUIElement *elem, int mx, int my)
{
    float rel_x, rel_y, rel_w, rel_h;
    GUIElement *next = elem->parent;
    GUITextGrid *grid = elem->textgrid;
    GUIGlyph *glyph;
    vec4 *transvec = &elem->layer->modelmat[3];
    uint grid_x, grid_y, rel_mx, rel_my;
    uint read, head, idx;
 
    rel_x = 0; rel_y = 0;
    rel_w = 1; rel_h = 1;

    while (next) {
        rel_x = (rel_x + elem->x) * next->w;
        rel_w *= elem->w;

        if (next->attr & GUI_ATTR_WH_RESIZE) {
            rel_y = (rel_y + elem->y) * next->w * next->ratio_h;
        } else {
            rel_y = (rel_y + elem->y) * next->h;
        }

        if (elem->attr & GUI_ATTR_WH_RESIZE) {
            rel_h *= elem->w * elem->ratio_h;
        } else {
            rel_h *= elem->h;
        }

        elem = next;
        next = elem->parent;
    }
    rel_x = (rel_x + elem->x) * guicam.width + (*transvec)[0];
    rel_w *= elem->w * guicam.width;

    if (elem->attr & GUI_ATTR_WH_RESIZE) {
        rel_y = (rel_y + elem->y) * guicam.width + (*transvec)[1];
        rel_h *= elem->w * elem->ratio_h * guicam.width;
    } else {
        rel_y = (rel_y + elem->y) * guicam.height + (*transvec)[1];
        rel_h *= elem->h * guicam.height;
    }

    rel_mx = mx - rel_x;
    rel_my = my - rel_y;

    grid_x = rel_mx / (rel_w / grid->colcount);
    grid_y = rel_my / (rel_h / grid->rowcount);
    grid_y = grid->startrow + grid->rowcount - grid_y - 1;

    idx = grid_x + grid_y * grid->colcount;

    if (idx >= grid->glyphs->size) {
        idx = grid->glyphs->size - 1;
    }

    for (read = 0, head = 0; head < grid->glyphs->size; head++) {
        glyph = listat(grid->glyphs, head);

        if (head == idx) {
            if (glyph->c == '\0') {
                read--;
            }
            while (glyph->c == '\0') {
                head--;
                glyph = listat(grid->glyphs, head);
            }
            grid->cursor.pos = head;
            grid->cursor.textpos = read;
            return;
        }
        
        if (glyph->c != '\0') {
            read++;
        }
    }
}

/* Handle mouse input and make relevant state changes */
int
guihandlemouse(GUIElement *elem, Event *e)
{
    uint i;
    int mx, my, isect, captured = 0;
    GUIElement *child;
    uint mstate = mousegetstate(&mx, &my);

    my = guicam.height - my;
    isect = guimouseintersects(elem, mx, my);
    interrupt = 0;

    if (isect &&
        !(elem->state & GUI_STATE_INVISIBLE) &&
        !(elem->state & GUI_STATE_DISABLED)) {
        sectstart:
        if (elem->children->size > 0) {
            for (i = 0; i < elem->children->size; i++) {
                child = listat(elem->children, i);

                if (guimouseintersects(child, mx, my) &&
                    !(child->state & GUI_STATE_INVISIBLE) &&
                    !(child->state & GUI_STATE_DISABLED) &&
                    !(child->attr  & GUI_ATTR_NOCAPTURE)) {
                    elem = child;
                    captured++;
                    goto sectstart;
                }
            }
        }
        elem->prevstate = elem->state;

        if (e->type == SDL_MOUSEMOTION) {
            if (!(elem->state & GUI_STATE_MOUSEOVER)) {
                if (elem->callbacks[GUI_EVENT_MOUSEOVER]) {
                    elem->callbacks[GUI_EVENT_MOUSEOVER](elem);
                }
                BIT_SET(elem->state, GUI_STATE_MOUSEOVER);
            }
        } else if (e->type == SDL_MOUSEBUTTONDOWN) {
            /* Pull current layer up to top of stack */
            for (i = 0; i < layerstack->size; i++) {
                GUILayer *layer = listat(layerstack, i);

                if (layer == elem->layer) {
                    listmoveelem(layerstack, i, layerstack->size - 1);
                    break;
                }
            }

            if (mstate & MOUSE_BUTTON(LEFT)) {
                if (elem->attr & GUI_ATTR_CLICKABLE && 
                    elem->callbacks[GUI_EVENT_MOUSEDOWN]) {
                    elem->callbacks[GUI_EVENT_MOUSEDOWN](elem,
                            MOUSE_BUTTON(LEFT));
                    if (interrupt) {
                        guihandleinterrupt();
                        return captured;
                    }
                }
                BIT_SET(elem->state, GUI_STATE_MOUSE_LB);
            }
            if (mstate & MOUSE_BUTTON(RIGHT)) {
                if (elem->attr & GUI_ATTR_CLICKABLE && 
                    elem->callbacks[GUI_EVENT_MOUSEDOWN]) {
                    elem->callbacks[GUI_EVENT_MOUSEDOWN](elem,
                            MOUSE_BUTTON(RIGHT));
                    if (interrupt) {
                        guihandleinterrupt();
                        return captured;
                    }
                }
                BIT_SET(elem->state, GUI_STATE_MOUSE_RB);
            }
            if (mstate & MOUSE_BUTTON(MIDDLE)) {
                if (elem->attr & GUI_ATTR_CLICKABLE && 
                    elem->callbacks[GUI_EVENT_MOUSEDOWN]) {
                    elem->callbacks[GUI_EVENT_MOUSEDOWN](elem,
                            MOUSE_BUTTON(MIDDLE));
                    if (interrupt) {
                        guihandleinterrupt();
                        return captured;
                    }
                }
                BIT_SET(elem->state, GUI_STATE_MOUSE_MB);
            }
            guisetfocus(elem);

        
            /* Set keyboard focus */
            if (elem->attr & GUI_ATTR_TEXTINPUT) {
                kbdfocus = elem;
                guicursormouseset(elem, mx, my);
                //elem->textgrid->dirty = 1;
                elem->textgrid->cursor.ticks = 0;
                elem->textgrid->cursor.blink = 1;
                elem->layer->dirty = 1;
            } else {
                if (kbdfocus) {
                    //elem->textgrid->dirty = 1;
                    kbdfocus->textgrid->cursor.ticks = 0;
                    kbdfocus->textgrid->cursor.blink = 0;
                    kbdfocus->layer->dirty = 1;
                    kbdfocus = NULL;
                }
            }
        } else if (e->type == SDL_MOUSEBUTTONUP) {
            if (!(mstate & MOUSE_BUTTON(LEFT)) &&
                elem->state & GUI_STATE_MOUSE_LB) {
                if (elem->attr & GUI_ATTR_CLICKABLE && 
                    elem->callbacks[GUI_EVENT_MOUSEUP]) {
                    elem->callbacks[GUI_EVENT_MOUSEUP](elem,
                            MOUSE_BUTTON(LEFT));
                    if (interrupt) {
                        guihandleinterrupt();
                        return captured;
                    }
                }
                BIT_CLR(elem->state, GUI_STATE_MOUSE_LB);
            }
            if (!(mstate & MOUSE_BUTTON(RIGHT)) &&
                elem->state & GUI_STATE_MOUSE_RB) {
                if (elem->attr & GUI_ATTR_CLICKABLE && 
                    elem->callbacks[GUI_EVENT_MOUSEUP]) {
                    elem->callbacks[GUI_EVENT_MOUSEUP](elem,
                            MOUSE_BUTTON(RIGHT));
                    if (interrupt) {
                        guihandleinterrupt();
                        return captured;
                    }
                }
                BIT_CLR(elem->state, GUI_STATE_MOUSE_RB);
            }
            if (!(mstate & MOUSE_BUTTON(MIDDLE)) &&
                elem->state & GUI_STATE_MOUSE_MB) {
                if (elem->attr & GUI_ATTR_CLICKABLE && 
                    elem->callbacks[GUI_EVENT_MOUSEUP]) {
                    elem->callbacks[GUI_EVENT_MOUSEUP](elem,
                            MOUSE_BUTTON(MIDDLE));
                    if (interrupt) {
                        guihandleinterrupt();
                        return captured;
                    }
                }
                BIT_CLR(elem->state, GUI_STATE_MOUSE_MB);
            }
        } else if (e->type == SDL_MOUSEWHEEL) {
            if (e->wheel.y > 0) {
                if (elem->callbacks[GUI_EVENT_SCROLLUP]) {
                    elem->callbacks[GUI_EVENT_SCROLLUP](elem);
                }
            } else if (e->wheel.y < 0) {
                if (elem->callbacks[GUI_EVENT_SCROLLDN]) {
                    elem->callbacks[GUI_EVENT_SCROLLDN](elem);
                }
            }
        }
        captured++;
    }

    /* Check for a mouse events and unset relevant state,
     * for all GUI elements. */
    if (e->type == SDL_MOUSEBUTTONUP) {
        for (i = 0; i < elemlist->size; i++) {
            elem = listat(elemlist, i);
            if (!(mstate & MOUSE_BUTTON(LEFT)) &&
                elem->state & GUI_STATE_MOUSE_LB) {

                BIT_CLR(elem->state, GUI_STATE_MOUSE_LB);
            }
            if (!(mstate & MOUSE_BUTTON(RIGHT)) &&
                elem->state & GUI_STATE_MOUSE_RB) {

                BIT_CLR(elem->state, GUI_STATE_MOUSE_RB);
            }
            if (!(mstate & MOUSE_BUTTON(MIDDLE)) &&
                elem->state & GUI_STATE_MOUSE_MB) {

                BIT_CLR(elem->state, GUI_STATE_MOUSE_MB);
            }
        }
    } else if (e->type == SDL_MOUSEMOTION) {
        for (i = 0; i < elemlist->size; i++) {
            elem = listat(elemlist, i);
            if (elem->state & GUI_STATE_MOUSEOVER &&
                !guimouseintersects(elem, mx, my)) {
                if (elem->callbacks[GUI_EVENT_MOUSELEAVE]) {
                    elem->callbacks[GUI_EVENT_MOUSELEAVE](elem);
                }
                BIT_CLR(elem->state, GUI_STATE_MOUSEOVER);
            }
        }
    }

    /* If event falls through, unset focuses */
    if (e->type == SDL_MOUSEBUTTONDOWN && !captured) {
        if (focus && focus->callbacks[GUI_EVENT_LOSEFOCUS]) {
            focus->callbacks[GUI_EVENT_LOSEFOCUS](focus);
        }
        focus = NULL;

        if (kbdfocus) {
            kbdfocus->textgrid->cursor.ticks = 0;
            kbdfocus->textgrid->cursor.blink = 0;
            kbdfocus->layer->dirty = 1;
            kbdfocus = NULL;
        }
    }

    return captured;
}

GUIElement *
guigetkbdfocus()
{
    return kbdfocus;
}

void
guihandletextinput(GUIElement *elem, uchar chr)
{
    GUITextGrid *grid = elem->textgrid;
    GUIGlyph *data;

    switch (chr) {
    case '\0':
        return;
    case '\b':
        guikbdbackspace(elem);
        break;
    case '\n':
        guikbdnewline(elem);
        break;
    case '\t':
        guikbdtab(elem);
        break;
    default:
        data = malloc(sizeof(GUIGlyph));
        data->c = chr;
        listinsert(grid->glyphs, data, grid->cursor.pos);
        guicursormoveright(grid);
        break;
    }
}

int
guihandlekeyboard(GUIElement *elem, Event *e)
{
    int captured = 0;
    GUITextGrid *grid = elem->textgrid;

    if (kbdfocus) {
        char chr = '\0';
        int change = 0;
        //Go through key events and handle appropriately
        if (e->type == SDL_TEXTINPUT) {
            chr = utf8toascii(e->text.text);
            change = 1;
        } else if (e->type == SDL_KEYDOWN) {
            switch (e->key.keysym.sym) {
            case KEYCODE(RETURN):
                chr = '\n';
                change = 1;
                break;
            case KEYCODE(BACKSPACE):
                chr = '\b';
                change = 1;
                break;
            case KEYCODE(TAB):
                chr = '\t';
                change = 1;
                break;
            case KEYCODE(DELETE):
                guikbddelete(elem);
                change = 1;
                break;
            case KEYCODE(RIGHT):
                guicursormoveright(elem->textgrid);
                change = 1;
                break;
            case KEYCODE(LEFT):
                guicursormoveleft(elem->textgrid);
                change = 1;
                break;
            case KEYCODE(UP):
                guicursormoveup(elem->textgrid);
                change = 1;
                break;
            case KEYCODE(DOWN):
                guicursormovedn(elem->textgrid);
                change = 1;
                break;
            default:
                break;
            }
            //printf("Got char: 0x%x\n", (uchar)chr);
        }
        if (change) {
            guihandletextinput(kbdfocus, chr);
            guitextreadtext(elem);
            guitextwritetext(elem);
            grid->cursor.ticks = 0;
            grid->cursor.blink = 1;
            //grid->dirty = 1;
            grid->textset = 1;
            kbdfocus->layer->dirty = 1;
        }
        captured += 1;
    } else {
        if (e->type == SDL_KEYDOWN) {
            if (elem->callbacks[GUI_EVENT_KEYDOWN]) {
                elem->callbacks[GUI_EVENT_KEYDOWN](elem, keygetstates());
            }
        } else if (e->type == SDL_KEYUP) {
            if (elem->callbacks[GUI_EVENT_KEYUP]) {
                elem->callbacks[GUI_EVENT_KEYUP](elem, keygetstates());
            }
        }
        //captured += 1;
    }
    return captured;
}

/* Sets new GUI focus, triggering appropriate callbacks in the process */
void
guisetfocus(GUIElement *elem)
{
    if (elem != focus) {
        if (elem->callbacks[GUI_EVENT_GAINFOCUS]) {
            elem->callbacks[GUI_EVENT_GAINFOCUS](elem);
        }
        if (focus) {
            if (focus->callbacks[GUI_EVENT_LOSEFOCUS]) {
                focus->callbacks[GUI_EVENT_LOSEFOCUS](focus);
            }
            BIT_CLR(focus->state, GUI_STATE_HASFOCUS);
        }
        BIT_SET(elem->state, GUI_STATE_HASFOCUS);
    }
    focus = elem;
}

void
guisetbordersize(GUIElement *elem, uint size)
{
    elem->bordersize = size;
}

/* Returns a pointer to the current focused window */
GUIElement *
guigetfocus(void)
{
    return focus;
}

/* Returns 1 if mouse intersects GUI element, otherwise returns 0 */
int
guimouseintersects(GUIElement *elem, int mx, int my)
{
    float rel_x, rel_y, rel_w, rel_h;
    GUIElement *next = elem->parent;
    vec4 *transvec = &elem->layer->modelmat[3];
    const float scr_ratio = guicam.height / (float)guicam.width;
 
    rel_x = 0; rel_y = 0;
    rel_w = 1; rel_h = 1;

    while (next) {
        rel_x = (rel_x + elem->x) * next->w;
        rel_w *= elem->w;
        
        if (next->attr & GUI_ATTR_WH_RESIZE) {
            rel_y = (rel_y + elem->y) * next->w * next->ratio_h;
        } else {
            rel_y = (rel_y + elem->y) * next->h;
        }

        if (elem->attr & GUI_ATTR_WH_RESIZE) {
            rel_h *= elem->w * elem->ratio_h;
        } else {
            rel_h *= elem->h;
        }
        elem = next;
        next = elem->parent;
    }
    rel_x = (rel_x + elem->x) * guicam.width + (*transvec)[0];
    rel_w *= elem->w * guicam.width;
    
    if (elem->attr & GUI_ATTR_WH_RESIZE) {
        float offset = elem->w * elem->ratio_h * elem->y;

        rel_y = (rel_y + elem->y * scr_ratio - offset) *
            guicam.width + (*transvec)[1];
        rel_h *= elem->w * elem->ratio_h * guicam.width;
    } else {
        rel_y = (rel_y + elem->y) * guicam.height + (*transvec)[1];
        rel_h *= elem->h * guicam.height;
    }

    if (mx < rel_x)
        return 0;
    else if (my < rel_y)
        return 0;
    else if (mx > rel_x + rel_w)
        return 0;
    else if (my > rel_y + rel_h)
        return 0;

    return 1;
}

/* Deallocates element and its contents.
 * Recursively deletes children if cascade is set. */
void
guideleteelem(GUIElement *elem, int cascade)
{
    uint i;
    GUITextGrid *grid = elem->textgrid;
    GUIElement *elem_tmp;

    if (cascade) {
        for (i = 0; i < elem->children->size; i++) {
            guideleteelem(listat(elem->children, i), 1);
        }
    }

    if (elem->parent) {
        for (i = 0; i < elem->parent->children->size; i++) {
            elem_tmp = listat(elem->parent->children, i);

            if (elem == elem_tmp) {
                listremove(elem->parent->children, i);
            }
        }
    }
    
    for (i = 0; i < elemtable->size; i++) {
        if (elem == elemtable->vals[i]) {
            htableremove(elemtable, elemtable->keys[i]);
        }
    }
    
    for (i = 0; i < elemlist->size; i++) {
        if (elem == elemlist->elem[i]) {
            listremove(elemlist, i);
        }
    }

    if (grid) {
        listdealloc(grid->glyphs);
        listdelete(grid->glyphs);
        free(grid->text);
        free(grid->view);
        free(grid);
    }
    listdelete(elem->children);
    free(elem);
}

void
guideletelayer(GUILayer *layer)
{
    vbufdelete(&layer->vbo);

    if (layer->textenabled) {
        vbufdelete(&layer->fontvbo);
        veclistdelete(&layer->fontcolors);
        shaderdelete(&layer->fontshader);
    }
    shaderdelete(&layer->shader);
    free(layer);
}

void
guishutdown(void)
{
    uint i;
    GUILayer *layer;

    for (i = 0; i < layerstack->size; i++) {
        layer = listat(layerstack, i);
        
        if (layer->baseelem) {
            guideleteelem(layer->baseelem, 1);
        }
        guideletelayer(layer);
    }
    listdelete(elemlist);
    listdelete(layerstack);
    htabledeallockeys(elemtable);
    htabledelete(elemtable);
    elemlist = NULL;
    layerstack = NULL;
    elemtable = NULL;
}

GUITextGrid *
guitextgridcreate(uint font_w, uint font_h, int fspacing, int lspacing)
{
    GUITextGrid *grid = calloc(1, sizeof(GUITextGrid));
    
    grid->fontwidth = font_w;
    grid->fontheight = font_h;
    grid->fontspacing = fspacing;
    grid->linespacing = lspacing;
    grid->text = NULL;
    grid->view = NULL;
    grid->glyphs = listcreate();
    grid->dirty = 1;
    grid->resized = 1;
    grid->textset = 1;
    grid->startrow = 0;
    grid->cursor = (GUITextCursor){0};
    grid->cursor.blink = 1;

    return grid;
}

GUIScrollBar *
guiscrollbarcreate(uint width, uint h_extra, uint bar_w_offset, uint x_offset)
{
    GUIScrollBar *bar = calloc(1, sizeof(GUIScrollBar));

    bar->width = width;
    bar->h_extra = h_extra;
    bar->bar_w_offset = bar_w_offset;
    bar->x_offset = x_offset;

    return bar;
}

GUIGlyph *
guiglyphcreate(void)
{
    GUIGlyph *glyph = malloc(sizeof(GUIGlyph));

    glyph->color[0] = 1.0f;
    glyph->color[1] = 1.0f;
    glyph->color[2] = 1.0f;
    glyph->color[3] = 1.0f;
    glyph->c = '\0';

    return glyph;
}

void
guitextgridadd(GUITextGrid *grid, GUIElement *parent)
{
    parent->textgrid = grid;
}

void
guiscrollbaradd(GUIScrollBar *bar, GUIElement *parent)
{
    parent->scrollbar = bar;
}

void
guisetgridtext(GUIElement *elem, const char *text)
{
    char *textcpy;
    GUITextGrid *grid = elem->textgrid;

    if (grid->text) {
        free(grid->text);
    }

    textcpy = strdup(text);
    grid->text = textcpy;
    grid->textset = 1;
}

void
guitextscrollup(GUIElement *elem, ...)
{
    GUITextGrid *grid = elem->textgrid;

    if (elem->textgrid->startrow > 0) {
        elem->textgrid->startrow--;
        //elem->textgrid->dirty = 1;
        elem->layer->dirty = 1;

        if (grid->cursor.pos > 
                (grid->startrow + grid->rowcount - 1) * grid->colcount) {
            guicursormoveup(grid);
        }
    }
}

void
guitextscrolldn(GUIElement *elem, ...)
{
    GUITextGrid *grid = elem->textgrid;
    uint maxlines = grid->glyphs->size / grid->colcount;

    if (maxlines < grid->rowcount) {
        return;
    }

    if (grid->startrow < maxlines - grid->rowcount) {
        elem->textgrid->startrow++;
        //elem->textgrid->dirty = 1;
        elem->layer->dirty = 1;

        if (grid->cursor.pos < grid->startrow * grid->colcount) {
            guicursormovedn(grid);
        }
    }
}

void
guitextscrollenable(GUIElement *elem)
{
    BIT_SET(elem->attr, GUI_ATTR_SCROLLABLE);
    guiseteventcallback(elem, GUI_EVENT_SCROLLUP, guitextscrollup);
    guiseteventcallback(elem, GUI_EVENT_SCROLLDN, guitextscrolldn);
}

void
guicalcfontgridvertices(GUITextGrid *grid, int x_min, int y_min,
        int x_max, int y_max, float depth)
{
    int i, j, idx;
    int linewidth, maxlines;
    float x0, y0, x1, y1;
    float linespace = grid->linespacing * 0.5;
    float fontspace = grid->fontspacing * 0.5;
    GUIGlyphBox *view;

    linewidth = (x_max - x_min) / (grid->fontwidth + grid->fontspacing);
    maxlines = (y_max - y_min) / (grid->fontheight + grid->linespacing);

    y1 = y_max - linespace;
    y0 = y1 - grid->fontheight;

    if (grid->view) {
        free(grid->view);
    }
            
    view = malloc(linewidth * maxlines * sizeof(GUIGlyphBox));
    idx = 0;

    // Get the container element width and calculate how many can fit in frame
    for (i = 0; i < maxlines; i++) {
        x0 = x_min + fontspace;
        x1 = x0 + grid->fontwidth;

        for (j = 0; j < linewidth; j++) {
            view[idx].verts[0][0] = x1;
            view[idx].verts[0][1] = y0;
            view[idx].verts[0][2] = depth;

            view[idx].verts[1][0] = x0;
            view[idx].verts[1][1] = y0;
            view[idx].verts[1][2] = depth;
            
            view[idx].verts[2][0] = x0;
            view[idx].verts[2][1] = y1;
            view[idx].verts[2][2] = depth;
            
            view[idx].verts[3][0] = x1;
            view[idx].verts[3][1] = y1;
            view[idx].verts[3][2] = depth;
            
            x0 = x1 + grid->fontspacing;
            x1 = x0 + grid->fontwidth;
            idx++;
        }
        y1 = y0 - grid->linespacing;
        y0 = y1 - grid->fontheight;
    }
    grid->colcount = linewidth;
    grid->rowcount = maxlines;
    grid->view = view;
}

void
guisetglyphcolor(GUIGlyph *glyph, float r, float g, float b, float a)
{
    glyph->color[0] = r;
    glyph->color[1] = g;
    glyph->color[2] = b;
    glyph->color[3] = a;
}

uint
guitextfindspace(GUITextGrid *grid, int pos, int step)
{
    uint dist, read;
    size_t len;
    char chr;

    len = strlen(grid->text) + 1;

    for (dist = 0, read = pos; (int)read >= 0 && read < len; dist++, read += step) {
        chr = grid->text[read];
        
        if (chr == '\0' || chr == ' ' ||
            chr == '\n' || chr == '\t') {
            return dist;
        }
    }

    return dist;
}

/* Moves glyph write head to next line, fills space with null */
void
guitextnewline(GUITextGrid *grid, uint *head)
{
    GUIGlyph *glyph;

    while ((*head + 1) % grid->colcount > 0) {
        glyph = guiglyphcreate();
        listpushback(grid->glyphs, glyph);
        *head += 1;
    }
}

/* Moves glyph write head 4 spaces or to next line, fills space with null */
void
guitexttab(GUITextGrid *grid, uint *head)
{
    uint i;
    GUIGlyph *glyph;

    for (i = 0; i < 3 && (*head + 1) % grid->colcount > 0; i++, *head += 1) {
        glyph = guiglyphcreate();
        listpushback(grid->glyphs, glyph);
    }
}

/* Sets the proper cursor position, correcting for line wraps. */
void
guitextadjustcursor(GUITextGrid *grid)
{
    uint read, head;
    GUIGlyph *glyph;
    size_t len;

    len = strlen(grid->text) + 1;

    for (read = 0, head = 0; read < len; read++, head++) {
        glyph = listat(grid->glyphs, head);

        while (glyph->c == '\0') {
            head++;
            glyph = listat(grid->glyphs, head);
        }

        if (read == grid->cursor.textpos) {
            grid->cursor.pos = head;
        }
    }
}

/* Perform relevant checks and wrap text to next line if applicable */
void
guitextwrapline(GUITextGrid *grid, uint read, uint *head)
{
    uint i, prevbreak, nextbreak, diff;
    char chr;

    for (i = 0; i < 2; i++) {
        chr = grid->text[read + i];

        if (chr == '\0' || chr == ' ' || chr == '\n' || chr == '\t') {
            return;
        }
    }
    prevbreak = guitextfindspace(grid, read, -1);
    nextbreak = guitextfindspace(grid, read, +1);
    diff = prevbreak + nextbreak;

    if (diff >= grid->colcount) {
        return;
    }
    *head -= prevbreak - 1;

    while (*head % grid->colcount != 0) {
        listinsert(grid->glyphs, guiglyphcreate(), *head);
        *head += 1;
    }
    *head += prevbreak - 1;

    //TODO: Attempt to get this working
    /*
    if (read - prevbreak < grid->cursor.textpos &&
            read + nextbreak > grid->cursor.textpos) {
        grid->cursor.pos = *head + ()
    }*/
}

/* Reads the contents of the the glyph grid into a string */
void
guitextreadtext(GUIElement *elem)
{
    uint read, strsize, strcap;
    GUITextGrid *grid = elem->textgrid;
    char *str, *newstr, *chr;
    GUIGlyph *glyph;

    strsize = 0;
    strcap = 8;
    str = malloc(8 * sizeof(char));
    
    for (read = 0; read <= grid->glyphs->size; read++) {
        if (strsize == strcap) {
            newstr = realloc(str, strcap * 2 * sizeof(char));

            if (!checkmalloc(newstr)) {
                str = newstr;
                strcap *= 2;
            } else {
                logprint(LOG_FATAL, "gui: Failed to allocate memory.");
            }
        }

        if (read == grid->glyphs->size) {
            str[strsize] = '\0';
            break;
        } 
        glyph = listat(grid->glyphs, read);

        if (glyph->c != '\0') {
            str[strsize] = glyph->c;
            strsize++;
        }
    }
    chr = strrchr(str, ' ');

    if (chr) {
        *chr = '\0';
    }
    if (grid->text) {
        free(grid->text);
    }
    grid->text = str;
    //TODO: Make sure this is freed on grid delete
}

/* Compiles the contents of text into a glyph grid. */
void
guitextwritetext(GUIElement *elem)
{
    uint read, head;
    GUITextGrid *grid = elem->textgrid;
    GUIGlyph *glyph;
    size_t len;

    if (grid->glyphs) {
        listdealloc(grid->glyphs);
        listclear(grid->glyphs);
    }

    len = strlen(grid->text);

    for (read = 0, head = 0; read < len; read++, head++) {
        glyph = guiglyphcreate();
        glyph->c = grid->text[read];
        listpushback(grid->glyphs, glyph);

        /* Special formatting cases */
        if (elem->attr & GUI_ATTR_MULTILINE) {
            switch (glyph->c) {
            case '\n':
                guitextnewline(grid, &head);
                break;
            case '\t':
                guitexttab(grid, &head);
                break;
            }
            
            /* Word wrapping */
            if (elem->attr & GUI_ATTR_WRAPTEXT &&
                head % grid->colcount == grid->colcount - 1) {
                guitextwrapline(grid, read, &head);
            }
        }
    }

    /* Add dummy space and fill rest of line with null */
    glyph = guiglyphcreate();
    glyph->c = ' ';
    listpushback(grid->glyphs, glyph);
    head++;
    
    while (head % grid->colcount > 0) {
        listpushback(grid->glyphs, guiglyphcreate());
        head++;
    }
  
    //TODO: Attempt to qualify more (WRAPTEXT, MULTILINE, etc.) 
    /* Adjust cursor */
    if (elem->attr & GUI_ATTR_TEXTINPUT) {
        guitextadjustcursor(grid);
    }
}

int
guimeshfont(GUITextGrid *grid, GUIElement *elem)
{
    uint read, head, indexcount, totalrows, viewsize, cursoridx;
    GUILayer *layer = elem->layer;
    VBuffer *vbo = &layer->fontvbo;
    FontTex *fonttex = &layer->fonttex;
    GUIGlyph *glyph;
    GUIGlyphBox *box;
    vec3 vert[4] = {{0}};
    vec2 uvs;
    vec3 norm = {0, 0, 1};
    ivec3 index;
    uchar chr;

    indexcount = vbo->indexcount - vbo->indexcount / 3;
    read = grid->startrow * grid->colcount;
    viewsize = grid->colcount * grid->rowcount;

    /* Link visible glyphs to boxes */
    for (read = grid->startrow * grid->colcount, head = 0;
            read < grid->glyphs->size && head < viewsize;
            read++, head++) {

        glyph = listat(grid->glyphs, read);
        box = &grid->view[head];
        chr = glyph->c;
        
        /* Set cursor vertices */
        if (kbdfocus &&
            elem->attr & GUI_ATTR_TEXTINPUT &&
            grid->cursor.pos == read) {

            vert[0][0] = box->verts[0][0] + 1.0f;
            vert[0][1] = box->verts[0][1] - 1.0f;
            vert[0][2] = box->verts[0][2] + 1.0f;

            vert[1][0] = box->verts[1][0] - 1.0f;
            vert[1][1] = box->verts[1][1] - 1.0f;
            vert[1][2] = box->verts[0][2] + 1.0f;

            vert[2][0] = box->verts[2][0] - 1.0f;
            vert[2][1] = box->verts[2][1] + 1.0f;
            vert[2][2] = box->verts[0][2] + 1.0f;

            vert[3][0] = box->verts[3][0] + 1.0f;
            vert[3][1] = box->verts[3][1] + 1.0f;
            vert[3][2] = box->verts[0][2] + 1.0f;
        }

        if (chr != '\0') {
            veclistpush(&layer->fontcolors, glyph->color);
            veclistpush(&layer->fontcolors, glyph->color);
            veclistpush(&layer->fontcolors, glyph->color);
            veclistpush(&layer->fontcolors, glyph->color);

            /* Render glyphs */
            uvs[0] = fonttex->uvs[chr][0];
            uvs[1] = fonttex->uvs[chr][1];
            vbufaddvertex(vbo, box->verts[0], uvs, norm);

            uvs[0] = fonttex->uvs[chr][2];
            uvs[1] = fonttex->uvs[chr][3];
            vbufaddvertex(vbo, box->verts[1], uvs, norm);

            uvs[0] = fonttex->uvs[chr][4];
            uvs[1] = fonttex->uvs[chr][5];
            vbufaddvertex(vbo, box->verts[2], uvs, norm);

            uvs[0] = fonttex->uvs[chr][6];
            uvs[1] = fonttex->uvs[chr][7];
            vbufaddvertex(vbo, box->verts[3], uvs, norm);

            index[0] = indexcount + 0;
            index[1] = indexcount + 1;
            index[2] = indexcount + 2;
            vbufaddindex(vbo, index);

            index[0] = indexcount + 2;
            index[1] = indexcount + 3;
            index[2] = indexcount + 0;
            vbufaddindex(vbo, index);

            indexcount += 4;
        }
    }
    totalrows = grid->glyphs->size / grid->colcount;

    /* TODO: Lock scrolling within range of text bounds */

    /* Render cursor */
    if (kbdfocus &&
            elem->attr & GUI_ATTR_TEXTINPUT &&
            grid->cursor.blink) {
        cursoridx = indexcount;
        glyph = listat(grid->glyphs, grid->startrow * grid->colcount);

        veclistpush(&layer->fontcolors, glyph->color);
        veclistpush(&layer->fontcolors, glyph->color);
        veclistpush(&layer->fontcolors, glyph->color);
        veclistpush(&layer->fontcolors, glyph->color);

        uvs[0] = grid->cursor.uvs[0];
        uvs[1] = grid->cursor.uvs[1];
        vbufaddvertex(vbo, vert[0], uvs, norm);

        uvs[0] = grid->cursor.uvs[2];
        uvs[1] = grid->cursor.uvs[3];
        vbufaddvertex(vbo, vert[1], uvs, norm);

        uvs[0] = grid->cursor.uvs[4];
        uvs[1] = grid->cursor.uvs[5];
        vbufaddvertex(vbo, vert[2], uvs, norm);

        uvs[0] = grid->cursor.uvs[6];
        uvs[1] = grid->cursor.uvs[7];
        vbufaddvertex(vbo, vert[3], uvs, norm);

        index[0] = cursoridx + 0;
        index[1] = cursoridx + 1;
        index[2] = cursoridx + 2;
        vbufaddindex(vbo, index);

        index[0] = cursoridx + 2;
        index[1] = cursoridx + 3;
        index[2] = cursoridx + 0;
        vbufaddindex(vbo, index);
    }

    return totalrows;
}

/* TODO: Refactor to merge loops */
void
guicursormoveup(GUITextGrid *grid)
{
    GUIGlyph *glyph;
    GUITextCursor *cursor = &grid->cursor;
    uint i;

    if (cursor->pos > grid->colcount - 1) {
        i = cursor->pos - 1;
        cursor->pos -= grid->colcount;
        glyph = listat(grid->glyphs, cursor->pos);

        while (glyph->c == '\0') {
            cursor->pos--;
            glyph = listat(grid->glyphs, cursor->pos);
        }
        if (cursor->pos == 0) {
            cursor->textpos = 0;
            return;
        }

        for (; i >= cursor->pos; i--) {
            glyph = listat(grid->glyphs, i);

            if (glyph->c != '\0') {
                cursor->textpos--;
            }
        }
        
        /* Readjust textbox view to follow cursor */
        if (cursor->pos < grid->colcount * grid->startrow) {
            grid->startrow--;
        }
    }
}

/* TODO: Refactor to merge loops */
void
guicursormovedn(GUITextGrid *grid)
{
    GUIGlyph *glyph;
    GUITextCursor *cursor = &grid->cursor;
    uint i;

    if (cursor->pos < grid->glyphs->size - grid->colcount) {
        i = cursor->pos + 1;
        cursor->pos += grid->colcount;
        glyph = listat(grid->glyphs, cursor->pos);
        
        while (glyph->c == '\0') {
            cursor->pos--;
            glyph = listat(grid->glyphs, cursor->pos);
        }

        for (; i <= cursor->pos; i++) {
            glyph = listat(grid->glyphs, i);

            if (glyph->c != '\0') {
                cursor->textpos++;
            }
        }
        
        /* Readjust textbox view to follow cursor */
        if (cursor->pos >= grid->colcount * grid->startrow +
                grid->colcount * grid->rowcount) {
            grid->startrow++;
        }
    }
}

void
guicursormoveright(GUITextGrid *grid)
{
    GUIGlyph *glyph;
    GUITextCursor *cursor = &grid->cursor;
    uint oldpos = cursor->pos;

    if (cursor->pos < grid->glyphs->size - 1) {
        do {
            cursor->pos++;
            glyph = listat(grid->glyphs, cursor->pos);

            if (cursor->pos == grid->glyphs->size - 1 && glyph->c == '\0') {
                cursor->pos = oldpos;
                return;
            }
        } while (glyph->c == '\0');
        cursor->textpos++;
        
        /* Readjust textbox view to follow cursor */
        if (cursor->pos >= grid->colcount * grid->startrow +
                grid->colcount * grid->rowcount) {
            grid->startrow++;
        }
    }
}

void
guicursormoveleft(GUITextGrid *grid)
{
    GUIGlyph *glyph;
    GUITextCursor *cursor = &grid->cursor;

    if (cursor->pos > 0) {
        do {
            cursor->pos--;
            glyph = listat(grid->glyphs, cursor->pos);
        } while (glyph->c == '\0');
        cursor->textpos--;
        /* Readjust textbox view to follow cursor */
        if (cursor->pos < grid->colcount * grid->startrow) {
            grid->startrow--;
        }
    }
}

void
guikbdnewline(GUIElement *elem)
{
    GUITextGrid *grid = elem->textgrid;
    GUIGlyph *data;

    data = malloc(sizeof(GUIGlyph));
    data->c = '\n';
    listinsert(grid->glyphs, data, grid->cursor.pos);
    guitextreadtext(elem);
    guitextwritetext(elem);
    guicursormoveright(grid);
}

void
guikbdbackspace(GUIElement *elem)
{
    GUITextGrid *grid = elem->textgrid;
    GUIGlyph *data;
    uint totalrows;

    if (grid->cursor.pos > 0) {
        guicursormoveleft(grid);
        listfreemove(grid->glyphs, grid->cursor.pos);
        guitextreadtext(elem);
        guitextwritetext(elem);
        data = listat(grid->glyphs, grid->cursor.pos);

        if (data->c == '\0') {
            guicursormoveleft(grid);
        }
        totalrows = grid->glyphs->size / grid->colcount;
       
        if (totalrows < grid->rowcount) {
            grid->startrow = 0;
        } else if (totalrows > grid->rowcount &&
            grid->startrow + grid->rowcount > totalrows) {
            grid->startrow = totalrows - grid->rowcount;
        } /*else {
            grid->startrow = 0;
        }*/
    }
}

void
guikbdtab(GUIElement *elem)
{
    GUITextGrid *grid = elem->textgrid;
    GUIGlyph *data;

    data = malloc(sizeof(GUIGlyph));
    data->c = '\t';
    listinsert(grid->glyphs, data, grid->cursor.pos);
    guitextreadtext(elem);
    guitextwritetext(elem);
    guicursormoveright(grid);
}

void 
guikbddelete(GUIElement *elem)
{
    GUITextGrid *grid = elem->textgrid;
    GUIGlyph *data;
    uint idx;

    idx = grid->cursor.pos;
    data = listat(grid->glyphs, idx);

    if (grid->cursor.pos < grid->glyphs->size) {
        if (data->c == ' ') {
            do {
                idx++;

                if (idx >= grid->glyphs->size) {
                    guikbdbackspace(elem);
                    return;
                }
                data = listat(grid->glyphs, idx);
            } while (data->c == '\0');
        }
        listfreemove(grid->glyphs, grid->cursor.pos);
    } else {
        guikbdbackspace(elem);
    }
}
