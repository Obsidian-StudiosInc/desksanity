#include "e_mod_main.h"

static Ecore_Event_Handler *eh = NULL;

static void
_ds_unmaximize_post(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;
   Eina_Rectangle *rect;
   int x, y, w, h;
   double time = 0.15;

   if (e_comp_config_get()->match.disable_borders) return;
   if (e_comp_config_get()->fast_borders)
     time /= 2;
   w = ec->w, h = ec->h;
   evas_object_geometry_get(ec->frame, &x, &y, NULL, NULL);

   rect = evas_object_data_del(obj, "__DSUMAX");
   evas_object_geometry_set(obj, rect->x, rect->y, rect->w, rect->h);
   free(rect);
   efx_resize(ec->frame, EFX_EFFECT_SPEED_SINUSOIDAL, EFX_POINT(x, y), w, h, time, NULL, NULL);
}

static void
_ds_unmaximize(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;
   Eina_Rectangle *rect;

   if (e_comp_config_get()->match.disable_borders) return;

   rect = malloc(sizeof(Eina_Rectangle));
   rect->x = ec->x, rect->y = ec->y;
   rect->w = ec->w, rect->h = ec->h;
   evas_object_data_set(ec->frame, "__DSUMAX", rect);
}

static void
_ds_unmaximize_pre(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;
   Eina_Bool max;

   max = !!evas_object_data_del(ec->frame, "__DSMAX");
   if (max)
     efx_resize_stop(ec->frame);
   ec->maximize_override = 0;
}

static void
_ds_maximize_done(void *data, Efx_Map_Data *emd EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   E_Client *ec = data;

   ec->maximize_override = 0;
   evas_object_data_del(ec->frame, "__DSMAX");
}

static void
_ds_maximize(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;
   int x, y, w, h;
   int ecx, ecy, ecw, ech;
   double time = 0.2;

   if (e_comp_config_get()->match.disable_borders) return;
   if (e_comp_config_get()->fast_borders)
     time /= 2;
   ecx = ec->x, ecy = ec->y, ecw = ec->w, ech = ec->h;
   e_comp_object_frame_xy_adjust(ec->frame, ec->saved.x + ec->zone->x, ec->saved.y + ec->zone->y, &x, &y);
   evas_object_geometry_get(ec->frame, NULL, NULL, &w, &h);
   if ((!w) || (!h)) return; //new client, don't break the universe
   ec->maximize_override = 1;
   evas_object_geometry_set(ec->frame, x, y, w, h);
   efx_resize(ec->frame, EFX_EFFECT_SPEED_SINUSOIDAL, EFX_POINT(ecx, ecy), ecw, ech, time, _ds_maximize_done, ec);
   evas_object_data_set(ec->frame, "__DSMAX", (void*)1);
}

static void
_ds_maximize_check(E_Client *ec)
{
   if (e_client_util_ignored_get(ec)) return;
   evas_object_smart_callback_add(ec->frame, "maximize_done", _ds_maximize, ec);
   evas_object_smart_callback_add(ec->frame, "unmaximize_pre", _ds_unmaximize_pre, ec);
   evas_object_smart_callback_add(ec->frame, "unmaximize", _ds_unmaximize, ec);
   evas_object_smart_callback_add(ec->frame, "unmaximize_done", _ds_unmaximize_post, ec);
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
        evas_object_smart_callback_del(ec->frame, "maximize_done", _ds_maximize);
        evas_object_smart_callback_del(ec->frame, "unmaximize_pre", _ds_unmaximize_pre);
        evas_object_smart_callback_del(ec->frame, "unmaximize", _ds_unmaximize);
        evas_object_smart_callback_del(ec->frame, "unmaximize_done", _ds_unmaximize_post);
     }
   E_FREE_FUNC(eh, ecore_event_handler_del);
}
