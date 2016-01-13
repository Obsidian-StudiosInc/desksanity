#include "e_mod_main.h"
#include "gadget.h"

typedef struct Bryce
{
   Evas_Object *bryce;
   Evas_Object *layout;
   Evas_Object *site;
   Evas_Object *scroller;
   Evas_Object *autohide_event;

   Evas_Object *parent; //comp_object is not an elm widget
   char *style;
   int size;
   int x, y;
   int autohide_size;
   E_Layer layer;

   Ecore_Job *calc_job;
   Ecore_Timer *autohide_timer;
   unsigned int autohide_blocked;

   Eina_Bool autosize : 1;
   Eina_Bool autohide : 1;

   Eina_Bool hidden : 1;
   Eina_Bool animating : 1;
   Eina_Bool mouse_in : 1;
} Bryce;

#define BRYCE_GET(obj) \
   Bryce *b; \
   b = evas_object_data_get((obj), "__bryce"); \
   if (!b) abort()

static void
_bryce_autohide_end(void *data, Efx_Map_Data *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   Bryce *b = data;

   b->animating = 0;
}

static void
_bryce_autohide_coords(Bryce *b, int *x, int *y)
{
   int ox, oy, ow, oh;
   Z_Gadget_Site_Anchor an;

   if (b->parent == e_comp->elm)
     {
        E_Zone *zone;
        
        zone = e_comp_object_util_zone_get(b->bryce);
        ox = zone->w, oy = zone->y, ow = zone->w, oh = zone->h;
     }
   else
     evas_object_geometry_get(b->parent, &ox, &oy, &ow, &oh);
   an = z_gadget_site_anchor_get(b->site);

   if (z_gadget_site_orient_get(b->site) == Z_GADGET_SITE_ORIENT_HORIZONTAL)
     {
        *x = b->x;

        if (an & Z_GADGET_SITE_ANCHOR_TOP)
          *y = oy - b->size + b->autohide_size;
        if (an & Z_GADGET_SITE_ANCHOR_BOTTOM)
          *y = oy + oh + b->size - b->autohide_size;
     }
   else if (z_gadget_site_orient_get(b->site) == Z_GADGET_SITE_ORIENT_VERTICAL)
     {
        *y = b->y;

        if (an & Z_GADGET_SITE_ANCHOR_LEFT)
          *x = ox - b->size + b->autohide_size;
        if (an & Z_GADGET_SITE_ANCHOR_RIGHT)
          *x = ox + ow + b->size - b->autohide_size;
     }
}

static void
_bryce_position(Bryce *b, int w, int h)
{
   int ox, oy, ow, oh;
   int x, y;
   Z_Gadget_Site_Anchor an;

   if (b->parent == e_comp->elm)
     {
        E_Zone *zone;
        
        zone = e_comp_object_util_zone_get(b->bryce);
        ox = zone->x, oy = zone->y, ow = zone->w, oh = zone->h;
        e_comp_object_util_center_pos_get(b->bryce, &x, &y);
     }
   else
     {
        evas_object_geometry_get(b->parent, &ox, &oy, &ow, &oh);
        x = ox + (ow - w) / 2;
        y = oy + (oh - h) / 2;
     }
   an = z_gadget_site_anchor_get(b->site);
   if (an & Z_GADGET_SITE_ANCHOR_LEFT)
     x = ox;
   if (an & Z_GADGET_SITE_ANCHOR_TOP)
     y = oy;
   if (z_gadget_site_orient_get(b->site) == Z_GADGET_SITE_ORIENT_HORIZONTAL)
     {
        if (an & Z_GADGET_SITE_ANCHOR_RIGHT)
          x = ox + ow - w;
        if (an & Z_GADGET_SITE_ANCHOR_BOTTOM)
          y = oy + oh - b->size;
     }
   else if (z_gadget_site_orient_get(b->site) == Z_GADGET_SITE_ORIENT_VERTICAL)
     {
        if (an & Z_GADGET_SITE_ANCHOR_RIGHT)
          x = ox + ow - b->size;
        if (an & Z_GADGET_SITE_ANCHOR_BOTTOM)
          y = oy + oh - h;
     }
   b->x = x, b->y = y;
   if (b->animating)
     {
        if (b->hidden)
          {
             _bryce_autohide_coords(b, &x, &y);
             efx_move(b->bryce, EFX_EFFECT_SPEED_LINEAR, EFX_POINT(x, y), 0.5, _bryce_autohide_end, b);
          }
        else
          efx_move(b->bryce, EFX_EFFECT_SPEED_LINEAR, EFX_POINT(x, y), 0.5, _bryce_autohide_end, b);
        return;
     }
   else if (b->hidden)
     _bryce_autohide_coords(b, &x, &y);

   evas_object_move(b->bryce, x, y);
}

