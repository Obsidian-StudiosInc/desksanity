#define HAVE_WAYLAND
#include "e_mod_main.h"
#include <Efl_Wl.h>
#include "e-gadget-server-protocol.h"
#include "action_route-server-protocol.h"

typedef enum
{
   EXIT_MODE_RESTART,
   EXIT_MODE_DELETE,
} Exit_Mode;

typedef struct Config_Item
{
   int id;
   int exit_mode;
   Eina_Stringshare *cmd;
   void *inst;
   Eina_Bool cmd_changed : 1;
} Config_Item;

typedef struct Instance
{
   Evas_Object *box;
   Evas_Object *obj;
   Ecore_Exe *exe;
   Config_Item *ci;
   Eina_Hash *allowed_pids;
   void *gadget_resource;
   Evas_Object *popup;
   Evas_Object *ctxpopup;
   Eina_List *extracted;
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
static Eina_List *wizards;

static Eina_Hash *sandbox_gadgets;

static Eina_List *handlers;
static Eio_Monitor *gadget_monitor;
static Eio_File *gadget_lister;

typedef struct Wizard_Item
{
   Evas_Object *site;
   Evas_Object *popup;
   E_Gadget_Wizard_End_Cb cb;
   void *data;
   int id;
   Eina_Bool sandbox : 1;
} Wizard_Item;

static void
runner_run(Instance *inst)
{
   char *preload = eina_strdup(getenv("LD_PRELOAD"));
   char buf[PATH_MAX];
   char *file = ecore_file_dir_get(mod->module->file);
   int pid;

   snprintf(buf, sizeof(buf), "%s/loader.so", file);
   e_util_env_set("LD_PRELOAD", buf);

   snprintf(buf, sizeof(buf), "%d", inst->ci->id);
   e_util_env_set("E_GADGET_ID", buf);

   inst->exe = efl_wl_run(inst->obj, inst->ci->cmd);

   e_util_env_set("E_GADGET_ID", NULL);
   e_util_env_set("LD_PRELOAD", preload);
   free(file);
   free(preload);
   eina_hash_free_buckets(inst->allowed_pids);
   pid = ecore_exe_pid_get(inst->exe);
   eina_hash_add(inst->allowed_pids, &pid, (void*)1);
}

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
   runner_run(inst);
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
   runner_run(inst);
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

   elm_object_focus_set(ent, 1);

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
     *id = ci->id = rconfig->items ? eina_list_count(rconfig->items) + 1 : 1;
   else
     ci->id = *id;

   if (ci->id < 1) return ci;
   rconfig->items = eina_list_append(rconfig->items, ci);
   e_config_save_queue();

   return ci;
}

static void
wizard_site_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Wizard_Item *wi = data;
   wi->site = NULL;
   evas_object_hide(wi->popup);
   evas_object_del(wi->popup);
}

static void
_wizard_config_end(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
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

   if (wi->site)
     wi->cb(wi->data, wi->id);
   wizards = eina_list_remove(wizards, wi);
   if (wi->site)
     evas_object_event_callback_del_full(wi->site, EVAS_CALLBACK_DEL, wizard_site_del, wi);
   free(wi);
}

static Evas_Object *
runner_wizard(E_Gadget_Wizard_End_Cb cb, void *data, Evas_Object *site)
{
   int id = 0;
   Config_Item *ci;
   Wizard_Item *wi;

   wi = E_NEW(Wizard_Item, 1);
   wi->cb = cb;
   wi->data = data;
   wi->site = site;
   evas_object_event_callback_add(wi->site, EVAS_CALLBACK_DEL, wizard_site_del, wi);
   wizards = eina_list_append(wizards, wi);

   ci = _conf_item_get(&id);
   wi->id = ci->id;
   wi->popup = config_runner(ci, NULL);
   evas_object_event_callback_add(wi->popup, EVAS_CALLBACK_DEL, _wizard_config_end, wi);
   return wi->popup;
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
   if (inst->box != event_info) return;
   rconfig->items = eina_list_remove(rconfig->items, inst->ci);
   eina_stringshare_del(inst->ci->cmd);
   E_FREE(inst->ci);
}

