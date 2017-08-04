#include <Elementary.h>

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

   evas_object_show(win);
   ecore_main_loop_begin();
   return 0;
}
