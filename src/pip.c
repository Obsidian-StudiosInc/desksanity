#include "e_mod_main.h"

static E_Client_Menu_Hook *hook = NULL;
static Eina_Hash *pips = NULL;
static E_Action *act = NULL;
static Eina_List *handlers = NULL;

static Ecore_Event_Handler *action_handler = NULL;

static Eina_Bool editing = EINA_FALSE;

typedef struct Pip
{
   Evas_Object *pip;
   Evas_Object *clip;
   Evas_Point down;
   unsigned char opacity;
   E_Pointer_Mode resize_mode;
   double zoom;
   Eina_Bool move : 1;
   Eina_Bool resize : 1;
   Eina_Bool crop : 1;
} Pip;

static void
pips_noedit()
{
   Pip *pip;
   Eina_Iterator *it;

   editing = EINA_FALSE;
   ds_fade_end(NULL, pips_noedit);
   e_comp_ungrab_input(1, 1);
   it = eina_hash_iterator_data_new(pips);
   EINA_ITERATOR_FOREACH(it, pip)
     {
        evas_object_layer_set(pip->pip, E_LAYER_CLIENT_PRIO);
        evas_object_pass_events_set(pip->pip, 1);
     }
   eina_iterator_free(it);
   e_comp_shape_queue();
   E_FREE_FUNC(action_handler, ecore_event_handler_del);
}

static void
pips_edit(void)
{
   Pip *pip;
   Eina_Iterator *it;

   if (e_comp->nocomp) return;
   editing = EINA_TRUE;
   ds_fade_setup(pips_noedit);
   e_comp_grab_input(1, 1);
   it = eina_hash_iterator_data_new(pips);
   EINA_ITERATOR_FOREACH(it, pip)
     {
        evas_object_layer_set(pip->pip, E_LAYER_MENU + 1);
        evas_object_pass_events_set(pip->pip, 0);
     }
   eina_iterator_free(it);
   e_comp_shape_queue();
}

static void
_pip_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;
   eina_hash_del_by_key(pips, &ec->frame);
   if (editing && (!eina_hash_population(pips)))
     pips_noedit();
}

static void
pip_free(Pip *pip)
{
   evas_object_event_callback_del(pip->pip, EVAS_CALLBACK_DEL, _pip_del);
   evas_object_del(pip->pip);
   evas_object_del(pip->clip);
   free(pip);
}

static void
_pip_resize(Pip *pip, int *ox, int *oy, int *ow, int *oh, Ecore_Event_Mouse_Move *ev)
{
   int x, y, w, h;

   x = *ox, y = *oy, w = *ow, h = *oh;

   if ((pip->resize_mode == E_POINTER_RESIZE_B) ||
       (pip->resize_mode == E_POINTER_RESIZE_BL) ||
       (pip->resize_mode == E_POINTER_RESIZE_BR))
     h = MAX(e_comp_canvas_y_root_adjust(ev->root.y) - y, 5);
   else if ((pip->resize_mode == E_POINTER_RESIZE_T) ||
       (pip->resize_mode == E_POINTER_RESIZE_TL) ||
       (pip->resize_mode == E_POINTER_RESIZE_TR))
     {
        h = MAX((y + h) - (e_comp_canvas_y_root_adjust(ev->root.y) - pip->down.y), 5);
        y = e_comp_canvas_y_root_adjust(ev->root.y) - pip->down.y;
     }
   if ((pip->resize_mode == E_POINTER_RESIZE_R) ||
       (pip->resize_mode == E_POINTER_RESIZE_TR) ||
       (pip->resize_mode == E_POINTER_RESIZE_BR))
     w = MAX(e_comp_canvas_x_root_adjust(ev->root.x) - x, 5);
   else if ((pip->resize_mode == E_POINTER_RESIZE_L) ||
       (pip->resize_mode == E_POINTER_RESIZE_TL) ||
       (pip->resize_mode == E_POINTER_RESIZE_BL))
     {
        w = MAX((x + w) - (e_comp_canvas_x_root_adjust(ev->root.x) - pip->down.x), 5);
        x = e_comp_canvas_x_root_adjust(ev->root.x) - pip->down.x;
     }
   {
      E_Client *ec;

      ec = evas_object_data_get(pip->pip, "E_Client");
      switch (pip->resize_mode)
        {
         case E_POINTER_RESIZE_TL:
         case E_POINTER_RESIZE_TR:
         case E_POINTER_RESIZE_BR:
         case E_POINTER_RESIZE_BL:
           if (abs(e_comp_canvas_x_root_adjust(ev->root.x) - pip->down.x) > abs(e_comp_canvas_y_root_adjust(ev->root.y) - pip->down.y))
             h = (ec->h * w) / ec->w;
           else
             w = (ec->w * h) / ec->h;
           break;

         case E_POINTER_RESIZE_T:
         case E_POINTER_RESIZE_B:
           w = (ec->w * h) / ec->h;
           break;

         case E_POINTER_RESIZE_R:
         case E_POINTER_RESIZE_L:
           h = (ec->h * w) / ec->w;
           break;
         default: break;
        }
   }
   *ox = x, *oy = y, *ow = w, *oh = h;
}

