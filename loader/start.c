#include <Elementary.h>
#include <e_gadget_types.h>

static E_Gadget_Site_Orient gorient;
static E_Gadget_Site_Anchor ganchor;
static char *menu_action;

static void
do_orient(Evas_Object *ly, E_Gadget_Site_Orient orient, E_Gadget_Site_Anchor anchor)
{
   char buf[4096];
   const char *s = "float";

   if (anchor & E_GADGET_SITE_ANCHOR_LEFT)
     {
        if (anchor & E_GADGET_SITE_ANCHOR_TOP)
          {
             switch (orient)
               {
                case E_GADGET_SITE_ORIENT_HORIZONTAL:
                  s = "top_left";
                  break;
                case E_GADGET_SITE_ORIENT_VERTICAL:
                  s = "left_top";
                  break;
                case E_GADGET_SITE_ORIENT_NONE:
                  s = "left_top";
                  break;
               }
          }
        else if (anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
          {
             switch (orient)
               {
                case E_GADGET_SITE_ORIENT_HORIZONTAL:
                  s = "bottom_left";
                  break;
                case E_GADGET_SITE_ORIENT_VERTICAL:
                  s = "left_bottom";
                  break;
                case E_GADGET_SITE_ORIENT_NONE:
                  s = "left_bottom";
                  break;
               }
          }
        else
          s = "left";
     }
   else if (anchor & E_GADGET_SITE_ANCHOR_RIGHT)
     {
        if (anchor & E_GADGET_SITE_ANCHOR_TOP)
          {
             switch (orient)
               {
                case E_GADGET_SITE_ORIENT_HORIZONTAL:
                  s = "top_right";
                  break;
                case E_GADGET_SITE_ORIENT_VERTICAL:
                  s = "right_top";
                  break;
                case E_GADGET_SITE_ORIENT_NONE:
                  s = "right_top";
                  break;
               }
          }
        else if (anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
          {
             switch (orient)
               {
                case E_GADGET_SITE_ORIENT_HORIZONTAL:
                  s = "bottom_right";
                  break;
                case E_GADGET_SITE_ORIENT_VERTICAL:
                  s = "right_bottom";
                  break;
                case E_GADGET_SITE_ORIENT_NONE:
                  s = "right_bottom";
                  break;
               }
          }
        else
          s = "right";
     }
   else if (anchor & E_GADGET_SITE_ANCHOR_TOP)
     s = "top";
   else if (anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
     s = "bottom";
   else
     {
        switch (orient)
          {
           case E_GADGET_SITE_ORIENT_HORIZONTAL:
             s = "horizontal";
             break;
           case E_GADGET_SITE_ORIENT_VERTICAL:
             s = "vertical";
             break;
           default: break;
          }
     }
   snprintf(buf, sizeof(buf), "e,state,orientation,%s", s);
   elm_layout_signal_emit(ly, buf, "e");
}

static void
_menu_cb_post(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   if (eina_streq(event_info, menu_action))
     elm_layout_signal_emit(data, "e,state,unfocused", "e");
}

static void
_button_cb_mouse_down(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Down *ev = event_info;

   if (ev->button != 1) return;
   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (!menu_action) return;
   ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
   evas_object_smart_callback_call(elm_win_get(obj), menu_action, "main");
   elm_layout_signal_emit(obj, "e,state,focused", "e");
}

static void
anchor_change(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   ganchor = (uintptr_t)event_info;
   do_orient(data, gorient, ganchor);
}

static void
orient_change(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   gorient = (uintptr_t)event_info;
   do_orient(data, gorient, ganchor);
}

static void
action_deleted(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   if (eina_streq(menu_action, event_info))
     {
        free(menu_action);
        menu_action = NULL;
     }
}

static void
action_return(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   fprintf(stderr, "AR RETURN: %s\n", (char*)event_info);
   menu_action = eina_strdup(event_info);
}

int
main(int argc, char *argv[])
{
   Evas_Object *win, *ly;

   elm_init(argc, (char**)argv);
   win = elm_win_add(NULL, "start", ELM_WIN_BASIC);
   elm_win_autodel_set(win, 1);
   elm_win_alpha_set(win, 1);
   ly = elm_layout_add(win);
   evas_object_size_hint_min_set(win, 100, 100);
   evas_object_size_hint_aspect_set(win, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   elm_layout_file_set(ly,
     elm_theme_group_path_find(NULL, "e/gadget/start/main"), "e/gadget/start/main");
   elm_win_resize_object_add(win, ly);
   evas_object_show(ly);
   evas_object_smart_callback_add(win, "gadget_site_anchor", anchor_change, ly);
   evas_object_smart_callback_add(win, "gadget_site_orient", orient_change, ly);
   evas_object_smart_callback_add(win, "gadget_action", action_return, NULL);
   evas_object_smart_callback_add(win, "gadget_action_end", _menu_cb_post, ly);
   evas_object_smart_callback_add(win, "gadget_action_deleted", action_deleted, NULL);
   evas_object_event_callback_add(ly, EVAS_CALLBACK_MOUSE_DOWN, _button_cb_mouse_down, NULL);
   evas_object_smart_callback_call(win, "gadget_action_request", "menu_show");
   evas_object_show(win);
   ecore_main_loop_begin();
   return 0;
}