static void
runner_site_gravity(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   if (inst->gadget_resource)
     e_gadget_send_gadget_gravity(inst->gadget_resource, e_gadget_site_gravity_get(obj));
}

static void
runner_site_anchor(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   if (inst->gadget_resource)
     e_gadget_send_gadget_anchor(inst->gadget_resource, e_gadget_site_anchor_get(obj));
}

static void
runner_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Evas_Object *site = e_gadget_site_get(obj);

   evas_object_smart_callback_del_full(site, "gadget_removed", runner_removed, inst);
   evas_object_smart_callback_del_full(site, "gadget_site_anchor", runner_site_anchor, inst);
   evas_object_smart_callback_del_full(site, "gadget_site_gravity", runner_site_gravity, inst);
   if (inst->ci)
     {
        inst->ci->inst = NULL;
        E_FREE_FUNC(inst->exe, ecore_exe_quit);
     }
   else
     E_FREE_FUNC(inst->exe, ecore_exe_terminate);
   instances = eina_list_remove(instances, inst);
   eina_hash_free(inst->allowed_pids);
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
   if (inst->box != event_info) return;
   e_gadget_configure_cb_set(inst->box, runner_gadget_configure);
   evas_object_smart_callback_del_full(obj, "gadget_created", runner_created, data);
}


static void
gadget_unbind(struct wl_resource *resource)
{
   Instance *inst = wl_resource_get_user_data(resource);
   inst->gadget_resource = NULL;
}

static void
gadget_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   struct wl_resource *res;
   Instance *inst = data;
   pid_t pid;
   Evas_Object *site;

   wl_client_get_credentials(client, &pid, NULL, NULL);
   if (pid != ecore_exe_pid_get(inst->exe))
     {
        wl_client_post_no_memory(client);
        return;
     }

   res = wl_resource_create(client, &e_gadget_interface, version, id);
   wl_resource_set_implementation(res, NULL, data, gadget_unbind);
   inst->gadget_resource = res;
   site = e_gadget_site_get(inst->box);
   e_gadget_send_gadget_orient(res, e_gadget_site_orient_get(site));
   e_gadget_send_gadget_gravity(res, e_gadget_site_gravity_get(site));
   e_gadget_send_gadget_anchor(res, e_gadget_site_anchor_get(site));
}

static void
ar_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   struct wl_resource *res;
   Instance *inst = data;
   int v;
   const void *ar_interface;
   pid_t pid;

   wl_client_get_credentials(client, &pid, NULL, NULL);
   if (pid != ecore_exe_pid_get(inst->exe))
     {
        wl_client_post_no_memory(client);
        return;
     }
   ar_interface = e_comp_wl_extension_action_route_interface_get(&v);

   if (!(res = wl_resource_create(client, &action_route_interface, MIN(v, version), id)))
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, ar_interface, inst->allowed_pids, NULL);
}

static void
child_close(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Evas_Object *ext;

   inst->popup = NULL;
   ext = evas_object_data_get(obj, "extracted");
   elm_box_unpack_all(obj);
   inst->extracted = eina_list_remove(inst->extracted, ext);
   evas_object_hide(ext);
}

