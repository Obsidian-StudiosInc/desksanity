#include "e_mod_main.h"
#include <Efl_Wl.h>

typedef enum
{
   EXIT_MODE_RESTART,
   EXIT_MODE_DELETE,
} Exit_Mode;

typedef struct Config_Item
{
   int id;
   int exit_mode;
   Eina_Bool allow_events;
   Eina_Stringshare *cmd;
   void *inst;
   Eina_Bool cmd_changed : 1;
} Config_Item;

typedef struct Instance
{
   Evas_Object *obj;
   Ecore_Exe *exe;
   Config_Item *ci;
} Instance;

typedef struct RConfig
{
   Eina_List *items;
   Evas_Object *config_dialog;
} RConfig;

static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;

static RConfig *rconfig;
static Eina_List *instances;

static Ecore_Event_Handler *exit_handler;

typedef struct Wizard_Item
{
   E_Gadget_Wizard_End_Cb cb;
   void *data;
   int id;
} Wizard_Item;

static void
_config_close(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;
   Instance *inst = ci->inst;

   e_comp_ungrab_input(1, 1);
   rconfig->config_dialog = NULL;
   if (ci->cmd_changed)
     {
        char *cmd;

        cmd = elm_entry_markup_to_utf8(elm_entry_entry_get(evas_object_data_get(obj, "entry")));
        eina_stringshare_replace(&ci->cmd, cmd);
        free(cmd);
        e_config_save_queue();
     }
   if (!inst) ci->cmd_changed = 0;
   if (!ci->cmd_changed) return;
   ci->cmd_changed = 0;
   if (inst->exe) ecore_exe_quit(inst->exe);
   inst->exe = efl_wl_run(inst->obj, inst->ci->cmd);
}

static void
_config_label_add(Evas_Object *tb, const char *txt, int row)
{
   Evas_Object *o;

   o = elm_label_add(tb);
   E_ALIGN(o, 0, 0.5);
   elm_object_text_set(o, txt);
   evas_object_show(o);
   elm_table_pack(tb, o, 0, row, 1, 1);
}

static void
_config_events_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;
   Instance *inst = ci->inst;

   if (inst)
     evas_object_pass_events_set(inst->obj, !ci->allow_events);
   e_config_save_queue();
}

static void
_config_cmd_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;

   ci->cmd_changed = 1;
}

static void
_config_cmd_activate(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;
   Instance *inst = ci->inst;
   char *cmd;

   ci->cmd_changed = 0;
   cmd = elm_entry_markup_to_utf8(elm_entry_entry_get(obj));
   eina_stringshare_replace(&ci->cmd, cmd);
   free(cmd);
   e_config_save_queue();
   if (!inst) return;
   if (inst->exe) ecore_exe_quit(inst->exe);
   inst->exe = efl_wl_run(inst->obj, inst->ci->cmd);
}

EINTERN Evas_Object *
config_runner(Config_Item *ci, E_Zone *zone)
{
   Evas_Object *popup, *tb, *o, *ent, *rg;
   int row = 0;

   if (!zone) zone = e_zone_current_get();
   popup = elm_popup_add(e_comp->elm);
   E_EXPAND(popup);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   tb = elm_table_add(popup);
   elm_table_align_set(tb, 0, 0.5);
   E_EXPAND(tb);
   evas_object_show(tb);
   elm_object_content_set(popup, tb);

   o = evas_object_rectangle_add(e_comp->evas);
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(200), 1);
   elm_table_pack(tb, o, 0, row++, 2, 1);

   _config_label_add(tb, D_("Command:"), row);
   ent = o = elm_entry_add(tb);
   E_FILL(o);
   evas_object_show(o);
   elm_entry_single_line_set(o, 1);
   elm_object_focus_set(o, 1);
   elm_entry_entry_set(o, ci->cmd);
   evas_object_smart_callback_add(o, "changed,user", _config_cmd_changed, ci);
   evas_object_smart_callback_add(o, "activated", _config_cmd_activate, ci);
   elm_table_pack(tb, o, 1, row++, 1, 1);

   _config_label_add(tb, D_("On Exit:"), row);
   o = rg = elm_radio_add(tb);
   E_FILL(o);
   evas_object_show(o);
   elm_object_text_set(o, D_("Restart"));
   elm_radio_state_value_set(o, EXIT_MODE_RESTART);
   elm_radio_value_pointer_set(o, &ci->exit_mode);
   elm_table_pack(tb, o, 1, row++, 1, 1);

   o = elm_radio_add(tb);
   E_FILL(o);
   elm_radio_group_add(o, rg);
   evas_object_show(o);
   elm_object_text_set(o, D_("Delete"));
   elm_radio_state_value_set(o, EXIT_MODE_DELETE);
   elm_table_pack(tb, o, 1, row++, 1, 1);


   /* FIXME */
