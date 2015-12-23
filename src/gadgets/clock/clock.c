#include "e.h"
#include "clock.h"

/* actual module specifics */
typedef struct _Instance Instance;

struct _Instance
{
   E_Gadcon_Client *gcc;
   Evas_Object     *o_clock, *o_table, *o_popclock, *o_cal;
   E_Gadcon_Popup  *popup;

   int              madj;

   char             year[8];
   char             month[64];
   const char      *daynames[7];
   unsigned char    daynums[7][6];
   Eina_Bool        dayweekends[7][6];
   Eina_Bool        dayvalids[7][6];
   Eina_Bool        daytoday[7][6];
   Config_Item     *cfg;
};

/* gadcon requirements */
static E_Gadcon_Client *_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style);
static void             _gc_shutdown(E_Gadcon_Client *gcc);
static void             _gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient);
static const char      *_gc_label(const E_Gadcon_Client_Class *client_class);
static Evas_Object     *_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas);
static const char      *_gc_id_new(const E_Gadcon_Client_Class *client_class);
static Config_Item     *_conf_item_get(const char *id);
static void             _clock_popup_free(Instance *inst);

Config *clock_config = NULL;

static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;
static Eina_List *clock_instances = NULL;
static E_Action *act = NULL;

/* and actually define the gadcon class that this module provides (just 1) */
static const E_Gadcon_Client_Class _gadcon_class =
{
   GADCON_CLIENT_CLASS_VERSION,
   "clock",
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL, NULL
   },
   E_GADCON_CLIENT_STYLE_PLAIN
};


static void
_clock_month_update(Instance *inst)
{
   Evas_Object *od, *oi;
   int x, y;

   oi = elm_layout_edje_get(inst->o_cal);
   edje_object_part_text_set(oi, "e.text.month", inst->month);
   edje_object_part_text_set(oi, "e.text.year", inst->year);
   for (x = 0; x < 7; x++)
     {
        od = edje_object_part_table_child_get(oi, "e.table.daynames", x, 0);
        edje_object_part_text_set(od, "e.text.label", inst->daynames[x]);
        edje_object_message_signal_process(od);
        if (inst->dayweekends[x][0])
          edje_object_signal_emit(od, "e,state,weekend", "e");
        else
          edje_object_signal_emit(od, "e,state,weekday", "e");
     }

   for (y = 0; y < 6; y++)
     {
        for (x = 0; x < 7; x++)
          {
             char buf[32];

             od = edje_object_part_table_child_get(oi, "e.table.days", x, y);
             snprintf(buf, sizeof(buf), "%i", (int)inst->daynums[x][y]);
             edje_object_part_text_set(od, "e.text.label", buf);
             if (inst->dayweekends[x][y])
               edje_object_signal_emit(od, "e,state,weekend", "e");
             else
               edje_object_signal_emit(od, "e,state,weekday", "e");
             if (inst->dayvalids[x][y])
               edje_object_signal_emit(od, "e,state,visible", "e");
             else
               edje_object_signal_emit(od, "e,state,hidden", "e");
             if (inst->daytoday[x][y])
               edje_object_signal_emit(od, "e,state,today", "e");
             else
               edje_object_signal_emit(od, "e,state,someday", "e");
             edje_object_message_signal_process(od);
          }
     }
   edje_object_message_signal_process(oi);
}

static void
_clock_month_prev_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Instance *inst = data;
   inst->madj--;
   _time_eval(inst);
   _clock_month_update(inst);
}

static void
_clock_month_next_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Instance *inst = data;
   inst->madj++;
   _time_eval(inst);
   _clock_month_update(inst);
}

static void
_clock_settings_cb(void *d1, void *d2 EINA_UNUSED)
{
   Instance *inst = d1;
   e_int_config_clock_module(NULL, inst->cfg);
   e_object_del(E_OBJECT(inst->popup));
   inst->popup = NULL;
   inst->o_popclock = NULL;
}

static void
_popclock_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *info EINA_UNUSED)
{
   Instance *inst = data;
   if (inst->o_popclock == obj)
     {
        inst->o_popclock = NULL;
     }
}

