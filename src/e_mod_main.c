#include "e_mod_main.h"
#include "gadget.h"


EINTERN void
gadget_demo(void);

EAPI E_Module_Api e_modapi = {E_MODULE_API_VERSION, "Desksanity"};
static E_Config_DD *conf_edd = NULL;

EINTERN Eina_List *save_cbs;

EINTERN Mod *mod = NULL;
EINTERN Config *ds_config = NULL;

static Evas_Object *fade_obj = NULL;

static E_Action *act;
static Eina_List *urgent;
static Eina_List *focus_list;

static Eina_List *handlers;
static Ecore_Timer *ds_key_focus_timeout;
static Eina_List *ds_key_focus_desks;

static Eina_Bool focus_last_focused_per_desktop;
static unsigned int pending_flip;

static void
_ds_fade_end(Ecore_Cb cb, Efx_Map_Data *emd EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   E_FREE_FUNC(fade_obj, evas_object_del);
   if (cb)
     cb(NULL);
}

static void
_e_mod_ds_config_load(void)
{
#undef T
#undef D
   conf_edd = E_CONFIG_DD_NEW("Config", Config);
   #define T Config
   #define D conf_edd
   E_CONFIG_VAL(D, T, config_version, UINT);
   E_CONFIG_VAL(D, T, disable_ruler, UCHAR);
   E_CONFIG_VAL(D, T, disable_maximize, UCHAR);
   E_CONFIG_VAL(D, T, disable_transitions, UCHAR);
   E_CONFIG_VAL(D, T, disabled_transition_count, UINT);

   E_CONFIG_VAL(D, T, types.disable_PAN, UCHAR);
   E_CONFIG_VAL(D, T, types.disable_FADE_OUT, UCHAR);
   E_CONFIG_VAL(D, T, types.disable_FADE_IN, UCHAR);
   E_CONFIG_VAL(D, T, types.disable_BATMAN, UCHAR);
   E_CONFIG_VAL(D, T, types.disable_ZOOM_IN, UCHAR);
   E_CONFIG_VAL(D, T, types.disable_ZOOM_OUT, UCHAR);
   E_CONFIG_VAL(D, T, types.disable_GROW, UCHAR);
   E_CONFIG_VAL(D, T, types.disable_ROTATE_OUT, UCHAR);
   E_CONFIG_VAL(D, T, types.disable_ROTATE_IN, UCHAR);
   E_CONFIG_VAL(D, T, types.disable_SLIDE_SPLIT, UCHAR);
   E_CONFIG_VAL(D, T, types.disable_QUAD_SPLIT, UCHAR);
   E_CONFIG_VAL(D, T, types.disable_QUAD_MERGE, UCHAR);
   E_CONFIG_VAL(D, T, types.disable_BLINK, UCHAR);
   E_CONFIG_VAL(D, T, types.disable_VIEWPORT, UCHAR);

   ds_config = e_config_domain_load("module.desksanity", conf_edd);
   if (ds_config)
     {
        if (!e_util_module_config_check("Desksanity", ds_config->config_version, MOD_CONFIG_FILE_VERSION))
          E_FREE(ds_config);
     }

   if (!ds_config)
     ds_config = E_NEW(Config, 1);
   ds_config->config_version = MOD_CONFIG_FILE_VERSION;
}

static E_Client *
ds_client_urgent_pop(E_Client *ec)
{
   Eina_List *l;

   if (!urgent) return NULL;
   l = eina_list_data_find_list(urgent, ec);
   if (!l) return NULL;
   urgent = eina_list_remove_list(urgent, l);
   return !!e_object_unref(E_OBJECT(ec)) ? ec : NULL;
}

static Eina_List *
ds_key_list_init(const E_Zone *zone)
{
   int i;
   Eina_List *desks = NULL;

   for (i = 0; i < zone->desk_x_count * zone->desk_y_count; i++)
     {
        if (zone->desks[i]->visible) continue;
        e_object_ref(E_OBJECT(zone->desks[i]));
        desks = eina_list_append(desks, zone->desks[i]);
     }
   return desks;
}

