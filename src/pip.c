#include "e_mod_main.h"

static E_Client_Menu_Hook *hook = NULL;
static Eina_Hash *pips = NULL;
static E_Action *act = NULL;
static Ecore_Event_Handler *handlers[2];

static Ecore_Event_Handler *action_handler = NULL;

static Eina_Bool editing = EINA_FALSE;

static Evas_Object *fade_obj = NULL;

typedef struct Pip
{
   Evas_Object *pip;
   Evas_Point down;
   unsigned char opacity;
   E_Pointer_Mode resize_mode;
   Eina_Bool move : 1;
   Eina_Bool resize : 1;
} Pip;


static void
fade_setup(E_Comp *comp)
{
   fade_obj = evas_object_rectangle_add(comp->evas);
   evas_object_name_set(fade_obj, "fade_obj");
   evas_object_geometry_set(fade_obj, 0, 0, comp->man->w, comp->man->h);
   evas_object_layer_set(fade_obj, E_LAYER_MENU + 1);
   evas_object_show(fade_obj);
   efx_fade(fade_obj, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(0, 0, 0), 0, 0.0, NULL, NULL);
   efx_fade(fade_obj, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(0, 0, 0), 192, 0.3, NULL, NULL);
}

static void
fade_end(void *d EINA_UNUSED, Efx_Map_Data *emd EINA_UNUSED, Evas_Object *obj)
{
   E_FREE_FUNC(obj, evas_object_del);
}

static void
pips_edit(void)
{
   Pip *pip;
   Eina_Iterator *it;
   E_Comp *comp;

   comp = e_comp_get(NULL);
   if (comp->nocomp) return;
   editing = EINA_TRUE;
   fade_setup(comp);
   it = eina_hash_iterator_data_new(pips);
   EINA_ITERATOR_FOREACH(it, pip)
     {
        evas_object_layer_set(pip->pip, E_LAYER_MENU + 1);
        evas_object_pass_events_set(pip->pip, 0);
     }
   eina_iterator_free(it);
   e_comp_shape_queue(comp);
}

static void
pips_noedit(void)
{
   Pip *pip;
   Eina_Iterator *it;

   editing = EINA_FALSE;
   efx_fade(fade_obj, EFX_EFFECT_SPEED_DECELERATE, EFX_COLOR(0, 0, 0), 0, 0.3, fade_end, NULL);
   it = eina_hash_iterator_data_new(pips);
   EINA_ITERATOR_FOREACH(it, pip)
     {
        evas_object_layer_set(pip->pip, E_LAYER_CLIENT_PRIO);
        evas_object_pass_events_set(pip->pip, 1);
     }
   eina_iterator_free(it);
   e_comp_shape_queue(e_comp_get(NULL));
   E_FREE_FUNC(action_handler, ecore_event_handler_del);
}

static void
pip_free(Pip *pip)
{
   evas_object_del(pip->pip);
   free(pip);
}