static void
_clock_popup_new(Instance *inst)
{
   Evas *evas;
   Evas_Object *o, *oi;
   char todaystr[128];

   if (inst->popup) return;

   _todaystr_eval(inst, todaystr, sizeof(todaystr) - 1);

   inst->madj = 0;

   _time_eval(inst);

   inst->popup = e_gadcon_popup_new(inst->gcc, 0);
   evas = e_comp->evas;

   inst->o_table = elm_table_add(e_comp->elm);

   oi = elm_layout_add(inst->o_table);
   inst->o_popclock = oi;
   evas_object_size_hint_weight_set(oi, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(oi, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(oi, EVAS_CALLBACK_DEL, _popclock_del_cb, inst);

   if (inst->cfg->digital_clock)
     e_theme_edje_object_set(oi, "base/theme/modules/clock",
                             "e/modules/clock/digital");
   else
     e_theme_edje_object_set(oi, "base/theme/modules/clock",
                             "e/modules/clock/main");
   if (inst->cfg->show_date)
     elm_object_signal_emit(oi, "e,state,date,on", "e");
   else
     elm_object_signal_emit(oi, "e,state,date,off", "e");
   if (inst->cfg->digital_24h)
     elm_object_signal_emit(oi, "e,state,24h,on", "e");
   else
     elm_object_signal_emit(oi, "e,state,24h,off", "e");
   if (inst->cfg->show_seconds)
     elm_object_signal_emit(oi, "e,state,seconds,on", "e");
   else
     elm_object_signal_emit(oi, "e,state,seconds,off", "e");

   elm_object_part_text_set(oi, "e.text.today", todaystr);

   elm_layout_sizing_eval(oi);
   elm_table_pack(inst->o_table, oi, 0, 0, 1, 1);
   evas_object_show(oi);

   o = evas_object_rectangle_add(evas);
   evas_object_size_hint_min_set(o, 80 * e_scale, 80 * e_scale);
   elm_table_pack(inst->o_table, o, 0, 0, 1, 1);

   o = e_widget_button_add(evas, _("Settings"), "preferences-system",
                           _clock_settings_cb, inst, NULL);
   elm_table_pack(inst->o_table, o, 0, 2, 1, 1);
   evas_object_show(o);

   oi = elm_layout_add(inst->o_table);
   inst->o_cal = oi;
   e_theme_edje_object_set(oi, "base/theme/modules/clock",
                           "e/modules/clock/calendar");
   _clock_month_update(inst);

   elm_object_signal_callback_add(oi, "e,action,prev", "*",
                                   _clock_month_prev_cb, inst);
   elm_object_signal_callback_add(oi, "e,action,next", "*",
                                   _clock_month_next_cb, inst);
   edje_object_message_signal_process(elm_layout_edje_get(oi));
   elm_layout_sizing_eval(oi);
   elm_table_pack(inst->o_table, oi, 0, 1, 1, 1);
   evas_object_show(oi);

   evas_smart_objects_calculate(evas);
   e_gadcon_popup_content_set(inst->popup, inst->o_table);
   e_gadcon_popup_show(inst->popup);
}

static void
_eval_instance_size(Instance *inst)
{
   Evas_Coord mw, mh, omw, omh;

   edje_object_size_min_get(inst->o_clock, &mw, &mh);
   omw = mw;
   omh = mh;

   if ((mw < 1) || (mh < 1))
     {
        Evas_Coord x, y, sw = 0, sh = 0, ow, oh;
        Eina_Bool horiz;
        const char *orient;

        switch (inst->gcc->gadcon->orient)
          {
           case E_GADCON_ORIENT_TOP:
           case E_GADCON_ORIENT_CORNER_TL:
           case E_GADCON_ORIENT_CORNER_TR:
           case E_GADCON_ORIENT_BOTTOM:
           case E_GADCON_ORIENT_CORNER_BL:
           case E_GADCON_ORIENT_CORNER_BR:
           case E_GADCON_ORIENT_HORIZ:
             horiz = EINA_TRUE;
             orient = "e,state,horizontal";
             break;

           case E_GADCON_ORIENT_LEFT:
           case E_GADCON_ORIENT_CORNER_LB:
           case E_GADCON_ORIENT_CORNER_LT:
           case E_GADCON_ORIENT_RIGHT:
           case E_GADCON_ORIENT_CORNER_RB:
           case E_GADCON_ORIENT_CORNER_RT:
           case E_GADCON_ORIENT_VERT:
             horiz = EINA_FALSE;
             orient = "e,state,vertical";
             break;

           default:
             horiz = EINA_TRUE;
             orient = "e,state,float";
          }

        if (inst->gcc->gadcon->shelf)
          {
             if (horiz)
               sh = inst->gcc->gadcon->shelf->h;
             else
               sw = inst->gcc->gadcon->shelf->w;
          }

        evas_object_geometry_get(inst->o_clock, NULL, NULL, &ow, &oh);
        if (orient)
          edje_object_signal_emit(inst->o_clock, orient, "e");
        evas_object_resize(inst->o_clock, sw, sh);
        edje_object_message_signal_process(inst->o_clock);

        edje_object_parts_extends_calc(inst->o_clock, &x, &y, &mw, &mh);
        evas_object_resize(inst->o_clock, ow, oh);
     }

   if (mw < 4) mw = 4;
   if (mh < 4) mh = 4;

   if (mw < omw) mw = omw;
   if (mh < omh) mh = omh;

   e_gadcon_client_aspect_set(inst->gcc, mw, mh);
   e_gadcon_client_min_size_set(inst->gcc, mw, mh);
}

void
e_int_clock_instances_redo(Eina_Bool all)
{
   Eina_List *l;
   Instance *inst;
   char todaystr[128];

   EINA_LIST_FOREACH(clock_instances, l, inst)
     {
        Evas_Object *o = inst->o_clock;

         if ((!all) && (!inst->cfg->changed)) continue;
        _todaystr_eval(inst, todaystr, sizeof(todaystr) - 1);
        if (inst->cfg->digital_clock)
          e_theme_edje_object_set(o, "base/theme/modules/clock",
                                  "e/modules/clock/digital");
        else
          e_theme_edje_object_set(o, "base/theme/modules/clock",
                                  "e/modules/clock/main");
        if (inst->cfg->show_date)
          edje_object_signal_emit(o, "e,state,date,on", "e");
        else
          edje_object_signal_emit(o, "e,state,date,off", "e");
        if (inst->cfg->digital_24h)
          edje_object_signal_emit(o, "e,state,24h,on", "e");
        else
          edje_object_signal_emit(o, "e,state,24h,off", "e");
        if (inst->cfg->show_seconds)
          edje_object_signal_emit(o, "e,state,seconds,on", "e");
        else
          edje_object_signal_emit(o, "e,state,seconds,off", "e");

        edje_object_part_text_set(o, "e.text.today", todaystr);
        edje_object_message_signal_process(o);
        _eval_instance_size(inst);
        
        if (inst->o_popclock)
          {
             o = inst->o_popclock;

             if (inst->cfg->digital_clock)
               e_theme_edje_object_set(o, "base/theme/modules/clock",
                                       "e/modules/clock/digital");
             else
               e_theme_edje_object_set(o, "base/theme/modules/clock",
                                       "e/modules/clock/main");
             if (inst->cfg->show_date)
               edje_object_signal_emit(o, "e,state,date,on", "e");
             else
               edje_object_signal_emit(o, "e,state,date,off", "e");
             if (inst->cfg->digital_24h)
               edje_object_signal_emit(o, "e,state,24h,on", "e");
             else
               edje_object_signal_emit(o, "e,state,24h,off", "e");
             if (inst->cfg->show_seconds)
               edje_object_signal_emit(o, "e,state,seconds,on", "e");
             else
               edje_object_signal_emit(o, "e,state,seconds,off", "e");

             edje_object_part_text_set(o, "e.text.today", todaystr);
             edje_object_message_signal_process(o);
          }
     }
}


static void
_clock_popup_free(Instance *inst)
{
   if (!inst->popup) return;
   E_FREE_FUNC(inst->popup, e_object_del);
   inst->o_popclock = NULL;
}

static void
_clock_menu_cb_cfg(void *data, E_Menu *menu EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Instance *inst = data;

   E_FREE_FUNC(inst->popup, e_object_del);
   e_int_config_clock_module(NULL, inst->cfg);
}

static void
_clock_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event;

   if (ev->button == 1)
     {
        if (inst->popup) _clock_popup_free(inst);
        else _clock_popup_new(inst);
     }
   else if (ev->button == 3)
     {
        E_Zone *zone;
        E_Menu *m;
        E_Menu_Item *mi;
        int x, y;

        zone = e_zone_current_get();

        m = e_menu_new();

        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, _("Settings"));
        e_util_menu_item_theme_icon_set(mi, "configure");
        e_menu_item_callback_set(mi, _clock_menu_cb_cfg, inst);

        m = e_gadcon_client_util_menu_items_append(inst->gcc, m, 0);

        e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon, &x, &y, NULL, NULL);
        e_menu_activate_mouse(m, zone, x + ev->output.x, y + ev->output.y,
                              1, 1, E_MENU_POP_DIRECTION_AUTO, ev->timestamp);
        evas_event_feed_mouse_up(inst->gcc->gadcon->evas, ev->button,
                                 EVAS_BUTTON_NONE, ev->timestamp, NULL);
     }
}