static void
_bryce_autosize(Bryce *b)
{
   int lw, lh, sw, sh, maxw, maxh;

   b->calc_job = NULL;
   if (b->parent == e_comp->elm) //screen-based bryce
     {
        E_Zone *zone;

        zone = e_comp_object_util_zone_get(b->bryce);
        maxw = zone->w, maxh = zone->h;
     }
   else
     evas_object_geometry_get(b->parent, NULL, NULL, &maxw, &maxh);
   evas_object_size_hint_min_get(b->layout, &lw, &lh);
   evas_object_size_hint_min_get(b->site, &sw, &sh);
   if (z_gadget_site_orient_get(b->site) == Z_GADGET_SITE_ORIENT_HORIZONTAL)
     evas_object_resize(b->bryce, MIN(lw + sw, maxw), b->size);
   else if (z_gadget_site_orient_get(b->site) == Z_GADGET_SITE_ORIENT_VERTICAL)
     evas_object_resize(b->bryce, b->size, MIN(lh + sh, maxh));
   if (b->parent == e_comp->elm) //screen-based bryce
     _bryce_position(b, lw + sw, lh + sh);
}

static Eina_Bool
_bryce_autohide_timeout(Bryce *b)
{
   int x, y;

   b->autohide_timer = NULL;
   b->hidden = b->animating = 1;
   _bryce_autohide_coords(b, &x, &y);
   efx_move(b->bryce, EFX_EFFECT_SPEED_LINEAR, EFX_POINT(x, y), 0.5, _bryce_autohide_end, b);
   return EINA_FALSE;
}

static void
_bryce_autohide_moveresize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Bryce *b = data;
   int x, y, w, h;

   evas_object_geometry_get(obj, &x, &y, &w, &h);
   evas_object_geometry_set(b->autohide_event, x, y, w, h);
}

static void
_bryce_autohide_show(Bryce *b)
{
   E_FREE_FUNC(b->autohide_timer, ecore_timer_del);
   if (b->animating && (!b->hidden)) return;
   efx_move(b->bryce, EFX_EFFECT_SPEED_LINEAR, EFX_POINT(b->x, b->y), 0.5, _bryce_autohide_end, b);
   b->animating = 1;
   b->hidden = 0;
}

static void
_bryce_autohide_hide(Bryce *b)
{
   if (!b->autohide_blocked)
     b->autohide_timer = ecore_timer_add(1.0, (Ecore_Task_Cb)_bryce_autohide_timeout, b);
}

static void
_bryce_autohide_mouse_out(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce *b = data;

   _bryce_autohide_hide(b);
   b->mouse_in = 0;
}

static void
_bryce_autohide_mouse_in(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce *b = data;

   b->mouse_in = 1;
   _bryce_autohide_show(b);
}

static void
_bryce_style(Evas_Object *site, Eina_Stringshare *name, Evas_Object *g)
{
   Evas_Object *ly, *prev;
   static int n;
   char buf[1024];

   BRYCE_GET(site);
   
   ly = elm_layout_add(b->site);
   if (name)
     snprintf(buf, sizeof(buf), "z/bryce/%s/%s", b->style ?: "default", name);
   else
     {
        if (n++ % 2)
          snprintf(buf, sizeof(buf), "z/bryce/%s/inset", b->style ?: "default");
        else
          snprintf(buf, sizeof(buf), "z/bryce/%s/plain", b->style ?: "default");
     }
   e_theme_edje_object_set(ly, NULL, buf);
   prev = z_gadget_util_layout_style_init(g, ly);
   elm_object_part_content_set(ly, "e.swallow.content", g);
   evas_object_del(prev);
}

static void
_bryce_site_hints(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce *b = data;
   if (b->autosize && (!b->calc_job))
     b->calc_job = ecore_job_add((Ecore_Cb)_bryce_autosize, b);
}

