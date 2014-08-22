#include "e_mod_main.h"

static E_Action *act_magnify = NULL;
static int current_mag = -1;
static Evas_Object **magnifiers = {NULL};
static Evas_Object *clip = NULL;

static Ecore_Event_Handler *handler = NULL;
static Ecore_Event_Handler *deskflip = NULL;
static Ecore_Timer *timer = NULL;

#define MAG_SIZE_FACTOR 10

static void
_magnify_end(void)
{
   unsigned int n;
   E_Comp *comp;

   if (!magnifiers) return;
   comp = e_comp_get(NULL);
   for (n = 0; n < eina_list_count(comp->zones); n++)
     E_FREE_FUNC(magnifiers[n], evas_object_del);
   E_FREE(magnifiers);
   E_FREE_FUNC(clip, evas_object_del);
   E_FREE_FUNC(handler, ecore_event_handler_del);
   E_FREE_FUNC(timer, ecore_timer_del);
   current_mag = -1;
}

static void
_current_update(int n)
{
   if (current_mag != -1)
     efx_zoom_reset(magnifiers[current_mag]);
   current_mag = n;
}

static void
_magnify_update(int x, int y)
{
   int w, h;
   E_Zone *zone;

   zone = e_comp_zone_xy_get(e_comp_get(NULL), x, y);
   if ((int)zone->num != current_mag)
     _current_update(zone->num);

   w = zone->w / MAG_SIZE_FACTOR;
   h = zone->h / MAG_SIZE_FACTOR;
   evas_object_geometry_set(clip, x - (w / 2), y - (h / 2), w, h);
   efx_zoom(magnifiers[zone->num], EFX_EFFECT_SPEED_LINEAR, 0, 2.0, EFX_POINT(x, y), 0, NULL, NULL);
}

static Eina_Bool
_magnify_move(void *data EINA_UNUSED, int t EINA_UNUSED, Ecore_Event_Mouse_Move *ev)
{
   _magnify_update(ev->root.x, ev->root.y);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_magnify_poll(void *d EINA_UNUSED)
{
   int x, y;

   ecore_evas_pointer_xy_get(e_comp_get(NULL)->ee, &x, &y);
   _magnify_update(x, y);
   return ECORE_CALLBACK_RENEW;
}

static void
_magnify_new(E_Desk *desk)
{
   int n = desk->zone->num;

   magnifiers[n] = e_deskmirror_add(desk, 0, 0);
   evas_object_pass_events_set(magnifiers[n], 1);
   evas_object_data_set(magnifiers[n], "comp_skip", (void*)1);
   evas_object_geometry_set(magnifiers[n], desk->zone->x, desk->zone->y, desk->zone->w, desk->zone->h);
   evas_object_layer_set(magnifiers[n], E_LAYER_MENU + 1);
   evas_object_show(magnifiers[n]);

   evas_object_clip_set(magnifiers[n], clip);
}

static void
_magnify_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   E_Comp *comp;
   E_Zone *zone;
   unsigned int n;
   int x, y, w, h;
   Eina_List *l;

   comp = e_comp_get(NULL);
   if (magnifiers)
     {
        _magnify_end();
        return;
     }

   clip = evas_object_rectangle_add(comp->evas);
   evas_object_show(clip);
   ecore_evas_pointer_xy_get(comp->ee, &x, &y);
   magnifiers = malloc(sizeof(void*) * eina_list_count(comp->zones));
   for (n = 0, l = comp->zones, zone = eina_list_data_get(l);
     n < eina_list_count(comp->zones);
     n++, l = eina_list_next(l), zone = eina_list_data_get(l))
     {
        _magnify_new(e_desk_current_get(zone));

        evas_object_clip_set(magnifiers[n], clip);
        if (zone != e_zone_current_get(comp)) continue;
        w = zone->w / MAG_SIZE_FACTOR;
        h = zone->h / MAG_SIZE_FACTOR;
        evas_object_geometry_set(clip, x - (w / 2), y - (h / 2), w, h);
        _current_update(n);
     }
   if (comp->comp_type == E_PIXMAP_TYPE_X)
     timer = ecore_timer_add(0.05, _magnify_poll, NULL);
   else
     handler = ecore_event_handler_add(ECORE_EVENT_MOUSE_MOVE, (Ecore_Event_Handler_Cb)_magnify_move, NULL);
}

static Eina_Bool
_magnify_deskflip(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Desk_Show *ev)
{
   if (!magnifiers) return ECORE_CALLBACK_RENEW;
   evas_object_del(magnifiers[ev->desk->zone->num]);
   _magnify_new(ev->desk);
   _current_update(ev->desk->zone->num);
   return ECORE_CALLBACK_RENEW;
}

EINTERN void
mag_init(void)
{
   act_magnify = e_action_add("magnify");
   if (act_magnify)
     {
        act_magnify->func.go = _magnify_cb;
        e_action_predef_name_set(D_("Compositor"), D_("Toggle magnification"),
                                 "magnify", NULL, NULL, 0);
     }
   deskflip = ecore_event_handler_add(E_EVENT_DESK_SHOW, (Ecore_Event_Handler_Cb)_magnify_deskflip, NULL);
}

EINTERN void
mag_shutdown(void)
{
   _magnify_end();
   e_action_predef_name_del(D_("Compositor"), D_("Toggle magnification"));
   e_action_del("magnify");
   act_magnify = NULL;
   E_FREE_FUNC(deskflip, ecore_event_handler_del);
}
