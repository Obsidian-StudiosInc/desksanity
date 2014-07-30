#include "e_mod_main.h"

static E_Int_Menu_Augmentation *maug = NULL;


static void
_ds_menu_ruler(void *data EINA_UNUSED, E_Menu *m EINA_UNUSED, E_Menu_Item *mi)
{
   ds_config->disable_ruler = mi->toggle;
   if (ds_config->disable_ruler)
     mr_shutdown();
   else
     mr_init();
}

static void
_ds_menu_maximize(void *data EINA_UNUSED, E_Menu *m EINA_UNUSED, E_Menu_Item *mi)
{
   ds_config->disable_maximize = mi->toggle;
   if (ds_config->disable_maximize)
     maximize_shutdown();
   else
     maximize_init();
}

static void
_ds_menu_add(void *data EINA_UNUSED, E_Menu *m)
{
   E_Menu_Item *mi;
   E_Menu *subm;

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, D_("Desksanity"));
   e_menu_item_icon_edje_set(mi, mod->edje_file, "icon");

   subm = e_menu_new();
   e_menu_title_set(subm, D_("Options"));
   e_menu_item_submenu_set(mi, subm);

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
