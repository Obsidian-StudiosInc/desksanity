#include "e_mod_main.h"

EAPI E_Module_Api e_modapi = {E_MODULE_API_VERSION, "Desksanity"};
static E_Config_DD *conf_edd = NULL;

EINTERN Mod *mod = NULL;
EINTERN Config *ds_config = NULL;

static Evas_Object *fade_obj = NULL;

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
   //efx_shutdown(); broken...
   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.desksanity", conf_edd, ds_config);
   return 1;
}

EINTERN void
ds_fade_setup(E_Comp *comp, Evas_Object_Event_Cb click_cb)
{
   if (fade_obj) return;
   fade_obj = evas_object_rectangle_add(comp->evas);
   if (click_cb)
     evas_object_event_callback_add(fade_obj, EVAS_CALLBACK_MOUSE_DOWN, click_cb, NULL);
   evas_object_name_set(fade_obj, "fade_obj");
   evas_object_geometry_set(fade_obj, 0, 0, comp->man->w, comp->man->h);
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
