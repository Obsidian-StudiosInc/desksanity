#include "gadget.h"
#include "ibar.h"

typedef struct _Instance  Instance;
typedef struct _IBar      IBar;
typedef struct _IBar_Icon IBar_Icon;

struct _Instance
{
   E_Comp_Object_Mover *iconify_provider;
   IBar            *ibar;
   E_Drop_Handler  *drop_handler;
   Config_Item     *ci;
   Z_Gadget_Site_Orient  orient;
};

typedef struct
{
  E_Order *eo;
  Eina_Inlist *bars;
} IBar_Order;

struct _IBar
{
   EINA_INLIST;
   Instance    *inst;
   Ecore_Job *resize_job;
   Evas_Object *o_outerbox;
   Evas_Object *o_box, *o_drop;
   Evas_Object *o_drop_over, *o_empty;
   Evas_Object *o_sep;
   unsigned int not_in_order_count;
   IBar_Icon   *ic_drop_before;
   int          drop_before;
   Eina_Hash    *icon_hash;
   Eina_Inlist  *icons;
   IBar_Order  *io;
   Evas_Coord   dnd_x, dnd_y;
   IBar_Icon   *menu_icon;
   Eina_Bool    focused : 1;
};

struct _IBar_Icon
{
   EINA_INLIST;
   IBar            *ibar;
   Evas_Object     *o_holder, *o_icon;
   Evas_Object     *o_holder2, *o_icon2;
   Eina_List       *client_objs;
   Efreet_Desktop  *app;
   Ecore_Timer     *reset_timer;
   Ecore_Timer     *timer;
   Ecore_Timer     *show_timer; //for menu
   Ecore_Timer     *hide_timer; //for menu
   E_Exec_Instance *exe_inst;
   Eina_List       *exes; //all instances
   Eina_List       *menu_pending; //clients with menu items pending
   E_Gadcon_Popup  *menu;
   const char      *hashname;
   int              mouse_down;
   struct
   {
      unsigned char start : 1;
      unsigned char dnd : 1;
      int           x, y;
   } drag;
   Eina_Bool       focused : 1;
   Eina_Bool       not_in_order : 1;
   Eina_Bool       menu_grabbed : 1;
};