static Eina_Bool
ds_key_focus_timeout_cb(void *d EINA_UNUSED)
{
   E_Client *ec;

   e_client_focus_track_thaw();
   ec = e_client_focused_get();
   if (ec)
     e_client_focus_latest_set(ec);
   ds_key_focus_timeout = NULL;
   E_FREE_LIST(ds_key_focus_desks, e_object_unref);
   return EINA_FALSE;
}

static void
ds_key_focus(void)
{
   Eina_List *l;
   E_Client *ec;
   E_Zone *focus_zone = NULL;
   static double last;
   double t = 0.0;
   Eina_Bool skip = EINA_FALSE;

   if (!focus_list)
     {
        focus_zone = e_zone_current_get();
        if (!ds_key_focus_desks)
          ds_key_focus_desks = ds_key_list_init(focus_zone);
        if (!ds_key_focus_timeout)
          {
             e_client_focus_track_freeze();
             ds_key_focus_timeout = ecore_timer_add(0.25, ds_key_focus_timeout_cb, NULL);
          }
        t = ecore_time_unix_get();
        skip = (t - last < 0.25);
        if (skip)
          ecore_timer_reset(ds_key_focus_timeout);
     }
   else
     {
        E_FREE_FUNC(ds_key_focus_timeout, ecore_timer_del);
        E_FREE_LIST(ds_key_focus_desks, e_object_unref);
        e_client_focus_track_thaw();
     }

   EINA_LIST_FOREACH(focus_list ?: e_client_focus_stack_get(), l, ec)
     if ((!ec->iconic) && (!ec->focused) &&
         ((!focus_zone) || ((ec->zone == focus_zone) && eina_list_data_find(ds_key_focus_desks, ec->desk))))
       {
          if (ds_key_focus_desks)
            {
               ds_key_focus_desks = eina_list_remove(ds_key_focus_desks, ec->desk);
               e_object_unref(E_OBJECT(ec->desk));
            }
          if (!pending_flip)
            focus_last_focused_per_desktop = e_config->focus_last_focused_per_desktop;
          if (!ec->desk->visible)
            {
               e_config->focus_last_focused_per_desktop = 0;
               pending_flip++;
            }
          if (ec->sticky)
            {
               E_Client *tec;

               E_CLIENT_FOREACH(tec)
                 if ((!tec->sticky) && (tec->desk == ec->desk)) break;
               /* do not flip to a sticky window if there are no other windows on its desk */
               if ((!tec) || (tec->desk != ec->desk)) continue;
               e_desk_show(ec->desk);
            }
          e_client_activate(ec, 1);
          break;
       }
   last = t;
   focus_list = eina_list_free(focus_list);
}

static void
ds_key(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   E_Client *ec = NULL;

   if (!urgent)
     {
        ds_key_focus();
        return;
     }

   while (!ec)
     ec = ds_client_urgent_pop(eina_list_data_get(urgent));
   if (ec)
     {
        eina_list_free(focus_list);
        focus_list = eina_list_clone(e_client_focus_stack_get());
        e_client_activate(ec, 1);
     }
   else
     ds_key_focus();
}