static Eina_Bool
_pip_mouse_move(Pip *pip, int t EINA_UNUSED, Ecore_Event_Mouse_Move *ev)
{
   int x, y, w, h;

   evas_object_geometry_get(pip->pip, &x, &y, &w, &h);
   if (pip->resize)
     {
        _pip_resize(pip, &x, &y, &w, &h, ev);
        evas_object_geometry_set(pip->pip, x, y, w, h);
        if (pip->clip)
          {
             evas_object_geometry_get(pip->clip, &x, &y, &w, &h);
             _pip_resize(pip, &x, &y, &w, &h, ev);
             evas_object_geometry_set(pip->clip, x, y, w, h);
          }
     }
   else if (pip->move)
     {
        if (pip->clip)
          {
             int cx, cy, cw, ch, dx, dy;

             evas_object_geometry_get(pip->clip, &cx, &cy, &cw, &ch);
             dx = cx - x, dy = cy - y;

             evas_object_move(pip->clip,
               E_CLAMP(e_comp_canvas_x_root_adjust(ev->root.x) - pip->down.x, 0, e_comp->w - (cw / 2)),
               E_CLAMP(e_comp_canvas_y_root_adjust(ev->root.y) - pip->down.y, 0, e_comp->h - (ch / 2)));
             evas_object_geometry_get(pip->clip, &cx, &cy, NULL, NULL);

             evas_object_move(pip->pip, cx - dx, cy - dy);
          }
        else
          evas_object_move(pip->pip,
            E_CLAMP(e_comp_canvas_x_root_adjust(ev->root.x) - pip->down.x, 0, e_comp->w - (w / 2)),
            E_CLAMP(e_comp_canvas_y_root_adjust(ev->root.y) - pip->down.y, 0, e_comp->h - (h / 2)));
     }
   else if (pip->crop)
     {
        int cx, cy;

        if (x + pip->down.x < e_comp_canvas_x_root_adjust(ev->root.x))
          cx = x + pip->down.x;
        else
          cx = e_comp_canvas_x_root_adjust(ev->root.x);
        if (y + pip->down.y < e_comp_canvas_y_root_adjust(ev->root.y))
          cy = y + pip->down.y;
        else
          cy = e_comp_canvas_y_root_adjust(ev->root.y);
        evas_object_geometry_set(pip->clip, cx, cy, abs(cx - e_comp_canvas_x_root_adjust(ev->root.x)), abs(cy - e_comp_canvas_y_root_adjust(ev->root.y)));
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_pip_mouse_wheel(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Wheel *ev = event_info;
   Pip *pip = data;

   if (ev->direction % 2) return;
   if (evas_key_modifier_is_set(ev->modifiers, "Control"))
     {
        if (ev->z < 0)
          e_efx_zoom(pip->pip, E_EFX_EFFECT_SPEED_LINEAR, 0, pip->zoom -= 0.05, E_EFX_POINT(ev->output.x, ev->output.y), 0.2, NULL, NULL);
        else if (ev->z > 0)
          e_efx_zoom(pip->pip, E_EFX_EFFECT_SPEED_LINEAR, 0, pip->zoom += 0.05, E_EFX_POINT(ev->output.x, ev->output.y), 0.2, NULL, NULL);
     }
   else
     {
        if (ev->z < 0)
          pip->opacity = E_CLAMP(pip->opacity + 15, 0, 255);
        else if (ev->z > 0)
          pip->opacity = E_CLAMP(pip->opacity - 15, 0, 255);
        e_efx_fade(pip->pip, E_EFX_EFFECT_SPEED_LINEAR, E_EFX_COLOR(pip->opacity, pip->opacity, pip->opacity), pip->opacity, 0.2, NULL, NULL);
     }
}

static void
_pip_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Pip *pip = data;
   Evas_Event_Mouse_Up *ev = event_info;

   if (pip->crop)
     {
        int x, y;

        evas_object_geometry_get(obj, &x, &y, NULL, NULL);
        if ((abs((x + pip->down.x) - ev->output.x) < 3) &&
            (abs((y + pip->down.y) - ev->output.y) < 3))
          E_FREE_FUNC(pip->clip, evas_object_del);
        else
          {
             evas_object_color_set(pip->clip, 255, 255, 255, 255);
             evas_object_clip_set(pip->pip, pip->clip);
          }
     }
   pip->down.x = pip->down.y = 0;
   pip->move = pip->resize = pip->crop = 0;
   pip->resize_mode = E_POINTER_RESIZE_NONE;
   E_FREE_FUNC(action_handler, ecore_event_handler_del);
}

static void
_pip_mouse_down(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Mouse_Down *ev = event_info;
   Pip *pip = data;
   int x, y, w, h;

   if (ev->button == 3)
     {
        eina_hash_del_by_data(pips, pip);
        return;
     }

   if (evas_key_modifier_is_set(ev->modifiers, "Shift"))
     pip->crop = ev->button == 1;
   else if (!evas_key_modifier_is_set(ev->modifiers, "Control"))
     {
        pip->move = ev->button == 1;
        pip->resize = ev->button == 2;
     }
   if (pip->clip && (!pip->crop))
     obj = pip->clip;
   evas_object_geometry_get(obj, &x, &y, &w, &h);
   pip->down.x = ev->output.x - x;
   pip->down.y = ev->output.y - y;
   if (pip->resize)
     {
        if ((ev->output.x > (x + w / 5)) &&
            (ev->output.x < (x + w * 4 / 5)))
          {
             if (ev->output.y < (y + h / 2))
               {
                  pip->resize_mode = E_POINTER_RESIZE_T;
               }
             else
               {
                  pip->resize_mode = E_POINTER_RESIZE_B;
               }
          }
        else if (ev->output.x < (x + w / 2))
          {
             if ((ev->output.y > (y + h / 5)) &&
                 (ev->output.y < (y + h * 4 / 5)))
               {
                  pip->resize_mode = E_POINTER_RESIZE_L;
               }
             else if (ev->output.y < (y + h / 2))
               {
                  pip->resize_mode = E_POINTER_RESIZE_TL;
               }
             else
               {
                  pip->resize_mode = E_POINTER_RESIZE_BL;
               }
          }
        else
          {
             if ((ev->output.y > (y + h / 5)) &&
                 (ev->output.y < (y + h * 4 / 5)))
               {
                  pip->resize_mode = E_POINTER_RESIZE_R;
               }
             else if (ev->output.y < (y + h / 2))
               {
                  pip->resize_mode = E_POINTER_RESIZE_TR;
               }
             else
               {
                  pip->resize_mode = E_POINTER_RESIZE_BR;
               }
          }
     }
   else if (pip->crop)
     {
        pip->down.x = E_CLAMP(pip->down.x, 0, w);
        pip->down.y = E_CLAMP(pip->down.y, 0, h);
        if (!pip->clip)
          {
             pip->clip = evas_object_rectangle_add(e);
             evas_object_pass_events_set(pip->clip, 1);
             evas_object_layer_set(pip->clip, evas_object_layer_get(pip->pip) + 1);
             evas_object_show(pip->clip);
          }
        evas_object_clip_unset(pip->pip);
        evas_object_color_set(pip->clip, 50, 50, 50, 50);
     }
   action_handler = ecore_event_handler_add(ECORE_EVENT_MOUSE_MOVE, (Ecore_Event_Handler_Cb)_pip_mouse_move, pip);
}

static void
_pip_delete(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   _pip_del(data, NULL, NULL, NULL);
}

static void
_pip_manage(void *data EINA_UNUSED, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   pips_edit();
}

static void
_pip_fade_end(void *d EINA_UNUSED, E_Efx_Map_Data *emd EINA_UNUSED, Evas_Object *obj)
{
   ecore_job_add((Ecore_Cb)e_efx_fade_reset, obj);
}

static void
_pip_create(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   E_Client *ec = data;
   Pip *pip;
   Evas_Object *o;

   o = e_comp_object_util_mirror_add(ec->frame);
   if (!o) return; //FIXME throw real error

   pip = E_NEW(Pip, 1);
   pip->pip = o;
   pip->resize_mode = E_POINTER_RESIZE_NONE;
   pip->opacity = 255;
   pip->zoom = 1.0;

   evas_object_geometry_set(o, ec->zone->x + 1, ec->zone->y + 1, (ec->w * (ec->zone->h / 4)) / ec->h, ec->zone->h / 4);
   e_comp_object_util_center(o);
   evas_object_data_set(o, "comp_skip", (void*)1);
   evas_object_layer_set(o, E_LAYER_CLIENT_PRIO);
   evas_object_pass_events_set(o, 1);
   evas_object_show(o);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN, _pip_mouse_down, pip);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_UP, _pip_mouse_up, pip);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_WHEEL, _pip_mouse_wheel, pip);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _pip_del, ec);

   e_efx_fade(o, E_EFX_EFFECT_SPEED_LINEAR, E_EFX_COLOR(0, 0, 0), 0, 0.0, NULL, NULL);
   e_efx_fade(o, E_EFX_EFFECT_SPEED_LINEAR, E_EFX_COLOR(255, 255, 255), 255, 0.2, _pip_fade_end, NULL);

   eina_hash_add(pips, &ec->frame, pip);
   if (editing)
     {
        evas_object_layer_set(pip->pip, E_LAYER_MENU + 1);
        evas_object_pass_events_set(pip->pip, 0);
        e_comp_shape_queue();
     }
   else
     pips_edit();
}