static IBar        *_ibar_new(Evas_Object *parent, Instance *inst);
static void         _ibar_free(IBar *b);
static void         _ibar_cb_empty_mouse_down(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibar_empty_handle(IBar *b);
static void         _ibar_instance_watch(void *data, E_Exec_Instance *inst, E_Exec_Watch_Type type);
static void         _ibar_fill(IBar *b);
static void         _ibar_empty(IBar *b);
static void         _ibar_orient_set(IBar *b, int horizontal);
static void         _ibar_resize_handle(IBar *b);
static void         _ibar_instance_drop_zone_recalc(Instance *inst);
static IBar_Icon   *_ibar_icon_at_coord(IBar *b, Evas_Coord x, Evas_Coord y);
static IBar_Icon   *_ibar_icon_new(IBar *b, Efreet_Desktop *desktop, Eina_Bool notinorder);
static IBar_Icon   *_ibar_icon_notinorder_new(IBar *b, E_Exec_Instance *exe);
static void         _ibar_icon_free(IBar_Icon *ic);
static void         _ibar_icon_fill(IBar_Icon *ic);
static void         _ibar_icon_empty(IBar_Icon *ic);
static void         _ibar_sep_create(IBar *b);
static void         _ibar_icon_signal_emit(IBar_Icon *ic, const char *sig, const char *src);
static void         _ibar_cb_app_change(void *data, E_Order *eo);
static void         _ibar_cb_obj_moveresize(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibar_cb_menu_icon_action_exec(void *data, E_Menu *m, E_Menu_Item *mi);
static void         _ibar_cb_menu_icon_new(void *data, E_Menu *m, E_Menu_Item *mi);
static void         _ibar_cb_menu_icon_add(void *data, E_Menu *m, E_Menu_Item *mi);
static void         _ibar_cb_menu_icon_properties(void *data, E_Menu *m, E_Menu_Item *mi);
static void         _ibar_cb_menu_icon_remove(void *data, E_Menu *m, E_Menu_Item *mi);
static void         _ibar_cb_menu_configuration(void *data, E_Menu *m, E_Menu_Item *mi);
static void         _ibar_cb_icon_mouse_in(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibar_cb_icon_mouse_out(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibar_cb_icon_mouse_down(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibar_cb_icon_mouse_up(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibar_cb_icon_mouse_move(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibar_cb_icon_move(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibar_cb_icon_resize(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibar_cb_icon_wheel(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void         _ibar_inst_cb_enter(void *data, const char *type, void *event_info);
static void         _ibar_inst_cb_move(void *data, const char *type, void *event_info);
static void         _ibar_inst_cb_leave(void *data, const char *type, void *event_info);
static void         _ibar_inst_cb_drop(void *data, const char *type, void *event_info);
static void         _ibar_cb_drag_finished(E_Drag *data, int dropped);
static void         _ibar_drop_position_update(Instance *inst, Evas_Coord x, Evas_Coord y);
static void         _ibar_inst_cb_scroll(void *data);
static void         _ibar_exec_new_client_show(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED);
static Eina_Bool    _ibar_cb_out_hide_delay(void *data);
static void         _ibar_icon_menu_show(IBar_Icon *ic, Eina_Bool grab);
static void         _ibar_icon_menu_hide(IBar_Icon *ic, Eina_Bool grab);

static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;

static Eina_Hash *ibar_orders = NULL;
static Eina_List *ibars = NULL;

Config *ibar_config = NULL;

static inline const char *
_desktop_name_get(const Efreet_Desktop *desktop)
{
   if (!desktop) return NULL;
   return desktop->orig_path; //allways return the orig_path
}

static Config_Item *
_ibar_config_item_get(int *id)
{
   Config_Item *ci;

   if (*id)
     {
        Eina_List *l;

        EINA_LIST_FOREACH(ibar_config->items, l, ci)
          if (*id == ci->id) return ci;
     }

   ci = E_NEW(Config_Item, 1);
   ci->id = ibar_config->items ? eina_list_count(ibar_config->items) : 1;
   ci->dir = eina_stringshare_add("default");
   ci->show_label = 1;
   ci->eap_label = 0;
   ci->lock_move = 0;
   ci->dont_add_nonorder = 0;
   ci->dont_track_launch = 0;
   ci->dont_icon_menu_mouseover = 0;
   ibar_config->items = eina_list_append(ibar_config->items, ci);
   return ci;
}

static IBar_Order *
_ibar_order_new(IBar *b, const char *path)
{
   IBar_Order *io;

   io = eina_hash_find(ibar_orders, path);
   if (io)
     {
        io->bars = eina_inlist_append(io->bars, EINA_INLIST_GET(b));
        return io;
     }
   io = E_NEW(IBar_Order, 1);
   io->eo = e_order_new(path);
   e_order_update_callback_set(io->eo, _ibar_cb_app_change, io);
   eina_hash_add(ibar_orders, path, io);
   io->bars = eina_inlist_append(io->bars, EINA_INLIST_GET(b));
   return io;
}

static void
_ibar_order_del(IBar *b)
{
   IBar_Order *io;
   if (!b->io) return;
   io = b->io;
   io->bars = eina_inlist_remove(io->bars, EINA_INLIST_GET(b));
   b->io = NULL;
   if (io->bars) return;
   eina_hash_del_by_key(ibar_orders, io->eo->path);
   e_order_update_callback_set(io->eo, NULL, NULL);
   e_object_del(E_OBJECT(io->eo));
   free(io);
}


static void
_ibar_order_refresh(IBar *b, const char *path)
{
   IBar_Order *io;
   IBar *bar;

   io = eina_hash_find(ibar_orders, path);
   if (io)
     {
        /* different order, remove/refresh */
        if (io != b->io)
          {
             if (b->io) _ibar_order_del(b);
             io->bars = eina_inlist_append(io->bars, EINA_INLIST_GET(b));
             b->io = io;
          }
        /* else same order, refresh all users */
     }
   else
     {
        _ibar_order_del(b);
        io = b->io = _ibar_order_new(b, path);
     }
   EINA_INLIST_FOREACH(io->bars, bar)
     {
        _ibar_empty(bar);
        _ibar_fill(bar);
     }
}

static Eina_Bool
_ibar_cb_config_icons(EINA_UNUSED void *data, EINA_UNUSED int ev_type, EINA_UNUSED void *ev)
{
   const Eina_List *l;
   Instance *inst;

   EINA_LIST_FOREACH(ibar_config->instances, l, inst)
     {
        IBar_Icon *icon;

        EINA_INLIST_FOREACH(inst->ibar->icons, icon)
          _ibar_icon_fill(icon);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static void
_ibar_cb_iconify_end_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   E_Client *ec = data;

   evas_object_layer_set(ec->frame, ec->layer);
   ec->layer_block = 0;
   if (ec->iconic)
     evas_object_hide(ec->frame);
}

static Eina_Bool
_ibar_cb_iconify_provider(void *data, Evas_Object *obj, const char *signal EINA_UNUSED)
{
   Instance *inst = data;
   IBar_Icon *ic;
   int ox, oy, ow, oh;
   E_Client *ec;

   ec = e_comp_object_client_get(obj);
   if (ec->zone != e_comp_object_util_zone_get(inst->ibar->o_outerbox)) return EINA_FALSE;
   ic = eina_hash_find(inst->ibar->icon_hash, _desktop_name_get(ec->exe_inst ? ec->exe_inst->desktop : ec->desktop));
   if (!ic) return EINA_FALSE;
   ec->layer_block = 1;
   evas_object_layer_set(ec->frame, E_LAYER_CLIENT_PRIO);
   evas_object_geometry_get(ic->o_holder, &ox, &oy, &ow, &oh);
   e_comp_object_effect_set(ec->frame, "iconify/ibar");
   e_comp_object_effect_params_set(ec->frame, 1, (int[]){ec->x, ec->y, ec->w, ec->h, ox, oy, ow, oh}, 8);
   e_comp_object_effect_params_set(ec->frame, 0, (int[]){!!strcmp(signal, "e,action,iconify")}, 1);
   e_comp_object_effect_start(ec->frame, _ibar_cb_iconify_end_cb, ec);
   return EINA_TRUE;
}

static Eina_Bool
_is_vertical(Instance *inst)
{
   return inst->orient == Z_GADGET_SITE_ORIENT_VERTICAL;
}

static IBar *
_ibar_new(Evas_Object *parent, Instance *inst)
{
   IBar *b;
   char buf[PATH_MAX];

   b = E_NEW(IBar, 1);
   inst->ibar = b;
   b->inst = inst;
   b->icon_hash = eina_hash_string_superfast_new(NULL);
   b->o_outerbox = elm_box_add(parent);
   E_EXPAND(b->o_outerbox);
   elm_box_horizontal_set(b->o_outerbox, 1);
   elm_box_align_set(b->o_outerbox, 0.5, 0.5);
   b->o_box = elm_box_add(b->o_outerbox);
   E_FILL(b->o_box);
   elm_box_homogeneous_set(b->o_box, 1);
   elm_box_horizontal_set(b->o_box, 1);
   elm_box_align_set(b->o_box, 0.5, 0.5);
   elm_box_pack_end(b->o_outerbox, b->o_box);
   if (inst->ci->dir[0] != '/')
     e_user_dir_snprintf(buf, sizeof(buf), "applications/bar/%s/.order",
                         inst->ci->dir);
   else
     eina_strlcpy(buf, inst->ci->dir, sizeof(buf));
   b->io = _ibar_order_new(b, buf);
   _ibar_fill(b);
   evas_object_show(b->o_box);
   evas_object_show(b->o_outerbox);
   ibars = eina_list_append(ibars, b);
   return b;
}

static void
_ibar_free(IBar *b)
{
   _ibar_empty(b);
   evas_object_del(b->o_outerbox);
   evas_object_del(b->o_box);
   if (b->o_drop) evas_object_del(b->o_drop);
   if (b->o_drop_over) evas_object_del(b->o_drop_over);
   if (b->o_empty) evas_object_del(b->o_empty);
   E_FREE_FUNC(b->resize_job, ecore_job_del);
   eina_hash_free(b->icon_hash);
   _ibar_order_del(b);
   ibars = eina_list_remove(ibars, b);
   free(b);
}

static void
_ibar_cb_empty_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Down *ev;
   IBar *b;
   E_Menu *m;
   E_Menu_Item *mi;
   int cx, cy, cw, ch;

   ev = event_info;
   b = data;
#warning FIXME
#if 0
   if (ev->button != 3) return;

   m = e_menu_new();

   if (e_configure_registry_exists("applications/new_application"))
     {
        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, _("Create new Icon"));
        e_util_menu_item_theme_icon_set(mi, "document-new");
        e_menu_item_callback_set(mi, _ibar_cb_menu_icon_new, NULL);

        mi = e_menu_item_new(m);
        e_menu_item_separator_set(mi, 1);
     }

   if (e_configure_registry_exists("applications/ibar_applications"))
     {
        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, _("Contents"));
        e_util_menu_item_theme_icon_set(mi, "list-add");
        e_menu_item_callback_set(mi, _ibar_cb_menu_icon_add, b);
     }

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Settings"));
   e_util_menu_item_theme_icon_set(mi, "configure");
   e_menu_item_callback_set(mi, _ibar_cb_menu_configuration, b);

   m = e_gadcon_client_util_menu_items_append(b->inst->gcc, m, 0);

   e_gadcon_canvas_zone_geometry_get(b->inst->gcc->gadcon,
                                     &cx, &cy, &cw, &ch);
   e_menu_activate_mouse(m,
                         e_zone_current_get(),
                         cx + ev->output.x, cy + ev->output.y, 1, 1,
                         E_MENU_POP_DIRECTION_DOWN, ev->timestamp);
   evas_event_feed_mouse_up(b->inst->gcc->gadcon->evas, ev->button,
                            EVAS_BUTTON_NONE, ev->timestamp, NULL);
#endif
}

static void
_ibar_empty_handle(IBar *b)
{
   if (!b->icons)
     {
        if (!b->o_empty)
          {
             Evas_Coord w, h;

             b->o_empty = evas_object_rectangle_add(evas_object_evas_get(b->o_box));
             E_EXPAND(b->o_empty);
             E_FILL(b->o_empty);
             evas_object_event_callback_add(b->o_empty,
                                            EVAS_CALLBACK_MOUSE_DOWN,
                                            _ibar_cb_empty_mouse_down, b);
             evas_object_color_set(b->o_empty, 0, 0, 0, 0);
             evas_object_show(b->o_empty);
             elm_box_pack_end(b->o_box, b->o_empty);
             evas_object_geometry_get(b->o_box, NULL, NULL, &w, &h);
             if (elm_box_horizontal_get(b->o_box))
               w = h;
             else
               h = w;
             evas_object_size_hint_min_set(b->o_empty, w, h);
          }
     }
   else if (b->o_empty)
     {
        evas_object_del(b->o_empty);
        b->o_empty = NULL;
     }
}

static void
_ibar_fill(IBar *b)
{
   IBar_Icon *ic;
   int w, h;

   if (b->io->eo)
     {
        Efreet_Desktop *desktop;
        const Eina_List *l;

        EINA_LIST_FOREACH(b->io->eo->desktops, l, desktop)
          {
             const Eina_List *ll;
             ic = _ibar_icon_new(b, desktop, 0);
             ll = e_exec_desktop_instances_find(desktop);
             if (ll)
               {
                  ic->exes = eina_list_clone(ll);
                  _ibar_icon_signal_emit(ic, "e,state,on", "e");
               }
          }
     }
   if (!b->inst->ci->dont_add_nonorder)
     {
        const Eina_Hash *execs = e_exec_instances_get();
        Eina_Iterator *it;
        const Eina_List *l, *ll;
        E_Exec_Instance *exe;

        it = eina_hash_iterator_data_new(execs);
        EINA_ITERATOR_FOREACH(it, l)
          {
             EINA_LIST_FOREACH(l, ll, exe)
               {
                  E_Client *ec;
                  Eina_List *lll;
                  Eina_Bool skip = EINA_TRUE;

                  if (!exe->desktop) continue;
                  EINA_LIST_FOREACH(exe->clients, lll, ec)
                    if (!ec->netwm.state.skip_taskbar)
                      {
                         skip = EINA_FALSE;
                         break;
                      }
                  if (skip) continue;
                  ic = eina_hash_find(b->icon_hash, _desktop_name_get(exe->desktop));
                  if (ic)
                    {
                       if (!eina_list_data_find(ic->exes, exe))
                         ic->exes = eina_list_append(ic->exes, exe);
                       continue;
                    }
                  _ibar_sep_create(b);
                  _ibar_icon_notinorder_new(b, exe);
               }
          }
        eina_iterator_free(it);
     }
   
   _ibar_empty_handle(b);
   _ibar_resize_handle(b);
}

static void
_ibar_empty(IBar *b)
{
   while (b->icons)
     _ibar_icon_free((IBar_Icon*)b->icons);

   E_FREE_FUNC(b->o_sep, evas_object_del);
   _ibar_empty_handle(b);
}

static void
_ibar_orient_set(IBar *b, int horizontal)
{
   Evas_Coord w, h;

   if (b->o_sep)
     {
        if (!horizontal)
          e_theme_edje_object_set(b->o_sep, "base/theme/modules/ibar", "e/modules/ibar/separator/horizontal");
        else
          e_theme_edje_object_set(b->o_sep, "base/theme/modules/ibar", "e/modules/ibar/separator/default");
     }
   elm_box_horizontal_set(b->o_box, horizontal);
   elm_box_horizontal_set(b->o_outerbox, horizontal);
}

static void
_ibar_resize_handle(IBar *b)
{
   IBar_Icon *ic;
   Evas_Coord w, h;

   evas_object_size_hint_min_get(b->o_outerbox, &w, &h);
   if (w && h)
     evas_object_size_hint_aspect_set(b->o_outerbox, EVAS_ASPECT_CONTROL_BOTH, w, h);
   return;
   evas_object_geometry_get(z_gadget_site_get(b->o_outerbox), NULL, NULL, &w, &h);
   EINA_INLIST_FOREACH(b->icons, ic)
     {
        evas_object_size_hint_min_set(ic->o_holder, MIN(w, h), MIN(w, h));
        evas_object_size_hint_max_set(ic->o_holder, MIN(w, h), MIN(w, h));
     }
}

static void
_ibar_instance_drop_zone_recalc(Instance *inst)
{
   //Evas_Coord x, y, w, h;

   //e_gadcon_client_viewport_geometry_get(inst->gcc, &x, &y, &w, &h);
   //e_drop_handler_geometry_set(inst->drop_handler, x, y, w, h);
}

void
_ibar_config_update(Config_Item *ci)
{
   const Eina_List *l;
   Instance *inst;
   IBar_Icon *ic;

   EINA_LIST_FOREACH(ibar_config->instances, l, inst)
     {
        char buf[PATH_MAX];

        if (inst->ci != ci) continue;

        if (inst->ci->dir[0] != '/')
          e_user_dir_snprintf(buf, sizeof(buf), "applications/bar/%s/.order",
                              inst->ci->dir);
        else
          eina_strlcpy(buf, inst->ci->dir, sizeof(buf));
        _ibar_order_refresh(inst->ibar, buf);
        _ibar_resize_handle(inst->ibar);
     }
   EINA_LIST_FOREACH(ibar_config->instances, l, inst)
     EINA_INLIST_FOREACH(inst->ibar->icons, ic)
          {
             switch (ci->eap_label)
               {
                case 0:
                  elm_object_part_text_set(ic->o_holder2, "e.text.label",
                                            ic->app->name);
                  break;

                case 1:
                  elm_object_part_text_set(ic->o_holder2, "e.text.label",
                                            ic->app->comment);
                  break;

                case 2:
                  elm_object_part_text_set(ic->o_holder2, "e.text.label",
                                            ic->app->generic_name);
                  break;
               }
          }
}

static void
_ibar_sep_create(IBar *b)
{
   Evas_Coord w, h;
   if (b->o_sep) return;

   b->o_sep = elm_layout_add(b->o_box);
   E_FILL(b->o_sep);
   if (_is_vertical(b->inst))
     {
        e_theme_edje_object_set(b->o_sep, "base/theme/modules/ibar", "e/modules/ibar/separator/horizontal");
        E_WEIGHT(b->o_sep, EVAS_HINT_EXPAND, 0);
     }
   else
     {
        e_theme_edje_object_set(b->o_sep, "base/theme/modules/ibar", "e/modules/ibar/separator/default");
        E_WEIGHT(b->o_sep, 0, EVAS_HINT_EXPAND);
     }
   evas_object_show(b->o_sep);
   elm_box_pack_end(b->o_outerbox, b->o_sep);
}

static IBar_Icon *
_ibar_icon_at_coord(IBar *b, Evas_Coord x, Evas_Coord y)
{
   IBar_Icon *ic;

   EINA_INLIST_FOREACH(b->icons, ic)
     {
        Evas_Coord dx, dy, dw, dh;

        /* block drops in the non-order section */
        if (ic->not_in_order) continue;
        evas_object_geometry_get(ic->o_holder, &dx, &dy, &dw, &dh);
        if (E_INSIDE(x, y, dx, dy, dw, dh))
          return ic;
     }
   return NULL;
}

static IBar_Icon *
_ibar_icon_new(IBar *b, Efreet_Desktop *desktop, Eina_Bool notinorder)
{
   IBar_Icon *ic;

   ic = E_NEW(IBar_Icon, 1);
   ic->ibar = b;
   ic->app = desktop;
   efreet_desktop_ref(desktop);
   ic->o_holder = elm_layout_add(b->o_box);
   evas_object_size_hint_aspect_set(ic->o_holder, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   evas_object_name_set(ic->o_holder, "o_holder");
   E_EXPAND(ic->o_holder);
   E_FILL(ic->o_holder);
   e_theme_edje_object_set(ic->o_holder, "base/theme/modules/ibar",
                           "e/modules/ibar/icon");
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_MOUSE_IN,
                                  _ibar_cb_icon_mouse_in, ic);
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_MOUSE_OUT,
                                  _ibar_cb_icon_mouse_out, ic);
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_MOUSE_DOWN,
                                  _ibar_cb_icon_mouse_down, ic);
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_MOUSE_UP,
                                  _ibar_cb_icon_mouse_up, ic);
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_MOUSE_MOVE,
                                  _ibar_cb_icon_mouse_move, ic);
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _ibar_cb_icon_wheel, ic);
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_MOVE,
                                  _ibar_cb_icon_move, ic);
   evas_object_event_callback_add(ic->o_holder, EVAS_CALLBACK_RESIZE,
                                  _ibar_cb_icon_resize, ic);
   evas_object_show(ic->o_holder);

   ic->o_holder2 = elm_layout_add(b->o_box);
   evas_object_name_set(ic->o_holder2, "ibar_icon->o_holder2");
   e_theme_edje_object_set(ic->o_holder2, "base/theme/modules/ibar",
                           "e/modules/ibar/icon_overlay");
   evas_object_layer_set(ic->o_holder2, 9999);
   evas_object_pass_events_set(ic->o_holder2, 1);
   evas_object_show(ic->o_holder2);

   _ibar_icon_fill(ic);
   b->icons = eina_inlist_append(b->icons, EINA_INLIST_GET(ic));
   if (eina_hash_find(b->icon_hash, _desktop_name_get(ic->app)))
     {
        char buf[PATH_MAX];

        ERR("Ibar - Unexpected: icon with same desktop path created twice");
        snprintf(buf, sizeof(buf), "%s::%1.20f",
                 _desktop_name_get(ic->app), ecore_time_get());
        ic->hashname = eina_stringshare_add(buf);
     }
   else ic->hashname = eina_stringshare_add(_desktop_name_get(ic->app));
   eina_hash_add(b->icon_hash, ic->hashname, ic);
   if (notinorder)
     {
        ic->not_in_order = 1;
        b->not_in_order_count++;
        elm_box_pack_end(b->o_outerbox, ic->o_holder);
     }
   else
     elm_box_pack_end(b->o_box, ic->o_holder);
   return ic;
}

static IBar_Icon *
_ibar_icon_notinorder_new(IBar *b, E_Exec_Instance *exe)
{
   IBar_Icon *ic;

   ic = _ibar_icon_new(b, exe->desktop, 1);
   ic->exes = eina_list_append(ic->exes, exe);
   _ibar_icon_signal_emit(ic, "e,state,on", "e");
   return ic;
}

static void
_ibar_cb_icon_menu_client_menu_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   IBar *b = data;

   evas_object_event_callback_del(obj, EVAS_CALLBACK_HIDE, _ibar_cb_icon_menu_client_menu_del);
   if (!b->menu_icon) return;
   if (b->menu_icon->hide_timer)
     ecore_timer_reset(b->menu_icon->hide_timer);
   else
     b->menu_icon->hide_timer = ecore_timer_add(0.5, _ibar_cb_out_hide_delay, b->menu_icon);
}

static void
_ibar_icon_free(IBar_Icon *ic)
{
   E_Exec_Instance *inst;
   Evas_Object *o;

   EINA_LIST_FREE(ic->client_objs, o) evas_object_del(o);
   if (ic->ibar->menu_icon == ic) ic->ibar->menu_icon = NULL;
   if (ic->ibar->ic_drop_before == ic) ic->ibar->ic_drop_before = NULL;
   if (ic->menu) e_object_data_set(E_OBJECT(ic->menu), NULL);
   E_FREE_FUNC(ic->menu, e_object_del);
   E_FREE_FUNC(ic->timer, ecore_timer_del);
   E_FREE_FUNC(ic->hide_timer, ecore_timer_del);
   E_FREE_FUNC(ic->show_timer, ecore_timer_del);
   ic->ibar->icons = eina_inlist_remove(ic->ibar->icons, EINA_INLIST_GET(ic));
   eina_hash_del_by_key(ic->ibar->icon_hash, ic->hashname);
   E_FREE_FUNC(ic->hashname, eina_stringshare_del);
   E_FREE_FUNC(ic->reset_timer, ecore_timer_del);
   if (ic->app) efreet_desktop_unref(ic->app);
   evas_object_event_callback_del_full(ic->o_holder, EVAS_CALLBACK_MOUSE_IN,
                                  _ibar_cb_icon_mouse_in, ic);
   evas_object_event_callback_del_full(ic->o_holder, EVAS_CALLBACK_MOUSE_OUT,
                                  _ibar_cb_icon_mouse_out, ic);
   evas_object_event_callback_del_full(ic->o_holder, EVAS_CALLBACK_MOUSE_DOWN,
                                  _ibar_cb_icon_mouse_down, ic);
   evas_object_event_callback_del_full(ic->o_holder, EVAS_CALLBACK_MOUSE_UP,
                                  _ibar_cb_icon_mouse_up, ic);
   evas_object_event_callback_del_full(ic->o_holder, EVAS_CALLBACK_MOUSE_MOVE,
                                  _ibar_cb_icon_mouse_move, ic);
   evas_object_event_callback_del_full(ic->o_holder, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _ibar_cb_icon_wheel, ic);
   evas_object_event_callback_del_full(ic->o_holder, EVAS_CALLBACK_MOVE,
                                  _ibar_cb_icon_move, ic);
   evas_object_event_callback_del_full(ic->o_holder, EVAS_CALLBACK_RESIZE,
                                  _ibar_cb_icon_resize, ic);
   ic->ibar->not_in_order_count -= ic->not_in_order;
   if (ic->ibar->ic_drop_before == ic)
     ic->ibar->ic_drop_before = NULL;
   _ibar_icon_empty(ic);
   EINA_LIST_FREE(ic->exes, inst)
     {
        E_Client *ec;
        Eina_List *ll;

        if (!ic->not_in_order)
          e_exec_instance_watcher_del(inst, _ibar_instance_watch, ic);
        EINA_LIST_FOREACH(inst->clients, ll, ec)
          if (ec->border_menu)
            evas_object_event_callback_del(ec->border_menu->comp_object, EVAS_CALLBACK_HIDE, _ibar_cb_icon_menu_client_menu_del);
     }
   evas_object_del(ic->o_holder);
   evas_object_del(ic->o_holder2);
   if (ic->exe_inst)
     {
        e_exec_instance_watcher_del(ic->exe_inst, _ibar_instance_watch, ic);
        ic->exe_inst = NULL;
     }
   free(ic);
}

static void
_ibar_icon_fill(IBar_Icon *ic)
{
   if (ic->o_icon) evas_object_del(ic->o_icon);
   ic->o_icon = elm_icon_add(ic->ibar->o_box);
   evas_object_name_set(ic->o_icon, "icon");
   elm_icon_standard_set(ic->o_icon, ic->app->icon);
   elm_object_part_content_set(ic->o_holder, "e.swallow.content", ic->o_icon);
   evas_object_show(ic->o_icon);
   if (ic->o_icon2) evas_object_del(ic->o_icon2);
   ic->o_icon2 = elm_icon_add(ic->ibar->o_box);
   elm_icon_standard_set(ic->o_icon2, ic->app->icon);
   elm_object_part_content_set(ic->o_holder2, "e.swallow.content", ic->o_icon2);
   evas_object_show(ic->o_icon2);

   switch (ic->ibar->inst->ci->eap_label)
     {
      case 0: /* Eap Name */
        elm_object_part_text_set(ic->o_holder2, "e.text.label", ic->app->name);
        break;

      case 1: /* Eap Comment */
        elm_object_part_text_set(ic->o_holder2, "e.text.label", ic->app->comment);
        break;

      case 2: /* Eap Generic */
        elm_object_part_text_set(ic->o_holder2, "e.text.label", ic->app->generic_name);
        break;
     }
}

static void
_ibar_icon_empty(IBar_Icon *ic)
{
   if (ic->o_icon) evas_object_del(ic->o_icon);
   if (ic->o_icon2) evas_object_del(ic->o_icon2);
   ic->o_icon = NULL;
   ic->o_icon2 = NULL;
}

static void
_ibar_icon_signal_emit(IBar_Icon *ic, const char *sig, const char *src)
{
   if (ic->o_holder)
     elm_object_signal_emit(ic->o_holder, sig, src);
   if (ic->o_icon)
     elm_object_signal_emit(ic->o_icon, sig, src);
   if (ic->o_holder2)
     elm_object_signal_emit(ic->o_holder2, sig, src);
   if (ic->o_icon2)
     elm_object_signal_emit(ic->o_icon2, sig, src);
}

static void
_ibar_cb_app_change(void *data, E_Order *eo EINA_UNUSED)
{
   IBar *b;
   IBar_Order *io = data;

   EINA_INLIST_FOREACH(io->bars, b)
     {
        _ibar_empty(b);
        if (b->inst)
          {
             _ibar_fill(b);
          }
     }
}

static void
_ibar_cb_resize_job(void *data)
{
   Instance *inst = data;
   _ibar_resize_handle(inst->ibar);
   _ibar_instance_drop_zone_recalc(inst);
   inst->ibar->resize_job = NULL;
}

static void
_ibar_cb_obj_hints(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   _ibar_resize_handle(inst->ibar);
}

static void
_ibar_cb_obj_moveresize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->ibar->resize_job) return;
   inst->ibar->resize_job = ecore_job_add((Ecore_Cb)_ibar_cb_resize_job, inst);
}

