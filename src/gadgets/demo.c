#include "e_mod_main.h"
#include "gadget.h"
#include "bryce.h"

EINTERN Evas_Object *start_create(Evas_Object *parent, int *id EINA_UNUSED, Z_Gadget_Site_Orient orient);;

EINTERN void clock_init(void);
EINTERN void clock_shutdown(void);
EINTERN void ibar_init(void);
EINTERN void wireless_init(void);

static Eina_List *handlers;
static Evas_Object *rect;
static Eina_Bool added = 1;

static void
_gadget_desklock_del(void)
{
   e_desklock_hide();
}

static void
_edit_end()
{
   E_FREE_LIST(handlers, ecore_event_handler_del);
}

static Eina_Bool
_gadget_key_handler(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Event_Key *ev)
{
   if (eina_streq(ev->key, "Escape"))
     _gadget_desklock_del();
   return ECORE_CALLBACK_DONE;
}

static void
_gadget_mouse_up_handler()
{
   if (!added)
     _gadget_desklock_del();
   added = 0;
}

static void
_gadget_added()
{
   added = 1;
}

static Eina_Bool
_gadget_desklock_handler(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Comp_Object *ev)
{
   E_Notification_Notify n;
   int w, h;
   const char *name;
   Evas_Object *site, *editor, *comp_object;

   name = evas_object_name_get(ev->comp_object);
   if (!name) return ECORE_CALLBACK_RENEW;
   if (strncmp(name, "desklock", 8)) return ECORE_CALLBACK_RENEW;
   evas_object_layer_set(ev->comp_object, E_LAYER_POPUP - 1);
   site = z_gadget_site_auto_add(Z_GADGET_SITE_ORIENT_NONE, name);
   evas_object_smart_callback_add(site, "gadget_added", _gadget_added, NULL);
   evas_object_layer_set(site, E_LAYER_POPUP);
   editor = z_gadget_editor_add(e_comp->elm, site);
   comp_object = e_comp_object_util_add(editor, E_COMP_OBJECT_TYPE_POPUP);
   evas_object_resize(comp_object, 300 * e_scale, 300 * e_scale);
   e_comp_object_util_center(comp_object);
   evas_object_layer_set(comp_object, E_LAYER_POPUP);
   evas_object_show(comp_object);
   evas_object_size_hint_min_get(editor, &w, &h);
   evas_object_resize(comp_object, 300 * e_scale, h * e_scale);
   e_comp_object_util_center(comp_object);
   e_comp_object_util_del_list_append(ev->comp_object, comp_object);
   e_comp_object_util_del_list_append(ev->comp_object, rect);

   memset(&n, 0, sizeof(E_Notification_Notify));
   n.timeout = 3000;
   n.summary = _("Lockscreen Gadgets");
   n.body = _("Press Escape or click the background to exit.");
   n.urgency = E_NOTIFICATION_NOTIFY_URGENCY_NORMAL;
   e_notification_client_send(&n, NULL, NULL);
   return ECORE_CALLBACK_RENEW;
}

static void
_gadget_conf()
{
   rect = evas_object_rectangle_add(e_comp->evas);
   evas_object_event_callback_add(rect, EVAS_CALLBACK_DEL, _edit_end, NULL);
   evas_object_color_set(rect, 0, 0, 0, 0);
   evas_object_resize(rect, e_comp->w, e_comp->h);
   evas_object_layer_set(rect, E_LAYER_POPUP);
   evas_object_show(rect);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_COMP_OBJECT_ADD, _gadget_desklock_handler, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_KEY_DOWN, _gadget_key_handler, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_BUTTON_UP, _gadget_mouse_up_handler, NULL);
   e_desklock_demo();
}

static void
_bryce_edit_end(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   e_bindings_disabled_set(0);
   evas_object_hide(data);
   evas_object_del(data);
}

static Eina_Bool
_bryce_editor_key_down()
{
   return EINA_TRUE;
}