static Eina_Bool
ds_desk_after_show(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Desk_After_Show *ev EINA_UNUSED)
{
   if (pending_flip)
     pending_flip--,
     e_config->focus_last_focused_per_desktop = focus_last_focused_per_desktop;
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
ds_client_remove(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Client *ev)
{
   ds_client_urgent_pop(ev->ec);
   if (focus_list)
     focus_list = eina_list_remove(focus_list, ev->ec);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
ds_client_urgent(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Client_Property *ev)
{
   if (!(ev->property & E_CLIENT_PROPERTY_URGENCY)) return ECORE_CALLBACK_RENEW;

   if (ev->ec->urgent)
     {
        e_object_ref(E_OBJECT(ev->ec));
        urgent = eina_list_append(urgent, ev->ec);
     }
   else
     ds_client_urgent_pop(ev->ec);
   return ECORE_CALLBACK_RENEW;
}

EAPI void *
e_modapi_init(E_Module *m)
{
   char buf[PATH_MAX];

   bindtextdomain(PACKAGE, LOCALEDIR);
   bind_textdomain_codeset(PACKAGE, "UTF-8");

   snprintf(buf, sizeof(buf), "%s/e-module-desksanity.edj", m->dir);
   elm_theme_overlay_add(NULL, buf);

   efx_init();
   _e_mod_ds_config_load();

   mod = E_NEW(Mod, 1);
   mod->module = m;
   mod->edje_file = eina_stringshare_add(buf);

   ds_config_init();
   if (!ds_config->disable_transitions)
     ds_init();
   if (!ds_config->disable_ruler)
     mr_init();
   if (!ds_config->disable_maximize)
     maximize_init();

   pip_init();
   zoom_init();
   mag_init();

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_PROPERTY, ds_client_urgent, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_REMOVE, ds_client_remove, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_DESK_AFTER_SHOW, ds_desk_after_show, NULL);

   act = e_action_add("ds_key");
   e_action_predef_name_set(D_("Desksanity"), D_("Manage Window Focus For Me"), "ds_key", NULL, NULL, 0);
   act->func.go = ds_key;

   gadget_demo();

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   mag_shutdown();
   zoom_shutdown();
   pip_shutdown();
   if (!ds_config->disable_maximize)
     maximize_shutdown();
   if (!ds_config->disable_ruler)
     mr_shutdown();
   if (!ds_config->disable_transitions)
     ds_shutdown();
   ds_config_shutdown();
   e_config_domain_save("module.desksanity", conf_edd, ds_config);
   E_FREE(ds_config);
   E_CONFIG_DD_FREE(conf_edd);
   eina_stringshare_del(mod->edje_file);
   E_FREE(mod);
   E_FREE_FUNC(act, e_action_del);
   e_action_predef_name_del(D_("Desksanity"), "ds_key");
   E_FREE_LIST(handlers, ecore_event_handler_del);
   E_FREE_LIST(urgent, e_object_unref);
   focus_list = eina_list_free(focus_list);
   E_FREE_FUNC(ds_key_focus_timeout, ecore_timer_del);
   E_FREE_LIST(ds_key_focus_desks, e_object_unref);
   save_cbs = eina_list_free(save_cbs);
   //efx_shutdown(); broken...

   z_gadget_type_del("Start");
   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   E_Comp_Cb cb;
   Eina_List *l;
   e_config_domain_save("module.desksanity", conf_edd, ds_config);
   EINA_LIST_FOREACH(save_cbs, l, cb)
     cb();
   return 1;
}

EINTERN void
ds_fade_setup(Evas_Object_Event_Cb click_cb)
{
   if (fade_obj) return;
   fade_obj = evas_object_rectangle_add(e_comp->evas);
   if (click_cb)
     evas_object_event_callback_add(fade_obj, EVAS_CALLBACK_MOUSE_DOWN, click_cb, NULL);
   evas_object_name_set(fade_obj, "fade_obj");
   evas_object_geometry_set(fade_obj, 0, 0, e_comp->w, e_comp->h);
   evas_object_layer_set(fade_obj, E_LAYER_MENU + 1);
   evas_object_show(fade_obj);
   efx_fade(fade_obj, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(0, 0, 0), 0, 0.0, NULL, NULL);
   efx_fade(fade_obj, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(0, 0, 0), 192, 0.3, NULL, NULL);
}

EINTERN void
ds_fade_end(Ecore_Cb end_cb, Evas_Object_Event_Cb click_cb)
{
   evas_object_pass_events_set(fade_obj, 1);
   efx_fade(fade_obj, EFX_EFFECT_SPEED_DECELERATE, EFX_COLOR(0, 0, 0), 0, 0.3, (Efx_End_Cb)_ds_fade_end, end_cb);
   if (click_cb)
     evas_object_event_callback_del(fade_obj, EVAS_CALLBACK_MOUSE_DOWN, click_cb);
}