static void
_ibar_cb_menu_icon_action_exec(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Efreet_Desktop_Action *action = (Efreet_Desktop_Action*)data;
   e_exec(NULL, NULL, action->exec, NULL, "ibar");
}

static void
_ibar_cb_menu_icon_new(void *data EINA_UNUSED, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   if (!e_configure_registry_exists("applications/new_application")) return;
   e_configure_registry_call("applications/new_application", NULL, NULL);
}

static void
_ibar_cb_menu_icon_add(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   char path[PATH_MAX];
   IBar *b;

   b = data;
   e_user_dir_snprintf(path, sizeof(path), "applications/bar/%s/.order",
                       b->inst->ci->dir);
   e_configure_registry_call("internal/ibar_other", NULL, path);
}

static void
_ibar_cb_menu_icon_properties(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   IBar_Icon *ic;

   ic = data;
   e_desktop_edit(ic->app);
}

static void
_ibar_cb_menu_icon_stick(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   IBar_Icon *ic = data;

   e_order_append(ic->ibar->io->eo, ic->app);
   _ibar_icon_free(ic);
}

static void
_ibar_cb_menu_icon_remove(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   IBar_Icon *ic = data;
   IBar *i;

   i = ic->ibar;
   e_order_remove(i->io->eo, ic->app);
   _ibar_icon_free(ic);
   _ibar_resize_handle(i);
}

static void
_ibar_cb_menu_configuration(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   IBar *b;

   b = data;
   //_config_ibar_module(b->inst->ci);
}

static void
_ibar_cb_icon_menu_hide_begin(IBar_Icon *ic)
{
   if (!ic->menu) return;
   evas_object_pass_events_set(ic->menu->comp_object, 1);
   elm_object_signal_emit(ic->menu->o_bg, "e,action,hide", "e");
}

static void
_ibar_cb_icon_menu_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   IBar_Icon *ic;
   E_Client *ec = data;
   Evas_Event_Mouse_Up *ev = event_info;

   ic = evas_object_data_get(obj, "ibar_icon");
   if (ev->button == 3)
     {
        e_int_client_menu_show(ec, ev->canvas.x, ev->canvas.y, 0, ev->timestamp);
        evas_object_event_callback_add(ec->border_menu->comp_object, EVAS_CALLBACK_HIDE, _ibar_cb_icon_menu_client_menu_del, ic->ibar);
        return;
     }
   e_client_activate(ec, 1);
   if (ic)
     _ibar_cb_icon_menu_hide_begin(ic);
}

static void
_ibar_cb_icon_menu_del(void *obj)
{
   IBar_Icon *ic = e_object_data_get(obj);

   if (!ic) return;
   ic->menu = NULL;
}

static void
_ibar_cb_icon_menu_autodel(void *data, Evas_Object *obj EINA_UNUSED)
{
   _ibar_cb_icon_menu_hide_begin(data);
}

static void
_ibar_cb_icon_menu_shown(void *data, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   IBar_Icon *ic = data;

   evas_object_pass_events_set(ic->menu->o_bg, 0);
}

static void
_ibar_cb_icon_menu_hidden(void *data, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   IBar_Icon *ic = data;
   E_Client *ec;

   E_OBJECT_DEL_SET(ic->menu, NULL);
   E_FREE_FUNC(ic->menu, e_object_del);
   E_FREE_FUNC(ic->hide_timer, ecore_timer_del);
   EINA_LIST_FREE(ic->menu_pending, ec)
     evas_object_event_callback_del_full(ec->frame, EVAS_CALLBACK_SHOW, _ibar_exec_new_client_show, ic);
}

