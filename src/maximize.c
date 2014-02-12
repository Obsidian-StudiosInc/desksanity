#include "e_mod_main.h"

static Ecore_Event_Handler *eh = NULL;

static void
_ds_unmaximize(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;
   int x, y, w, h;

   e_comp_object_effect_set(ec->frame, "desksanity/maximize");
   e_comp_object_frame_xy_adjust(ec->frame, ec->saved.x, ec->saved.y, &x, &y);
   e_comp_object_frame_wh_adjust(ec->frame, ec->saved.w, ec->saved.h, &w, &h);
   e_comp_object_effect_params_set(ec->frame, 1, (int[]){ec->x - x, ec->y - y, (ec->x + ec->w) - (x + w), (ec->y + ec->h) - (y + h)}, 4);
   e_comp_object_effect_params_set(ec->frame, 0, (int[]){1}, 1);
   e_comp_object_effect_start(ec->frame, NULL, NULL);
}

static void
_ds_maximize(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;
   int x, y, w, h;

   e_comp_object_effect_set(ec->frame, "desksanity/maximize");
   e_comp_object_frame_xy_adjust(ec->frame, ec->saved.x, ec->saved.y, &x, &y);
   e_comp_object_frame_wh_adjust(ec->frame, ec->saved.w, ec->saved.h, &w, &h);
   e_comp_object_effect_params_set(ec->frame, 1, (int[]){x - ec->x, y - ec->y, (x + w) - (ec->x + ec->w), (y + h) - (ec->y + ec->h)}, 4);
   e_comp_object_effect_params_set(ec->frame, 0, (int[]){1}, 1);
   e_comp_object_effect_start(ec->frame, NULL, NULL);
}

static void
_ds_maximize_check(E_Client *ec)
{
   if (e_client_util_ignored_get(ec)) return;
   evas_object_smart_callback_add(ec->frame, "maximize_done", _ds_maximize, ec);
   evas_object_smart_callback_add(ec->frame, "unmaximize", _ds_unmaximize, ec);
}

static Eina_Bool
_ds_maximize_add(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Client *ev)
{
   _ds_maximize_check(ev->ec);
   return ECORE_CALLBACK_RENEW;
}

EINTERN void
maximize_init(void)
{
   E_Client *ec;

   E_CLIENT_FOREACH(e_comp_get(NULL), ec)
     _ds_maximize_check(ec);
   eh = ecore_event_handler_add(E_EVENT_CLIENT_ADD, (Ecore_Event_Handler_Cb)_ds_maximize_add, NULL);
}

EINTERN void
maximize_shutdown(void)
{
   E_Client *ec;

   E_CLIENT_FOREACH(e_comp_get(NULL), ec)
     {
        if (e_client_util_ignored_get(ec)) continue;
        evas_object_smart_callback_del(ec->frame, "maximize", _ds_maximize);
        evas_object_smart_callback_del(ec->frame, "unmaximize", _ds_unmaximize);
     }
   E_FREE_FUNC(eh, ecore_event_handler_del);
}