static void
_pip_hook(void *d EINA_UNUSED, E_Client *ec)
{
   E_Menu_Item *mi;
   E_Menu *subm;
   const Eina_List *l;
   Eina_Bool exists;

   if (!ec->border_menu) return;
   if (!e_comp_config_get()->enable_advanced_features) return;

   /* only one per client for now */
   exists = !!eina_hash_find(pips, &ec->frame);

   EINA_LIST_REVERSE_FOREACH(ec->border_menu->items, l, mi)
     if (mi->separator) break;

   mi = eina_list_data_get(l->prev);
   subm = mi->submenu;
   mi = e_menu_item_new(subm);
   if (exists)
     e_menu_item_label_set(mi, D_("Delete Mini"));
   else
     e_menu_item_label_set(mi, D_("Create Mini"));
   e_menu_item_icon_edje_set(mi, mod->edje_file, "icon");
   if (exists)
     e_menu_item_callback_set(mi, _pip_delete, ec);
   else
     e_menu_item_callback_set(mi, _pip_create, ec);
   if (!exists) return;

   mi = e_menu_item_new(subm);
   e_menu_item_label_set(mi, D_("Manage Minis"));
   e_menu_item_icon_edje_set(mi, mod->edje_file, "icon");
   e_menu_item_callback_set(mi, _pip_manage, ec);
}

