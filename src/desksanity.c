#include "e_mod_main.h"

static E_Desk *desk_show = NULL;
static Evas_Object *dm_show = NULL;
static E_Desk *desk_hide = NULL;
static Evas_Object *dm_hide = NULL;

enum
{
   DS_PAN,
   DS_BATMAN,
   DS_ZOOM_IN,
   DS_ZOOM_OUT,
   DS_GROW,
   DS_LAST,
} DS_Type;

static void
_ds_end(void *data EINA_UNUSED, Efx_Map_Data *emd EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   /* hide/delete previous desk's mirror */
   evas_object_hide(dm_hide);
   E_FREE_FUNC(dm_hide, evas_object_del);
   desk_hide = NULL;

   /* trigger desk flip end if there's a current desk set */
   if (desk_show) e_desk_flip_end(desk_show);

   /* hide/delete current desk's mirror */
   evas_object_hide(dm_show);
   E_FREE_FUNC(dm_show, evas_object_del);
   desk_show = NULL;
}

static Evas_Object *
dm_add(E_Desk *desk)
{
   Evas_Object *o;

   /* add new mirror: not a pager or taskbar */
   o = e_deskmirror_add(desk, 0, 0);
   /* cover desk */
   evas_object_geometry_set(o, desk->zone->x, desk->zone->y, desk->zone->w, desk->zone->h);
   /* don't receive events */
   evas_object_pass_events_set(o, 1);
   /* clip to current screen */
   evas_object_clip_set(o, desk->zone->bg_clip_object);
   /* above all menus/popups/clients */
   evas_object_layer_set(o, E_LAYER_MENU + 100);
   evas_object_show(o);
   return o;
}

static void
_ds_show(E_Desk *desk, int dx, int dy)
{
   E_Client *ec;

   /* free existing mirror */
   E_FREE_FUNC(dm_show, evas_object_del);

   /* iterate all clients */
   E_CLIENT_FOREACH(desk->zone->comp, ec)
     {
        /* skip clients from other screens, iconic clients, and ignorable clients */
        if ((ec->desk->zone != desk->zone) || (ec->iconic) || e_client_util_ignored_get(ec)) continue;
        /* always keep user-moving clients visible */
        if (ec->moving)
          {
             e_client_desk_set(ec, desk);
             evas_object_show(ec->frame);
             continue;
          }
        /* skip clients from other desks and clients visible on all desks */
        if ((ec->desk != desk) || (ec->sticky)) continue;
        /* comp unignore the client */
        e_client_comp_hidden_set(ec, EINA_FALSE);
        ec->hidden = 0;
        evas_object_show(ec->frame);
     }
   desk_show = desk;
   /* create mirror for current desk */
   dm_show = dm_add(desk);
   evas_object_name_set(dm_show, "dm_show");

   /* pick a random flip */
   switch (rand() % DS_LAST)
     {
      int x, y, hx, hy, w, h;
      Evas_Object *o;

      case DS_PAN:
        switch (dx)
          {
           case -1: // left -> right
             x = desk->zone->x - desk->zone->w;
             hx = desk->zone->x + desk->zone->w;
             break;
           case 0: // X
             x = desk->zone->x;
             hx = desk->zone->x;
             break;
           case 1: // left <- right
             x = desk->zone->x + desk->zone->w;
             hx = desk->zone->x - desk->zone->w;
             break;
          }
        switch (dy)
          {
           case -1: // up -> down
             y = desk->zone->y - desk->zone->h;
             hy = desk->zone->y + desk->zone->h;
             break;
           case 0: // X
             y = desk->zone->y;
             hy = desk->zone->y;
             break;
           case 1: // up <- down
             y = desk->zone->y + desk->zone->h;
             hy = desk->zone->y - desk->zone->h;
             break;
          }
        evas_object_move(dm_show, x, y);
        efx_move(dm_hide, EFX_EFFECT_SPEED_DECELERATE, EFX_POINT(hx, hy), 0.2, NULL, NULL);
        efx_move(dm_show, EFX_EFFECT_SPEED_DECELERATE, EFX_POINT(desk->zone->x, desk->zone->y), 0.2, _ds_end, NULL);
        break;
      case DS_BATMAN:
        E_FREE_FUNC(dm_show, evas_object_del);
        evas_object_raise(dm_hide);
        efx_spin_start(dm_hide, 1080.0, NULL);
        efx_zoom(dm_hide, EFX_EFFECT_SPEED_LINEAR, 1.0, 0.00001, NULL, 0.4, _ds_end, NULL);
        break;
      case DS_ZOOM_IN:
        efx_zoom(dm_show, EFX_EFFECT_SPEED_LINEAR, 0.000001, 1.0, NULL, 0.4, _ds_end, NULL);
        break;
      case DS_ZOOM_OUT:
        E_FREE_FUNC(dm_show, evas_object_del);
        evas_object_raise(dm_hide);
        efx_zoom(dm_hide, EFX_EFFECT_SPEED_LINEAR, 1.0, 0.0000001, NULL, 0.4, _ds_end, NULL);
        break;
      case DS_GROW:
        x = hx = desk->zone->x;
        w = 1;
        if (dx == 1) // grow right to left
          x = desk->zone->x + desk->zone->w;
        else if (!dx)
          w = desk->zone->w;
        y = hy = desk->zone->y;
        h = 1;
        if (dy == 1) // grow bottom to top
          y = desk->zone->y + desk->zone->h;
        else if (!dy)
          h = desk->zone->h;
        o = evas_object_rectangle_add(e_comp_get(desk)->evas);
        evas_object_geometry_set(o, x, y, w, h);
        evas_object_clip_set(dm_show, o);
        evas_object_show(o);
        e_comp_object_util_del_list_append(dm_show, o);
        efx_resize(o, EFX_EFFECT_SPEED_LINEAR, EFX_POINT(hx, hy), desk->zone->w, desk->zone->h, 0.4, _ds_end, NULL);
        break;
      default: break;
     }
}

static void
_ds_hide(E_Desk *desk)
{
   E_Client *ec;

   E_FREE_FUNC(dm_hide, evas_object_del);
   E_CLIENT_FOREACH(desk->zone->comp, ec)
     {
        /* same as above */
        if ((ec->desk->zone != desk->zone) || (ec->iconic) || e_client_util_ignored_get(ec)) continue;
        if (ec->moving) continue;
        if ((ec->desk != desk) || (ec->sticky)) continue;
        /* comp hide clients */
        e_client_comp_hidden_set(ec, EINA_TRUE);
        ec->hidden = 1;
        evas_object_hide(ec->frame);
     }
   desk_hide = desk;
   /* create mirror for previous desk */
   dm_hide = dm_add(desk);
   evas_object_name_set(dm_hide, "dm_hide");
}


static void
_ds_flip(void *data EINA_UNUSED, E_Desk *desk, int dx, int dy, Eina_Bool show)
{
   /* this is called for desk hide, then for desk show. always in that order. always. */
   if (show)
     _ds_show(desk, dx, dy);
   else
     _ds_hide(desk);
}

EINTERN void
ds_init(void)
{
   /* set a desk flip replacement callback */
   e_desk_flip_cb_set(_ds_flip, NULL);
}

EINTERN void
ds_shutdown(void)
{
   e_desk_flip_cb_set(NULL, NULL);
   _ds_end(NULL, NULL, NULL);
}
