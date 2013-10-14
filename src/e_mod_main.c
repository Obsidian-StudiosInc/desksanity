#include "e_mod_main.h"

EAPI E_Module_Api e_modapi = {E_MODULE_API_VERSION, "Desksanity"};
static E_Config_DD *conf_edd = NULL;
static Eina_List *handlers = NULL;

EINTERN Mod *mod = NULL;
EINTERN Config *ds_config = NULL;

static void
_e_mod_ds_config_load(void)
{
#undef T
#undef D
   conf_edd = E_CONFIG_DD_NEW("Config", Config);
   #define T Config
   #define D conf_edd
   E_CONFIG_VAL(D, T, config_version, UINT);

   //ds_config = e_config_domain_load("module.desksanity", conf_edd);
   if (ds_config)
     {
        if (!e_util_module_config_check("Desksanity", ds_config->config_version, MOD_CONFIG_FILE_VERSION))
          {}
     }

   if (!ds_config)
     {
        ds_config = E_NEW(Config, 1);
        ds_config->config_version = (MOD_CONFIG_FILE_EPOCH << 16);
     }
}

EAPI void *
e_modapi_init(E_Module *m)
{
   char buf[4096];

   bindtextdomain(PACKAGE, LOCALEDIR);
   bind_textdomain_codeset(PACKAGE, "UTF-8");

   snprintf(buf, sizeof(buf), "%s/e-module-desksanity.edj", m->dir);
//
   //e_configure_registry_category_add("appearance", 80, D_("Look"),
                                     //NULL, "preferences-look");
   //e_configure_registry_item_add("extensions/desksanity", 110, D_("Echievements"),
                                 //NULL, buf, e_int_config_desksanity);

   efx_init();

   mod = E_NEW(Mod, 1);
   mod->module = m;

   ds_init();
   e_moveresize_replace(EINA_TRUE);
   mr_init();

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   e_configure_registry_item_del("appearance/desksanity");

   e_configure_registry_category_del("extensions");

   E_FREE_FUNC(mod->cfd, e_object_del);
   mr_shutdown();
   ds_shutdown();
   //e_config_domain_save("module.desksanity", conf_edd, ds_config);
   E_CONFIG_DD_FREE(conf_edd);
   E_FREE(mod);
   E_FREE_LIST(handlers, ecore_event_handler_del);
   efx_shutdown();
   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   //e_config_domain_save("module.desksanity", conf_edd, ds_config);
   return 1;
}

