#include "e_mod_main.h"
#include "gadget.h"

typedef struct Gadget_Item
{
   Evas_Object *editor;
   Evas_Object *gadget;
   Evas_Object *site;
} Gadget_Item;

static Evas_Object *pointer_site;
static Eina_List *handlers;

static void
_editor_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_FREE_LIST(data, free);
}

static void
_editor_pointer_site_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   free(data);
}

static void
_editor_site_hints(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   int x, y, w, h;

   evas_object_size_hint_min_get(obj, &w, &h);
   evas_pointer_canvas_xy_get(e_comp->evas, &x, &y);
   evas_object_geometry_set(pointer_site, x - (w / 2), y - (h / 2), w, h);
}

static Eina_Bool
_editor_pointer_button(Gadget_Item *active, int t EINA_UNUSED, Ecore_Event_Mouse_Button *ev)
{
   if (ev->buttons == 1)
     evas_object_smart_callback_call(active->site, "gadget_site_dropped", pointer_site);
   evas_object_pass_events_set(active->site, 0);
   elm_object_disabled_set(active->editor, 1);
   E_FREE_FUNC(pointer_site, evas_object_del);
   E_FREE_LIST(handlers, ecore_event_handler_del);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_editor_pointer_move(Gadget_Item *active EINA_UNUSED, int t EINA_UNUSED, Ecore_Event_Mouse_Move *ev)
{
   int w, h;

   evas_object_geometry_get(pointer_site, NULL, NULL, &w, &h);
   evas_object_move(pointer_site, ev->x - (w / 2), ev->y - (h / 2));
   return ECORE_CALLBACK_RENEW;
}

static void
_editor_gadget_new(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Gadget_Item *active, *gi = data;
   Evas_Object *site;
   Z_Gadget_Site_Orient orient;
   int size;

   orient = z_gadget_site_orient_get(gi->site);

   pointer_site = site = z_gadget_site_add(e_comp->elm, orient);
   if (orient == Z_GADGET_SITE_ORIENT_HORIZONTAL)
     evas_object_geometry_get(gi->site, NULL, NULL, NULL, &size);
   else if (orient == Z_GADGET_SITE_ORIENT_VERTICAL)
     evas_object_geometry_get(gi->site, NULL, NULL, &size, NULL);
   else
     {} /* FIXME */
   evas_object_resize(site, size, size);
   evas_object_layer_set(site, E_LAYER_MENU);
   evas_object_pass_events_set(site, 1);
   evas_object_show(site);
   active = E_NEW(Gadget_Item, 1);
   active->editor = gi->editor;
   active->site = gi->site;
   evas_object_pass_events_set(active->site, 1);
   evas_object_event_callback_add(site, EVAS_CALLBACK_CHANGED_SIZE_HINTS, _editor_site_hints, active);
   evas_object_event_callback_add(site, EVAS_CALLBACK_DEL, _editor_pointer_site_del, active);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_MOVE, _editor_pointer_move, active);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_BUTTON_DOWN, _editor_pointer_button, active);
   z_gadget_site_gadget_add(site, z_gadget_type_get(gi->gadget), 0);
   elm_object_disabled_set(gi->editor, 1);
   elm_list_item_selected_set(event_info, 0);
}

static void
_editor_gadget_configure(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   
}

Z_API Evas_Object *
z_gadget_editor_add(Evas_Object *parent, Evas_Object *site)
{
   Evas_Object *list, *tempsite, *g;
   Eina_Iterator *it;
   Eina_List *gadgets, *items = NULL;
   const char *type;

   list = elm_list_add(parent);
   E_EXPAND(list);
   E_FILL(list);
   elm_list_mode_set(list, ELM_LIST_COMPRESS);
   elm_scroller_content_min_limit(list, 0, 1);
   tempsite = z_gadget_site_add(list, Z_GADGET_SITE_ORIENT_HORIZONTAL);
   z_gadget_site_gravity_set(tempsite, Z_GADGET_SITE_GRAVITY_NONE);

   it = z_gadget_type_iterator_get();
   /* FIXME: no types available */
   EINA_ITERATOR_FOREACH(it, type)
     z_gadget_site_gadget_add(tempsite, type, 1);
   eina_iterator_free(it);

   gadgets = z_gadget_site_gadgets_list(tempsite);
   EINA_LIST_FREE(gadgets, g)
     {
        Evas_Object *box, *button = NULL;
        char buf[1024];
        Gadget_Item *gi;

        gi = E_NEW(Gadget_Item, 1);
        gi->editor = list;
        gi->gadget = g;
        gi->site = site;
        items = eina_list_append(items, gi);
        box = elm_box_add(list);
        elm_box_horizontal_set(box, 1);
        E_EXPAND(g);
        E_FILL(g);
        elm_box_pack_end(box, g);
        evas_object_pass_events_set(g, 1);
        if (z_gadget_has_wizard(g))
          {
             button = elm_button_add(list);
             elm_object_text_set(button, "Configure...");
             evas_object_smart_callback_add(button, "clicked", _editor_gadget_configure, gi);
             evas_object_propagate_events_set(button, 0);
          }
        strncpy(buf, z_gadget_type_get(g), sizeof(buf) - 1);
        buf[0] = toupper(buf[0]);
        elm_list_item_append(list, buf, box, button, _editor_gadget_new, gi);
        elm_box_recalculate(box);
     }
   evas_object_data_set(list, "__gadget_items", items);
   evas_object_event_callback_add(list, EVAS_CALLBACK_DEL, _editor_del, items);
   elm_list_go(list);
   return list;
}