static void
child_added(void *data, Evas_Object *obj, void *event_info)
{
   Evas_Object *popup, *bx;
   E_Zone *zone = e_comp_object_util_zone_get(obj);
   Instance *inst = data;

   if (!efl_wl_surface_extract(event_info)) return;
   inst->extracted = eina_list_append(inst->extracted, event_info);

   popup = elm_popup_add(e_comp->elm);
   E_EXPAND(popup);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   bx = elm_box_add(popup);
   E_EXPAND(event_info);
   E_FILL(event_info);
   elm_box_homogeneous_set(bx, 1);
   evas_object_show(bx);
   elm_box_pack_end(bx, event_info);
   elm_object_content_set(popup, bx);

   inst->popup = popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   evas_object_move(popup, zone->x, zone->y);
   evas_object_resize(popup, zone->w / 4, zone->h / 3);
   e_comp_object_util_center(popup);
   evas_object_show(popup);
   e_comp_object_util_autoclose(popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   evas_object_event_callback_add(bx, EVAS_CALLBACK_DEL, child_close, inst);
   evas_object_data_set(bx, "extracted", event_info);
   e_comp_grab_input(1, 1);
   evas_object_focus_set(event_info, 1);
}

static void
popup_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Evas_Object *ext;

   inst->ctxpopup = NULL;
   ext = evas_object_data_get(obj, "extracted");
   elm_box_unpack_all(obj);
   inst->extracted = eina_list_remove(inst->extracted, ext);
   evas_object_hide(ext);
}

static void
popup_dismissed(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   evas_object_del(obj);
}

static void
popup_hide(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   elm_ctxpopup_dismiss(inst->ctxpopup);
}

static void
popup_added(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Instance *inst = data;
   Evas_Object *bx;

   if (!efl_wl_surface_extract(event_info)) return;
   inst->extracted = eina_list_append(inst->extracted, event_info);

   inst->ctxpopup = elm_ctxpopup_add(inst->box);
   elm_object_style_set(inst->ctxpopup, "noblock");
   evas_object_smart_callback_add(inst->ctxpopup, "dismissed", popup_dismissed, inst);
   evas_object_event_callback_add(event_info, EVAS_CALLBACK_DEL, popup_hide, inst);

   bx = elm_box_add(inst->ctxpopup);
   elm_box_homogeneous_set(bx, 1);
   evas_object_show(bx);
   elm_box_pack_end(bx, event_info);
   evas_object_data_set(bx, "extracted", event_info);
   evas_object_event_callback_add(bx, EVAS_CALLBACK_DEL, popup_del, inst);
   elm_object_content_set(inst->ctxpopup, bx);

   e_gadget_util_ctxpopup_place(inst->box, inst->ctxpopup, NULL);
   evas_object_show(inst->ctxpopup);
   evas_object_focus_set(event_info, 1);
}

static void
runner_hints(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   int w, h;
   Evas_Aspect_Control aspect;

   evas_object_size_hint_min_get(obj, &w, &h);
   evas_object_size_hint_min_set(inst->box, w, h);
   evas_object_size_hint_max_get(obj, &w, &h);
   evas_object_size_hint_max_set(inst->box, w, h);
   evas_object_size_hint_aspect_get(obj, &aspect, &w, &h);
   evas_object_size_hint_aspect_set(inst->box, aspect, w, h);
}