static void
_bryce_conf()
{
   Evas_Object *editor, *comp_object;
   E_Zone *zone;
   int x, y, w, h;

   zone = e_zone_current_get();
   x = zone->x, y = zone->y, w = zone->w, h = zone->h;
   e_bindings_disabled_set(1);
   editor = z_bryce_editor_add(e_comp->elm);

   evas_object_geometry_set(editor, x, y, w, h);
   comp_object = e_comp_object_util_add(editor, E_COMP_OBJECT_TYPE_NONE);
   evas_object_event_callback_add(editor, EVAS_CALLBACK_DEL, _bryce_edit_end, comp_object);
   evas_object_layer_set(comp_object, E_LAYER_POPUP);
   evas_object_show(comp_object);

   e_comp_object_util_autoclose(comp_object, NULL, _bryce_editor_key_down, NULL);
}

static void
_gadget_menu(void *d EINA_UNUSED, E_Menu *m)
{
   E_Menu_Item *mi;

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Lockscreen Gadgets"));
   e_util_menu_item_theme_icon_set(mi, "preferences-desktop-wallpaper");
   e_menu_item_callback_set(mi, _gadget_conf, NULL);

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Bryces"));
   //e_util_menu_item_theme_icon_set(mi, "preferences-desktop-wallpaper");
   e_menu_item_callback_set(mi, _bryce_conf, NULL);
}

EINTERN void
gadget_demo(void)
{
   Evas_Object *b, *site;

   if (!eina_streq(getenv("USER"), "cedric")) return;

   z_gadget_type_add("Start", start_create);
   clock_init();
   ibar_init();
   wireless_init();
   z_gadget_init();
   z_bryce_init();

   if (!e_config->null_container_win)
     {
        Eina_List *l;
        E_Config_Binding_Mouse *ebm;

        e_module_disable(e_module_find("connman"));
        EINA_LIST_FOREACH(e_bindings->mouse_bindings, l, ebm)
          {
             if (eina_streq(ebm->action, "window_move"))
               {
                  e_bindings_mouse_add(E_BINDING_CONTEXT_ANY, ebm->button, ebm->modifiers,
                                       ebm->any_mod, "gadget_move", NULL);
               }
             else if (eina_streq(ebm->action, "window_resize"))
               {
                  e_bindings_mouse_add(E_BINDING_CONTEXT_ANY, ebm->button, ebm->modifiers,
                                       ebm->any_mod, "gadget_resize", NULL);
               }
             else if (eina_streq(ebm->action, "window_menu"))
               {
                  e_bindings_mouse_add(E_BINDING_CONTEXT_ANY, ebm->button, ebm->modifiers,
                                       ebm->any_mod, "gadget_menu", NULL);
                  e_bindings_mouse_add(E_BINDING_CONTEXT_ANY, ebm->button, ebm->modifiers,
                                       ebm->any_mod, "bryce_menu", NULL);
               }
          }
        e_bindings_wheel_add(E_BINDING_CONTEXT_ANY, 0, 1, E_BINDING_MODIFIER_CTRL, 0, "bryce_resize", NULL);
        e_bindings_wheel_add(E_BINDING_CONTEXT_ANY, 0, -1, E_BINDING_MODIFIER_CTRL, 0, "bryce_resize", NULL);
        e_config->null_container_win = 1;
        ecore_job_add(_bryce_conf, NULL);
        e_config_save_queue();
     }

   //b = z_bryce_add(e_comp->elm, "demo");
   //site = z_bryce_site_get(b);

   //z_gadget_site_gadget_add(site, "Start", 0);
   //z_gadget_site_gadget_add(site, "Clock", 0);
   //z_gadget_site_gadget_add(site, "IBar", 0);
   //z_bryce_autosize_set(b, 1);
   //z_bryce_autohide_set(b, 1);

   e_int_menus_menu_augmentation_add_sorted("config/1", "Gadgets 2.0", _gadget_menu, NULL, NULL, NULL);
}
