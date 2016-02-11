#include "wireless.h"

typedef struct Instance
{
   Evas_Object *box;
   Evas_Object *wifi;
   Evas_Object *popup;
   Evas_Object *popup_list;
   Eina_Hash *popup_items;
} Instance;

static Wifi_State wifi_state;

static Eina_Array *wifi_networks;
static Wifi_Network *wifi_current;
static Eina_Bool wifi_enabled;
static Eina_List *instances;

static void
_wifi_icon_init(Evas_Object *icon, Wifi_Network *wn)
{
   Edje_Message_Int_Set *msg;

   msg = alloca(sizeof(Edje_Message_Int_Set) + sizeof(int));
   msg->count = 2;
   msg->val[0] = wn->state;
   msg->val[1] = wn->strength;
   edje_object_message_send(elm_layout_edje_get(icon), EDJE_MESSAGE_INT_SET, 1, msg);
}

static void
_wifi_state_update(Evas_Object *g)
{
   switch (wifi_state)
     {
      case WIFI_STATE_NONE: break;
      case WIFI_STATE_ETHERNET:
        elm_object_signal_emit(g, "e,state,ethernet", "e");
        break;
      case WIFI_STATE_WIFI:
        elm_object_signal_emit(g, "e,state,wifi", "e");
        break;
     }
}

static void
_wifi_popup_network_click(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{

}

static void
_wifi_popup_list_populate(Instance *inst)
{
   Eina_Iterator *it;
   Wifi_Network *wn;

   it = eina_array_iterator_new(wifi_networks);
   EINA_ITERATOR_FOREACH(it, wn)
     {
        Evas_Object *icon;
        Elm_Object_Item *item;

        icon = elm_layout_add(inst->popup_list);
        e_theme_edje_object_set(icon, NULL, "e/modules/wireless/wifi");
        _wifi_icon_init(icon, wn);
        item = elm_list_item_append(inst->popup_list, wn->name, icon, NULL, _wifi_popup_network_click, inst);
        eina_hash_add(inst->popup_items, &wn, item);
     }
   eina_iterator_free(it);
}

static void
_wifi_popup_wifi_toggle(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
}

static void
_wifi_popup_dismissed(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   evas_object_del(obj);
}

static void
_wifi_popup_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   E_FREE_FUNC(inst->popup_items, eina_hash_free);
   inst->popup_list = NULL;
   inst->popup = NULL;
}

static void
_wifi_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Down *ev = event_info;
   Instance *inst = data;
   Evas_Object *ctx, *box, *list, *toggle;

   if (ev->button != 1) return;
   inst->popup_items = eina_hash_pointer_new(NULL);
   inst->popup = ctx = elm_ctxpopup_add(e_comp->elm);
   elm_object_style_set(inst->popup, "noblock");
   evas_object_smart_callback_add(inst->popup, "dismissed", _wifi_popup_dismissed, inst);
   evas_object_event_callback_add(inst->popup, EVAS_CALLBACK_DEL, _wifi_popup_del, inst);

   box = elm_box_add(ctx);
   E_EXPAND(box);
   E_FILL(box);

   inst->popup_list = list = elm_list_add(ctx);
   E_EXPAND(list);
   E_FILL(list);
   _wifi_popup_list_populate(inst);
   elm_list_go(list);
   elm_box_pack_end(box, list);
   toggle = elm_check_add(ctx);
   elm_object_style_set(toggle, "toggle");
   elm_object_text_set(toggle, "Wifi State");
   elm_object_part_text_set(toggle, "on", "On");
   elm_object_part_text_set(toggle, "off", "Off");
   elm_check_state_pointer_set(toggle, &wifi_enabled);
   evas_object_smart_callback_add(toggle, "changed", _wifi_popup_wifi_toggle, inst);
   elm_box_pack_end(box, toggle);
   elm_object_content_set(ctx, box);
   z_gadget_util_ctxpopup_place(inst->box, ctx);
   evas_object_show(ctx);
}

static void
wireless_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   instances = eina_list_remove(instances, inst);
   evas_object_del(inst->popup);
   free(inst);
}

static Evas_Object *
wireless_create(Evas_Object *parent, int *id, Z_Gadget_Site_Orient orient)
{
   Evas_Object *g;
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->box = elm_box_add(parent);
   elm_box_horizontal_set(inst->box, orient != Z_GADGET_SITE_ORIENT_VERTICAL);
   elm_box_homogeneous_set(inst->box, 1);

   inst->wifi = g = elm_layout_add(parent);
   E_EXPAND(g);
   E_FILL(g);
   e_theme_edje_object_set(g, NULL, "e/modules/wireless/wifi");
   elm_box_pack_end(inst->box, g);

   evas_object_size_hint_aspect_set(inst->box, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   evas_object_event_callback_add(inst->box, EVAS_CALLBACK_DEL, wireless_del, inst);
   evas_object_event_callback_add(g, EVAS_CALLBACK_MOUSE_DOWN, _wifi_mouse_down, inst);

   if (*id < 0)
     elm_object_signal_emit(g, "e,state,wifi", "e");
   else
     {
        _wifi_state_update(g);
        if (wifi_current)
          _wifi_icon_init(g, wifi_current);
     }
   instances = eina_list_append(instances, inst);

   return inst->box;
}

static void
wireless_init(void)
{
   z_gadget_type_add("Wireless", wireless_create);
}


EINTERN void
wireless_wifi_state_set(Wifi_State state)
{
   Eina_List *l;
   Instance *inst;

   if (wifi_state == state) return;

   wifi_state = state;
   EINA_LIST_FOREACH(instances, l, inst)
     _wifi_state_update(inst->wifi);
}

EINTERN void
wireless_wifi_current_network_set(Wifi_Network *wn)
{
   Eina_List *l;
   Instance *inst;

   wifi_current = wn;
   EINA_LIST_FOREACH(instances, l, inst)
     if (inst->popup)
       {
          Elm_Object_Item *it;
          Evas_Object *icon;

          it = eina_hash_find(inst->popup_items, &wn);
          icon = elm_object_item_content_get(it);
          _wifi_icon_init(icon, wn);
       }
}

EINTERN Eina_Array *
wireless_wifi_networks_set(Eina_Array *networks)
{
   Eina_Array *prev = wifi_networks;
   Eina_List *l;
   Instance *inst;

   wifi_networks = networks;
   EINA_LIST_FOREACH(instances, l, inst)
     if (inst->popup)
       {
          elm_list_clear(inst->popup_list);
          eina_hash_free_buckets(inst->popup_items);
          _wifi_popup_list_populate(inst);
       }

   return prev;
}

EINTERN void
wireless_airplane_mode_set(Eina_Bool enabled)
{

}
