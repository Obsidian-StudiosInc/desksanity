#include "e_mod_main.h"

static E_Int_Menu_Augmentation *maug = NULL;


static const char *type_desc[] =
{
   [DS_PAN] = D_("Pan"),
   [DS_FADE_OUT] = D_("Fade Out"),
   [DS_FADE_IN] = D_("Fade In"),
   [DS_BATMAN] = D_("Batman"),
   [DS_ZOOM_IN] = D_("Zoom In"),
   [DS_ZOOM_OUT] = D_("Zoom Out"),
   [DS_GROW] = D_("Grow"),
   [DS_ROTATE_OUT] = D_("Rotate Out"),
   [DS_ROTATE_IN] = D_("Rotate In"),
   [DS_SLIDE_SPLIT] = D_("Slide Split"),
   [DS_QUAD_SPLIT] = D_("Quad Split"),
   [DS_QUAD_MERGE] = D_("Quad Merge"),
   [DS_BLINK] = D_("Blink"),
   [DS_VIEWPORT] = D_("Viewport"),
   [DS_LAST] = NULL
};

static void
_ds_menu_ruler(void *data EINA_UNUSED, E_Menu *m EINA_UNUSED, E_Menu_Item *mi)
{
   ds_config->disable_ruler = mi->toggle;
   if (ds_config->disable_ruler)
     mr_shutdown();
   else
     mr_init();
   e_config_save_queue();
}

static void
_ds_menu_maximize(void *data EINA_UNUSED, E_Menu *m EINA_UNUSED, E_Menu_Item *mi)
{
   ds_config->disable_maximize = mi->toggle;
   if (ds_config->disable_maximize)
     maximize_shutdown();
   else
     maximize_init();
   e_config_save_queue();
}

static void
_ds_menu_transitions(void *data EINA_UNUSED, E_Menu *m EINA_UNUSED, E_Menu_Item *mi)
{
   ds_config->disable_transitions = mi->toggle;
   if (ds_config->disable_transitions)
     ds_shutdown();
   else
     ds_init();
   e_config_save_queue();
}

static void
_ds_menu_transition_type(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi)
{
   Eina_Bool *types = (Eina_Bool*)&ds_config->types;
   unsigned int t = (uintptr_t)data;

   types[t] = mi->toggle;
   if (mi->toggle)
     ds_config->disabled_transition_count++;
   else
     ds_config->disabled_transition_count--;
   e_config_save_queue();
}

static void
_ds_menu_add(void *data EINA_UNUSED, E_Menu *m)
{
   E_Menu_Item *mi;
   E_Menu *subm;
   unsigned int t;

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, D_("Desksanity"));
   e_menu_item_icon_edje_set(mi, mod->edje_file, "icon");

   subm = e_menu_new();
   e_menu_title_set(subm, D_("Options"));
   e_menu_item_submenu_set(mi, subm);
   e_object_unref(E_OBJECT(subm));

   mi = e_menu_item_new(subm);
   e_menu_item_label_set(mi, D_("Disable Move/Resize Ruler"));
   e_menu_item_check_set(mi, 1);
   e_menu_item_toggle_set(mi, ds_config->disable_ruler);
   e_menu_item_callback_set(mi, _ds_menu_ruler, NULL);

   mi = e_menu_item_new(subm);
   e_menu_item_label_set(mi, D_("Disable Maximize Effects"));
   e_menu_item_check_set(mi, 1);
   e_menu_item_toggle_set(mi, ds_config->disable_maximize);
   e_menu_item_callback_set(mi, _ds_menu_maximize, NULL);

   mi = e_menu_item_new(subm);
   e_menu_item_label_set(mi, D_("Disable Transition Effects"));
   e_menu_item_check_set(mi, 1);
   e_menu_item_toggle_set(mi, ds_config->disable_transitions);
   e_menu_item_callback_set(mi, _ds_menu_transitions, NULL);

   if (ds_config->disable_transitions) return;

   subm = e_menu_new();
   e_menu_title_set(subm, D_("Disable Transitions"));
   e_menu_item_submenu_set(mi, subm);
   e_object_unref(E_OBJECT(subm));

   for (t = 0; t < DS_LAST; t++)
     {
        Eina_Bool *types = (Eina_Bool*)&ds_config->types;
        mi = e_menu_item_new(subm);
        e_menu_item_label_set(mi, type_desc[t]);
        e_menu_item_check_set(mi, 1);
        e_menu_item_toggle_set(mi, types[t]);
        e_menu_item_callback_set(mi, _ds_menu_transition_type, (void*)(uintptr_t)t);
     }
}

EINTERN void
ds_config_init(void)
{
   maug = e_int_menus_menu_augmentation_add_sorted
     ("config/1",  D_("Desksanity"), _ds_menu_add, NULL, NULL, NULL);
}

EINTERN void
ds_config_shutdown(void)
{
   e_int_menus_menu_augmentation_del("config/1", maug);
   maug = NULL;
}