static Evas_Object *
gadget_create(Evas_Object *parent, Config_Item *ci, int *id, E_Gadget_Site_Orient orient)
{
   Instance *inst;
   int ar_version;

   inst = E_NEW(Instance, 1);
   instances = eina_list_append(instances, inst);
   inst->ci = ci;
   if (!inst->ci)
     inst->ci = _conf_item_get(id);
   inst->ci->inst = inst;
   inst->allowed_pids = eina_hash_int32_new(NULL);
   inst->obj = efl_wl_add(e_comp->evas);
   E_EXPAND(inst->obj);
   E_FILL(inst->obj);
   evas_object_show(inst->obj);
   efl_wl_aspect_set(inst->obj, 1);
   efl_wl_minmax_set(inst->obj, 1);
   efl_wl_global_add(inst->obj, &e_gadget_interface, 1, inst, gadget_bind);
   evas_object_smart_callback_add(inst->obj, "child_added", child_added, inst);
   evas_object_smart_callback_add(inst->obj, "popup_added", popup_added, inst);
   e_comp_wl_extension_action_route_interface_get(&ar_version);
   efl_wl_global_add(inst->obj, &action_route_interface, ar_version, inst, ar_bind);
   evas_object_data_set(inst->obj, "runner", inst);
   evas_object_event_callback_add(inst->obj, EVAS_CALLBACK_MOUSE_DOWN, mouse_down, inst);
   evas_object_smart_callback_add(parent, "gadget_created", runner_created, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", runner_removed, inst);
   evas_object_smart_callback_add(parent, "gadget_site_anchor", runner_site_anchor, inst);
   evas_object_smart_callback_add(parent, "gadget_site_gravity", runner_site_gravity, inst);
   runner_run(inst);
   e_util_size_debug_set(inst->obj, 1);
   ecore_exe_data_set(inst->exe, inst);
   evas_object_event_callback_add(inst->box, EVAS_CALLBACK_DEL, runner_del, inst);
   inst->box = elm_box_add(e_comp->elm);
   evas_object_event_callback_add(inst->obj, EVAS_CALLBACK_CHANGED_SIZE_HINTS, runner_hints, inst);
   elm_box_homogeneous_set(inst->box, 1);
   elm_box_pack_end(inst->box, inst->obj);
   return inst->box;
}

static Evas_Object *
runner_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient)
{
   Evas_Object *obj;
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
   return gadget_create(parent, ci, id, orient);
}

static Eina_Bool
runner_exe_del(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Exe_Event_Del *ev)
{
   Instance *inst = ecore_exe_data_get(ev->exe);

   if ((!inst) || (!instances) || (!eina_list_data_find(instances, inst))) return ECORE_CALLBACK_RENEW;
   switch (inst->ci->exit_mode)
     {
      case EXIT_MODE_RESTART:
        /* FIXME: probably notify? */
        if (ev->exit_code == 255) //exec error
          e_gadget_del(inst->box);
        else
          {
             runner_run(inst);
             ecore_exe_data_set(inst->exe, inst);
          }
        break;
      case EXIT_MODE_DELETE:
        e_gadget_del(inst->box);
        break;
     }
   return ECORE_CALLBACK_RENEW;
}

///////////////////////////////

static Evas_Object *
sandbox_create(Evas_Object *parent, const char *type, int *id, E_Gadget_Site_Orient orient)
{
   Efreet_Desktop *ed = eina_hash_find(sandbox_gadgets, type);
   Config_Item *ci = NULL;

   if (*id > 0) ci = _conf_item_get(id);
   if ((*id < 0) || (ci && ci->inst))
     {
        if (ed->icon)
          {
             int w, h;
             Eina_Bool fail = EINA_FALSE;
             Evas_Object *obj;

             obj = elm_image_add(parent);
             if (ed->icon[0] == '/')
               {
                  if (eina_str_has_extension(ed->icon, ".edj"))
                    fail = !elm_image_file_set(obj, ed->icon, "icon");
                  else
                    fail = !elm_image_file_set(obj, ed->icon, NULL);
               }
             else
               {
                  if (!elm_image_file_set(obj, e_theme_edje_file_get(NULL, ed->icon), ed->icon))
                    fail = !elm_icon_standard_set(obj, ed->icon);
               }
             if (!fail)
               {
                  elm_image_object_size_get(obj, &w, &h);
                  if (w && h)
                    evas_object_size_hint_aspect_set(obj, EVAS_ASPECT_CONTROL_BOTH, w, h);
                  return obj;
               }
             evas_object_del(obj);
          }
     }
   if (!ci)
     {
        ci = _conf_item_get(id);
        ci->cmd = eina_stringshare_add(ed->exec);
        ci->exit_mode = EXIT_MODE_RESTART;
     }
   return gadget_create(parent, ci, id, orient);
}

static char *
sandbox_name(const char *filename)
{
   Efreet_Desktop *ed = eina_hash_find(sandbox_gadgets, filename);
   const char *name = ed->name ?: ed->generic_name;
   char buf[1024];

   if (name) return strdup(name);
   strncpy(buf, ed->orig_path, sizeof(buf) - 1);
   buf[0] = toupper(buf[0]);
   return strdup(buf);
}

