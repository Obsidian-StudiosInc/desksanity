#include "e.h"
#include "clock.h"


static void
_e_mod_action(const char *params)
{
   Eina_List *l;
   Instance *inst;

   if (!params) return;
   if (strcmp(params, "show_calendar")) return;

   EINA_LIST_FOREACH(clock_instances, l, inst)
     if (inst->popup)
       _clock_popup_free(inst);
     else
       _clock_popup_new(inst);
}

static void
_e_mod_action_cb(E_Object *obj EINA_UNUSED, const char *params)
{
   _e_mod_action(params);
}


/* module setup */
E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Clock"
};

E_API void *
e_modapi_init(E_Module *m)
{
   conf_item_edd = E_CONFIG_DD_NEW("Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, STR);
   E_CONFIG_VAL(D, T, weekend.start, INT);
   E_CONFIG_VAL(D, T, weekend.len, INT);
   E_CONFIG_VAL(D, T, week.start, INT);
   E_CONFIG_VAL(D, T, digital_clock, INT);
   E_CONFIG_VAL(D, T, digital_24h, INT);
   E_CONFIG_VAL(D, T, show_seconds, INT);
   E_CONFIG_VAL(D, T, show_date, INT);

   conf_edd = E_CONFIG_DD_NEW("Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_LIST(D, T, items, conf_item_edd);

   clock_config = e_config_domain_load("module.clock", conf_edd);

   if (!clock_config)
     clock_config = E_NEW(Config, 1);

   act = e_action_add("clock");
   if (act)
     {
        act->func.go = _e_mod_action_cb;
        act->func.go_key = _e_mod_action_cb;
        act->func.go_mouse = _e_mod_action_cb;
        act->func.go_edge = _e_mod_action_cb;

        e_action_predef_name_set(N_("Clock"), N_("Toggle calendar"), "clock", "show_calendar", NULL, 0);
     }

   clock_config->module = m;

   z_gadget_type_add("clock", clock_create);

   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   if (act)
     {
        e_action_predef_name_del("Clock", "Toggle calendar");
        e_action_del("clock");
        act = NULL;
     }
   if (clock_config)
     {
        Config_Item *ci;

        if (clock_config->config_dialog)
          e_object_del(E_OBJECT(clock_config->config_dialog));

        EINA_LIST_FREE(clock_config->items, ci)
          {
             eina_stringshare_del(ci->id);
             free(ci);
          }

        free(clock_config);
        clock_config = NULL;
     }
   E_CONFIG_DD_FREE(conf_edd);
   E_CONFIG_DD_FREE(conf_item_edd);
   conf_item_edd = NULL;
   conf_edd = NULL;

   e_gadcon_provider_unregister(&_gadcon_class);

   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.clock", conf_edd, clock_config);
   return 1;
}
