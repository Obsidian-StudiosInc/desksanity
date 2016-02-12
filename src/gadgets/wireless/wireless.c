#include "wireless.h"

typedef struct Instance
{
   Evas_Object *box;
   Evas_Object *wifi;
   Evas_Object *popup;
   Evas_Object *popup_list;
   Eina_Hash *popup_items;
} Instance;

static Wireless_Network_State wifi_network_state;

static Eina_Array *wifi_networks;
static Wireless_Connection *wifi_current;
static Eina_Bool wifi_enabled;
static Eina_List *instances;

static void
_wifi_icon_init(Evas_Object *icon, Wireless_Connection *wn)
{
   Edje_Message_Int_Set *msg;
   int state = 0, strength = 0;

   if (wn)
     {
        state = wn->state;
        strength = wn->strength;
     }
   msg = alloca(sizeof(Edje_Message_Int_Set) + sizeof(int));
   msg->count = 2;
   msg->val[0] = state;
   msg->val[1] = strength;
   edje_object_message_send(elm_layout_edje_get(icon), EDJE_MESSAGE_INT_SET, 1, msg);

   if (!wn)
     {
        elm_object_signal_emit(icon, "e,state,disconnected", "e");
        return;
     }
   switch (wn->type)
     {
      case WIRELESS_SERVICE_TYPE_ETHERNET:
        elm_object_signal_emit(icon, "e,state,ethernet", "e");
        break;
      case WIRELESS_SERVICE_TYPE_WIFI:
        elm_object_signal_emit(icon, "e,state,wifi", "e");
        if (wn->security > WIRELESS_NETWORK_SECURITY_WEP)
          elm_object_signal_emit(icon, "e,state,secure", "e");
        else if (wn->security == WIRELESS_NETWORK_SECURITY_WEP)
          elm_object_signal_emit(icon, "e,state,insecure", "e");
        else if (!wn->security)
          elm_object_signal_emit(icon, "e,state,unsecured", "e");
        break;
      default: break;
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
   Wireless_Connection *wn;

   it = eina_array_iterator_new(wifi_networks);
   EINA_ITERATOR_FOREACH(it, wn)
     {
        Evas_Object *icon;
        Elm_Object_Item *item;

        if (wn->type != WIRELESS_SERVICE_TYPE_WIFI) continue;
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
_wifi_popup_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   E_FREE_FUNC(inst->popup_items, eina_hash_free);
   inst->popup_list = NULL;
   inst->popup = NULL;
}

static Eina_Bool
_wifi_popup_key()
{
   return EINA_TRUE;
}

static void
_wifi_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Down *ev = event_info;
   Instance *inst = data;
   Evas_Object *ctx, *box, *list, *toggle;

   if (ev->button != 1) return;
   if (inst->popup)
     {
        evas_object_hide(inst->popup);
        evas_object_del(inst->popup);
        return;
     }
   inst->popup_items = eina_hash_pointer_new(NULL);
   ctx = elm_ctxpopup_add(e_comp->elm);
   elm_object_style_set(ctx, "noblock");

   box = elm_box_add(ctx);
   E_EXPAND(box);
   E_FILL(box);

   inst->popup_list = list = elm_list_add(ctx);
   elm_list_mode_set(list, ELM_LIST_COMPRESS);
   elm_scroller_content_min_limit(list, 0, 1);
   evas_object_size_hint_max_set(list, -1, e_comp_object_util_zone_get(inst->box)-> h / 3);
   E_EXPAND(list);
   E_FILL(list);
   _wifi_popup_list_populate(inst);
   elm_list_go(list);
   evas_object_show(list);
   elm_box_pack_end(box, list);
   toggle = elm_check_add(ctx);
   evas_object_show(toggle);
   elm_object_style_set(toggle, "toggle");
   elm_object_text_set(toggle, "Wifi State");
   elm_object_part_text_set(toggle, "on", "On");
   elm_object_part_text_set(toggle, "off", "Off");
   elm_check_state_pointer_set(toggle, &wifi_enabled);
   evas_object_smart_callback_add(toggle, "changed", _wifi_popup_wifi_toggle, inst);
   elm_box_pack_end(box, toggle);
   elm_object_content_set(ctx, box);
   z_gadget_util_ctxpopup_place(inst->box, ctx);
   evas_object_smart_callback_call(inst->box, "gadget_popup", ctx);
   inst->popup = e_comp_object_util_add(ctx, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(inst->popup, evas_object_layer_get(inst->popup) + 1);
   e_comp_object_util_autoclose(inst->popup, NULL, _wifi_popup_key, NULL);
   evas_object_show(inst->popup);
   evas_object_event_callback_add(inst->popup, EVAS_CALLBACK_DEL, _wifi_popup_del, inst);
}

static void
_wifi_tooltip_row(Evas_Object *tb, const char *label, const char *value, int row)
{
   Evas_Object *lbl;

   lbl = elm_label_add(tb);
   evas_object_show(lbl);
   E_ALIGN(lbl, 0, 0.5);
   elm_object_text_set(lbl, label);
   elm_table_pack(tb, lbl, 0, row, 1, 1);

   lbl = elm_label_add(tb);
   evas_object_show(lbl);
   E_ALIGN(lbl, 0, 0.5);
   elm_object_text_set(lbl, value);
   elm_table_pack(tb, lbl, 1, row, 1, 1);
}

static Evas_Object *
_wifi_tooltip(void *data, Evas_Object *obj EINA_UNUSED, Evas_Object *tooltip)
{
   Instance *inst = data;
   Evas_Object *tb;
   int row = 0;
   const char *val;
   char buf[1024];

   if (!wifi_current) return NULL;
   tb = elm_table_add(tooltip);
   elm_table_padding_set(tb, 5, 1);

   _wifi_tooltip_row(tb, "Name:", wifi_current->name, row++);
   val = "Disabled";
   if (wifi_current->ipv6)
     {
        if (wifi_current->method == WIRELESS_NETWORK_IPV6_METHOD_MANUAL)
          val = "Manual";
        else if (wifi_current->method == WIRELESS_NETWORK_IPV6_METHOD_AUTO)
          val = "Auto";
        else if (wifi_current->method == WIRELESS_NETWORK_IPV6_METHOD_6TO4)
          val = "6to4";
     }
   else
     {
        if (wifi_current->method == WIRELESS_NETWORK_IPV4_METHOD_MANUAL)
          val = "Manual";
        else if (wifi_current->method == WIRELESS_NETWORK_IPV4_METHOD_DHCP)
          val = "DHCP";
     }
   _wifi_tooltip_row(tb, "Method:", val, row++);

   if (wifi_current->type == WIRELESS_SERVICE_TYPE_WIFI)
     {
        snprintf(buf, sizeof(buf), "%u%%", wifi_current->strength);
        _wifi_tooltip_row(tb, "Signal:", buf, row++);
     }

   if ((wifi_current->state == WIRELESS_NETWORK_STATE_CONNECTED) ||
       (wifi_current->state == WIRELESS_NETWORK_STATE_ONLINE))
     {
        _wifi_tooltip_row(tb, "Address:", wifi_current->address, row++);
     }
   return tb;
}

static void
wireless_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   instances = eina_list_remove(instances, inst);
   evas_object_hide(inst->popup);
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
   evas_object_show(g);
   elm_box_pack_end(inst->box, g);
   evas_object_show(inst->box);
   elm_object_tooltip_content_cb_set(g, _wifi_tooltip, inst, NULL);

   evas_object_size_hint_aspect_set(inst->box, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   evas_object_event_callback_add(inst->box, EVAS_CALLBACK_DEL, wireless_del, inst);
   evas_object_event_callback_add(g, EVAS_CALLBACK_MOUSE_DOWN, _wifi_mouse_down, inst);

   if (*id < 0)
     elm_object_signal_emit(g, "e,state,wifi", "e");
   else if (wifi_current)
     _wifi_icon_init(g, wifi_current);
   instances = eina_list_append(instances, inst);

   return inst->box;
}

EINTERN void
wireless_gadget_init(void)
{
   z_gadget_type_add("Wireless", wireless_create);
}

EINTERN void
wireless_gadget_shutdown(void)
{
   z_gadget_type_del("Wireless");
}

EINTERN void
wireless_wifi_network_state_set(Wireless_Network_State state)
{
   Eina_List *l;
   Instance *inst;

   if (wifi_network_state == state) return;

   wifi_network_state = state;
   //EINA_LIST_FOREACH(instances, l, inst)
     //_wifi_network_state_update(inst->wifi);
}

EINTERN void
wireless_wifi_current_network_set(Wireless_Connection *wn)
{
   Eina_List *l;
   Instance *inst;
   Wireless_Connection *prev;

   prev = wifi_current;
   wifi_current = wn;
   EINA_LIST_FOREACH(instances, l, inst)
     {
        if (inst->popup)
          {
             Elm_Object_Item *it;
             Evas_Object *icon;

             if (wn)
               {
                  it = eina_hash_find(inst->popup_items, &wn);
                  icon = elm_object_item_content_get(it);
                  _wifi_icon_init(icon, wn);
               }
             if (prev)
               {
                  it = eina_hash_find(inst->popup_items, &prev);
                  icon = elm_object_item_content_get(it);
                  _wifi_icon_init(icon, prev);
               }
          }
        _wifi_icon_init(inst->wifi, wn);
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