///////////////////////////////

static void
gadget_dir_add(const char *filename)
{
   const char *file;
   char buf[PATH_MAX];
   Efreet_Desktop *ed;

   file = ecore_file_file_get(filename);
   snprintf(buf, sizeof(buf), "%s/%s.desktop", filename, file);
   ed = efreet_desktop_new(buf);
   EINA_SAFETY_ON_NULL_RETURN(ed);
   eina_hash_add(sandbox_gadgets, filename, ed);
   e_gadget_external_type_add("runner_sandbox", filename, sandbox_create, NULL);
   e_gadget_external_type_name_cb_set("runner_sandbox", filename, sandbox_name);
}

static Eina_Bool
monitor_dir_create(void *d EINA_UNUSED, int t EINA_UNUSED, Eio_Monitor_Event *ev)
{
   if (!eina_hash_find(sandbox_gadgets, ev->filename))
     gadget_dir_add(ev->filename);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
monitor_dir_del(void *d EINA_UNUSED, int t EINA_UNUSED, Eio_Monitor_Event *ev)
{
   eina_hash_del_by_key(sandbox_gadgets, ev->filename);
   e_gadget_external_type_del("runner_sandbox", ev->filename);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
monitor_error(void *d EINA_UNUSED, int t EINA_UNUSED, Eio_Monitor_Error *ev)
{
   /* panic? */
   return ECORE_CALLBACK_RENEW;
}


static Eina_Bool
list_filter_cb(void *d EINA_UNUSED, Eio_File *ls EINA_UNUSED, const Eina_File_Direct_Info *info)
{
   struct stat st;
   char buf[PATH_MAX];

   if (info->type != EINA_FILE_DIR) return EINA_FALSE;
   if (info->path[info->name_start] == '.') return EINA_FALSE;
   snprintf(buf, sizeof(buf), "%s/%s.desktop", info->path, info->path + info->name_start);
   return !stat(info->path, &st);
}

static void
list_main_cb(void *d EINA_UNUSED, Eio_File *ls EINA_UNUSED, const Eina_File_Direct_Info *info)
{
   gadget_dir_add(info->path);
}

static void
list_done_cb(void *d EINA_UNUSED, Eio_File *ls EINA_UNUSED)
{
   gadget_lister = NULL;
}

static void
list_error_cb(void *d EINA_UNUSED, Eio_File *ls EINA_UNUSED, int error EINA_UNUSED)
{
   gadget_lister = NULL;
}

EINTERN void
runner_init(void)
{
   conf_item_edd = E_CONFIG_DD_NEW("Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, INT);
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
   {
      gadget_monitor = eio_monitor_add(GADGET_DIR);
      gadget_lister = eio_file_direct_ls(GADGET_DIR, list_filter_cb, list_main_cb, list_done_cb, list_error_cb, NULL);
   }
   E_LIST_HANDLER_APPEND(handlers, ECORE_EXE_EVENT_DEL, runner_exe_del, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_DIRECTORY_CREATED, monitor_dir_create, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_DIRECTORY_DELETED, monitor_dir_del, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_ERROR, monitor_error, NULL);

   sandbox_gadgets = eina_hash_string_superfast_new((Eina_Free_Cb)efreet_desktop_free);
}

EINTERN void
runner_shutdown(void)
{
   e_gadget_type_del("runner");
   e_gadget_external_type_del("runner_sandbox", NULL);

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
   E_FREE_LIST(handlers, ecore_event_handler_del);
   E_FREE_FUNC(sandbox_gadgets, eina_hash_free);
   E_FREE_FUNC(gadget_lister, eio_file_cancel);
}

EINTERN void
runner_save(void)
{
   e_config_domain_save("module.runner", conf_edd, rconfig);
}