static void
_bryce_restack(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Bryce *b = data;
   E_Layer layer;

   layer = evas_object_layer_get(obj);
   if (layer == b->layer) return;
   if (layer == E_LAYER_DESKTOP)
     e_comp_object_util_type_set(b->bryce, E_COMP_OBJECT_TYPE_NONE);
   else
     e_comp_object_util_type_set(b->bryce, E_COMP_OBJECT_TYPE_POPUP);
   b->layer = layer;
}

static void
_bryce_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce *b = data;

   free(b->style);
   free(data);
}

static void
_bryce_style_menu(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Bryce *b = data;
   char buf[1024];

   snprintf(buf, sizeof(buf), "z/bryce/%s", b->style ?: "default");
   e_object_data_set(event_info, e_theme_collection_items_find(NULL, buf));
}

static void
_bryce_gadgets_menu_close(void *data, Evas_Object *obj)
{
   Bryce *b = data;

   b->autohide_blocked--;
   evas_object_layer_set(b->bryce, b->layer);
   evas_object_hide(obj);
   evas_object_del(obj);
   if (!b->mouse_in)
     _bryce_autohide_hide(b);
}

static Eina_Bool
_bryce_gadgets_menu_key()
{
   return EINA_TRUE;
}

static void
_bryce_gadgets_menu(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Bryce *b = data;
   Evas_Object *editor, *comp_object;
   int w, h;

   b->autohide_blocked++;
   editor = z_gadget_editor_add(e_comp->elm, b->site);
   comp_object = e_comp_object_util_add(editor, E_COMP_OBJECT_TYPE_POPUP);
   evas_object_resize(comp_object, 300 * e_scale, 300 * e_scale);
   e_comp_object_util_center(comp_object);
   evas_object_layer_set(comp_object, E_LAYER_POPUP);
   evas_object_show(comp_object);
   evas_object_layer_set(b->bryce, E_LAYER_POPUP);
   evas_object_size_hint_min_get(editor, &w, &h);
   evas_object_resize(comp_object, 300 * e_scale, h * e_scale);
   e_comp_object_util_center(comp_object);
   e_comp_object_util_autoclose(comp_object, _bryce_gadgets_menu_close, _bryce_gadgets_menu_key, b);
}

static void
_bryce_owner_menu(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Bryce *b = data;
   E_Menu_Item *mi = event_info;
   E_Menu *subm;

   e_menu_item_label_set(mi, "Bryce");

   subm = e_menu_new();
   e_menu_item_submenu_set(mi, subm);
   e_object_unref(E_OBJECT(subm));

   mi = e_menu_item_new(subm);
   e_menu_item_label_set(mi, "Gadgets");
   e_menu_item_callback_set(mi, _bryce_gadgets_menu, b);
}

static void
_bryce_popup_hide(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce *b = data;

   b->autohide_blocked--;
   if (!b->autohide) return;
   if (!b->mouse_in)
     _bryce_autohide_hide(b);
}

static void
_bryce_popup(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Bryce *b = data;

   evas_object_event_callback_add(event_info, EVAS_CALLBACK_HIDE, _bryce_popup_hide, b);
   b->autohide_blocked++;
   if (b->autohide)
     _bryce_autohide_show(b);
}

