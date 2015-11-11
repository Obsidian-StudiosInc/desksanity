#include "e_mod_main.h"

#define MAX_COLS 4

#define DRAG_RESIST 10

typedef Eina_Bool (*Zoom_Filter_Cb)(const E_Client *, E_Zone *);

static Eina_List *zoom_objs = NULL;
static Eina_List *current = NULL;
static Eina_List *handlers = NULL;
static E_Action *act_zoom_desk = NULL;
static E_Action *act_zoom_desk_all = NULL;
static E_Action *act_zoom_zone = NULL;
static E_Action *act_zoom_zone_all = NULL;

static E_Action *cur_act = NULL;

static int zmw, zmh;

static Evas_Coord dx = -1;
static Evas_Coord dy = -1;
static Evas_Object *dm, *dm_drag;

static inline unsigned int
_cols_calc(unsigned int count)
{
   if (count < 3) return 1;
   if (count < 5) return 2;
   if (count < 10) return 3;
   return 4;
}

static inline void
_drag_reset(void)
{
   dx = dy = -1;
   dm = NULL;
   E_FREE_FUNC(dm_drag, evas_object_del);
}

static void
_edje_custom_setup(Evas_Object *obj, const E_Client *ec, int x, int y, int w, int h)
{
   Edje_Message_Int_Set *msg;

   msg = alloca(sizeof(Edje_Message_Int_Set) + ((4 - 1) * sizeof(int)));
   msg->count = 4;
   msg->val[0] = ec->client.x - x;
   msg->val[1] = ec->client.y - y;
   msg->val[2] = (ec->client.x + ec->client.w) - (x + w);
   msg->val[3] = (ec->client.y + ec->client.h) - (y + h);
   edje_object_message_send(obj, EDJE_MESSAGE_INT_SET, 0, msg);
   edje_object_message_signal_process(obj);
}

static void
_hid(void *data EINA_UNUSED, Evas_Object *obj, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   e_comp_shape_queue();
   evas_object_hide(obj);
   evas_object_del(obj);
}

static void
_zoom_hide(void)
{
   Evas_Object *zoom_obj;

   if (dm_drag)
     EINA_LIST_FREE(zoom_objs, zoom_obj)
       {
          evas_object_hide(zoom_obj);
          evas_object_del(zoom_obj);
       }
   else
     EINA_LIST_FREE(zoom_objs, zoom_obj)
       elm_layout_signal_emit(zoom_obj, "e,state,inactive", "e");
   E_FREE_LIST(handlers, ecore_event_handler_del);
   e_comp_ungrab_input(1, 1);
   e_comp_shape_queue();
   current = NULL;
   cur_act = NULL;
}


static void
_dismiss()
{
   _zoom_hide();
   _drag_reset();
}

static void
_client_mouse_down(E_Client *ec EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, Evas_Event_Mouse_Down *ev)
{
   dx = ev->output.x;
   dy = ev->output.y;
   dm = edje_object_part_swallow_get(obj, "e.swallow.client");
}

static void
_client_mouse_out(E_Client *ec EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *ev EINA_UNUSED)
{
   if (current)
     edje_object_signal_emit(evas_object_smart_parent_get(eina_list_data_get(current)), "e,state,inactive", "e");
}

static void
_client_mouse_in(E_Client *ec EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *ev EINA_UNUSED)
{
   Eina_List *l;
   Evas_Object *zoom_obj, *m;

   evas_object_raise(obj);
   m = edje_object_part_swallow_get(obj, "e.swallow.client");
   if (current && (eina_list_data_get(current) != m))
     edje_object_signal_emit(evas_object_smart_parent_get(eina_list_data_get(current)), "e,state,inactive", "e");
   zoom_obj = evas_object_data_get(obj, "__DSZOOMOBJ");
   l = evas_object_data_get(zoom_obj, "__DSCLIENTS");
   current = eina_list_data_find_list(l, m);
   edje_object_signal_emit(obj, "e,state,active", "e");
}

