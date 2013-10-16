#include "e_mod_main.h"

#define EC_HOOK_COUNT 6

static Evas_Object *mr_line_x = NULL;
static Evas_Object *move_text_x = NULL;
static Evas_Object *mr_line_y = NULL;
static Evas_Object *move_text_y = NULL;

static Evas_Object *resize_text = NULL;
static Evas_Object *resize_text_start = NULL;
static Eina_Rectangle resize_start = {0};
static Evas_Object *resize_rect[4] = {NULL};


static Evas_Object *fade_obj = NULL;
static E_Client *client = NULL;

static E_Client_Hook *ec_hooks[EC_HOOK_COUNT] = {NULL};

static void
clear_all(void)
{
   E_FREE_FUNC(fade_obj, evas_object_del);
   E_FREE_FUNC(mr_line_x, evas_object_del);
   E_FREE_FUNC(mr_line_y, evas_object_del);
   E_FREE_FUNC(move_text_x, evas_object_del);
   E_FREE_FUNC(move_text_y, evas_object_del);

   E_FREE_FUNC(resize_text, evas_object_del);
   E_FREE_FUNC(resize_text_start, evas_object_del);
   E_FREE_FUNC(resize_rect[0], evas_object_del);
   E_FREE_FUNC(resize_rect[1], evas_object_del);
   E_FREE_FUNC(resize_rect[2], evas_object_del);
   E_FREE_FUNC(resize_rect[3], evas_object_del);

   if (client && (!e_object_is_del(E_OBJECT(client))))
     {
        evas_object_layer_set(client->frame, client->layer);
        client->layer_block = 0;
     }
   client = NULL;
}

static void
fade_end(void *d EINA_UNUSED, Efx_Map_Data *emd EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   e_comp_shape_queue_block(client->comp, 0);
   clear_all();
}

static void
move_x_update(E_Client *ec)
{
   char buf[128];
   int w, h;
   E_Zone *zone;

   zone = e_comp_zone_xy_get(ec->comp, ec->x, ec->y);
   if (!zone) zone = ec->zone;

   if (evas_object_clip_get(mr_line_x) != zone->bg_clip_object)
     {
        evas_object_clip_set(mr_line_x, zone->bg_clip_object);
        efx_reclip(mr_line_x);
     }
   evas_object_line_xy_set(mr_line_x, ec->x, zone->y, ec->x, ec->y);

   if (zone->x == 0)
     snprintf(buf, sizeof(buf), "%d", ec->x);
   else
     snprintf(buf, sizeof(buf), "%d (%d)", ec->x - zone->x, ec->x);
   edje_object_part_text_set(move_text_x, "e.text", buf);
   edje_object_size_min_calc(move_text_x, &w, &h);
   evas_object_geometry_set(move_text_x, ec->x - w - 2, ec->y + 2, w, h);
}

static void
move_y_update(E_Client *ec)
{
   char buf[128];
   int w, h;
   E_Zone *zone;

   zone = e_comp_zone_xy_get(ec->comp, ec->x, ec->y);
   if (!zone) zone = ec->zone;

   if (evas_object_clip_get(mr_line_y) != zone->bg_clip_object)
     {
        evas_object_clip_set(mr_line_y, zone->bg_clip_object);
        efx_reclip(mr_line_y);
     }
   evas_object_line_xy_set(mr_line_y, zone->x, ec->y, ec->x, ec->y);

   if (zone->y == 0)
     snprintf(buf, sizeof(buf), "%d", ec->y);
   else
     snprintf(buf, sizeof(buf), "%d (%d)", ec->y - zone->y, ec->y);
   edje_object_part_text_set(move_text_y, "e.text", buf);
   edje_object_size_min_calc(move_text_y, &w, &h);
   evas_object_geometry_set(move_text_y, ec->x + 2, ec->y - h - 2, w, h);
}