ci->allow_events = 1;
   _config_label_add(tb, D_("Allow events"), row);
   o = elm_check_add(tb);
elm_object_disabled_set(o, 1);
   E_FILL(o);
   evas_object_show(o);
   elm_object_style_set(o, "toggle");
   elm_object_part_text_set(o, "on", D_("Yes"));
   elm_object_part_text_set(o, "off", D_("No"));
   elm_check_state_pointer_set(o, &ci->allow_events);
   elm_object_disabled_set(o, 1);
   evas_object_smart_callback_add(o, "changed", _config_events_changed, ci);
   evas_object_data_set(o, "table", tb);
   elm_table_pack(tb, o, 1, row++, 1, 1);

   popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   evas_object_move(popup, zone->x, zone->y);
   evas_object_resize(popup, zone->w / 4, zone->h / 3);
   e_comp_object_util_center(popup);
   evas_object_show(popup);
   e_comp_object_util_autoclose(popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   evas_object_event_callback_priority_add(popup, EVAS_CALLBACK_DEL, EVAS_CALLBACK_PRIORITY_BEFORE, _config_close, ci);
   evas_object_data_set(popup, "entry", ent);
   e_comp_grab_input(1, 1);

   return rconfig->config_dialog = popup;
}

static Config_Item *
_conf_item_get(int *id)
{
   Config_Item *ci;
   Eina_List *l;

   if (*id > 0)
     {
        EINA_LIST_FOREACH(rconfig->items, l, ci)
          if (*id == ci->id) return ci;
     }

   ci = E_NEW(Config_Item, 1);
   if (!*id)
     ci->id = rconfig->items ? eina_list_count(rconfig->items) + 1 : 1;
   else
     ci->id = -1;

   ci->allow_events = 0;

   if (ci->id < 1) return ci;
   rconfig->items = eina_list_append(rconfig->items, ci);
   e_config_save_queue();

   return ci;
}

static void
_wizard_end(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Wizard_Item *wi = data;
   Eina_List *l;
   Config_Item *ci;

   EINA_LIST_FOREACH(rconfig->items, l, ci)
     {
        if (ci->id == wi->id)
          {
             if (ci->cmd) break;
             wi->id = 0;
             free(ci);
             rconfig->items = eina_list_remove_list(rconfig->items, l);
             break;
          }
     }

   wi->cb(wi->data, wi->id);
   free(wi);
}

static Evas_Object *
runner_wizard(E_Gadget_Wizard_End_Cb cb, void *data)
{
   int id = 0;
   Config_Item *ci;
   Wizard_Item *wi;
   Evas_Object *obj;

   wi = E_NEW(Wizard_Item, 1);
   wi->cb = cb;
   wi->data = data;

   ci = _conf_item_get(&id);
   wi->id = ci->id;
   obj = config_runner(ci, NULL);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_DEL, _wizard_end, wi);
   return obj;
}

/////////////////////////////////////////

static void
mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   evas_object_focus_set(inst->obj, 1);
}

static void
runner_removed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Instance *inst = data;
   if (inst->obj != event_info) return;
   rconfig->items = eina_list_remove(rconfig->items, inst->ci);
   eina_stringshare_del(inst->ci->cmd);
   E_FREE(inst->ci);
}

static void
runner_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   evas_object_smart_callback_del_full(e_gadget_site_get(obj), "gadget_removed", runner_removed, inst);
   if (inst->ci) inst->ci->inst = NULL;
   instances = eina_list_remove(instances, inst);
   free(inst);
}

static Evas_Object *
runner_gadget_configure(Evas_Object *g)
{
   Instance *inst = evas_object_data_get(g, "runner");
   return config_runner(inst->ci, e_comp_object_util_zone_get(g));
}