static void
_client_mouse_up(E_Client *ec, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, Evas_Event_Mouse_Up *ev)
{
   E_Zone *zone;
   E_Desk *desk;
   int x, y, w, h;

   if (!dm_drag)
     {
        _drag_reset();
        return;
     }
   zone = e_comp_zone_xy_get(ev->output.x, ev->output.y);
   desk = e_desk_current_get(zone);
   ec->hidden = 0;
   e_client_desk_set(ec, desk);
   e_client_activate(ec, 1);

   evas_object_geometry_get(elm_object_part_content_get(dm_drag, "e.swallow.client"), &x, &y, &w, &h);
   _edje_custom_setup(elm_layout_edje_get(dm_drag), ec, x, y, w, h);

   elm_layout_signal_emit(dm_drag, "e,drag,release", "e");
}

static Eina_Bool
_client_mouse_move(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Event_Mouse_Move *ev)
{
   int x, y, w, h;

   if (!dm) return ECORE_CALLBACK_RENEW;

   evas_object_geometry_get(dm, &x, &y, &w, &h);
   if (!dm_drag)
     {
        Evas_Object *m, *zoom_obj;
        Eina_List *l;

        /* no adjust, not X coords */
        if ((abs(ev->root.x - dx) < DRAG_RESIST) && (abs(ev->root.y - dy) < DRAG_RESIST)) return ECORE_CALLBACK_RENEW;
        dm_drag = elm_layout_add(e_comp->elm);
        evas_object_pass_events_set(dm_drag, 1);
        evas_object_size_hint_min_get(dm, &w, &h);
        e_theme_edje_object_set(dm_drag, NULL, "e/modules/desksanity/zoom/client/drag");
        elm_layout_signal_callback_add(dm_drag, "e,action,done", "e", _dismiss, NULL);
        evas_object_layer_set(dm_drag, E_LAYER_POPUP);
        evas_object_resize(dm_drag, w, h);
        m = e_comp_object_util_mirror_add(dm);
        e_comp_object_util_del_list_append(dm_drag, m);
        evas_object_size_hint_min_set(m, w, h);
        elm_object_part_content_set(dm_drag, "e.swallow.client", m);
        evas_object_show(dm_drag);

        EINA_LIST_FOREACH(zoom_objs, l, zoom_obj)
          {
             elm_layout_signal_emit(zoom_obj, "e,state,dragging", "e");
             if (e_comp_object_util_zone_get(zoom_obj) == e_zone_current_get())
               elm_layout_signal_emit(zoom_obj, "e,state,current", "e");
          }
     }
   evas_object_move(dm_drag,
     e_comp_canvas_x_root_adjust(ev->root.x) - (dx - x),
     e_comp_canvas_y_root_adjust(ev->root.y) - (dy - y));
   return ECORE_CALLBACK_RENEW;
}

static void
_client_activate(void *data, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   e_client_activate(data, 1);
   _zoom_hide();
}

static void
_zoomobj_pack_client(const E_Client *ec, const E_Zone *zone, Evas_Object *tb, Evas_Object *m, unsigned int id, unsigned int cols)
{
   int w, h;
   unsigned int c, r;
   Evas_Object *e;

   e = evas_object_smart_parent_get(m);
   if (ec->client.w > ec->client.h)
     {
        w = MIN((zone->w / cols) - zmw, ec->client.w);
        h = (ec->client.h * w) / ec->client.w;
     }
   else
     {
        h = MIN((zone->w / cols) - zmh, ec->client.h);
        w = (ec->client.w * h) / ec->client.h;
     }

   evas_object_size_hint_min_set(m, w, h);

   r = (id - 1) / cols;
   c = (id - 1) % cols;
   evas_object_size_hint_min_set(e, zmw + w, zmh + h);
   elm_table_pack(tb, e, c, r, 1, 1);
}

