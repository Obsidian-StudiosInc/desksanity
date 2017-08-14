#include <Elementary.h>

static Evas_Object *popup;
static Evas_Object *child;

static void
popup_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   popup = NULL;
}

static void
child_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   child = NULL;
}

static void
popup_unfocus(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   evas_object_del(obj);
}

static void
mouse_button(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info)
{
   Evas_Object *ic;
   Evas_Event_Mouse_Down *ev = event_info;
   char buf[PATH_MAX];
   int w, h;
   Evas_Object *win;
   Elm_Win_Type type = ELM_WIN_POPUP_MENU;

   if ((ev->button != 1) && (ev->button != 3)) return;
   if (ev->button == 3)
     {
        type = ELM_WIN_BASIC;
        if (child)
          {
             evas_object_del(child);
             return;
          }
     }
   else
     {
        if (popup)
          {
             evas_object_del(popup);
             return;
          }
     }
   win = elm_win_add(elm_win_get(obj), "win", type);
   elm_win_alpha_set(win, 1);
   if (ev->button == 3)
     {
        child = win;
        evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, child_del, NULL);
     }
   else
     {
        popup = win;
        evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, popup_del, NULL);
     }
   if (ev->button == 3)
     evas_object_event_callback_add(win, EVAS_CALLBACK_FOCUS_OUT, popup_unfocus, NULL);
   ic = elm_icon_add(win);
   snprintf(buf, sizeof(buf), "%s/images/bubble.png", elm_app_data_dir_get());
   elm_image_file_set(ic, buf, NULL);
   elm_image_object_size_get(ic, &w, &h);
   evas_object_size_hint_aspect_set(win, EVAS_ASPECT_CONTROL_BOTH, w, h);
   if (ev->button == 1)
     {
        elm_image_resizable_set(ic, EINA_FALSE, EINA_FALSE);
        elm_image_no_scale_set(ic, EINA_TRUE);
     }
   evas_object_size_hint_weight_set(ic, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(ic, 0.5, 0.5);
   evas_object_size_hint_min_set(ic, 100, 100);
   elm_win_resize_object_add(win, ic);
   evas_object_show(ic);
   evas_object_show(win);
}

int
main(int argc, char *argv[])
{
   Evas_Object *win, *ic;
   char buf[PATH_MAX];
   int w, h;

   elm_init(argc, (char**)argv);
   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   elm_app_info_set(main, "elementary", "images/logo.png");
   win = elm_win_add(NULL, "icon-transparent", ELM_WIN_BASIC);
   elm_win_title_set(win, "Icon Transparent");
   elm_win_autodel_set(win, EINA_TRUE);
   elm_win_alpha_set(win, EINA_TRUE);

   ic = elm_icon_add(win);
   snprintf(buf, sizeof(buf), "%s/images/logo.png", elm_app_data_dir_get());
   elm_image_file_set(ic, buf, NULL);
   elm_image_object_size_get(ic, &w, &h);
   evas_object_size_hint_aspect_set(win, EVAS_ASPECT_CONTROL_BOTH, w, h);
   if (argc > 1)
     {
        elm_image_resizable_set(ic, EINA_FALSE, EINA_FALSE);
        elm_image_no_scale_set(ic, EINA_TRUE);
     }
   evas_object_size_hint_weight_set(ic, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(ic, 0.5, 0.5);
   evas_object_size_hint_min_set(ic, 100, 100);
   elm_win_resize_object_add(win, ic);
   evas_object_show(ic);
   evas_object_event_callback_add(ic, EVAS_CALLBACK_MOUSE_DOWN, mouse_button, NULL);

   evas_object_show(win);
   ecore_main_loop_begin();
   return 0;
}
