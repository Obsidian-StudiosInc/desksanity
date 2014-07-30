#include "e_mod_main.h"

EAPI E_Module_Api e_modapi = {E_MODULE_API_VERSION, "Desksanity"};
static E_Config_DD *conf_edd = NULL;

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
   E_CONFIG_VAL(D, T, disable_ruler, UCHAR);
   E_CONFIG_VAL(D, T, disable_maximize, UCHAR);

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
   ds_init();
   if (!ds_config->disable_ruler)
     mr_init();
   if (!ds_config->disable_maximize)
     maximize_init();

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   if (!ds_config->disable_maximize)
     maximize_shutdown();
   if (!ds_config->disable_ruler)
     mr_shutdown();
   ds_shutdown();
   ds_config_shutdown();
   e_config_domain_save("module.desksanity", conf_edd, ds_config);
   E_FREE(ds_config);
   E_CONFIG_DD_FREE(conf_edd);
   eina_stringshare_del(mod->edje_file);
   E_FREE(mod);
   //efx_shutdown(); broken...
   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.desksanity", conf_edd, ds_config);
   return 1;
}