static void
_zoomobj_add_client(Evas_Object *zoom_obj, Eina_List *l, Evas_Object *m)
{
   E_Client *ec;
   Evas_Object *ic, *e;

   ec = evas_object_data_get(m, "E_Client");
   e = elm_layout_add(e_comp->elm);
   evas_object_data_set(elm_layout_edje_get(e), "__DSZOOMOBJ", zoom_obj);
   e_comp_object_util_del_list_append(zoom_obj, e);
   e_comp_object_util_del_list_append(zoom_obj, m);
   e_theme_edje_object_set(e, NULL, "e/modules/desksanity/zoom/client");
   evas_object_event_callback_add(elm_layout_edje_get(e), EVAS_CALLBACK_MOUSE_DOWN, (Evas_Object_Event_Cb)_client_mouse_down, ec);
   evas_object_event_callback_add(elm_layout_edje_get(e), EVAS_CALLBACK_MOUSE_UP, (Evas_Object_Event_Cb)_client_mouse_up, ec);
   evas_object_event_callback_add(elm_layout_edje_get(e), EVAS_CALLBACK_MOUSE_IN, (Evas_Object_Event_Cb)_client_mouse_in, ec);
   evas_object_event_callback_add(elm_layout_edje_get(e), EVAS_CALLBACK_MOUSE_OUT, (Evas_Object_Event_Cb)_client_mouse_out, ec);
   if ((!zmw) && (!zmh))
     edje_object_size_min_calc(elm_layout_edje_get(e), &zmw, &zmh);
   elm_layout_signal_callback_add(e, "e,action,activate", "e", _client_activate, ec);
   if (e_client_focused_get() == ec)
     {
        elm_layout_signal_emit(e, "e,state,focused", "e");
        current = l;
     }
   elm_object_part_content_set(e, "e.swallow.client", m);
   elm_object_part_text_set(e, "e.text.title", e_client_util_name_get(ec));
   if (ec->urgent)
     elm_layout_signal_emit(e, "e,state,urgent", "e");
   ic = e_client_icon_add(ec, e_comp->evas);
   if (ic)
     {
        elm_object_part_content_set(e, "e.swallow.icon", ic);
        e_comp_object_util_del_list_append(zoom_obj, ic);
     }
   evas_object_show(e);
}

static void
_zoomobj_position_client(Evas_Object *m)
{
   int x, y, w, h;
   E_Client *ec;
   Evas_Object *e;

   e = evas_object_smart_parent_get(m);
   ec = evas_object_data_get(m, "E_Client");
   evas_object_geometry_get(e, &x, &y, NULL, NULL);
   evas_object_size_hint_min_get(e, &w, &h);
   _edje_custom_setup(e, ec, x, y, w, h);
   edje_object_signal_emit(e, "e,action,show", "e");
}

static Eina_Bool
_zoom_key(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Event_Key *ev)
{
   Eina_List *n = NULL;

   if (!e_util_strcmp(ev->key, "Escape"))
     _zoom_hide();
   else if (!e_util_strcmp(ev->key, "Left"))
     {
        if (current)
          n = eina_list_prev(current) ?: eina_list_last(current);
        else
          {
             n = evas_object_data_get(eina_list_nth(zoom_objs, e_zone_current_get()->num), "__DSCLIENTS");
             n = eina_list_last(n);
          }
     }
   else if (!e_util_strcmp(ev->key, "Right"))
     {
        if (current)
          {
             n = eina_list_next(current);
             if (!n)
               {
                  Eina_List *f;

                  for (f = n = current; f; n = f, f = eina_list_prev(f));
               }
          }
        else
          n = evas_object_data_get(eina_list_nth(zoom_objs, e_zone_current_get()->num), "__DSCLIENTS");
     }
   else if ((!strcmp(ev->key, "Return")) || (!strcmp(ev->key, "KP_Enter")))
     {
        e_client_activate(evas_object_data_get(eina_list_data_get(current), "E_Client"), 1);
        _zoom_hide();
        return ECORE_CALLBACK_DONE;
     }
   if (n)
     {
        Evas_Object *e, *scr;
        int x, y, w ,h;
        E_Zone *zone;

        e = evas_object_smart_parent_get(eina_list_data_get(n));
        edje_object_signal_emit(e, "e,state,active", "e");
        edje_object_signal_emit(evas_object_smart_parent_get(eina_list_data_get(current)), "e,state,inactive", "e");
        current = n;
        evas_object_geometry_get(e, &x, &y, &w, &h);
        scr = elm_object_part_content_get(evas_object_data_get(e, "__DSZOOMOBJ"), "e.swallow.layout");
        zone = e_comp_object_util_zone_get(scr);
        elm_scroller_region_show(scr, x - zone->x, y - zone->y, w, h);
     }
   return ECORE_CALLBACK_DONE;
}

