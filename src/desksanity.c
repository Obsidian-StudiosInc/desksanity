#include "e_mod_main.h"

static E_Desk *desk_show = NULL;
static Evas_Object *dm_show = NULL;
static E_Desk *desk_hide = NULL;
static Evas_Object *dm_hide = NULL;

static DS_Type cur_type = DS_PAN;

static void
_ds_end(void *data EINA_UNUSED, Efx_Map_Data *emd EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   /* hide/delete previous desk's mirror */
   evas_object_hide(dm_hide);
   E_FREE_FUNC(dm_hide, evas_object_del);
   desk_hide = NULL;

   /* trigger desk flip end if there's a current desk set */
   if (desk_show)
     {
        e_desk_flip_end(desk_show);
        e_comp_shape_queue_block(e_comp, 0);
     }

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
_ds_blink2(void *data EINA_UNUSED, Efx_Map_Data *emd EINA_UNUSED, Evas_Object *obj)
{
   E_FREE_FUNC(dm_hide, evas_object_del);
   evas_object_show(dm_show);
   efx_resize(obj, EFX_EFFECT_SPEED_DECELERATE,
     EFX_POINT(desk_show->zone->x, desk_show->zone->y),
     desk_show->zone->w, desk_show->zone->h,
     0.45, _ds_end, NULL);
}

static void
_ds_show(E_Desk *desk, int dx, int dy)
{
   E_Client *ec;
   DS_Type use_type;
   DS_Type *disabled_types = (DS_Type*)&ds_config->types;

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
        e_comp_object_effect_unclip(ec->frame);
        e_comp_object_effect_set(ec->frame, NULL);
        /* comp unignore the client */
        e_client_comp_hidden_set(ec, EINA_FALSE);
        ec->hidden = 0;
        evas_object_show(ec->frame);
     }
   if (ds_config->disabled_transition_count == DS_LAST)
     {
        e_desk_flip_end(desk);
        return;
     }
   desk_show = desk;

   e_comp_shape_queue_block(e_comp, 1);
   /* guarantee that the user gets to see each flip
    * at least once
    */
   if (cur_type < DS_LAST)
     use_type = cur_type++;
   else
     use_type = rand() % DS_LAST;
   while (disabled_types[use_type])
     {
        use_type++;
        if (use_type == DS_LAST)
          {
             cur_type = DS_LAST;
             use_type = DS_PAN;
          }
     }

   /* pick a random flip */
   switch (use_type)
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
        dm_show = dm_add(desk);
        evas_object_move(dm_show, x, y);
        efx_move(dm_hide, EFX_EFFECT_SPEED_DECELERATE, EFX_POINT(hx, hy), 0.2, NULL, NULL);
        efx_move(dm_show, EFX_EFFECT_SPEED_DECELERATE, EFX_POINT(desk->zone->x, desk->zone->y), 0.2, _ds_end, NULL);
        break;
      case DS_FADE_OUT:
        efx_fade(dm_hide, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(0, 0, 0), 0, 0.25, _ds_end, NULL);
        break;
      case DS_FADE_IN:
        E_FREE_FUNC(dm_hide, evas_object_del);
        dm_show = dm_add(desk);
        efx_fade(dm_show, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(0, 0, 0), 0, 0.0, NULL, NULL);
        efx_fade(dm_show, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(255, 255, 255), 255, 0.25, _ds_end, NULL);
        break;
      case DS_BATMAN:
        evas_object_raise(dm_hide);
        efx_spin_start(dm_hide, 1080.0, NULL);
        efx_zoom(dm_hide, EFX_EFFECT_SPEED_LINEAR, 1.0, 0.00001, NULL, 0.4, _ds_end, NULL);
        break;
      case DS_ZOOM_IN:
        dm_show = dm_add(desk);
        efx_zoom(dm_show, EFX_EFFECT_SPEED_LINEAR, 0.000001, 1.0, NULL, 0.4, _ds_end, NULL);
        break;
      case DS_ZOOM_OUT:
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
        dm_show = dm_add(desk);
        o = evas_object_rectangle_add(e_comp->evas);
        evas_object_geometry_set(o, x, y, w, h);
        evas_object_clip_set(dm_show, o);
        evas_object_show(o);
        e_comp_object_util_del_list_append(dm_show, o);
        efx_resize(o, EFX_EFFECT_SPEED_LINEAR, EFX_POINT(hx, hy), desk->zone->w, desk->zone->h, 0.4, _ds_end, NULL);
        break;
      case DS_ROTATE_OUT:
        efx_move_circle(dm_hide, EFX_EFFECT_SPEED_LINEAR, EFX_POINT(desk->zone->x + (desk->zone->w / 2), desk->zone->y + (desk->zone->h / 2)),
          720, 0.4, NULL, NULL);
        efx_resize(dm_hide, EFX_EFFECT_SPEED_LINEAR, NULL, 1, 1, 0.4, _ds_end, NULL);
        break;
      case DS_ROTATE_IN:
        dm_show = dm_add(desk);
        evas_object_resize(dm_show, 1, 1);
        efx_move_circle(dm_show, EFX_EFFECT_SPEED_LINEAR, EFX_POINT(desk->zone->x + (desk->zone->w / 2), desk->zone->y + (desk->zone->h / 2)),
          720, 0.4, NULL, NULL);
        efx_resize(dm_show, EFX_EFFECT_SPEED_LINEAR, NULL, desk->zone->w, desk->zone->h, 0.4, _ds_end, NULL);
        break;
      case DS_SLIDE_SPLIT:
      {
         Evas_Object *dmh, *clip;
         Evas_Point exy;
         unsigned int i, num;

         num = (rand() % 4) + 2;

         dmh = dm_hide;
         /* create other parts of split */
         w = desk->zone->w;
         h = desk->zone->h;
         if (dy)
           w /= num;
         else
           h /= num;
         for (i = 0; i < num; i++)
           {
              if (!dmh)
                {
                   /* create other deskmirrors where necessary */
                   dmh = dm_add(desk_hide);
                   e_comp_object_util_del_list_append(dm_hide, dmh);
                }
              clip = evas_object_rectangle_add(e_comp->evas);
              e_comp_object_util_del_list_append(dm_hide, clip);
              if (dx)
                evas_object_geometry_set(clip, desk->zone->x, desk->zone->y + (i * h), w, h);
              else
                evas_object_geometry_set(clip, desk->zone->x + (i * w), desk->zone->y, w, h);
              evas_object_clip_set(dmh, clip);
              evas_object_show(clip);
              exy.x = desk->zone->x;
              exy.y = desk->zone->y;
              if (dx)
                {
                   if (i % 2 == 0)
                     exy.x = desk_show->zone->x - (dx * desk_show->zone->w);
                   else
                     exy.x = desk_show->zone->x + (dx * desk_show->zone->w);
                }
              if (dy)
                {
                   if (i % 2 == 0)
                     exy.y = desk_show->zone->y - (dy * desk_show->zone->h);
                   else
                     exy.y = desk_show->zone->y + (dy * desk_show->zone->h);
                }
              efx_move(dmh, EFX_EFFECT_SPEED_ACCELERATE,
                &exy, 0.5, (i == (num - 1)) ? _ds_end : NULL, NULL);
              dmh = NULL;
           }
      }
        break;
      case DS_QUAD_SPLIT:
      {
         int i;
         Evas_Object *dmh[4] = {NULL};
         Evas_Object *clip[4];
         /* set up quadrant position coords */
         Evas_Point cxy[4] = {{desk->zone->x, desk->zone->y},
                              {desk->zone->x + (desk->zone->w / 2), desk->zone->y},
                              {desk->zone->x, desk->zone->y + (desk->zone->h / 2)},
                              {desk->zone->x + (desk->zone->w / 2), desk->zone->y + (desk->zone->h / 2)}
                             };
         /* set up quadrant effect coords */
         Evas_Point exy[4] = {{desk->zone->x - desk->zone->w, desk->zone->y - desk->zone->h},
                              {desk->zone->x + (desk->zone->w * 2), desk->zone->y - desk->zone->h},
                              {desk->zone->x - desk->zone->w, desk->zone->y + (desk->zone->h / 2)},
                              {desk->zone->x + (desk->zone->w * 2), desk->zone->y + (desk->zone->h * 2)}
                             };

         dmh[0] = dm_hide;
         for (i = 0; i < 4; i++)
           {
              if (!dmh[i])
                {
                   /* create other 3 quadrants */
                   dmh[i] = dm_add(desk_hide);
                   e_comp_object_util_del_list_append(dm_hide, dmh[i]);
                }
              /* clip the quad */
              clip[i] = evas_object_rectangle_add(e_comp->evas);
              e_comp_object_util_del_list_append(dm_hide, clip[i]);
              /* lay out 2x2 grid */
              evas_object_geometry_set(clip[i], cxy[i].x, cxy[i].y, desk->zone->w / 2, desk->zone->h / 2);
              evas_object_clip_set(dmh[i], clip[i]);
              evas_object_show(clip[i]);
              /* apply effect coords */
              efx_move(clip[i], EFX_EFFECT_SPEED_ACCELERATE,
                &exy[i], 0.8, (i == 3) ? _ds_end : NULL, NULL);
           }
      }
        break;
      case DS_QUAD_MERGE:
      {
         int i;
         Evas_Object *dmh[4] = {NULL};
         Evas_Object *clip[4];
         /* set up quadrant position coords */
         Evas_Point cxy[4] = {{desk->zone->x, desk->zone->y},
                              {desk->zone->x + (desk->zone->w / 2), desk->zone->y},
                              {desk->zone->x, desk->zone->y + (desk->zone->h / 2)},
                              {desk->zone->x + (desk->zone->w / 2), desk->zone->y + (desk->zone->h / 2)}
                             };

         dmh[0] = dm_hide;
         for (i = 0; i < 4; i++)
           {
              if (!dmh[i])
                {
                   /* create other 3 quadrants */
                   dmh[i] = dm_add(desk_hide);
                   e_comp_object_util_del_list_append(dm_hide, dmh[i]);
                }
              /* clip the quad */
              clip[i] = evas_object_rectangle_add(e_comp->evas);
              e_comp_object_util_del_list_append(dm_hide, clip[i]);
              /* lay out 2x2 grid */
              evas_object_geometry_set(clip[i], cxy[i].x, cxy[i].y, desk->zone->w / 2, desk->zone->h / 2);
              evas_object_clip_set(dmh[i], clip[i]);
              evas_object_show(clip[i]);
              /* resize all quads to 1x1 while moving towards center */
              efx_resize(clip[i], EFX_EFFECT_SPEED_ACCELERATE,
                EFX_POINT(desk->zone->x + (desk->zone->w / 2), desk->zone->y + (desk->zone->h / 2)),
                1, 1 ,0.8, (i == 3) ? _ds_end : NULL, NULL);
           }
      }
        break;
      case DS_BLINK:
      {
         Evas_Object *clip, *bg;

         dm_show = dm_add(desk);
         evas_object_geometry_set(dm_show, desk->zone->x, desk->zone->y, desk->zone->w, desk->zone->h);
         evas_object_hide(dm_show);

         bg = evas_object_rectangle_add(e_comp->evas);
         e_comp_object_util_del_list_append(dm_show, bg);
         evas_object_color_set(bg, 0, 0, 0, 255);
         evas_object_layer_set(bg, E_LAYER_MENU + 99);
         evas_object_geometry_set(bg, desk->zone->x, desk->zone->y, desk->zone->w, desk->zone->h);
         evas_object_show(bg);
         clip = evas_object_rectangle_add(e_comp->evas);
         e_comp_object_util_del_list_append(dm_show, clip);
         /* fit clipper to zone */
         evas_object_geometry_set(clip, desk->zone->x, desk->zone->y, desk->zone->w, desk->zone->h);
         evas_object_clip_set(dm_hide, clip);
         evas_object_clip_set(dm_show, clip);
         evas_object_show(clip);
         /* resize clip to 1px high while moving towards center */
         efx_resize(clip, EFX_EFFECT_SPEED_ACCELERATE,
           EFX_POINT(desk->zone->x, desk->zone->y + (desk->zone->h / 2)),
           desk->zone->w, 1, 0.45, _ds_blink2, NULL);
      }
        break;
      case DS_VIEWPORT:
      {
         Evas_Object *clip;

         clip = evas_object_rectangle_add(e_comp->evas);
         /* fit clipper to zone */
         evas_object_geometry_set(clip, desk->zone->x, desk->zone->y, desk->zone->w, desk->zone->h);
         evas_object_clip_set(dm_hide, clip);
         e_comp_object_util_del_list_append(dm_hide, clip);
         evas_object_show(clip);
         /* resize clip to 1x1 while moving towards center */
         efx_resize(clip, EFX_EFFECT_SPEED_DECELERATE,
           EFX_POINT(desk->zone->x + (desk->zone->w / 2), desk->zone->y + (desk->zone->h / 2)),
           1, 1, 0.6, _ds_end, NULL);
      }
        break;
      default: break;
     }
   if (dm_show) evas_object_name_set(dm_show, "dm_show");
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
   if (ds_config->disabled_transition_count == DS_LAST) return;
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
   cur_type = DS_PAN;
}
