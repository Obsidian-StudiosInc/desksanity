#include "e_mod_main.h"
#include "gadget.h"

#define DEFAULT_LAYER E_LAYER_POPUP

typedef struct Bryce
{
   Eina_Stringshare *name;

   Evas_Object *bryce;
   Evas_Object *layout;
   Evas_Object *site;
   Evas_Object *scroller;
   Evas_Object *autohide_event;
   Eina_List *zone_obstacles;

   Evas_Object *parent; //comp_object is not an elm widget
   Eina_Stringshare *style;
   int size;
   int x, y;
   int autohide_size;
   E_Layer layer;
   unsigned int zone;
   Z_Gadget_Site_Orient orient;
   Z_Gadget_Site_Anchor anchor;

   Ecore_Job *calc_job;
   Ecore_Timer *autohide_timer;
   unsigned int autohide_blocked;
   Eina_List *popups;

   /* config: do not bitfield! */
   Eina_Bool autosize;
   Eina_Bool autohide;

   Eina_Bool hidden : 1;
   Eina_Bool animating : 1;
   Eina_Bool mouse_in : 1;
   Eina_Bool noshadow : 1;
} Bryce;

typedef struct Bryces
{
   Eina_List *bryces;
} Bryces;

static E_Config_DD *edd_bryces;
static E_Config_DD *edd_bryce;
static Bryces *bryces;

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
        ox = zone->x, oy = zone->y, ow = zone->w, oh = zone->h;
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
          *y = oy + oh - b->autohide_size;
     }
   else if (z_gadget_site_orient_get(b->site) == Z_GADGET_SITE_ORIENT_VERTICAL)
     {
        *y = b->y;

        if (an & Z_GADGET_SITE_ANCHOR_LEFT)
          *x = ox - b->size + b->autohide_size;
        if (an & Z_GADGET_SITE_ANCHOR_RIGHT)
          *x = ox + ow - b->autohide_size;
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

   E_FREE_FUNC(b->calc_job, ecore_job_del);
   if (!b->autosize)
     {
        int w, h;
        evas_object_geometry_get(b->parent, NULL, NULL, &w, &h);
        if (z_gadget_site_orient_get(b->site) == Z_GADGET_SITE_ORIENT_HORIZONTAL)
          evas_object_resize(b->bryce, w, b->size);
        else if (z_gadget_site_orient_get(b->site) == Z_GADGET_SITE_ORIENT_VERTICAL)
          evas_object_resize(b->bryce, b->size, h);
        _bryce_position(b, lw + sw, lh + sh);
        return;
     }
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
   if (b->autohide_blocked) return;
   if (b->autohide_timer)
     ecore_timer_reset(b->autohide_timer);
   else
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
_bryce_autohide_setup(Bryce *b)
{
   int x, y, w, h;

   if (!b->autohide) return;
   b->autohide_event = evas_object_rectangle_add(evas_object_evas_get(b->bryce));
   evas_object_geometry_get(b->bryce, &x, &y, &w, &h);
   evas_object_geometry_set(b->autohide_event, x, y, w, h);
   evas_object_color_set(b->autohide_event, 0, 0, 0, 0);
   evas_object_repeat_events_set(b->autohide_event, 1);
   evas_object_layer_set(b->autohide_event, E_LAYER_MENU + 1);
   evas_object_show(b->autohide_event);
   evas_object_event_callback_add(b->autohide_event, EVAS_CALLBACK_MOUSE_IN, _bryce_autohide_mouse_in, b);
   evas_object_event_callback_add(b->autohide_event, EVAS_CALLBACK_MOUSE_OUT, _bryce_autohide_mouse_out, b);
   evas_object_event_callback_add(b->bryce, EVAS_CALLBACK_MOVE, _bryce_autohide_moveresize, b);
   evas_object_event_callback_add(b->bryce, EVAS_CALLBACK_RESIZE, _bryce_autohide_moveresize, b);
   ecore_evas_pointer_xy_get(e_comp->ee, &x, &y);
   if (!E_INSIDE(x, y, b->x, b->y, w, h))
     _bryce_autohide_hide(b);
}

static void
_bryce_style(Evas_Object *site, Eina_Stringshare *name, Evas_Object *g)
{
   Evas_Object *ly, *prev;
   char buf[1024];

   BRYCE_GET(site);
   
   ly = elm_layout_add(b->site);
   snprintf(buf, sizeof(buf), "z/bryce/%s/%s", b->style ?: "default", name ?: "plain");
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

static E_Comp_Object_Type
_bryce_shadow_type(const Bryce *b)
{
   if ((b->layer == E_LAYER_DESKTOP) || b->noshadow)
     return E_COMP_OBJECT_TYPE_NONE;
   return E_COMP_OBJECT_TYPE_POPUP;
}

static void
_bryce_restack(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Bryce *b = data;
   E_Layer layer;

   layer = evas_object_layer_get(obj);
   b->layer = layer;
   if ((!b->noshadow) && (layer != b->layer))
     e_comp_object_util_type_set(b->bryce, _bryce_shadow_type(b));
}

static void
_bryce_moveresize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Bryce *b = data;
   int x, y, w, h;
   E_Zone *zone;
   int size;

   if (b->autohide)
     {
        E_FREE_LIST(b->zone_obstacles, e_object_del);
        return;
     }
   evas_object_geometry_get(obj, &x, &y, &w, &h);
   if (z_gadget_site_orient_get(b->site) == Z_GADGET_SITE_ORIENT_HORIZONTAL)
     size = h;
   else
     size = w;

   if (b->size != size)
     e_config_save_queue();
   b->size = size;
   zone = e_comp_object_util_zone_get(obj);
   if (zone)
     {
        if (b->zone_obstacles)
          {
             Eina_List *l;
             E_Zone_Obstacle *obs;

             EINA_LIST_FOREACH(b->zone_obstacles, l, obs)
               e_zone_obstacle_modify(obs, &(Eina_Rectangle){b->x, b->y, w, h});
          }
        else
          b->zone_obstacles = eina_list_append(b->zone_obstacles,
            e_zone_obstacle_add(e_comp_object_util_zone_get(obj), NULL, &(Eina_Rectangle){b->x, b->y, w, h}));
     }
   else
     {
        /* determine "closest" zone:
         * calculate size of rect between bryce and zone
         * smallest rect = closest zone
         */
        Eina_List *l;
        E_Zone *lz;
        size = 0;

        E_FREE_LIST(b->zone_obstacles, e_object_del);
        EINA_LIST_FOREACH(e_comp->zones, l, lz)
          {
             int cw, ch;

             if (x < lz->x)
               cw = lz->x + lz->w - x;
             else
               cw = x + w - lz->x;
             if (y < lz->y)
               ch = lz->y + lz->h - y;
             else
               ch = y + h - lz->y;
             if (size >= cw * ch) continue;
             size = cw * ch;
             zone = lz;
          }
     }
   if (b->zone != zone->num)
     e_config_save_queue();
   b->zone = zone->num;
}

static void
_bryce_popup_hide(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Bryce *b = data;

   b->autohide_blocked--;
   b->popups = eina_list_remove(b->popups, obj);
   if (!b->autohide) return;
   if (!b->mouse_in)
     _bryce_autohide_hide(b);
}

static void
_bryce_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce *b = data;
   Evas_Object *p;

   bryces->bryces = eina_list_remove(bryces->bryces, b);
   E_FREE_LIST(b->zone_obstacles, e_object_del);
   evas_object_del(b->autohide_event);
   E_FREE_FUNC(b->calc_job, ecore_job_del);
   E_FREE_FUNC(b->autohide_timer, ecore_timer_del);
   eina_stringshare_del(b->name);
   EINA_LIST_FREE(b->popups, p)
     evas_object_event_callback_del(p, EVAS_CALLBACK_HIDE, _bryce_popup_hide);
   eina_stringshare_del(b->style);
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
_bryce_remove_menu(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Bryce *b = data;
   bryces->bryces = eina_list_remove(bryces->bryces, data);
   z_gadget_site_del(b->site);
   evas_object_hide(b->bryce);
   evas_object_del(b->bryce);
   e_config_save_queue();
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

   mi = e_menu_item_new(subm);
   e_menu_item_label_set(mi, "Remove");
   e_util_menu_item_theme_icon_set(mi, "list-remove");
   e_menu_item_callback_set(mi, _bryce_remove_menu, b);
}

static void
_bryce_popup(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Bryce *b = data;

   evas_object_event_callback_add(event_info, EVAS_CALLBACK_HIDE, _bryce_popup_hide, b);
   b->autohide_blocked++;
   b->popups = eina_list_append(b->popups, event_info);
   if (b->autohide)
     _bryce_autohide_show(b);
}

static void
_bryce_orient(Bryce *b)
{
   char buf[1024];
   
   evas_object_del(b->site);

   snprintf(buf, sizeof(buf), "__bryce%s", b->name);
   b->site = z_gadget_site_add(b->orient, buf);
   evas_object_data_set(b->site, "__bryce", b);
   elm_object_content_set(b->scroller, b->site);
   z_gadget_site_owner_setup(b->site, b->anchor, _bryce_style);
   if (b->orient == Z_GADGET_SITE_ORIENT_HORIZONTAL)
     elm_layout_signal_emit(b->layout, "e,state,orient,horizontal", "e");
   else
     elm_layout_signal_emit(b->layout, "e,state,orient,vertical", "e");
}

static void
_bryce_style_apply(Bryce *b)
{
   char buf[1024];
   Eina_Bool noshadow;

   snprintf(buf, sizeof(buf), "z/bryce/%s/base", b->style ?: "default");
   e_theme_edje_object_set(b->layout, NULL, buf);
   noshadow = b->noshadow;
   b->noshadow = !!elm_layout_data_get(b->layout, "noshadow");
   if (b->bryce && (noshadow != b->noshadow))
     e_comp_object_util_type_set(b->bryce, _bryce_shadow_type(b));
}

static void
_bryce_create(Bryce *b, Evas_Object *parent)
{
   Evas_Object *ly, *bryce, *scr;

   b->layout = ly = elm_layout_add(parent);
   _bryce_style_apply(b);

   b->scroller = scr = elm_scroller_add(ly);
   elm_object_style_set(scr, "bryce");
   _bryce_orient(b);
   elm_object_part_content_set(ly, "e.swallow.content", scr);
   evas_object_show(ly);
   b->bryce = bryce = e_comp_object_util_add(ly, _bryce_shadow_type(b));
   evas_object_data_set(bryce, "comp_skip", (void*)1);
   evas_object_layer_set(bryce, b->layer);
   evas_object_lower(bryce);

   b->parent = parent;
   {
      const char *str;

      str = elm_layout_data_get(ly, "hidden_state_size");
      if (str && str[0])
        b->autohide_size = strtol(str, NULL, 10);
   }
   evas_object_data_set(bryce, "__bryce", b);
   evas_object_event_callback_add(bryce, EVAS_CALLBACK_DEL, _bryce_del, b);
   evas_object_event_callback_add(bryce, EVAS_CALLBACK_RESTACK, _bryce_restack, b);
   evas_object_event_callback_add(bryce, EVAS_CALLBACK_MOVE, _bryce_moveresize, b);
   evas_object_event_callback_add(bryce, EVAS_CALLBACK_RESIZE, _bryce_moveresize, b);
   evas_object_event_callback_add(b->site, EVAS_CALLBACK_CHANGED_SIZE_HINTS, _bryce_site_hints, b);

   evas_object_smart_callback_add(b->site, "gadget_style_menu", _bryce_style_menu, b);
   evas_object_smart_callback_add(b->site, "gadget_owner_menu", _bryce_owner_menu, b);
   evas_object_smart_callback_add(b->site, "gadget_popup", _bryce_popup, b);

   evas_object_clip_set(bryce, e_comp_zone_number_get(b->zone)->bg_clip_object);
   _bryce_autohide_setup(b);
   _bryce_autosize(b);
}

Z_API Evas_Object *
z_bryce_add(Evas_Object *parent, const char *name, Z_Gadget_Site_Orient orient, Z_Gadget_Site_Anchor an)
{
   Bryce *b;

   b = E_NEW(Bryce, 1);
   b->size = 48;
   b->name = eina_stringshare_add(name);
   b->anchor = an;
   b->orient = orient;
   b->layer = DEFAULT_LAYER;
   _bryce_create(b, parent);
   bryces->bryces = eina_list_append(bryces->bryces, b);
   e_config_save_queue();
   return b->bryce;
}

Z_API void
z_bryce_orient(Evas_Object *bryce, Z_Gadget_Site_Orient orient, Z_Gadget_Site_Anchor an)
{
   BRYCE_GET(bryce);
   b->orient = orient;
   b->anchor = an;
   _bryce_orient(b);
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
   BRYCE_GET(bryce);
   set = !!set;

   if (b->autosize == set) return;
   b->autosize = set;

   e_config_save_queue();
   _bryce_autosize(b);
}

Z_API void
z_bryce_autohide_set(Evas_Object *bryce, Eina_Bool set)
{
   BRYCE_GET(bryce);
   set = !!set;

   if (b->autohide == set) return;
   b->autohide = set;

   if (set)
     _bryce_autohide_setup(b);
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
   e_config_save_queue();
}

Z_API Eina_List *
z_bryce_list(Evas_Object *parent)
{
   Eina_List *l, *ret = NULL;
   Bryce *b;

   if (!parent) parent = e_comp->elm;
   EINA_LIST_FOREACH(bryces->bryces, l, b)
     {
        if (!b->bryce) continue;
        if (parent == b->parent)
          ret = eina_list_append(ret, b->bryce);
     }
   return ret;
}

Z_API Eina_Bool
z_bryce_exists(Evas_Object *parent, Z_Gadget_Site_Orient orient, Z_Gadget_Site_Anchor an)
{
   Eina_List *l;
   Bryce *b;

   if (!parent) parent = e_comp->elm;
   EINA_LIST_FOREACH(bryces->bryces, l, b)
     {
        if (!b->bryce) continue;
        if (parent != b->parent) continue;
        if (b->orient != orient) continue;
        if ((b->anchor & an) == an) return EINA_TRUE;
     }
   return EINA_FALSE;
}

Z_API void
z_bryce_style_set(Evas_Object *bryce, const char *style)
{
   BRYCE_GET(bryce);

   eina_stringshare_replace(&b->style, style);
   _bryce_style_apply(b);
   e_config_save_queue();
   evas_object_smart_callback_call(b->site, "gadget_site_style", NULL);
}

/* FIXME */
static void
bryce_save(void)
{
   e_config_domain_save("bryces", edd_bryces, bryces);
}

Z_API void
z_bryce_init(void)
{
   edd_bryce = E_CONFIG_DD_NEW("Bryce", Bryce);
   E_CONFIG_VAL(edd_bryce, Bryce, name, STR);
   E_CONFIG_VAL(edd_bryce, Bryce, style, STR);
   E_CONFIG_VAL(edd_bryce, Bryce, zone, UINT);
   E_CONFIG_VAL(edd_bryce, Bryce, size, INT);
   E_CONFIG_VAL(edd_bryce, Bryce, layer, UINT);
   E_CONFIG_VAL(edd_bryce, Bryce, autosize, UCHAR);
   E_CONFIG_VAL(edd_bryce, Bryce, autohide, UCHAR);
   E_CONFIG_VAL(edd_bryce, Bryce, orient, UINT);
   E_CONFIG_VAL(edd_bryce, Bryce, anchor, UINT);

   edd_bryces = E_CONFIG_DD_NEW("Bryces", Bryces);
   E_CONFIG_LIST(edd_bryces, Bryces, bryces, edd_bryce);
   bryces = e_config_domain_load("bryces", edd_bryces);
   /* FIXME: set up zone add/del handlers */
   if (bryces)
     {
        Eina_List *l;
        Bryce *b;

        EINA_LIST_FOREACH(bryces->bryces, l, b)
          {
             if (e_comp_zone_number_get(b->zone))
               _bryce_create(b, e_comp->elm);
             evas_object_show(b->bryce);
          }
     }
   else
     bryces = E_NEW(Bryces, 1);
   save_cbs = eina_list_append(save_cbs, bryce_save);
}

Z_API void
z_bryce_shutdown(void)
{
   Bryce *b;

   E_CONFIG_DD_FREE(edd_bryce);
   E_CONFIG_DD_FREE(edd_bryces);
   EINA_LIST_FREE(bryces->bryces, b)
     {
        evas_object_event_callback_del(b->bryce, EVAS_CALLBACK_DEL, _bryce_del);
        eina_list_free(b->popups);
        evas_object_hide(b->bryce);
        evas_object_del(b->bryce);
        eina_stringshare_del(b->name);
        eina_stringshare_del(b->style);
        ecore_job_del(b->calc_job);
        ecore_timer_del(b->autohide_timer);
        free(b);
     }
   E_FREE(bryces);
}
