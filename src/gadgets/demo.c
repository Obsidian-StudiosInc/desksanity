#include "e_mod_main.h"
#include "gadget.h"
#include "bryce.h"

EINTERN Evas_Object *start_create(Evas_Object *parent, int *id EINA_UNUSED, Z_Gadget_Site_Orient orient);;

EINTERN void clock_init(void);
EINTERN void clock_shutdown(void);
EINTERN void ibar_init(void);

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
_gadget_menu(void *d EINA_UNUSED, E_Menu *m)
{
   E_Menu_Item *mi;

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Gadgets 2.0"));
   e_util_menu_item_theme_icon_set(mi, "preferences-desktop-wallpaper");
   e_menu_item_callback_set(mi, _gadget_conf, NULL);
}

EINTERN void
gadget_demo(void)
{
   Evas_Object *b, *site;

   if (!eina_streq(getenv("USER"), "zmike")) return;

   if (e_comp->w > 1200) return;
   z_gadget_type_add("Start", start_create);
   clock_init();
   ibar_init();
   z_gadget_init();
   z_bryce_init();

   //b = z_bryce_add(e_comp->elm, "demo");
   //site = z_bryce_site_get(b);

   //z_gadget_site_gadget_add(site, "Start", 0);
   //z_gadget_site_gadget_add(site, "Clock", 0);
   //z_gadget_site_gadget_add(site, "IBar", 0);
   //z_bryce_autosize_set(b, 1);
   //z_bryce_autohide_set(b, 1);

   e_int_menus_menu_augmentation_add_sorted("config/1", "Gadgets 2.0", _gadget_menu, NULL, NULL, NULL);
}