static void
resize_text_update(E_Client *ec)
{
   char buf[128];
   int x, y, w, h, fw, fh;

   e_moveresize_client_extents(ec, &w, &h);
   e_comp_object_frame_wh_adjust(ec->frame, w, h, &fw, &fh);
   if ((ec->w != fw) || (ec->h != fh))
     snprintf(buf, sizeof(buf), "%dx%d (%dx%d)", w, h, ec->w, ec->h);
   else
     snprintf(buf, sizeof(buf), "%dx%d", ec->w, ec->h);
   edje_object_part_text_set(resize_text, "e.text", buf);
   edje_object_size_min_calc(resize_text, &w, &h);
   evas_object_resize(resize_text, w, h);
   switch (ec->resize_mode)
     {
      case E_POINTER_RESIZE_TL:
        x = ec->x;
        y = ec->y - h;
        break;
      case E_POINTER_RESIZE_T:
        x = ec->moveinfo.down.mx - (w / 2);
        x = E_CLAMP(x, ec->x, ec->x + ec->w - w);
        y = ec->y - h;
        break;
      case E_POINTER_RESIZE_TR:
        x = ec->x + ec->w - w;
        y = ec->y - h;
        break;
      case E_POINTER_RESIZE_R:
        x = ec->x + ec->w - w;
        y = ec->moveinfo.down.my - (h / 2);
        y = E_CLAMP(y, ec->y, ec->y + ec->h - h);
        break;
      case E_POINTER_RESIZE_BR:
        x = ec->x + ec->w - w;
        y = ec->y + ec->h - h;
        break;
      case E_POINTER_RESIZE_B:
        x = ec->moveinfo.down.mx - (w / 2);
        x = E_CLAMP(x, ec->x, ec->x + ec->w - w);
        y = ec->y + ec->h - h;
        break;
      case E_POINTER_RESIZE_BL:
        x = ec->x;
        y = ec->y + ec->h - h;
        break;
      case E_POINTER_RESIZE_L:
        x = ec->x;
        y = ec->moveinfo.down.my - (h / 2);
        y = E_CLAMP(y, ec->y, ec->y + ec->h - h);
        break;
      case E_POINTER_MOVE:
      case E_POINTER_RESIZE_NONE:
      default:
        e_comp_object_util_center_on(resize_text, ec->frame);
        return;
     }
   evas_object_move(resize_text, x, y);
}

static Evas_Object *
text_add(Evas *e)
{
   Evas_Object *o;

   o = edje_object_add(e);
   evas_object_layer_set(o, E_LAYER_MENU + 2);
   edje_object_file_set(o, mod->edje_file, "e/modules/desksanity/moveresize");
   evas_object_show(o);
   return o;
}

static Evas_Object *
line_add(Evas *e)
{
   Evas_Object *o;

   o = evas_object_line_add(e);
   evas_object_layer_set(o, E_LAYER_MENU + 2);
   evas_object_color_set(o, 51, 153, 255, 255);
   evas_object_show(o);
   return o;
}

static void
fade_setup(E_Client *ec)
{
   fade_obj = evas_object_rectangle_add(ec->comp->evas);
   evas_object_name_set(fade_obj, "fade_obj");
   evas_object_geometry_set(fade_obj, 0, 0, ec->comp->man->w, ec->comp->man->h);
   evas_object_layer_set(fade_obj, E_LAYER_MENU + 1);
   evas_object_show(fade_obj);
   efx_fade(fade_obj, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(0, 0, 0), 0, 0.0, NULL, NULL);
   efx_fade(fade_obj, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(0, 0, 0), 192, 0.3, NULL, NULL);
}

static void
pulse(void *d EINA_UNUSED, Efx_Map_Data *emd EINA_UNUSED, Evas_Object *obj)
{
   efx_queue_append(obj, EFX_EFFECT_SPEED_SINUSOIDAL, EFX_QUEUED_EFFECT(EFX_EFFECT_FADE(255, 255, 255, 255)), 0.6, NULL, NULL);
   efx_queue_append(obj, EFX_EFFECT_SPEED_SINUSOIDAL, EFX_QUEUED_EFFECT(EFX_EFFECT_FADE(120, 120, 120, 120)), 0.9, pulse, NULL);
   efx_queue_run(obj);
}

static void
move_begin(void *d EINA_UNUSED, E_Client *ec)
{
   clear_all();
   client = ec;
   e_comp_shape_queue_block(ec->comp, 1);

   fade_setup(ec);

   ec->layer_block = 1;
   evas_object_layer_set(ec->frame, E_LAYER_MENU + 1);

   mr_line_x = line_add(ec->comp->evas);
   mr_line_y = line_add(ec->comp->evas);

   move_text_x = text_add(ec->comp->evas);
   move_x_update(ec);

   move_text_y = text_add(ec->comp->evas);
   move_y_update(ec);

   pulse(NULL, NULL, mr_line_x);
   pulse(NULL, NULL, mr_line_y);
}

static void
move_update(void *d EINA_UNUSED, E_Client *ec)
{
   move_x_update(ec);
   move_y_update(ec);
}

static void
move_end(void *d EINA_UNUSED, E_Client *ec EINA_UNUSED)
{
   efx_queue_clear(mr_line_x);
   efx_queue_clear(mr_line_y);
   efx_fade_reset(mr_line_x);
   efx_fade_reset(mr_line_y);
   efx_fade(mr_line_x, EFX_EFFECT_SPEED_DECELERATE, EFX_COLOR(0, 0, 0), 0, 0.3, NULL, NULL);
   efx_fade(mr_line_y, EFX_EFFECT_SPEED_DECELERATE, EFX_COLOR(0, 0, 0), 0, 0.3, NULL, NULL);
   efx_fade(move_text_x, EFX_EFFECT_SPEED_DECELERATE, EFX_COLOR(0, 0, 0), 0, 0.3, NULL, NULL);
   efx_fade(move_text_y, EFX_EFFECT_SPEED_DECELERATE, EFX_COLOR(0, 0, 0), 0, 0.3, NULL, NULL);
   efx_fade(fade_obj, EFX_EFFECT_SPEED_DECELERATE, EFX_COLOR(0, 0, 0), 0, 0.3, fade_end, NULL);
}