Z_API Evas_Object *
z_bryce_add(Evas_Object *parent)
{
   Evas_Object *ly, *bryce, *scr, *site;
   Bryce *b;

   ly = elm_layout_add(parent);
   e_theme_edje_object_set(ly, NULL, "z/bryce/default/base");

   scr = elm_scroller_add(ly);
   elm_object_style_set(scr, "bryce");
   site = z_gadget_site_add(scr, Z_GADGET_SITE_ORIENT_HORIZONTAL);
   z_gadget_site_owner_setup(site, Z_GADGET_SITE_ANCHOR_TOP, _bryce_style);
   elm_object_content_set(scr, site);
   elm_object_part_content_set(ly, "e.swallow.content", scr);
   elm_layout_signal_emit(ly, "e,state,orient,top", "e");
   //evas_object_geometry_set(ly, 0, 0, 48, e_comp->h);
   evas_object_show(ly);
   bryce = e_comp_object_util_add(ly, E_COMP_OBJECT_TYPE_POPUP);
   evas_object_data_set(bryce, "comp_skip", (void*)1);
   evas_object_layer_set(bryce, E_LAYER_POPUP);
   evas_object_lower(bryce);

   b = E_NEW(Bryce, 1);
   b->bryce = bryce;
   b->layout = ly;
   b->site = site;
   b->scroller = scr;
   b->parent = parent;
   b->size = 48;
   {
      const char *str;

      str = elm_layout_data_get(ly, "hidden_state_size");
      if (str && str[0])
        b->autohide_size = strtol(str, NULL, 10);
   }
   evas_object_data_set(bryce, "__bryce", b);
   evas_object_data_set(site, "__bryce", b);
   evas_object_event_callback_add(bryce, EVAS_CALLBACK_DEL, _bryce_del, b);
   evas_object_event_callback_add(bryce, EVAS_CALLBACK_RESTACK, _bryce_restack, b);
   evas_object_event_callback_add(site, EVAS_CALLBACK_CHANGED_SIZE_HINTS, _bryce_site_hints, b);

   evas_object_smart_callback_add(site, "gadget_style_menu", _bryce_style_menu, b);
   evas_object_smart_callback_add(site, "gadget_owner_menu", _bryce_owner_menu, b);
   evas_object_smart_callback_add(site, "gadget_popup", _bryce_popup, b);

   evas_object_clip_set(bryce, e_comp_zone_xy_get(0, 0)->bg_clip_object);
   return bryce;
}

Z_API Evas_Object *
z_bryce_site_get(Evas_Object *bryce)
{
   BRYCE_GET(bryce);

   return b->site;
}

Z_API void
z_bryce_autosize_set(Evas_Object *bryce, Eina_Bool set)
{
   int w, h;

   BRYCE_GET(bryce);
   set = !!set;

   if (b->autosize == set) return;
   b->autosize = set;

   if (set)
     {
        _bryce_autosize(b);
        return;
     }
   evas_object_geometry_get(b->parent, NULL, NULL, &w, &h);
   _bryce_position(b, w, h);
}

Z_API void
z_bryce_autohide_set(Evas_Object *bryce, Eina_Bool set)
{
   BRYCE_GET(bryce);
   set = !!set;

   if (b->autohide == set) return;
   b->autohide = set;

   if (set)
     {
        int x, y, w, h;

        b->autohide_event = evas_object_rectangle_add(evas_object_evas_get(bryce));
        evas_object_geometry_get(bryce, &x, &y, &w, &h);
        evas_object_geometry_set(b->autohide_event, x, y, w, h);
        evas_object_color_set(b->autohide_event, 0, 0, 0, 0);
        evas_object_repeat_events_set(b->autohide_event, 1);
        evas_object_layer_set(b->autohide_event, E_LAYER_POPUP + 1);
        evas_object_show(b->autohide_event);
        evas_object_event_callback_add(b->autohide_event, EVAS_CALLBACK_MOUSE_IN, _bryce_autohide_mouse_in, b);
        evas_object_event_callback_add(b->autohide_event, EVAS_CALLBACK_MOUSE_OUT, _bryce_autohide_mouse_out, b);
        evas_object_event_callback_add(bryce, EVAS_CALLBACK_MOVE, _bryce_autohide_moveresize, b);
        evas_object_event_callback_add(bryce, EVAS_CALLBACK_RESIZE, _bryce_autohide_moveresize, b);
        ecore_evas_pointer_xy_get(e_comp->ee, &x, &y);
        if (!E_INSIDE(x, y, b->x, b->y, w, h))
          _bryce_autohide_hide(b);
     }
   else
     {
        E_FREE_FUNC(b->autohide_event, evas_object_del);
        evas_object_event_callback_del_full(bryce, EVAS_CALLBACK_MOVE, _bryce_autohide_moveresize, b);
        evas_object_event_callback_del_full(bryce, EVAS_CALLBACK_RESIZE, _bryce_autohide_moveresize, b);
        if (!b->hidden) return;
        efx_move(b->bryce, EFX_EFFECT_SPEED_LINEAR, EFX_POINT(b->x, b->y), 0.5, _bryce_autohide_end, b);
        b->animating = 1;
        b->hidden = 0;
     }
}
