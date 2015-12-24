#include "clock.h"

static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;
static E_Action *act = NULL;

static void
_e_mod_action_cb(E_Object *obj EINA_UNUSED, const char *params, ...)
{
   Eina_List *l;
   Instance *inst;

   if (!eina_streq(params, "show_calendar")) return;

   EINA_LIST_FOREACH(clock_instances, l, inst)
     if (inst->popup)
       clock_popup_free(inst);
     else
       clock_popup_new(inst);
}

EINTERN void
clock_init(void)
{
   conf_item_edd = E_CONFIG_DD_NEW("Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, UINT);
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
        act->func.go = (void*)_e_mod_action_cb;
        act->func.go_key = (void*)_e_mod_action_cb;
        act->func.go_mouse = (void*)_e_mod_action_cb;
        act->func.go_edge = (void*)_e_mod_action_cb;

        e_action_predef_name_set(N_("Clock"), N_("Toggle calendar"), "clock", "show_calendar", NULL, 0);
     }

   z_gadget_type_add("Clock", clock_create);
   time_init();
}

EINTERN void
clock_shutdown(void)
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
          free(ci);

        free(clock_config);
        clock_config = NULL;
     }
   E_CONFIG_DD_FREE(conf_edd);
   E_CONFIG_DD_FREE(conf_item_edd);
   conf_item_edd = NULL;
   conf_edd = NULL;

   z_gadget_type_del("Clock");
   time_shutdown();
}

#if 0
/* module setup */
E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Clock"
};

E_API void *
e_modapi_init(E_Module *m)
{
   clock_init();

   clock_config->module = m;
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{


   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.clock", conf_edd, clock_config);
   return 1;
}
#endif
