#include "e_mod_main.h"

static Eina_Inarray *ec_hooks = NULL;

static Evas_Object *mr_obj_x = NULL;
static Evas_Object *mr_obj_y = NULL;
static Evas_Object *fade_obj = NULL;

static void
fade_end(void *d EINA_UNUSED, Efx_Map_Data *emd EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   E_FREE_FUNC(fade_obj, evas_object_del);
}

static void
move_begin(void *d EINA_UNUSED, E_Client *ec)
{
   if (fade_obj)
     {
        efx_fade_reset(fade_obj);
        E_FREE_FUNC(fade_obj, evas_object_del);
        E_FREE_FUNC(mr_obj_x, evas_object_del);
        E_FREE_FUNC(mr_obj_y, evas_object_del);
     }
   ec->layer_block = 1;

   fade_obj = evas_object_rectangle_add(ec->comp->evas);
   evas_object_name_set(fade_obj, "fade_obj");
   evas_object_geometry_set(fade_obj, 0, 0, ec->comp->man->w, ec->comp->man->h);
   evas_object_layer_set(fade_obj, E_LAYER_MENU + 1);
   evas_object_show(fade_obj);
   efx_fade(fade_obj, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(0, 0, 0), 0, 0.0, NULL, NULL);
   efx_fade(fade_obj, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(0, 0, 0), 192, 0.3, NULL, NULL);

   evas_object_layer_set(ec->frame, E_LAYER_MENU + 2);

   mr_obj_x = evas_object_line_add(ec->comp->evas);
   evas_object_name_set(mr_obj_x, "mr_obj_x");
   evas_object_layer_set(mr_obj_x, E_LAYER_MENU + 2);
   evas_object_clip_set(mr_obj_x, ec->zone->bg_clip_object);
   evas_object_line_xy_set(mr_obj_x, ec->x, ec->zone->y, ec->x, ec->y);
   evas_object_color_set(mr_obj_x, 51, 153, 255, 255);
   evas_object_show(mr_obj_x);

   mr_obj_y = evas_object_line_add(ec->comp->evas);
   evas_object_name_set(mr_obj_y, "mr_obj_y");
   evas_object_layer_set(mr_obj_y, E_LAYER_MENU + 2);
   evas_object_clip_set(mr_obj_y, ec->zone->bg_clip_object);
   evas_object_line_xy_set(mr_obj_y, ec->zone->x, ec->y, ec->x, ec->y);
   evas_object_color_set(mr_obj_y, 51, 153, 255, 255);
   evas_object_show(mr_obj_y);
}

static void
move_update(void *d EINA_UNUSED, E_Client *ec)
{
   evas_object_line_xy_set(mr_obj_x, ec->x, ec->zone->y, ec->x, ec->y);
   evas_object_line_xy_set(mr_obj_y, ec->zone->x, ec->y, ec->x, ec->y);
}

static void
move_end(void *d EINA_UNUSED, E_Client *ec)
{
   efx_fade(fade_obj, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(0, 0, 0), 0, 0.3, fade_end, NULL);
   E_FREE_FUNC(mr_obj_x, evas_object_del);
   E_FREE_FUNC(mr_obj_y, evas_object_del);

   evas_object_layer_set(ec->frame, ec->layer);
   ec->layer_block = 1;
}

static void
resize_begin(void *d EINA_UNUSED, E_Client *ec)
{

}

static void
resize_update(void *d EINA_UNUSED, E_Client *ec)
{

}

static void
resize_end(void *d EINA_UNUSED, E_Client *ec)
{

}

EINTERN void
mr_init(void)
{
   E_Client_Hook *ech;

   ec_hooks = eina_inarray_new(sizeof(E_Client_Hook*), 6);

   ech = e_client_hook_add(E_CLIENT_HOOK_MOVE_BEGIN, move_begin, NULL);
   eina_inarray_push(ec_hooks, &ech);
   ech = e_client_hook_add(E_CLIENT_HOOK_MOVE_UPDATE, move_update, NULL);
   eina_inarray_push(ec_hooks, &ech);
   ech = e_client_hook_add(E_CLIENT_HOOK_MOVE_END, move_end, NULL);
   eina_inarray_push(ec_hooks, &ech);

   ech = e_client_hook_add(E_CLIENT_HOOK_RESIZE_BEGIN, resize_begin, NULL);
   eina_inarray_push(ec_hooks, &ech);
   ech = e_client_hook_add(E_CLIENT_HOOK_RESIZE_UPDATE, resize_update, NULL);
   eina_inarray_push(ec_hooks, &ech);
   ech = e_client_hook_add(E_CLIENT_HOOK_RESIZE_END, resize_end, NULL);
   eina_inarray_push(ec_hooks, &ech);
}

EINTERN void
mr_shutdown(void)
{
   E_Client_Hook *ech;

   EINA_INARRAY_FOREACH(ec_hooks, ech)
     e_client_hook_del(ech);
   E_FREE_FUNC(ec_hooks, eina_inarray_free);
}