static void
_clock_sizing_changed_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   _eval_instance_size(data);
}

static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Evas_Object *o;
   E_Gadcon_Client *gcc;
   Instance *inst;
   char todaystr[128];

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id);

   _todaystr_eval(inst, todaystr, sizeof(todaystr) - 1);
   
   o = edje_object_add(gc->evas);
   edje_object_signal_callback_add(o, "e,state,sizing,changed", "*",
                                   _clock_sizing_changed_cb, inst);
   if (inst->cfg->digital_clock)
     e_theme_edje_object_set(o, "base/theme/modules/clock",
                             "e/modules/clock/digital");
   else
     e_theme_edje_object_set(o, "base/theme/modules/clock",
                             "e/modules/clock/main");
   if (inst->cfg->show_date)
     edje_object_signal_emit(o, "e,state,date,on", "e");
   else
     edje_object_signal_emit(o, "e,state,date,off", "e");
   if (inst->cfg->digital_24h)
     edje_object_signal_emit(o, "e,state,24h,on", "e");
   else
     edje_object_signal_emit(o, "e,state,24h,off", "e");
   if (inst->cfg->show_seconds)
     edje_object_signal_emit(o, "e,state,seconds,on", "e");
   else
     edje_object_signal_emit(o, "e,state,seconds,off", "e");

   edje_object_part_text_set(o, "e.text.today", todaystr);
   edje_object_message_signal_process(o);
   evas_object_show(o);

   gcc = e_gadcon_client_new(gc, name, id, style, o);
   gcc->data = inst;

   inst->gcc = gcc;
   inst->o_clock = o;

   evas_object_event_callback_add(inst->o_clock,
                                  EVAS_CALLBACK_MOUSE_DOWN,
                                  _clock_cb_mouse_down,
                                  inst);

   clock_instances = eina_list_append(clock_instances, inst);

   

   return gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst;

   inst = gcc->data;
   clock_instances = eina_list_remove(clock_instances, inst);
   evas_object_del(inst->o_clock);
   _clock_popup_free(inst);
   _clear_timestrs(inst);
   free(inst);

   if ((!clock_instances) && (update_today))
     {
        ecore_timer_del(update_today);
        update_today = NULL;
     }
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient EINA_UNUSED)
{
   _eval_instance_size(gcc->data);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return _("Clock");
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   Evas_Object *o;
   char buf[4096];

   o = edje_object_add(evas);
   snprintf(buf, sizeof(buf), "%s/e-module-clock.edj",
            e_module_dir_get(clock_config->module));
   edje_object_file_set(o, buf, "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   Config_Item *ci = NULL;

   ci = _conf_item_get(NULL);
   return ci->id;
}

static Config_Item *
_conf_item_get(const char *id)
{
   Config_Item *ci;

   GADCON_CLIENT_CONFIG_GET(Config_Item, clock_config->items, _gadcon_class, id);

   ci = E_NEW(Config_Item, 1);
   ci->id = eina_stringshare_add(id);
   ci->weekend.start = 6;
   ci->weekend.len = 2;
   ci->week.start = 1;
   ci->digital_clock = 1;
   ci->digital_24h = 0;
   ci->show_seconds = 0;
   ci->show_date = 0;

   clock_config->items = eina_list_append(clock_config->items, ci);
   e_config_save_queue();

   return ci;
}

EINTERN Evas_Object *
clock_create(Evas_Object *parent, unsigned int *id)
{

}