static void
runner_created(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   if (inst->obj != event_info) return;
   e_gadget_configure_cb_set(inst->obj, runner_gadget_configure);
   evas_object_smart_callback_del_full(obj, "gadget_created", runner_created, data);
}

static Evas_Object *
runner_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient)
{
   Evas_Object *obj;
   Instance *inst;
   Config_Item *ci = NULL;

   if (orient) return NULL;
   if (*id > 0) ci = _conf_item_get(id);
   if ((*id < 0) || ci->inst)
     {
        obj = elm_image_add(parent);
        elm_image_file_set(obj, e_theme_edje_file_get(NULL, "e/icons/modules-launcher"), "e/icons/modules-launcher");
        evas_object_size_hint_aspect_set(obj, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
        return obj;
     }
   inst = E_NEW(Instance, 1);
   instances = eina_list_append(instances, inst);
   inst->ci = ci;
   if (!inst->ci)
     inst->ci = _conf_item_get(id);
   inst->ci->inst = inst;
   inst->obj = efl_wl_add(e_comp->evas);
   efl_wl_aspect_set(inst->obj, 1);
   evas_object_data_set(inst->obj, "runner", inst);
   evas_object_event_callback_add(inst->obj, EVAS_CALLBACK_MOUSE_DOWN, mouse_down, inst);
   evas_object_smart_callback_add(parent, "gadget_created", runner_created, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", runner_removed, inst);
   evas_object_pass_events_set(inst->obj, !inst->ci->allow_events);
   inst->exe = efl_wl_run(inst->obj, inst->ci->cmd);
   ecore_exe_data_set(inst->exe, inst);
   evas_object_event_callback_add(inst->obj, EVAS_CALLBACK_DEL, runner_del, inst);
   return inst->obj;
}

static Eina_Bool
runner_exe_del(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Exe_Event_Del *ev)
{
   Instance *inst = ecore_exe_data_get(ev->exe);

   if ((!inst) || (!instances) || (!eina_list_data_find(instances, inst))) return ECORE_CALLBACK_RENEW;
   switch (inst->ci->exit_mode)
     {
      case EXIT_MODE_RESTART:
        inst->exe = efl_wl_run(inst->obj, inst->ci->cmd);
        ecore_exe_data_set(inst->exe, inst);
        break;
      case EXIT_MODE_DELETE:
        e_gadget_del(inst->obj);
        break;
     }
   return ECORE_CALLBACK_RENEW;
}

///////////////////////////////

EINTERN void
runner_init(void)
{
   conf_item_edd = E_CONFIG_DD_NEW("Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, INT);
   E_CONFIG_VAL(D, T, allow_events, UCHAR);
   E_CONFIG_VAL(D, T, exit_mode, INT);
   E_CONFIG_VAL(D, T, cmd, STR);

   conf_edd = E_CONFIG_DD_NEW("RConfig", RConfig);
#undef T
#undef D
#define T RConfig
#define D conf_edd
   E_CONFIG_LIST(D, T, items, conf_item_edd);

   rconfig = e_config_domain_load("module.runner", conf_edd);
   if (!rconfig) rconfig = E_NEW(RConfig, 1);

   e_gadget_type_add("runner", runner_create, runner_wizard);
   exit_handler = ecore_event_handler_add(ECORE_EXE_EVENT_DEL, (Ecore_Event_Handler_Cb)runner_exe_del, NULL);
}

EINTERN void
runner_shutdown(void)
{
   e_gadget_type_del("runner");

   if (rconfig)
     {
        Config_Item *ci;

        if (rconfig->config_dialog)
          {
             evas_object_hide(rconfig->config_dialog);
             evas_object_del(rconfig->config_dialog);
          }

        EINA_LIST_FREE(rconfig->items, ci)
          {
             eina_stringshare_del(ci->cmd);
             free(ci);
          }

     }
   E_FREE(rconfig);
   E_CONFIG_DD_FREE(conf_edd);
   E_CONFIG_DD_FREE(conf_item_edd);
   E_FREE_FUNC(exit_handler, ecore_event_handler_del);
}

EINTERN void
runner_save(void)
{
   e_config_domain_save("module.runner", conf_edd, rconfig);
}