static void
_ibar_icon_menu_recalc(IBar_Icon *ic)
{
   int x, y, w, h, iw, ih, ox, oy;
   Evas_Object *o;
   E_Zone *zone;

   o = ic->menu->o_bg;

   edje_object_calc_force(o);
   edje_object_size_min_calc(o, &w, &h);
   zone = e_comp_object_util_zone_get(ic->ibar->o_outerbox);
   evas_object_geometry_get(ic->o_holder, &x, &y, &iw, &ih);
   evas_object_size_hint_min_set(o, w, h);
   ic->menu->w = w, ic->menu->h = h;
   evas_object_resize(ic->menu->comp_object, w, h);
   e_gadcon_popup_show(ic->menu);
   evas_object_geometry_get(ic->menu->comp_object, &ox, &oy, NULL, NULL);
   if (elm_box_horizontal_get(ic->ibar->o_box))
     {
        ox = (x + (iw / 2)) - (w / 2);
        if (E_INTERSECTS(ox, oy, w, h, x, y, iw, ih))
          {
             if (y > h / 2)
               oy = y - h;
             else
               oy = y + ih;
          }
     }
   else
     oy = (y + (ih / 2)) - (h / 2);
   ox = E_CLAMP(ox, zone->x, zone->x + zone->w - w);
   evas_object_move(ic->menu->comp_object, ox, oy);
}

static void
_ibar_cb_icon_menu_focus_change(void *data, Evas_Object *obj, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   E_Client *ec;

   ec = e_comp_object_client_get(obj);
   if (ec->focused)
     elm_object_signal_emit(data, "e,state,focused", "e");
   else
     elm_object_signal_emit(data, "e,state,unfocused", "e");
}

static void
_ibar_cb_icon_menu_desk_change(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   E_Client *ec = event_info;
   IBar_Icon *ic;

   ic = evas_object_data_get(data, "ibar_icon");
   if (!ic) return;

   if (ec->sticky || (ec->zone != e_comp_object_util_zone_get(ic->ibar->o_outerbox)))
     elm_object_signal_emit(data, "e,state,other,screen", "e");
   else if (!ec->desk->visible)
     elm_object_signal_emit(data, "e,state,other,desk", "e");
   else
     elm_object_signal_emit(data, "e,state,other,none", "e");
}

static void
_ibar_cb_icon_menu_img_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   int w, h;
   E_Client *ec;
   IBar_Icon *ic;

   ic = evas_object_data_del(data, "ibar_icon");
   if (!ic) return; //menu is closing
   if (ic) ic->client_objs = eina_list_remove(ic->client_objs, obj);
   if (!ic->menu) return; //who knows
   edje_object_part_box_remove(ic->menu->o_bg, "e.box", data);
   ec = evas_object_data_get(obj, "E_Client");
   if (ec)
     {
        e_comp_object_signal_callback_del_full(ec->frame, "e,state,*focused", "e", _ibar_cb_icon_menu_focus_change, data);
        evas_object_smart_callback_del_full(ec->frame, "desk_change", _ibar_cb_icon_menu_desk_change, data);
     }
   evas_object_del(data);
   if (eina_list_count(ic->exes) <= 1)
     {
        E_Exec_Instance *inst = eina_list_data_get(ic->exes);

        if ((!inst) || (!inst->clients))
          {
             _ibar_cb_icon_menu_hide_begin(ic);
             return;
          }
     }
   edje_object_calc_force(ic->menu->o_bg);
   edje_object_size_min_calc(ic->menu->o_bg, &w, &h);
   evas_object_size_hint_min_set(ic->menu->o_bg, w, h);
   if (elm_box_horizontal_get(ic->ibar->o_box))
     {
        int cx, cy, cw, ch, ny;
        E_Zone *zone;

        evas_object_geometry_get(ic->menu->comp_object, &cx, &cy, &cw, &ch);
        zone = e_comp_object_util_zone_get(ic->ibar->o_outerbox);
        if (cy > (zone->h / 2))
          ny = cy - (h - ch);
        else
          ny = cy;
        evas_object_geometry_set(ic->menu->comp_object, cx, ny, w, h);
     }
   else
      evas_object_resize(ic->menu->comp_object, w, h);
}

static void
_ibar_icon_menu_mouse_in(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   IBar_Icon *ic = data;

   E_FREE_FUNC(ic->hide_timer, ecore_timer_del);
}

static void
_ibar_icon_menu_mouse_out(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   IBar_Icon *ic = data;

   if (e_comp_util_mouse_grabbed()) return;
   if (ic->hide_timer)
     ecore_timer_reset(ic->hide_timer);
   else
     ic->hide_timer = ecore_timer_add(0.5, _ibar_cb_out_hide_delay, ic);
}

static void
_ibar_cb_icon_frame_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   IBar_Icon *ic = evas_object_data_del(obj, "ibar_icon");
   if (ic) ic->client_objs = eina_list_remove(ic->client_objs, obj);
   e_comp_object_signal_callback_del_full(data, "e,state,*focused", "e", _ibar_cb_icon_menu_focus_change, obj);
   evas_object_smart_callback_del_full(data, "desk_change", _ibar_cb_icon_menu_desk_change, obj);
}

static Eina_Bool
_ibar_icon_menu_client_add(IBar_Icon *ic, E_Client *ec)
{
   Evas_Object *o, *it, *img;
   Eina_Stringshare *txt;
   int w, h;

   if (ec->netwm.state.skip_taskbar) return EINA_FALSE;
   o = ic->menu->o_bg;
   it = elm_layout_add(ic->o_holder);
   ic->client_objs = eina_list_append(ic->client_objs, it);
   e_comp_object_util_del_list_append(ic->menu->comp_object, it);
   e_theme_edje_object_set(it, "base/theme/modules/ibar",
                           "e/modules/ibar/menu/item");
   img = e_comp_object_util_mirror_add(ec->frame);
   ic->client_objs = eina_list_append(ic->client_objs, img);
   e_comp_object_signal_callback_add(ec->frame, "e,state,*focused", "e", _ibar_cb_icon_menu_focus_change, it);
   evas_object_smart_callback_add(ec->frame, "desk_change", _ibar_cb_icon_menu_desk_change, it);
   evas_object_event_callback_add(it, EVAS_CALLBACK_DEL,
                                  _ibar_cb_icon_frame_del, ec->frame);
   evas_object_event_callback_add(img, EVAS_CALLBACK_DEL,
                                  _ibar_cb_icon_menu_img_del, it);
   txt = e_client_util_name_get(ec);
   w = ec->client.w;
   h = ec->client.h;
   e_comp_object_util_del_list_append(ic->menu->comp_object, img);
   evas_object_show(img);
   edje_extern_object_aspect_set(img, EDJE_ASPECT_CONTROL_BOTH, w, h);
   elm_object_part_content_set(it, "e.swallow.icon", img);
   elm_object_part_text_set(it, "e.text.title", txt);
   if (ec->focused)
     elm_object_signal_emit(it, "e,state,focused", "e");
   if (ec->sticky || (ec->zone != e_comp_object_util_zone_get(ic->ibar->o_outerbox)))
     elm_object_signal_emit(it, "e,state,other,screen", "e");
   else if (!ec->desk->visible)
     elm_object_signal_emit(it, "e,state,other,desk", "e");
   evas_object_show(it);
   evas_object_event_callback_add(it, EVAS_CALLBACK_MOUSE_UP, _ibar_cb_icon_menu_mouse_up, ec);
   evas_object_data_set(it, "ibar_icon", ic);
   edje_object_part_box_append(o, "e.box", it);
   return EINA_TRUE;
}

static void
_ibar_icon_menu(IBar_Icon *ic, Eina_Bool grab)
{
   Evas_Object *o;
   Eina_List *l;
   E_Exec_Instance *exe;
   Evas *e;
   Eina_Bool empty = EINA_TRUE;
   E_Client *ec;
#if 0
   if (!ic->exes) return; //FIXME
   EINA_LIST_FREE(ic->menu_pending, ec)
     evas_object_event_callback_del_full(ec->frame, EVAS_CALLBACK_SHOW, _ibar_exec_new_client_show, ic);
   ic->menu = e_gadcon_popup_new(ic->ibar->inst->gcc, 1);
   e_object_data_set(E_OBJECT(ic->menu), ic);
   E_OBJECT_DEL_SET(ic->menu, _ibar_cb_icon_menu_del);
   e = e_comp->evas;
   o = edje_object_add(e);
   e_theme_edje_object_set(o, "base/theme/modules/ibar",
                           "e/modules/ibar/menu");
   /* gadcon popups don't really prevent this,
    * so away we go!
    */
   evas_object_del(ic->menu->comp_object);
   ic->menu->o_bg = o;
   ic->menu->comp_object = e_comp_object_util_add(o, E_COMP_OBJECT_TYPE_NONE);
   evas_object_clip_set(ic->menu->comp_object, e_gadcon_zone_get(ic->ibar->inst->gcc->gadcon)->bg_clip_object);
   evas_object_layer_set(ic->menu->comp_object, E_LAYER_POPUP);
   EINA_LIST_FOREACH(ic->exes, l, exe)
     {
        Eina_List *ll;

        EINA_LIST_FOREACH(exe->clients, ll, ec)
          {
             if (_ibar_icon_menu_client_add(ic, ec))
               empty = EINA_FALSE;
          }
     }
   if (empty)
     {
        /* something crazy happened */
        evas_object_del(o);
        e_object_del(E_OBJECT(ic->menu));
        return;
     }

   if (!grab)
     {
        evas_object_event_callback_add(ic->menu->comp_object, EVAS_CALLBACK_MOUSE_IN, _ibar_icon_menu_mouse_in, ic);
        evas_object_event_callback_add(ic->menu->comp_object, EVAS_CALLBACK_MOUSE_OUT, _ibar_icon_menu_mouse_out, ic);
     }

   edje_object_signal_callback_add(o, "e,action,show,done", "*",
                                   _ibar_cb_icon_menu_shown, ic);
   edje_object_signal_callback_add(o, "e,action,hide,done", "*",
                                   _ibar_cb_icon_menu_hidden, ic);
   elm_object_signal_emit(o, "e,state,hidden", "e");
   edje_object_message_signal_process(o);
   ic->ibar->menu_icon = ic;
   _ibar_icon_menu_recalc(ic);

   evas_object_pass_events_set(o, 1);
   elm_object_signal_emit(o, "e,action,show", "e");
   ic->menu_grabbed = grab;
   if (grab)
     e_comp_object_util_autoclose(ic->menu->comp_object, _ibar_cb_icon_menu_autodel, NULL, ic);
#endif
}

static void
_ibar_exec_new_client_show(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   IBar_Icon *ic = data;
   E_Client *ec = e_comp_object_client_get(obj);

   if (!ic->menu) return;
   _ibar_icon_menu_client_add(ic, ec);
   _ibar_icon_menu_recalc(ic);
   ic->menu_pending = eina_list_remove(ic->menu_pending, ec);
   evas_object_event_callback_del_full(ec->frame, EVAS_CALLBACK_SHOW, _ibar_exec_new_client_show, ic);
}

static void
_ibar_icon_menu_show(IBar_Icon *ic, Eina_Bool grab)
{
   if (ic->ibar->menu_icon && (ic->ibar->menu_icon != ic))
     _ibar_icon_menu_hide(ic->ibar->menu_icon, ic->ibar->menu_icon->menu_grabbed);
   if (ic->menu)
     {
        if (ic->ibar->menu_icon != ic)
          {
             elm_object_signal_emit(ic->menu->o_bg, "e,action,show", "e");
             ic->ibar->menu_icon = ic;
          }
        return;
     }
   ic->drag.start = 0;
   ic->drag.dnd = 0;
   ic->mouse_down = 0;
   _ibar_icon_menu(ic, grab);
}

static void
_ibar_icon_menu_hide(IBar_Icon *ic, Eina_Bool grab)
{
   if (!ic->menu) return;
   if (ic->menu_grabbed != grab) return;
   if (ic->ibar && (ic->ibar->menu_icon == ic))
     ic->ibar->menu_icon = NULL;
   E_FREE_FUNC(ic->hide_timer, ecore_timer_del);
   ic->menu_grabbed = EINA_FALSE;
   _ibar_cb_icon_menu_hide_begin(ic);
}

static Eina_Bool
_ibar_icon_mouse_in_timer(void *data)
{
   IBar_Icon *ic = data;

   ic->show_timer = NULL;
   _ibar_icon_menu_show(ic, EINA_FALSE);
   return EINA_FALSE;
}