static void
resize_begin(void *d EINA_UNUSED, E_Client *ec)
{
   unsigned int x;
   int w, h, fw, fh;
   char buf[128];

   clear_all();
   client = ec;
   e_comp_shape_queue_block(ec->comp, 1);
   EINA_RECTANGLE_SET(&resize_start, ec->x, ec->y, ec->w, ec->h);

   fade_setup(ec);

   ec->layer_block = 1;
   evas_object_layer_set(ec->frame, E_LAYER_MENU + 1);

   resize_text_start = text_add(ec->comp->evas);
   e_moveresize_client_extents(ec, &w, &h);
   e_comp_object_frame_wh_adjust(ec->frame, w, h, &fw, &fh);
   if ((ec->w != fw) || (ec->h != fh))
     snprintf(buf, sizeof(buf), "%dx%d (%dx%d)", w, h, ec->w, ec->h);
   else
     snprintf(buf, sizeof(buf), "%dx%d", ec->w, ec->h);
   edje_object_part_text_set(resize_text_start, "e.text", buf);
   edje_object_size_min_calc(resize_text_start, &w, &h);
   evas_object_resize(resize_text_start, w, h);
   e_comp_object_util_center_on(resize_text_start, ec->frame);
   pulse(NULL, NULL, resize_text_start);

   for (x = 0; x < 4; x++)
     {
        resize_rect[x] = line_add(ec->comp->evas);
        pulse(NULL, NULL, resize_rect[x]);
     }
   evas_object_line_xy_set(resize_rect[0], ec->x, ec->y, ec->x + ec->w, ec->y);
   evas_object_line_xy_set(resize_rect[1], ec->x + ec->w, ec->y, ec->x + ec->w, ec->y + ec->h);
   evas_object_line_xy_set(resize_rect[2], ec->x, ec->y + ec->h, ec->x + ec->w, ec->y + ec->h);
   evas_object_line_xy_set(resize_rect[3], ec->x, ec->y, ec->x, ec->y + ec->h);

   resize_text = text_add(ec->comp->evas);
   resize_text_update(ec);
}

static void
resize_update(void *d EINA_UNUSED, E_Client *ec)
{
   resize_text_update(ec);
}

static void
resize_end(void *d EINA_UNUSED, E_Client *ec EINA_UNUSED)
{
   unsigned int x;

   efx_fade(resize_text, EFX_EFFECT_SPEED_DECELERATE, EFX_COLOR(0, 0, 0), 0, 0.3, NULL, NULL);
   efx_fade(resize_text_start, EFX_EFFECT_SPEED_DECELERATE, EFX_COLOR(0, 0, 0), 0, 0.3, NULL, NULL);
   for (x = 0; x < 4; x++)
     {
        efx_fade(resize_rect[x], EFX_EFFECT_SPEED_DECELERATE, EFX_COLOR(0, 0, 0), 0, 0.3, NULL, NULL);
        efx_queue_clear(resize_rect[x]);
     }
   efx_fade(fade_obj, EFX_EFFECT_SPEED_DECELERATE, EFX_COLOR(0, 0, 0), 0, 0.3, fade_end, NULL);
}

EINTERN void
mr_init(void)
{
   unsigned int x = 0;

   ec_hooks[x++] = e_client_hook_add(E_CLIENT_HOOK_MOVE_BEGIN, move_begin, NULL);
   ec_hooks[x++] = e_client_hook_add(E_CLIENT_HOOK_MOVE_UPDATE, move_update, NULL);
   ec_hooks[x++] = e_client_hook_add(E_CLIENT_HOOK_MOVE_END, move_end, NULL);

   ec_hooks[x++] = e_client_hook_add(E_CLIENT_HOOK_RESIZE_BEGIN, resize_begin, NULL);
   ec_hooks[x++] = e_client_hook_add(E_CLIENT_HOOK_RESIZE_UPDATE, resize_update, NULL);
   ec_hooks[x++] = e_client_hook_add(E_CLIENT_HOOK_RESIZE_END, resize_end, NULL);
}

EINTERN void
mr_shutdown(void)
{
   unsigned int x = 0;

   for (; x < EC_HOOK_COUNT; x++)
     E_FREE_FUNC(ec_hooks[x], e_client_hook_del);
   clear_all();
}
