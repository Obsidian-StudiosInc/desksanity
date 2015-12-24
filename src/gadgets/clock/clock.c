#include "clock.h"

EINTERN Config *clock_config = NULL;
EINTERN Eina_List *clock_instances = NULL;

static void
_clock_calendar_month_update(Instance *inst)
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
   time_instance_update(inst);
   _clock_calendar_month_update(inst);
}

static void
_clock_month_next_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Instance *inst = data;
   inst->madj++;
   time_instance_update(inst);
   _clock_calendar_month_update(inst);
}

static void
_clock_settings_cb(void *d1, void *d2 EINA_UNUSED)
{
   Instance *inst = d1;
   //e_int_config_clock_module(NULL, inst->cfg);
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

EINTERN void
clock_popup_new(Instance *inst)
{
/*
   Evas *evas;
   Evas_Object *o, *oi;
   char todaystr[128];

   if (inst->popup) return;

   time_string_format(inst, todaystr, sizeof(todaystr) - 1);

   inst->madj = 0;

   time_instance_update(inst);

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
   _clock_calendar_month_update(inst);

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
*/
}

static void
_eval_instance_size(Instance *inst)
{
   Evas_Coord mw, mh;
   int sw = 0, sh = 0;

   edje_object_size_min_get(elm_layout_edje_get(inst->o_clock), &mw, &mh);

   if ((mw < 1) || (mh < 1))
     {
        Evas_Coord ow, oh;
        Evas_Object *owner;

        owner = z_gadget_site_get(inst->o_clock);
        switch (z_gadget_site_orient_get(owner))
          {
           case Z_GADGET_SITE_ORIENT_HORIZONTAL:
             evas_object_geometry_get(owner, NULL, NULL, NULL, &sh);
             break;

           case Z_GADGET_SITE_ORIENT_VERTICAL:
             evas_object_geometry_get(owner, NULL, NULL, &sw, NULL);
             break;

           default: break;
          }

        evas_object_geometry_get(inst->o_clock, NULL, NULL, &ow, &oh);
        evas_object_resize(inst->o_clock, sw, sh);
        edje_object_message_signal_process(elm_layout_edje_get(inst->o_clock));

        edje_object_parts_extends_calc(elm_layout_edje_get(inst->o_clock), NULL, NULL, &mw, &mh);
        evas_object_resize(inst->o_clock, ow, oh);
     }

   if (mw < 4) mw = 4;
   if (mh < 4) mh = 4;

   if (mw < sw) mw = sw;
   if (mh < sh) mh = sh;

   evas_object_size_hint_aspect_set(inst->o_clock, EVAS_ASPECT_CONTROL_BOTH, mw, mh);
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
        time_string_format(inst, todaystr, sizeof(todaystr) - 1);
        if (inst->cfg->digital_clock)
          e_theme_edje_object_set(o, "base/theme/modules/clock",
                                  "e/modules/clock/digital");
        else
          e_theme_edje_object_set(o, "base/theme/modules/clock",
                                  "e/modules/clock/main");
        if (inst->cfg->show_date)
          elm_layout_signal_emit(o, "e,state,date,on", "e");
        else
          elm_layout_signal_emit(o, "e,state,date,off", "e");
        if (inst->cfg->digital_24h)
          elm_layout_signal_emit(o, "e,state,24h,on", "e");
        else
          elm_layout_signal_emit(o, "e,state,24h,off", "e");
        if (inst->cfg->show_seconds)
          elm_layout_signal_emit(o, "e,state,seconds,on", "e");
        else
          elm_layout_signal_emit(o, "e,state,seconds,off", "e");

        elm_object_part_text_set(o, "e.text.today", todaystr);
        edje_object_message_signal_process(elm_layout_edje_get(o));
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


EINTERN void
clock_popup_free(Instance *inst)
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
   //e_int_config_clock_module(NULL, inst->cfg);
}

static void
_clock_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event;

   if (ev->button == 1)
     {
        if (inst->popup) clock_popup_free(inst);
        else clock_popup_new(inst);
     }
   else if (ev->button == 3)
     {
#warning FIXME
#if 0
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
#endif
     }
}

static void
_clock_sizing_changed_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   _eval_instance_size(data);
}

static void
clock_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   clock_instances = eina_list_remove(clock_instances, inst);
   clock_popup_free(inst);
   time_daynames_clear(inst);
   free(inst);
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

static Config_Item *
_conf_item_get(unsigned int *id)
{
   Config_Item *ci;
   Eina_List *l;

   if (*id)
     {
        EINA_LIST_FOREACH(clock_config->items, l, ci)
          if (*id == ci->id) return ci;
     }

   ci = E_NEW(Config_Item, 1);
   ci->id = clock_config->items ? eina_list_count(clock_config->items) : 1;
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

static void
_clock_gadget_added_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   _eval_instance_size(inst);
   z_gadget_configure_cb_set(inst->o_clock, config_clock);
   evas_object_smart_callback_del_full(obj, "gadget_added", _clock_gadget_added_cb, data);
}

EINTERN Evas_Object *
clock_create(Evas_Object *parent, unsigned int *id, Z_Gadget_Site_Orient orient)
{
   Evas_Object *o;
   Instance *inst;
   char todaystr[128];
   const char *sig = NULL;

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id);

   time_string_format(inst, todaystr, sizeof(todaystr) - 1);
   
   o = elm_layout_add(parent);
   elm_layout_signal_callback_add(o, "e,state,sizing,changed", "*",
                                   _clock_sizing_changed_cb, inst);
   if (inst->cfg->digital_clock)
     e_theme_edje_object_set(o, "base/theme/modules/clock",
                             "e/modules/clock/digital");
   else
     e_theme_edje_object_set(o, "base/theme/modules/clock",
                             "e/modules/clock/main");
   if (inst->cfg->show_date)
     elm_layout_signal_emit(o, "e,state,date,on", "e");
   else
     elm_layout_signal_emit(o, "e,state,date,off", "e");
   if (inst->cfg->digital_24h)
     elm_layout_signal_emit(o, "e,state,24h,on", "e");
   else
     elm_layout_signal_emit(o, "e,state,24h,off", "e");
   if (inst->cfg->show_seconds)
     elm_layout_signal_emit(o, "e,state,seconds,on", "e");
   else
     elm_layout_signal_emit(o, "e,state,seconds,off", "e");

   elm_object_part_text_set(o, "e.text.today", todaystr);

   switch (orient)
     {
      case Z_GADGET_SITE_ORIENT_HORIZONTAL:
        sig = "e,state,horizontal";
        break;

      case Z_GADGET_SITE_ORIENT_VERTICAL:
        sig = "e,state,vertical";
        break;

      default:
        sig = "e,state,float";
     }

   elm_layout_signal_emit(inst->o_clock, sig, "e");

   inst->o_clock = o;
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, clock_del, inst);
   evas_object_smart_callback_add(parent, "gadget_added", _clock_gadget_added_cb, inst);
   evas_object_data_set(o, "clock", inst);

   evas_object_event_callback_add(inst->o_clock,
                                  EVAS_CALLBACK_MOUSE_DOWN,
                                  _clock_cb_mouse_down,
                                  inst);

   clock_instances = eina_list_append(clock_instances, inst);

   return o;
}