static void
_ibar_cb_icon_mouse_in(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   IBar_Icon *ic;

   ic = data;
   E_FREE_FUNC(ic->reset_timer, ecore_timer_del);
   ic->focused = EINA_TRUE;
   _ibar_icon_signal_emit(ic, "e,state,focused", "e");
   if (ic->ibar->inst->ci->show_label)
     _ibar_icon_signal_emit(ic, "e,action,show,label", "e");
   E_FREE_FUNC(ic->hide_timer, ecore_timer_del);
   if (!ic->ibar->inst->ci->dont_icon_menu_mouseover)
     {
        if (ic->show_timer)
          ecore_timer_reset(ic->show_timer);
        else
          ic->show_timer = ecore_timer_add(0.2, _ibar_icon_mouse_in_timer, ic);
     }
}

static Eina_Bool
_ibar_cb_out_hide_delay(void *data)
{
   IBar_Icon *ic = data;

   ic->hide_timer = NULL;
   _ibar_icon_menu_hide(ic, EINA_FALSE);
   return EINA_FALSE;
}

static void
_ibar_cb_icon_mouse_out(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   IBar_Icon *ic;

   ic = data;
   E_FREE_FUNC(ic->reset_timer, ecore_timer_del);
   E_FREE_FUNC(ic->show_timer, ecore_timer_del);
   ic->focused = EINA_FALSE;
   _ibar_icon_signal_emit(ic, "e,state,unfocused", "e");
   if (ic->ibar->inst->ci->show_label)
     _ibar_icon_signal_emit(ic, "e,action,hide,label", "e");
   if (!ic->ibar->inst->ci->dont_icon_menu_mouseover)
     {
        if (ic->hide_timer)
          ecore_timer_reset(ic->hide_timer);
        else
          ic->hide_timer = ecore_timer_add(0.75, _ibar_cb_out_hide_delay, ic);
     }
}

static Eina_Bool
_ibar_cb_icon_menu_cb(void *data)
{
   IBar_Icon *ic = data;

   ic->timer = NULL;
   _ibar_icon_menu_show(ic, EINA_TRUE);
   return EINA_FALSE;
}

static void
_ibar_cb_icon_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Down *ev;
   IBar_Icon *ic;

   ev = event_info;
   ic = data;
   if (ev->button == 1)
     {
        ic->drag.x = ev->output.x;
        ic->drag.y = ev->output.y;
        ic->drag.start = 1;
        ic->drag.dnd = 0;
        ic->mouse_down = 1;
        if (!ic->timer)
          ic->timer = ecore_timer_add(0.35, _ibar_cb_icon_menu_cb, ic);
     }
   else if (ev->button == 2)
     {
        E_FREE_FUNC(ic->show_timer, ecore_timer_del);
        E_FREE_FUNC(ic->hide_timer, ecore_timer_del);
        E_FREE_FUNC(ic->timer, ecore_timer_del);
        _ibar_icon_menu_show(ic, EINA_TRUE);
     }
   else if (ev->button == 3)
     {
#warning FIXME
#if 0
        Eina_List *it;
        E_Menu *m, *mo;
        E_Menu_Item *mi;
        Efreet_Desktop_Action *action;
        char buf[256];
        int cx, cy;

        E_FREE_FUNC(ic->show_timer, ecore_timer_del);
        E_FREE_FUNC(ic->hide_timer, ecore_timer_del);
        E_FREE_FUNC(ic->timer, ecore_timer_del);
        if (ic->menu)
          _ibar_icon_menu_hide(ic, ic->menu_grabbed);
        m = e_menu_new();

        /* FIXME: other icon options go here too */
        mo = e_menu_new();

        if (e_configure_registry_exists("applications/new_application"))
          {
             mi = e_menu_item_new(m);
             e_menu_item_label_set(mi, _("Create new Icon"));
             e_util_menu_item_theme_icon_set(mi, "document-new");
             e_menu_item_callback_set(mi, _ibar_cb_menu_icon_new, NULL);

             mi = e_menu_item_new(m);
             e_menu_item_separator_set(mi, 1);
          }

        if (e_configure_registry_exists("applications/ibar_applications"))
          {
             mi = e_menu_item_new(m);
             e_menu_item_label_set(mi, _("Contents"));
             e_util_menu_item_theme_icon_set(mi, "list-add");
             e_menu_item_callback_set(mi, _ibar_cb_menu_icon_add, ic->ibar);
          }

        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, _("Settings"));
        e_util_menu_item_theme_icon_set(mi, "configure");
        e_menu_item_callback_set(mi, _ibar_cb_menu_configuration, ic->ibar);

        m = e_gadcon_client_util_menu_items_append(ic->ibar->inst->gcc, m, 0);

        mi = e_menu_item_new(mo);
        e_menu_item_label_set(mi, _("Properties"));
        e_util_menu_item_theme_icon_set(mi, "configure");
        e_menu_item_callback_set(mi, _ibar_cb_menu_icon_properties, ic);

        mi = e_menu_item_new(mo);
        if (ic->not_in_order)
          {
             e_menu_item_label_set(mi, _("Add to bar"));
             e_util_menu_item_theme_icon_set(mi, "list-add");
             e_menu_item_callback_set(mi, _ibar_cb_menu_icon_stick, ic);
          }
        else
          {
             e_menu_item_label_set(mi, _("Remove from bar"));
             e_util_menu_item_theme_icon_set(mi, "list-remove");
             e_menu_item_callback_set(mi, _ibar_cb_menu_icon_remove, ic);
          }

        mi = e_menu_item_new_relative(m, NULL);
        snprintf(buf, sizeof(buf), _("Icon %s"), ic->app->name);
        e_menu_item_label_set(mi, buf);
        e_util_desktop_menu_item_icon_add(ic->app,
                                          e_util_icon_size_normalize(24 * e_scale),
                                          mi);
        e_menu_item_submenu_set(mi, mo);
        e_object_unref(E_OBJECT(mo));

        if (ic->app->actions)
          {
             mi = NULL;
             EINA_LIST_FOREACH(ic->app->actions, it, action)
               {
                  mi = e_menu_item_new_relative(m, mi);
                  e_menu_item_label_set(mi, action->name);
                  e_util_menu_item_theme_icon_set(mi, action->icon);
                  e_menu_item_callback_set(mi, _ibar_cb_menu_icon_action_exec, action);
               }
             mi = e_menu_item_new_relative(m, mi);
             e_menu_item_separator_set(mi, 1);
          }

        e_gadcon_client_menu_set(ic->ibar->inst->gcc, m);

        e_gadcon_canvas_zone_geometry_get(ic->ibar->inst->gcc->gadcon,
                                          &cx, &cy, NULL, NULL);
        e_menu_activate_mouse(m,
                              e_zone_current_get(),
                              cx + ev->output.x, cy + ev->output.y, 1, 1,
                              E_MENU_POP_DIRECTION_DOWN, ev->timestamp);
#endif
     }
}

static Eina_Bool
_ibar_cb_icon_reset(void *data)
{
   IBar_Icon *ic = data;
   
   if (ic->focused)
     {
        _ibar_icon_signal_emit(ic, "e,state,focused", "e");
        if (ic->ibar->inst->ci->show_label)
          _ibar_icon_signal_emit(ic, "e,action,show,label", "e");
     }
   ic->reset_timer = NULL;
   return EINA_FALSE;
}

static void
_ibar_cb_icon_wheel(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Wheel *ev = event_info;
   E_Exec_Instance *exe;
   IBar_Icon *ic = data;
   E_Client *cur, *sel = NULL;
   Eina_List *l, *exe_current = NULL;

   if (!ic->exes) return;

   cur = e_client_focused_get();
   if (cur && cur->exe_inst)
     {
        EINA_LIST_FOREACH(ic->exes, l, exe)
          if (cur->exe_inst == exe)
            {
               exe_current = l;
               break;
            }
     }
   if (!exe_current)
     exe_current = ic->exes;

   exe = eina_list_data_get(exe_current);
   if (ev->z < 0)
     {
        if (cur && (cur->exe_inst == exe))
          {
             l = eina_list_data_find_list(exe->clients, cur);
             if (l) sel = eina_list_data_get(eina_list_next(l));
          }
        if (!sel)
          {
             exe_current = eina_list_next(exe_current);
             if (!exe_current)
               exe_current = ic->exes;
          }
     }
   else if (ev->z > 0)
     {
        if (cur && (cur->exe_inst == exe))
          {
             l = eina_list_data_find_list(exe->clients, cur);
             if (l) sel = eina_list_data_get(eina_list_prev(l));
          }
        if (!sel)
          {
             exe_current = eina_list_prev(exe_current);
             if (!exe_current)
               exe_current = eina_list_last(ic->exes);
          }
     }

   if (!sel)
     {
        exe = eina_list_data_get(exe_current);
        sel = eina_list_data_get(exe->clients);
        if (sel == cur)
          sel = eina_list_data_get(eina_list_next(exe->clients));
     }

   if (sel)
     e_client_activate(sel, 1);
}

static void
_ibar_instance_watch(void *data, E_Exec_Instance *inst, E_Exec_Watch_Type type)
{
   IBar_Icon *ic = data;
     
   switch (type)
     {
      case E_EXEC_WATCH_STARTED:
        _ibar_icon_signal_emit(ic, "e,state,started", "e");
        if (!ic->exes) _ibar_icon_signal_emit(ic, "e,state,on", "e");
        if (ic->exe_inst == inst) ic->exe_inst = NULL;
        if (!eina_list_data_find(ic->exes, inst))
          ic->exes = eina_list_append(ic->exes, inst);
        break;
      default:
        break;
     }
}

static void
_ibar_icon_go(IBar_Icon *ic, Eina_Bool keep_going)
{
   if (ic->not_in_order)
     {
        Eina_List *l, *ll;
        E_Exec_Instance *exe;
        E_Client *ec, *eclast = NULL;
        unsigned int count = 0;

        EINA_LIST_FOREACH(ic->exes, l, exe)
          {
             EINA_LIST_FOREACH(exe->clients, ll, ec)
               {
                  count++;
                  if (count > 1)
                    {
                       ecore_job_add((Ecore_Cb)_ibar_cb_icon_menu_cb, ic);
                       return;
                    }
                  eclast = ec;
               }
          }
        if (eclast)
          e_client_activate(eclast, 1);
        return;
     }
   if (ic->app->type == EFREET_DESKTOP_TYPE_APPLICATION)
     {
        if (ic->ibar->inst->ci->dont_track_launch)
          e_exec(e_comp_object_util_zone_get(ic->ibar->o_outerbox),
                 ic->app, NULL, NULL, "ibar");
        else
          {
             E_Exec_Instance *einst;
             
             einst = e_exec(e_comp_object_util_zone_get(ic->ibar->o_outerbox),
                            ic->app, NULL, NULL, "ibar");
             if (einst)
               {
                  ic->exe_inst = einst;
                  e_exec_instance_watcher_add(einst, _ibar_instance_watch, ic);
                  _ibar_icon_signal_emit(ic, "e,state,starting", "e");
               }
          }
     }
   else if (ic->app->type == EFREET_DESKTOP_TYPE_LINK)
     {
        if (!strncasecmp(ic->app->url, "file:", 5))
          {
             E_Action *act;
             
             act = e_action_find("fileman");
             if (act) act->func.go(NULL, ic->app->url + 5);
          }
     }
   _ibar_icon_signal_emit(ic, "e,action,exec", "e");
   if (keep_going)
     ic->reset_timer = ecore_timer_add(1.0, _ibar_cb_icon_reset, ic);
}

static void
_ibar_cb_icon_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Up *ev;
   IBar_Icon *ic;

   ev = event_info;
   ic = data;

   if ((ev->button == 1) && (ic->mouse_down == 1))
     {
        if (!ic->drag.dnd) _ibar_icon_go(ic, EINA_FALSE);
        ic->drag.start = 0;
        ic->drag.dnd = 0;
        ic->mouse_down = 0;
        E_FREE_FUNC(ic->timer, ecore_timer_del);
     }
}