static Eina_Bool
_pip_mouse_move(Pip *pip, int t EINA_UNUSED, Ecore_Event_Mouse_Move *ev)
{
   int x, y, w, h;

   evas_object_geometry_get(pip->pip, &x, &y, &w, &h);
   if (pip->resize)
     {
        if ((pip->resize_mode == E_POINTER_RESIZE_B) ||
            (pip->resize_mode == E_POINTER_RESIZE_BL) ||
            (pip->resize_mode == E_POINTER_RESIZE_BR))
          h = MAX(ev->root.y - y, 5);
        else if ((pip->resize_mode == E_POINTER_RESIZE_T) ||
            (pip->resize_mode == E_POINTER_RESIZE_TL) ||
            (pip->resize_mode == E_POINTER_RESIZE_TR))
          {
             h = MAX((y + h) - (ev->root.y - pip->down.y), 5);
             y = ev->root.y - pip->down.y;
          }
        if ((pip->resize_mode == E_POINTER_RESIZE_R) ||
            (pip->resize_mode == E_POINTER_RESIZE_TR) ||
            (pip->resize_mode == E_POINTER_RESIZE_BR))
          w = MAX(ev->root.x - x, 5);
        else if ((pip->resize_mode == E_POINTER_RESIZE_L) ||
            (pip->resize_mode == E_POINTER_RESIZE_TL) ||
            (pip->resize_mode == E_POINTER_RESIZE_BL))
          {
             w = MAX((x + w) - (ev->root.x - pip->down.x), 5);
             x = ev->root.x - pip->down.x;
          }
        evas_object_geometry_set(pip->pip, x, y, w, h);
     }
   else if (pip->move)
     {
        E_Comp *comp = e_comp_util_evas_object_comp_get(pip->pip);
        evas_object_move(pip->pip,
          E_CLAMP(ev->root.x - pip->down.x, 0, comp->man->w - (w / 2)),
          E_CLAMP(ev->root.y - pip->down.y, 0, comp->man->h - (h / 2)));
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_pip_mouse_wheel(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Ecore_Event_Mouse_Wheel *ev = event_info;
   Pip *pip = data;

   if (ev->z < 0)
     pip->opacity = E_CLAMP(pip->opacity + 15, 0, 255);
   else if (ev->z > 0)
     pip->opacity = E_CLAMP(pip->opacity - 15, 0, 255);
   efx_fade(pip->pip, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(pip->opacity, pip->opacity, pip->opacity), pip->opacity, 0.2, NULL, NULL);
}

static void
_pip_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Pip *pip = data;

   pip->down.x = pip->down.y = 0;
   pip->move = pip->resize = 0;
   pip->resize_mode = E_POINTER_RESIZE_NONE;
   E_FREE_FUNC(action_handler, ecore_event_handler_del);
}

static void
_pip_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info)
{
   Evas_Event_Mouse_Down *ev = event_info;
   Pip *pip = data;
   int x, y, w, h;

   if (ev->button == 3)
     {
        eina_hash_del_by_data(pips, pip);
        return;
     }
   evas_object_geometry_get(obj, &x, &y, &w, &h);
   pip->down.x = ev->output.x - x;
   pip->down.y = ev->output.y - y;
   pip->move = ev->button == 1;
   pip->resize = ev->button == 2;
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
   action_handler = ecore_event_handler_add(ECORE_EVENT_MOUSE_MOVE, (Ecore_Event_Handler_Cb)_pip_mouse_move, pip);
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
_pip_delete(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   _pip_del(data, NULL, NULL, NULL);
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

   efx_fade(o, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(0, 0, 0), 0, 0.0, NULL, NULL);
   efx_fade(o, EFX_EFFECT_SPEED_LINEAR, EFX_COLOR(255, 255, 255), 255, 0.2, NULL, NULL);

   eina_hash_add(pips, &ec->frame, pip);
}

static void
_pip_hook(void *d EINA_UNUSED, E_Client *ec)
{
   E_Menu_Item *mi;
   const Eina_List *l;
   Eina_Bool exists;

   if (!ec->border_menu) return;
   if (!e_comp_config_get()->enable_advanced_features) return;

   /* only one per client for now */
   exists = !!eina_hash_find(pips, &ec->frame);

   EINA_LIST_REVERSE_FOREACH(ec->border_menu->items, l, mi)
     if (mi->separator) break;

   mi = eina_list_data_get(l->prev);
   mi = e_menu_item_new(mi->submenu);
   if (exists)
     e_menu_item_label_set(mi, D_("Delete Mini"));
   else
     e_menu_item_label_set(mi, D_("Create Mini"));
   e_menu_item_icon_edje_set(mi, mod->edje_file, "icon");
   if (exists)
     e_menu_item_callback_set(mi, _pip_delete, ec);
   else
     e_menu_item_callback_set(mi, _pip_create, ec);
}

static void
_pip_action_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
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
   handlers[0] = ecore_event_handler_add(E_EVENT_COMPOSITOR_DISABLE, pip_comp_disable, NULL);
   handlers[1] = ecore_event_handler_add(E_EVENT_COMPOSITOR_ENABLE, pip_comp_enable, NULL);

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
   E_FREE_FUNC(handlers[0], ecore_event_handler_del);
   E_FREE_FUNC(handlers[1], ecore_event_handler_del);
   E_FREE_FUNC(fade_obj, evas_object_del);
   e_action_predef_name_del(D_("Compositor"), D_("Manage Minis"));
   e_action_del("pips");
   act = NULL;
   E_FREE_FUNC(action_handler, ecore_event_handler_del);
   editing = EINA_FALSE;
}