static void
_relayout(Evas_Object *zoom_obj, Evas_Object *scr, Evas_Object *tb)
{
   Eina_List *l, *clients;
   Evas_Object *m;
   unsigned int id = 1;

   clients = evas_object_data_get(zoom_obj, "__DSCLIENTS");
   e_comp_object_util_del_list_remove(zoom_obj, tb);
   evas_object_del(tb);
   tb = elm_table_add(e_comp->elm);
   E_EXPAND(tb);
   E_FILL(tb);
   e_comp_object_util_del_list_append(zoom_obj, tb);
   elm_table_homogeneous_set(tb, 1);
   EINA_LIST_FOREACH(clients, l, m)
     _zoomobj_pack_client(evas_object_data_get(m, "E_Client"),
     e_comp_object_util_zone_get(zoom_obj), tb, m, id++,
     _cols_calc(eina_list_count(clients)));
   elm_object_content_set(scr, tb);
   evas_object_smart_need_recalculate_set(tb, 1);
   evas_object_smart_calculate(tb);
   E_LIST_FOREACH(clients, _zoomobj_position_client);
}

static void
_zoom_client_add_post(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Evas_Object *scr, *tb, *m;
   Eina_List *clients;
   unsigned int c, pc;
   E_Client *ec;

   ec = evas_object_data_get(obj, "E_Client");
   evas_object_event_callback_del(ec->frame, EVAS_CALLBACK_SHOW, _zoom_client_add_post);
   m = e_comp_object_util_mirror_add(ec->frame);
   if (!m) return;
   clients = evas_object_data_get(data, "__DSCLIENTS");
   clients = eina_list_append(clients, m);
   scr = elm_object_part_content_get(data, "e.swallow.layout");
   tb = elm_object_content_get(scr);
   c = _cols_calc(eina_list_count(clients));
   pc = _cols_calc(eina_list_count(clients) - 1);
   _zoomobj_add_client(data, eina_list_last(clients), m);
   if (c == pc)
     {
        _zoomobj_pack_client(ec, ec->zone, tb, m, eina_list_count(clients), c);
        _zoomobj_position_client(m);
     }
   else
     _relayout(data, scr, tb);
}