static void
_ibar_cb_icon_mouse_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Move *ev = event_info;
   IBar_Icon *ic = data;
   int dx, dy;

   E_FREE_FUNC(ic->timer, ecore_timer_del);
   if (!ic->drag.start) return;

   dx = ev->cur.output.x - ic->drag.x;
   dy = ev->cur.output.y - ic->drag.y;
   if (((dx * dx) + (dy * dy)) >
       (e_config->drag_resist * e_config->drag_resist))
     {
        E_Drag *d;
        Evas_Object *o;
        Evas_Coord x, y, w, h;
        unsigned int size;
        IBar *i;
        const char *drag_types[] = { "enlightenment/desktop" };

        ic->drag.dnd = 1;
        ic->drag.start = 0;

        if (ic->ibar->inst->ci->lock_move) return;

        evas_object_geometry_get(ic->o_icon, &x, &y, &w, &h);
        d = e_drag_new(x, y, drag_types, 1,
                       ic->app, -1, NULL, _ibar_cb_drag_finished);
        d->button_mask = evas_pointer_button_down_mask_get(e_comp->evas);
        efreet_desktop_ref(ic->app);
        size = MAX(w, h);
        o = e_util_desktop_icon_add(ic->app, size, e_drag_evas_get(d));
        e_drag_object_set(d, o);

        e_drag_resize(d, w, h);
        e_drag_start(d, ic->drag.x, ic->drag.y);
        i = ic->ibar;
        e_object_data_set(E_OBJECT(d), i);
        if (!ic->not_in_order)
          e_order_remove(i->io->eo, ic->app);
        _ibar_icon_free(ic);
        _ibar_resize_handle(i);
     }
}

static void
_ibar_cb_icon_move(void *data, Evas *e, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   IBar_Icon *ic;
   int x, y, w, h, cw, chx, len = 0;
   const char *sig = "e,origin,center";
   E_Zone *zone;

   ic = data;
   evas_object_geometry_get(ic->o_holder, &x, &y, &w, &h);
   evas_object_move(ic->o_holder2, x, y);
   evas_output_size_get(e, &cw, NULL);

   edje_object_part_geometry_get(elm_layout_edje_get(ic->o_holder2), "e.text.label", NULL, NULL, &len, NULL);
   chx = x + (w / 2);
   zone = e_comp_object_util_zone_get(obj);
   if (!zone)
     {
        if (x < 1)
          zone = e_comp_zone_xy_get(0, y);
        else
          zone = e_comp_zone_xy_get(e_comp->w - 5, y);
        if (!zone)
          zone = eina_list_data_get(e_comp->zones);
     }
   if (chx - (len / 2) < zone->x)
     sig = "e,origin,left";
   else if ((chx + (len / 2) > cw) || ((chx + (len / 2) > zone->x + zone->w)))
     sig = "e,origin,right";
   _ibar_icon_signal_emit(ic, sig, "e");
}

static void
_ibar_cb_icon_resize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   IBar_Icon *ic;
   Evas_Coord w, h;

   ic = data;
   evas_object_geometry_get(ic->o_holder, NULL, NULL, &w, &h);
   evas_object_resize(ic->o_holder2, w, h);
}

static void
_ibar_cb_drop_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   IBar *b;
   Evas_Coord x, y;

   b = data;
   evas_object_geometry_get(b->o_drop, &x, &y, NULL, NULL);
   evas_object_move(b->o_drop_over, x, y);
}

static void
_ibar_cb_drop_resize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   IBar *b;
   Evas_Coord w, h;

   b = data;
   evas_object_geometry_get(b->o_drop, NULL, NULL, &w, &h);
   evas_object_resize(b->o_drop_over, w, h);
}

static void
_ibar_cb_drag_finished(E_Drag *drag, int dropped)
{
   IBar *i = e_object_data_get(E_OBJECT(drag));

   efreet_desktop_unref(drag->data);
   if (!i) return;
   if (!dropped)
     {
        _ibar_empty(i);
        _ibar_fill(i);
        _ibar_resize_handle(i);
     }
}

static void
_ibar_inst_cb_scroll(void *data)
{
   Instance *inst;

   /* Update the position of the dnd to handle for autoscrolling
    * gadgets. */
   inst = data;
   _ibar_drop_position_update(inst, inst->ibar->dnd_x, inst->ibar->dnd_y);
}

static void
_ibar_drop_position_update(Instance *inst, Evas_Coord x, Evas_Coord y)
{
   IBar_Icon *ic;

   inst->ibar->dnd_x = x;
   inst->ibar->dnd_y = y;

   ic = _ibar_icon_at_coord(inst->ibar, x, y);
   if (ic && (ic == inst->ibar->ic_drop_before)) return;

   if (inst->ibar->o_drop)
     {
        int ox, oy, ow, oh;

        evas_object_geometry_get(inst->ibar->o_drop, &ox, &oy, &ow, &oh);
        /* if cursor is still inside last drop area, do nothing */
        if (E_INSIDE(x, y, ox, oy, ow, oh)) return;
        elm_box_unpack(inst->ibar->o_box, inst->ibar->o_drop);
     }

   inst->ibar->ic_drop_before = ic;
   if (ic)
     {
        Evas_Coord ix, iy, iw, ih;
        int before = 0;

        evas_object_geometry_get(ic->o_holder, &ix, &iy, &iw, &ih);
        if (elm_box_horizontal_get(inst->ibar->o_box))
          {
             if (x < (ix + (iw / 2))) before = 1;
          }
        else
          {
             if (y < (iy + (ih / 2))) before = 1;
          }
        if (before)
          elm_box_pack_before(inst->ibar->o_box, inst->ibar->o_drop, ic->o_holder);
        else
          elm_box_pack_after(inst->ibar->o_box, inst->ibar->o_drop, ic->o_holder);
        inst->ibar->drop_before = before;
     }
   else elm_box_pack_end(inst->ibar->o_box, inst->ibar->o_drop);
   evas_object_size_hint_min_set(inst->ibar->o_drop, 1, 1);
   _ibar_resize_handle(inst->ibar);
}

static void
_ibar_inst_cb_enter(void *data, const char *type EINA_UNUSED, void *event_info)
{
   E_Event_Dnd_Enter *ev;
   Instance *inst;
   Evas_Object *o, *o2;

   ev = event_info;
   inst = data;
   o = elm_layout_add(inst->ibar->o_box);
   inst->ibar->o_drop = o;
   E_EXPAND(o);
   E_FILL(o);
   o2 = elm_layout_add(inst->ibar->o_box);
   inst->ibar->o_drop_over = o2;
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOVE,
                                  _ibar_cb_drop_move, inst->ibar);
   evas_object_event_callback_add(o, EVAS_CALLBACK_RESIZE,
                                  _ibar_cb_drop_resize, inst->ibar);
   e_theme_edje_object_set(o, "base/theme/modules/ibar",
                           "e/modules/ibar/drop");
   e_theme_edje_object_set(o2, "base/theme/modules/ibar",
                           "e/modules/ibar/drop_overlay");
   evas_object_layer_set(o2, 19999);
   evas_object_show(o);
   evas_object_show(o2);

   _ibar_drop_position_update(inst, ev->x, ev->y);
   //e_gadcon_client_autoscroll_cb_set(inst->gcc, _ibar_inst_cb_scroll, inst);
   //e_gadcon_client_autoscroll_update(inst->gcc, ev->x, ev->y);
}

static void
_ibar_inst_cb_move(void *data, const char *type EINA_UNUSED, void *event_info)
{
   E_Event_Dnd_Move *ev;
   Instance *inst;
   int x, y;

   ev = event_info;
   inst = data;
   _ibar_drop_position_update(inst, ev->x, ev->y);
   evas_object_geometry_get(inst->ibar->o_outerbox, &x, &y, NULL, NULL);
   //e_gadcon_client_autoscroll_update(inst->gcc, ev->x - x, ev->y - y);
}

static void
_ibar_inst_cb_leave(void *data, const char *type EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst;

   inst = data;
   inst->ibar->ic_drop_before = NULL;
   evas_object_del(inst->ibar->o_drop);
   inst->ibar->o_drop = NULL;
   evas_object_del(inst->ibar->o_drop_over);
   inst->ibar->o_drop_over = NULL;
   _ibar_resize_handle(inst->ibar);
   //e_gadcon_client_autoscroll_cb_set(inst->gcc, NULL, NULL);
}

static void
_ibar_inst_cb_drop(void *data, const char *type, void *event_info)
{
   E_Event_Dnd_Drop *ev;
   Instance *inst;
   Efreet_Desktop *app = NULL;
   Eina_List *fl = NULL;
   IBar_Icon *ic;

   ev = event_info;
   inst = data;

   if (!strcmp(type, "enlightenment/desktop"))
     app = ev->data;
   else if (!strcmp(type, "enlightenment/border"))
     {
        E_Client *ec;

        ec = ev->data;
        app = ec->desktop;
        if (!app)
          {
             app = e_desktop_client_create(ec);
             efreet_desktop_save(app);
             e_desktop_edit(app);
          }
     }
   else if (!strcmp(type, "text/uri-list"))
     fl = ev->data;

   ic = inst->ibar->ic_drop_before;
   if (ic)
     {
        /* Add new eapp before this icon */
        if (!inst->ibar->drop_before)
          {
             IBar_Icon *ic2;

             EINA_INLIST_FOREACH(inst->ibar->icons, ic2)
               {
                  if (ic2 == ic)
                    {
                       if (EINA_INLIST_GET(ic2)->next)
                         ic = (IBar_Icon*)EINA_INLIST_GET(ic2)->next;
                       else
                         ic = NULL;
                       break;
                    }
               }
          }
        if (!ic) goto atend;
        if (app)
          e_order_prepend_relative(ic->ibar->io->eo, app, ic->app);
        else if (fl)
          e_order_files_prepend_relative(ic->ibar->io->eo, fl, ic->app);
     }
   else
     {
atend:
        if (inst->ibar->io->eo)
          {
             if (app)
               e_order_append(inst->ibar->io->eo, app);
             else if (fl)
               e_order_files_append(inst->ibar->io->eo, fl);
          }
     }
   evas_object_del(inst->ibar->o_drop);
   inst->ibar->o_drop = NULL;
   evas_object_del(inst->ibar->o_drop_over);
   inst->ibar->o_drop_over = NULL;
   //e_gadcon_client_autoscroll_cb_set(inst->gcc, NULL, NULL);
   _ibar_empty_handle(inst->ibar);
   _ibar_resize_handle(inst->ibar);
}

static E_Action *act_ibar_focus = NULL;
static Ecore_X_Window _ibar_focus_win = 0;
static Ecore_Event_Handler *_ibar_key_down_handler = NULL;

static void _ibar_go_unfocus(void);

static IBar *
_ibar_manager_find(void)
{
   E_Zone *z = e_zone_current_get();
   IBar *b;
   Eina_List *l;
   
   if (!z) return NULL;
   // find iubar on current zone
   EINA_LIST_FOREACH(ibars, l, b)
     {
        if (e_comp_object_util_zone_get(b->o_outerbox) == z) return b;
     }
   // no ibars on current zone - return any old ibar
   EINA_LIST_FOREACH(ibars, l, b)
     {
        return b;
     }
   // no ibars. null.
   return NULL;
}

static void
_ibar_icon_unfocus_focus(IBar_Icon *ic1, IBar_Icon *ic2)
{
   if (ic1)
     {
        ic1->focused = EINA_FALSE;
        _ibar_icon_signal_emit(ic1, "e,state,unfocused", "e");
        if (ic1->ibar->inst->ci->show_label)
          _ibar_icon_signal_emit(ic1, "e,action,hide,label", "e");
     }
   if (ic2)
     {
        ic2->focused = EINA_TRUE;
        _ibar_icon_signal_emit(ic2, "e,state,focused", "e");
        if (ic2->ibar->inst->ci->show_label)
          _ibar_icon_signal_emit(ic2, "e,action,show,label", "e");
     }
}

