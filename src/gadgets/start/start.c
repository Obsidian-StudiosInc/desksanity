/**
 * @addtogroup Optional_Gadgets
 * @{
 *
 * @defgroup Module_Start Start Button
 *
 * Shows a "start here" button or icon.
 *
 * @}
 */

#include "gadget.h"

/* actual module specifics */
typedef struct _Instance Instance;

struct _Instance
{
   Evas_Object     *o_button;
   E_Menu          *main_menu;
};

static void
orient(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   char buf[4096];
   const char *s = "float";

   switch (z_gadget_site_gravity_get(obj))
     {
      case Z_GADGET_SITE_GRAVITY_LEFT:
        s = "left";
        break;

      case Z_GADGET_SITE_GRAVITY_RIGHT:
        s = "right";
        break;

      case Z_GADGET_SITE_GRAVITY_TOP:
        s = "top";
        break;

      case Z_GADGET_SITE_GRAVITY_BOTTOM:
        s = "bottom";
        break;

      default:
        s = "none";
        break;
     }
   snprintf(buf, sizeof(buf), "e,state,orientation,%s", s);
   elm_layout_signal_emit(inst->o_button, buf, "e");
}

static void
_menu_cb_post(void *data, E_Menu *m)
{
   Instance *inst = data;
   Eina_Bool fin;

   if (stopping || (!inst->main_menu)) return;
   fin = m == inst->main_menu;
   e_object_del(E_OBJECT(m));
   if (!fin) return;
   /* FIXME
   e_gadcon_locked_set(inst->gcc->gadcon, 0);
   */
   elm_layout_signal_emit(inst->o_button, "e,state,unfocused", "e");
   inst->main_menu = NULL;
}

static void
_button_cb_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event_info;
   Evas_Coord x, y, w, h;
   int cx, cy;
   Evas_Object *win;

   if (ev->button != 1) return;

   evas_object_geometry_get(inst->o_button, &x, &y, &w, &h);
   win = e_win_evas_object_win_get(inst->o_button);
   evas_object_geometry_get(win, &cx, &cy, NULL, NULL);
   x += cx;
   y += cy;
   if (!inst->main_menu)
     inst->main_menu = e_int_menus_main_new();
   if (!inst->main_menu) return;
   e_menu_post_deactivate_callback_set(inst->main_menu,
                                       _menu_cb_post, inst);
   /* FIXME
   e_gadcon_locked_set(inst->gcc->gadcon, 1);
   */
   e_menu_activate_mouse(inst->main_menu,
                         e_zone_current_get(),
                         x, y, w, h, E_MENU_POP_DIRECTION_AUTO, ev->timestamp);
   elm_layout_signal_emit(inst->o_button, "e,state,focused", "e");
}

static void
start_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->main_menu)
     {
        e_menu_post_deactivate_callback_set(inst->main_menu, NULL, NULL);
        e_object_del(E_OBJECT(inst->main_menu));
     }
   free(inst);
}

EINTERN Evas_Object *
start_create(Evas_Object *parent, unsigned int *id EINA_UNUSED)
{
   Evas_Object *o;
   Instance *inst;

   inst = E_NEW(Instance, 1);

   o = elm_layout_add(parent);

   e_theme_edje_object_set(o, "base/theme/modules/start",
                           "e/modules/start/main");
   elm_layout_signal_emit(o, "e,state,unfocused", "e");

   inst->o_button = o;

   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _button_cb_mouse_down, inst);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, start_del, inst);
   evas_object_smart_callback_add(parent, "gadget_gravity", orient, inst);

   return o;
}
#if 0
/* module setup */
E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Start"
};

E_API void *
e_modapi_init(E_Module *m)
{
   z_gadget_type_add("Start", start_create);
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   z_gadget_type_del("Start");
   return 1;
}
#endif