static Eina_Bool
_zoom_client_add(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Client *ev)
{
   Evas_Object *zoom_obj;
   Eina_List *l;

   if (e_client_util_ignored_get(ev->ec)) return ECORE_CALLBACK_RENEW;
   if (ev->ec->iconic && (!e_config->winlist_list_show_iconified)) return ECORE_CALLBACK_RENEW;
   if (((cur_act == act_zoom_zone) || (cur_act == act_zoom_desk)) &&
     (ev->ec->zone != e_zone_current_get())) return ECORE_CALLBACK_RENEW;
   if (((cur_act == act_zoom_desk) || (cur_act == act_zoom_desk_all)) &&
     (!ev->ec->desk->visible)) return ECORE_CALLBACK_RENEW;

   EINA_LIST_FOREACH(zoom_objs, l, zoom_obj)
     {
        if (e_comp_object_util_zone_get(zoom_obj) != ev->ec->zone) continue;

        evas_object_event_callback_add(ev->ec->frame, EVAS_CALLBACK_SHOW, _zoom_client_add_post, zoom_obj);
        break;
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_zoom_client_del(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Client *ev)
{
   Evas_Object *zoom_obj;
   Eina_List *l;

   if (e_client_util_ignored_get(ev->ec)) return ECORE_CALLBACK_RENEW;
   if (ev->ec->iconic && (!e_config->winlist_list_show_iconified)) return ECORE_CALLBACK_RENEW;

   EINA_LIST_FOREACH(zoom_objs, l, zoom_obj)
     {
        Eina_List *ll, *clients = evas_object_data_get(zoom_obj, "__DSCLIENTS");
        Evas_Object *m;

        EINA_LIST_FOREACH(clients, ll, m)
          {
             Evas_Object *e, *scr, *tb, *ic;

             if (evas_object_data_get(m, "E_Client") != ev->ec) continue;
             e = evas_object_smart_parent_get(m);
             e_comp_object_util_del_list_remove(zoom_obj, m);
             e_comp_object_util_del_list_remove(zoom_obj, e);
             ic = elm_object_part_content_get(e, "e.swallow.icon");
             e_comp_object_util_del_list_remove(zoom_obj, ic);
             evas_object_del(ic);
             evas_object_data_set(zoom_obj, "__DSCLIENTS", eina_list_remove_list(clients, ll));
             evas_object_del(ic);
             evas_object_del(e);
             evas_object_del(m);
             scr = elm_object_part_content_get(zoom_obj, "e.swallow.layout");
             tb = elm_object_content_get(scr);
             _relayout(zoom_obj, scr, tb);
             return ECORE_CALLBACK_RENEW;
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_zoom_client_property(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Client_Property *ev)
{
   Eina_List *l;
   Evas_Object *zoom_obj;

   if (!(ev->property & E_CLIENT_PROPERTY_URGENCY)) return ECORE_CALLBACK_RENEW;

   EINA_LIST_FOREACH(zoom_objs, l, zoom_obj)
     {
        Evas_Object *m;
        Eina_List *ll, *clients = evas_object_data_get(zoom_obj, "__DSCLIENTS");

        EINA_LIST_FOREACH(clients, ll, m)
          {
             if (evas_object_data_get(m, "E_Client") != ev->ec) continue;

             if (ev->ec->urgent)
               edje_object_signal_emit(evas_object_smart_parent_get(m), "e,state,urgent", "e");
             else
               edje_object_signal_emit(evas_object_smart_parent_get(m), "e,state,not_urgent", "e");
             return ECORE_CALLBACK_RENEW;
          }
     }

   return ECORE_CALLBACK_RENEW;
}

static void
_hiding(void *data EINA_UNUSED, Evas_Object *obj, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   Eina_List *clients = evas_object_data_get(obj, "__DSCLIENTS");
   Evas_Object *m, *e;

   EINA_LIST_FREE(clients, m)
     {
        e = evas_object_smart_parent_get(m);
        edje_object_signal_emit(e, "e,action,hide", "e");
     }
}

static void
zoom(Eina_List *clients, E_Zone *zone)
{
   Evas_Object *m, *bg_obj, *scr, *tb, *zoom_obj;
   unsigned int cols, id = 1;
   Eina_Stringshare *bgf;
   Eina_List *l;

   if (!zoom_objs)
     {
        e_comp_shape_queue();
        e_comp_grab_input(1, 1);
        E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_KEY_DOWN, _zoom_key, NULL);
        E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_PROPERTY, _zoom_client_property, NULL);
        E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_ADD, _zoom_client_add, NULL);
        E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_REMOVE, _zoom_client_del, NULL);
        E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_MOVE, _client_mouse_move, NULL);
     }

   zoom_obj = elm_layout_add(e_comp->elm);
   elm_layout_signal_callback_add(zoom_obj, "e,state,hiding", "e", _hiding, NULL);
   elm_layout_signal_callback_add(zoom_obj, "e,action,dismiss", "e", _dismiss, NULL);
   elm_layout_signal_callback_add(zoom_obj, "e,action,done", "e", _hid, NULL);
   evas_object_geometry_set(zoom_obj, zone->x, zone->y, zone->w, zone->h);
   evas_object_layer_set(zoom_obj, E_LAYER_POPUP);
   e_theme_edje_object_set(zoom_obj, NULL, "e/modules/desksanity/zoom/base");

   bg_obj = e_icon_add(e_comp->evas);
   bgf = e_bg_file_get(zone->num, zone->desk_x_current, zone->desk_y_current);
   if (eina_str_has_extension(bgf, ".edj"))
     e_icon_file_edje_set(bg_obj, bgf, "e/desktop/background");
   else
     e_icon_file_set(bg_obj, bgf);
   eina_stringshare_del(bgf);
   e_comp_object_util_del_list_append(zoom_obj, bg_obj);
   elm_object_part_content_set(zoom_obj, "e.swallow.background", bg_obj);

   scr = elm_scroller_add(zoom_obj);
   e_theme_edje_object_set(scr, NULL, "e/modules/desksanity/zoom/scrollframe");
   elm_object_part_content_set(zoom_obj, "e.swallow.layout", scr);

   tb = elm_table_add(scr);
   E_EXPAND(tb);
   E_FILL(tb);
   elm_table_homogeneous_set(tb, 1);
   evas_object_show(tb);

   evas_object_show(zoom_obj);

   cols = _cols_calc(eina_list_count(clients));

   EINA_LIST_FOREACH(clients, l, m)
     {
        _zoomobj_add_client(zoom_obj, l, m);
        _zoomobj_pack_client(evas_object_data_get(m, "E_Client"), zone, tb, m, id++, cols);
     }

   elm_object_content_set(scr, tb);
   evas_object_smart_need_recalculate_set(tb, 1);
   evas_object_smart_calculate(tb);
   elm_layout_signal_emit(zoom_obj, "e,state,active", "e");

   E_LIST_FOREACH(clients, _zoomobj_position_client);
   evas_object_data_set(zoom_obj, "__DSCLIENTS", clients);

   zoom_objs = eina_list_append(zoom_objs, zoom_obj);
}