static IBar *
_ibar_focused_find(void)
{
   IBar *b;
   Eina_List *l;
   
   EINA_LIST_FOREACH(ibars, l, b)
     {
        if (b->focused) return b;
     }
   return NULL;
}

static int
_ibar_cb_sort(IBar *b1, IBar *b2)
{
   E_Zone *z1 = NULL, *z2 = NULL;
   
   z1 = e_comp_object_util_zone_get(b1->o_outerbox);
   z2 = e_comp_object_util_zone_get(b2->o_outerbox);
   if ((z1) && (!z2)) return -1;
   else if ((!z1) && (z2)) return 1;
   else if ((!z1) && (!z2)) return 0;
   else
     {
        int id1, id2;
        
        id1 = z1->id;
        id2 = z2->id;
        return id2 - id1;
     }
   return 0;
}

static IBar *
_ibar_focused_next_find(void)
{
   IBar *b, *bn = NULL;
   Eina_List *l;
   Eina_List *tmpl = NULL;
  
   EINA_LIST_FOREACH(ibars, l, b)
     {
        if (!b->icons) continue;
        tmpl = eina_list_sorted_insert
          (tmpl, EINA_COMPARE_CB(_ibar_cb_sort), b);
     }
   if (!tmpl) tmpl = ibars;
   EINA_LIST_FOREACH(tmpl, l, b)
     {
        if (b->focused)
          {
             if (l->next)
               {
                  bn = l->next->data;
                  break;
               }
             else
               {
                  bn = tmpl->data;
                  break;
               }
          }
     }
   if (tmpl != ibars) eina_list_free(tmpl);
   return bn;
}

static IBar *
_ibar_focused_prev_find(void)
{
   IBar *b, *bn = NULL;
   Eina_List *l;
   Eina_List *tmpl = NULL;
  
   EINA_LIST_FOREACH(ibars, l, b)
     {
        if (!b->icons) continue;
        tmpl = eina_list_sorted_insert
          (tmpl, EINA_COMPARE_CB(_ibar_cb_sort), b);
     }
   if (!tmpl) tmpl = ibars;
   EINA_LIST_FOREACH(tmpl, l, b)
     {
        if (b->focused)
          {
             if (l->prev)
               {
                  bn = l->prev->data;
                  break;
               }
             else
               {
                  bn = eina_list_last_data_get(tmpl);
                  break;
               }
          }
     }
   if (tmpl != ibars) eina_list_free(tmpl);
   return bn;
}

static void
_ibar_focus(IBar *b)
{
   IBar_Icon *ic;
   
   if (b->focused) return;
   b->focused = EINA_TRUE;
   EINA_INLIST_FOREACH(b->icons, ic)
     {
        if (ic->focused)
          {
             _ibar_icon_unfocus_focus(ic, NULL);
             break;
          }
     }
   if (b->icons)
     _ibar_icon_unfocus_focus(NULL, (IBar_Icon*)b->icons);
}

static void
_ibar_unfocus(IBar *b)
{
   IBar_Icon *ic;

   if (!b->focused) return;
   b->focused = EINA_FALSE;
   EINA_INLIST_FOREACH(b->icons, ic)
     {
        if (ic->focused)
          {
             _ibar_icon_unfocus_focus(ic, NULL);
             break;
          }
     }
}

static void
_ibar_focus_next(IBar *b)
{
   IBar_Icon *ic, *ic1 = NULL, *ic2 = NULL;
   
   if (!b->focused) return;
   if (!b->icons) return;
   EINA_INLIST_FOREACH(b->icons, ic)
     {
        if (!ic1)
          {
             if (ic->focused) ic1 = ic;
          }
        else
          {
             ic2 = ic;
             break;
          }
     }
   // wrap to start
   if ((ic1) && (!ic2)) ic2 = (IBar_Icon*)b->icons;
   if ((ic1) && (ic2) && (ic1 != ic2))
     _ibar_icon_unfocus_focus(ic1, ic2);
}

static void
_ibar_focus_prev(IBar *b)
{
   IBar_Icon *ic, *ic1 = NULL, *ic2 = NULL;
   
   if (!b->focused) return;
   if (!b->icons) return;
   EINA_INLIST_FOREACH(b->icons, ic)
     {
        if (ic->focused)
          {
             ic1 = ic;
             break;
          }
        ic2 = ic;
     }
   // wrap to end
   if ((ic1) && (!ic2)) ic2 = (IBar_Icon*)b->icons;
   if ((ic1) && (ic2) && (ic1 != ic2))
     _ibar_icon_unfocus_focus(ic1, ic2);
}

static void
_ibar_focus_launch(IBar *b)
{
   IBar_Icon *ic;
   
   if (!b->focused) return;
   EINA_INLIST_FOREACH(b->icons, ic)
     {
        if (ic->focused)
          {
             _ibar_icon_go(ic, EINA_TRUE);
             break;
          }
     }
}