static void
_pip_action_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   if (!eina_hash_population(pips)) return;
   if (editing)
     pips_noedit();
   else
     pips_edit();
}

static Eina_Bool
pip_comp_disable()
{
   if (editing)
     {
        pips_noedit();
        editing = EINA_TRUE;
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
pip_comp_enable()
{
   if (editing) pips_edit();
   return ECORE_CALLBACK_RENEW;
}

EINTERN void
pip_init(void)
{
   hook = e_int_client_menu_hook_add(_pip_hook, NULL);
   pips = eina_hash_pointer_new((Eina_Free_Cb)pip_free);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_COMPOSITOR_DISABLE, pip_comp_disable, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_COMPOSITOR_ENABLE, pip_comp_enable, NULL);

   act = e_action_add("pip");
   if (act)
     {
        act->func.go = _pip_action_cb;
        e_action_predef_name_set(D_("Compositor"), D_("Manage Minis"),
                                 "pip", NULL, NULL, 0);
     }
}

EINTERN void
pip_shutdown(void)
{
   E_FREE_FUNC(hook, e_int_client_menu_hook_del);
   E_FREE_FUNC(pips, eina_hash_free);
   E_FREE_LIST(handlers, ecore_event_handler_del);
   ds_fade_end(NULL, pips_noedit);
   e_action_predef_name_del(D_("Compositor"), D_("Manage Minis"));
   e_action_del("pips");
   act = NULL;
   E_FREE_FUNC(action_handler, ecore_event_handler_del);
   editing = EINA_FALSE;
}