static Eina_Bool
_filter_desk(const E_Client *ec, E_Zone *zone)
{
   return e_client_util_desk_visible(ec, e_desk_current_get(zone));
}

static Eina_Bool
_filter_desk_all(const E_Client *ec, E_Zone *zone)
{
   return ec->desk == e_desk_current_get(zone);
}

static Eina_Bool
_filter_zone(const E_Client *ec, E_Zone *zone)
{
   return ec->zone == zone;
}

static void
_zoom_begin(Zoom_Filter_Cb cb, E_Zone *zone)
{
   Eina_List *clients = NULL, *l;
   Evas_Object *m;
   E_Client *ec;

   EINA_LIST_FOREACH(e_client_focus_stack_get(), l, ec)
     {
        if (e_client_util_ignored_get(ec)) continue;
        if (ec->iconic && (!e_config->winlist_list_show_iconified)) continue;
        if (!cb(ec, zone)) continue;

        m = e_comp_object_util_mirror_add(ec->frame);
        if (!m) continue;
        clients = eina_list_append(clients, m);
     }
   zoom(clients, zone);
}

#define ZOOM_CHECK \
   if (zoom_objs) \
     { \
        _zoom_hide(); \
        return; \
     }

static void
_zoom_desk_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   ZOOM_CHECK;
   cur_act = act_zoom_desk;
   _zoom_begin(_filter_desk, e_zone_current_get());
}

static void
_zoom_desk_all_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   E_Zone *zone;
   Eina_List *l;

   ZOOM_CHECK;
   cur_act = act_zoom_desk_all;
   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     _zoom_begin(_filter_desk_all, zone);
}

static void
_zoom_zone_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   ZOOM_CHECK;
   cur_act = act_zoom_zone;
   _zoom_begin(_filter_zone, e_zone_current_get());
}

static void
_zoom_zone_all_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   E_Zone *zone;
   Eina_List *l;

   ZOOM_CHECK;
   cur_act = act_zoom_zone_all;
   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     _zoom_begin(_filter_zone, zone);
}

EINTERN void
zoom_init(void)
{
   act_zoom_desk = e_action_add("zoom_desk");
   if (act_zoom_desk)
     {
        act_zoom_desk->func.go = _zoom_desk_cb;
        e_action_predef_name_set(D_("Compositor"), D_("Toggle zoom current desk"),
                                 "zoom_desk", NULL, NULL, 0);
     }
   act_zoom_desk_all = e_action_add("zoom_desk_all");
   if (act_zoom_desk_all)
     {
        act_zoom_desk_all->func.go = _zoom_desk_all_cb;
        e_action_predef_name_set(D_("Compositor"), D_("Toggle zoom current desks"),
                                 "zoom_desk_all", NULL, NULL, 0);
     }
   act_zoom_zone = e_action_add("zoom_zone");
   if (act_zoom_zone)
     {
        act_zoom_zone->func.go = _zoom_zone_cb;
        e_action_predef_name_set(D_("Compositor"), D_("Toggle zoom current screen"),
                                 "zoom_zone", NULL, NULL, 0);
     }
   act_zoom_zone_all = e_action_add("zoom_zone_all");
   if (act_zoom_zone_all)
     {
        act_zoom_zone_all->func.go = _zoom_zone_all_cb;
        e_action_predef_name_set(D_("Compositor"), D_("Toggle zoom all screens"),
                                 "zoom_zone_all", NULL, NULL, 0);
     }
}

EINTERN void
zoom_shutdown(void)
{
   e_action_predef_name_del(D_("Compositor"), D_("Toggle zoom current desk"));
   e_action_del("zoom_desk");
   act_zoom_desk = NULL;
   e_action_predef_name_del(D_("Compositor"), D_("Toggle zoom current desks"));
   e_action_del("zoom_desk_all");
   act_zoom_desk_all = NULL;
   e_action_predef_name_del(D_("Compositor"), D_("Toggle zoom current screen"));
   e_action_del("zoom_zone");
   act_zoom_zone = NULL;
   e_action_predef_name_del(D_("Compositor"), D_("Toggle zoom all screens"));
   e_action_del("zoom_zone_all");
   act_zoom_zone_all = NULL;
}