static Eina_Bool
_ibar_focus_cb_key_down(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Key *ev;
   IBar *b, *b2;
   
   ev = event;
   if (ev->window != _ibar_focus_win) return ECORE_CALLBACK_PASS_ON;
   b = _ibar_focused_find();
   if (!b) return ECORE_CALLBACK_PASS_ON;
   if (!strcmp(ev->key, "Up"))
     {
        if (b->inst)
          {  
             if (b->inst->orient == Z_GADGET_SITE_ORIENT_VERTICAL)
               _ibar_focus_prev(b);
          }
     }
   else if (!strcmp(ev->key, "Down"))
     {
        if (b->inst)
          {  
             if (b->inst->orient == Z_GADGET_SITE_ORIENT_VERTICAL)
               _ibar_focus_next(b);
          }
     }
   else if (!strcmp(ev->key, "Left"))
     {
        if (b->inst)
          {  
             if (b->inst->orient == Z_GADGET_SITE_ORIENT_HORIZONTAL)
               _ibar_focus_prev(b);
          }
     }
   else if (!strcmp(ev->key, "Right"))
     {
        if (b->inst)
          {  
             if (b->inst->orient == Z_GADGET_SITE_ORIENT_HORIZONTAL)
               _ibar_focus_next(b);
          }
     }
   else if (!strcmp(ev->key, "space"))
     {
        _ibar_focus_launch(b);
     }
   else if ((!strcmp(ev->key, "Return")) ||
            (!strcmp(ev->key, "KP_Enter")))
     {
        _ibar_focus_launch(b);
        _ibar_go_unfocus();
     }
   else if (!strcmp(ev->key, "Escape"))
     {
        _ibar_go_unfocus();
     }
   else if (!strcmp(ev->key, "Tab"))
     {
        if (ev->modifiers & ECORE_EVENT_MODIFIER_SHIFT)
          {
             b2 = _ibar_focused_prev_find();
             if ((b) && (b2) && (b != b2))
               {
                  _ibar_unfocus(b);
                  _ibar_focus(b2);
               }
          }
        else
          {
             b2 = _ibar_focused_next_find();
             if ((b) && (b2) && (b != b2))
               {
                  _ibar_unfocus(b);
                  _ibar_focus(b2);
               }
          }
     }
   else if (!strcmp(ev->key, "ISO_Left_Tab"))
     {
        b2 = _ibar_focused_prev_find();
        if ((b) && (b2) && (b != b2))
          {
             _ibar_unfocus(b);
             _ibar_focus(b2);
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static void
_ibar_go_focus(void)
{
   IBar *b;
   
   if (_ibar_focus_win) return;
   b = _ibar_manager_find();
   if (!b) return;
   if (!e_comp_grab_input(0, 1)) return;
   _ibar_focus_win = e_comp->ee_win;
   _ibar_key_down_handler = ecore_event_handler_add(ECORE_EVENT_KEY_DOWN,
     _ibar_focus_cb_key_down, NULL);
   _ibar_focus(b);
}

static void
_ibar_go_unfocus(void)
{
   IBar *b;
   
   if (!_ibar_focus_win) return;
   b = _ibar_focused_find();
   if (b) _ibar_unfocus(b);
   e_comp_ungrab_input(0, 1);
   _ibar_focus_win = 0;
   ecore_event_handler_del(_ibar_key_down_handler);
   _ibar_key_down_handler = NULL;
}
   
static void
_ibar_cb_action_focus(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED, Ecore_Event_Key *ev EINA_UNUSED)
{
   _ibar_go_focus();
}

static Eina_Bool
_ibar_cb_client_prop(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Client_Property *ev)
{
   IBar *b;
   Eina_List *l;
   E_Client *ec;
   Eina_Bool skip = EINA_TRUE;

   if (e_client_util_ignored_get(ev->ec) || (!ev->ec->exe_inst) || 
       (!ev->ec->exe_inst->desktop)) return ECORE_CALLBACK_RENEW;
   if ((!(ev->property & E_CLIENT_PROPERTY_NETWM_STATE)) && (!(ev->property & E_CLIENT_PROPERTY_ICON)))
     return ECORE_CALLBACK_RENEW;
   EINA_LIST_FOREACH(ev->ec->exe_inst->clients, l, ec)
     if (!ec->netwm.state.skip_taskbar)
       {
          skip = EINA_FALSE;
          break;
       }
   EINA_LIST_FOREACH(ibars, l, b)
     {
        IBar_Icon *ic;

        ic = eina_hash_find(b->icon_hash, _desktop_name_get(ev->ec->exe_inst->desktop));
        if (skip && (!ic)) continue;
        if (!skip)
          {
             if (ic)
               {
                  _ibar_icon_signal_emit(ic, "e,state,started", "e");
                  if (!ic->exes) _ibar_icon_signal_emit(ic, "e,state,on", "e");
                  if (!eina_list_data_find(ic->exes, ev->ec->exe_inst))
                    ic->exes = eina_list_append(ic->exes, ev->ec->exe_inst);
               }
            else if (!b->inst->ci->dont_add_nonorder)
              {
                 _ibar_sep_create(b);
                 _ibar_icon_notinorder_new(b, ev->ec->exe_inst);
                 _ibar_resize_handle(b);
              }
          }
        else
          {
             ic->exes = eina_list_remove(ic->exes, ev->ec->exe_inst);
             if (ic->exe_inst == ev->ec->exe_inst) ic->exe_inst = NULL;
             if (!ic->exes)
               {
                  if (ic->not_in_order)
                    {
                       _ibar_icon_free(ic);
                       if (!b->not_in_order_count)
                         {
                            E_FREE_FUNC(b->o_sep, evas_object_del);
                         }
                       _ibar_resize_handle(b);
                    }
                  else
                    _ibar_icon_signal_emit(ic, "e,state,off", "e");
               }
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_ibar_cb_exec_del(void *d EINA_UNUSED, int t EINA_UNUSED, E_Exec_Instance *exe)
{
   IBar *b;
   Eina_List *l;

   if (!exe->desktop) return ECORE_CALLBACK_RENEW; //can't do anything here :(
   EINA_LIST_FOREACH(ibars, l, b)
     {
        IBar_Icon *ic;

        ic = eina_hash_find(b->icon_hash, _desktop_name_get(exe->desktop));
        if (ic)
          {
             _ibar_icon_signal_emit(ic, "e,state,started", "e");
             ic->exes = eina_list_remove(ic->exes, exe);
             if (ic->exe_inst == exe) ic->exe_inst = NULL;
             if (!ic->exes)
               {
                  if (ic->not_in_order)
                    {
                       _ibar_icon_free(ic);
                       if (!b->not_in_order_count)
                         {
                            E_FREE_FUNC(b->o_sep, evas_object_del);
                         }
                       _ibar_resize_handle(b);
                    }
                  else
                    _ibar_icon_signal_emit(ic, "e,state,off", "e");
               }
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_ibar_cb_exec_new_client(void *d EINA_UNUSED, int t EINA_UNUSED, E_Exec_Instance *exe)
{
   IBar *b;
   E_Client *ec;
   Eina_List *l;
   Eina_Bool skip;

   if (!exe->desktop) return ECORE_CALLBACK_RENEW; //can't do anything here :(
   if (!exe->desktop->icon) return ECORE_CALLBACK_RENEW;
   ec = eina_list_last_data_get(exe->clients); //only care about last (new) one
   skip = ec->netwm.state.skip_taskbar;
   EINA_LIST_FOREACH(ibars, l, b)
     {
        IBar_Icon *ic;

        ic = eina_hash_find(b->icon_hash, _desktop_name_get(exe->desktop));
        if (ic)
          {
             _ibar_icon_signal_emit(ic, "e,state,started", "e");
             if (!ic->exes) _ibar_icon_signal_emit(ic, "e,state,on", "e");
             if (skip) continue;
             if (!eina_list_data_find(ic->exes, exe))
               ic->exes = eina_list_append(ic->exes, exe);
             if (ic->menu)
               {
                  /* adding will fail if client hasn't been shown yet */
                  ic->menu_pending = eina_list_append(ic->menu_pending, ec);
                  evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_SHOW, _ibar_exec_new_client_show, ic);
               }
          }
        else if (!b->inst->ci->dont_add_nonorder)
          {
             if (skip) continue;
             _ibar_sep_create(b);
             _ibar_icon_notinorder_new(b, exe);
             _ibar_resize_handle(b);
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_ibar_cb_exec_new(void *d EINA_UNUSED, int t EINA_UNUSED, E_Exec_Instance *exe)
{
   IBar *b;
   E_Client *ec;
   Eina_List *l;
   Eina_Bool skip = EINA_TRUE;

   if (!exe->desktop) return ECORE_CALLBACK_RENEW; //can't do anything here :(
   if (!exe->desktop->icon) return ECORE_CALLBACK_RENEW;
   EINA_LIST_FOREACH(exe->clients, l, ec)
     if (!ec->netwm.state.skip_taskbar)
       {
          skip = EINA_FALSE;
          break;
       }
   EINA_LIST_FOREACH(ibars, l, b)
     {
        IBar_Icon *ic;

        ic = eina_hash_find(b->icon_hash, _desktop_name_get(exe->desktop));
        if (ic)
          {
             _ibar_icon_signal_emit(ic, "e,state,started", "e");
             if (!ic->exes) _ibar_icon_signal_emit(ic, "e,state,on", "e");
             if (skip) continue;
             if (!eina_list_data_find(ic->exes, exe))
               ic->exes = eina_list_append(ic->exes, exe);
             if (ic->menu)
               {
                  /* adding will fail if client hasn't been shown yet */
                  ic->menu_pending = eina_list_append(ic->menu_pending, ec);
                  evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_SHOW, _ibar_exec_new_client_show, ic);
               }
          }
        else if (!b->inst->ci->dont_add_nonorder)
          {
             if (skip) continue;
             _ibar_sep_create(b);
             ic = _ibar_icon_notinorder_new(b, exe);
             _ibar_resize_handle(b);
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static void
ibar_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   e_comp_object_effect_mover_del(inst->iconify_provider);
   ibar_config->instances = eina_list_remove(ibar_config->instances, inst);
   e_drop_handler_del(inst->drop_handler);
   _ibar_free(inst->ibar);
   free(inst);
}

EINTERN Evas_Object *
ibar_create(Evas_Object *parent, int *id, Z_Gadget_Site_Orient orient)
{
   IBar *b;
   Instance *inst;
   Evas_Coord x, y, w, h;
   const char *drop[] = { "enlightenment/desktop", "enlightenment/border", "text/uri-list" };
   Config_Item *ci;

   inst = E_NEW(Instance, 1);

   ci = _ibar_config_item_get(id);
   inst->ci = ci;
   if (!ci->dir) ci->dir = eina_stringshare_add("default");
   b = _ibar_new(parent, inst);
   evas_object_data_set(b->o_outerbox, "ibar", inst);
   //e_gadcon_client_autoscroll_toggle_disabled_set(gcc, !ci->dont_add_nonorder);

   inst->orient = orient;
   _ibar_orient_set(inst->ibar, orient == Z_GADGET_SITE_ORIENT_HORIZONTAL);

   evas_object_geometry_get(b->o_box, &x, &y, &w, &h);
   //inst->drop_handler =
     //e_drop_handler_add(E_OBJECT(inst->gcc), NULL, inst,
                        //_ibar_inst_cb_enter, _ibar_inst_cb_move,
                        //_ibar_inst_cb_leave, _ibar_inst_cb_drop,
                        //drop, 3, x, y, w, h);
   evas_object_event_callback_add(b->o_outerbox, EVAS_CALLBACK_DEL,
                                  ibar_del, inst);
   evas_object_event_callback_add(b->o_outerbox, EVAS_CALLBACK_RESIZE,
                                  _ibar_cb_obj_moveresize, inst);
   evas_object_event_callback_add(b->o_outerbox, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                  _ibar_cb_obj_hints, inst);
   ibar_config->instances = eina_list_append(ibar_config->instances, inst);
   inst->iconify_provider = e_comp_object_effect_mover_add(80, "e,action,*iconify", _ibar_cb_iconify_provider, inst);
   return b->o_outerbox;
}

EINTERN void
ibar_init(void)
{
   conf_item_edd = E_CONFIG_DD_NEW("IBar_Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, UINT);
   E_CONFIG_VAL(D, T, dir, STR);
   E_CONFIG_VAL(D, T, show_label, INT);
   E_CONFIG_VAL(D, T, eap_label, INT);
   E_CONFIG_VAL(D, T, lock_move, INT);
   E_CONFIG_VAL(D, T, dont_add_nonorder, INT);
   E_CONFIG_VAL(D, T, dont_track_launch, UCHAR);
   E_CONFIG_VAL(D, T, dont_icon_menu_mouseover, UCHAR);
   
   conf_edd = E_CONFIG_DD_NEW("IBar_Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_LIST(D, T, items, conf_item_edd);

   ibar_config = e_config_domain_load("module.ibar", conf_edd);

   if (!ibar_config)
     {
        Config_Item *ci;

        ibar_config = E_NEW(Config, 1);

        ci = E_NEW(Config_Item, 1);
        ci->dir = eina_stringshare_add("default");
        ci->show_label = 1;
        ci->eap_label = 0;
        ci->lock_move = 0;
        ci->dont_add_nonorder = 0;
        ci->dont_track_launch = 0;
        ci->dont_icon_menu_mouseover = 0;
        ibar_config->items = eina_list_append(ibar_config->items, ci);
     }

   E_LIST_HANDLER_APPEND(ibar_config->handlers, E_EVENT_CONFIG_ICON_THEME,
                         _ibar_cb_config_icons, NULL);
   E_LIST_HANDLER_APPEND(ibar_config->handlers, EFREET_EVENT_ICON_CACHE_UPDATE,
                         _ibar_cb_config_icons, NULL);
   E_LIST_HANDLER_APPEND(ibar_config->handlers, E_EVENT_EXEC_NEW,
                         _ibar_cb_exec_new, NULL);
   E_LIST_HANDLER_APPEND(ibar_config->handlers, E_EVENT_EXEC_NEW_CLIENT,
                         _ibar_cb_exec_new_client, NULL);
   E_LIST_HANDLER_APPEND(ibar_config->handlers, E_EVENT_EXEC_DEL,
                         _ibar_cb_exec_del, NULL);
   E_LIST_HANDLER_APPEND(ibar_config->handlers, E_EVENT_CLIENT_PROPERTY,
                         _ibar_cb_client_prop, NULL);

   ibar_orders = eina_hash_string_superfast_new(NULL);
   
   act_ibar_focus = e_action_add("ibar_focus");
   if (act_ibar_focus)
     {
        act_ibar_focus->func.go_key = _ibar_cb_action_focus;
        e_action_predef_name_set(N_("IBar"), N_("Focus IBar"),
                                 "ibar_focus", "<none>", NULL, 0);
     }

   z_gadget_type_add("IBar", ibar_create);
}

#if 0
/* module setup */
E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION, "IBar"
};

E_API void *
e_modapi_init(E_Module *m)
{
   conf_item_edd = E_CONFIG_DD_NEW("IBar_Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, STR);
   E_CONFIG_VAL(D, T, dir, STR);
   E_CONFIG_VAL(D, T, show_label, INT);
   E_CONFIG_VAL(D, T, eap_label, INT);
   E_CONFIG_VAL(D, T, lock_move, INT);
   E_CONFIG_VAL(D, T, dont_add_nonorder, INT);
   E_CONFIG_VAL(D, T, dont_track_launch, UCHAR);
   E_CONFIG_VAL(D, T, dont_icon_menu_mouseover, UCHAR);
   
   conf_edd = E_CONFIG_DD_NEW("IBar_Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_LIST(D, T, items, conf_item_edd);

   ibar_config = e_config_domain_load("module.ibar", conf_edd);

   if (!ibar_config)
     {
        Config_Item *ci;

        ibar_config = E_NEW(Config, 1);

        ci = E_NEW(Config_Item, 1);
        ci->id = eina_stringshare_add("ibar.1");
        ci->dir = eina_stringshare_add("default");
        ci->show_label = 1;
        ci->eap_label = 0;
        ci->lock_move = 0;
        ci->dont_add_nonorder = 0;
        ci->dont_track_launch = 0;
        ci->dont_icon_menu_mouseover = 0;
        ibar_config->items = eina_list_append(ibar_config->items, ci);
     }

   ibar_config->module = m;

   E_LIST_HANDLER_APPEND(ibar_config->handlers, E_EVENT_CONFIG_ICON_THEME,
                         _ibar_cb_config_icons, NULL);
   E_LIST_HANDLER_APPEND(ibar_config->handlers, EFREET_EVENT_ICON_CACHE_UPDATE,
                         _ibar_cb_config_icons, NULL);
   E_LIST_HANDLER_APPEND(ibar_config->handlers, E_EVENT_EXEC_NEW,
                         _ibar_cb_exec_new, NULL);
   E_LIST_HANDLER_APPEND(ibar_config->handlers, E_EVENT_EXEC_NEW_CLIENT,
                         _ibar_cb_exec_new_client, NULL);
   E_LIST_HANDLER_APPEND(ibar_config->handlers, E_EVENT_EXEC_DEL,
                         _ibar_cb_exec_del, NULL);
   E_LIST_HANDLER_APPEND(ibar_config->handlers, E_EVENT_CLIENT_PROPERTY,
                         _ibar_cb_client_prop, NULL);

   e_gadcon_provider_register(&_gadcon_class);
   ibar_orders = eina_hash_string_superfast_new(NULL);
   
   act_ibar_focus = e_action_add("ibar_focus");
   if (act_ibar_focus)
     {
        act_ibar_focus->func.go_key = _ibar_cb_action_focus;
        e_action_predef_name_set(N_("IBar"), N_("Focus IBar"),
                                 "ibar_focus", "<none>", NULL, 0);
     }
   
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   Ecore_Event_Handler *eh;
   Config_Item *ci;

   _ibar_go_unfocus();
   
   e_action_del("ibar_focus");
   e_action_predef_name_del("IBar", "Focus IBar");
   act_ibar_focus = NULL;
   
   e_gadcon_provider_unregister(&_gadcon_class);

   if (ibar_config->config_dialog)
     e_object_del(E_OBJECT(ibar_config->config_dialog));

   EINA_LIST_FREE(ibar_config->handlers, eh)
     ecore_event_handler_del(eh);

   EINA_LIST_FREE(ibar_config->items, ci)
     {
        if (ci->id) eina_stringshare_del(ci->id);
        if (ci->dir) eina_stringshare_del(ci->dir);
        E_FREE(ci);
     }
   E_FREE(ibar_config);
   ibar_config = NULL;
   eina_hash_free(ibar_orders);
   ibar_orders = NULL;
   E_CONFIG_DD_FREE(conf_item_edd);
   E_CONFIG_DD_FREE(conf_edd);
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.ibar", conf_edd, ibar_config);
   return 1;
}

#endif