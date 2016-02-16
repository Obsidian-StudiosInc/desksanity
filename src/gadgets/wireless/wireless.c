#include "wireless.h"

static const char *wireless_theme_groups[] =
{
  [WIRELESS_SERVICE_TYPE_ETHERNET] = "e/modules/wireless/ethernet",
  [WIRELESS_SERVICE_TYPE_WIFI] = "e/modules/wireless/wifi",
  [WIRELESS_SERVICE_TYPE_BLUETOOTH] = "e/modules/wireless/bluetooth",
  [WIRELESS_SERVICE_TYPE_CELLULAR] = "e/modules/wireless/cellular",
};

typedef struct Instance
{
   Z_Gadget_Site_Orient orient;
   Evas_Object *box;
   Evas_Object *icon[WIRELESS_SERVICE_TYPE_LAST];

   struct
   {
      Evas_Object *popup;
      Evas_Object *popup_list;
      Eina_Hash *popup_items;
      Wireless_Service_Type type;
   } popup;

   struct
   {
      Evas_Object *address;
      Evas_Object *method;
      Evas_Object *signal;
      Wireless_Service_Type type;
   } tooltip;
} Instance;

typedef struct Wireless_Auth_Popup
{
   Evas_Object *popup;
   Wireless_Auth_Cb cb;
   void *data;
   Eina_Bool sent : 1;
} Wireless_Auth_Popup;

static Eina_Array *wifi_networks;
static Wireless_Connection *wireless_current[WIRELESS_SERVICE_TYPE_LAST];
static Eina_Bool wireless_type_enabled[WIRELESS_SERVICE_TYPE_LAST];
static Eina_Bool wireless_type_available[WIRELESS_SERVICE_TYPE_LAST];
static Eina_List *instances;
static Eina_List *wireless_auth_pending;
static Wireless_Auth_Popup *wireless_auth_popup;

static void
_wifi_icon_signal(Evas_Object *icon, int state, int strength)
{
   Edje_Message_Int_Set *msg;

   msg = alloca(sizeof(Edje_Message_Int_Set) + sizeof(int));
   msg->count = 2;
   msg->val[0] = state;
   msg->val[1] = strength;
   edje_object_message_send(elm_layout_edje_get(icon), EDJE_MESSAGE_INT_SET, 1, msg);
}

static void
_wifi_icon_init(Evas_Object *icon, Wireless_Network *wn)
{
   int state = 0, strength = 0;

   if (wn)
     {
        state = wn->state;
        strength = wn->strength;
     }
   _wifi_icon_signal(icon, state, strength);

   if (!wn)
     {
        elm_object_signal_emit(icon, "e,state,default", "e");
        return;
     }
   if (wn->state == WIRELESS_NETWORK_STATE_FAILURE)
     {
        elm_object_signal_emit(icon, "e,state,error", "e");
        return;
     }
   elm_object_signal_emit(icon, "e,state,default", "e");
   switch (wn->type)
     {
      case WIRELESS_SERVICE_TYPE_WIFI:
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
_wireless_popup_network_click(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Wireless_Network *wn = data;
   Instance *inst;

   inst = evas_object_data_get(obj, "instance");
   if ((wn->state == WIRELESS_NETWORK_STATE_CONNECTED) || (wn->state == WIRELESS_NETWORK_STATE_ONLINE))
   {}
   else
     {
        /* FIXME */
        if (!wn->connect_cb(wn))
          {}
     }
}

static void
_wireless_popup_list_populate(Instance *inst)
{
   Eina_Iterator *it;
   Wireless_Network *wn;

   it = eina_array_iterator_new(wifi_networks);
   EINA_ITERATOR_FOREACH(it, wn)
     {
        Evas_Object *icon;
        Elm_Object_Item *item;
        const char *name = wn->name;

        if (wn->type != inst->popup.type) continue;
        icon = elm_layout_add(inst->popup.popup_list);
        e_theme_edje_object_set(icon, NULL, wireless_theme_groups[inst->popup.type]);
        _wifi_icon_init(icon, wn);
        if (!name)
          name = "<SSID hidden>";
        item = elm_list_item_append(inst->popup.popup_list, name, icon, NULL, _wireless_popup_network_click, wn);
        eina_hash_add(inst->popup.popup_items, &wn, item);
     }
   eina_iterator_free(it);
}

static void
_wireless_popup_toggle(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   /* FIXME */
   void connman_technology_enabled_set(Wireless_Service_Type type, Eina_Bool state);
   
   connman_technology_enabled_set(inst->popup.type, elm_check_state_get(obj));
}

static void
_wireless_popup_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   E_FREE_FUNC(inst->popup.popup_items, eina_hash_free);
   inst->popup.popup_list = NULL;
   inst->popup.popup = NULL;
   inst->popup.type = -1;
}

static Eina_Bool
_wireless_popup_key(void *d EINA_UNUSED, Ecore_Event_Key *ev)
{
   return strcmp(ev->key, "Escape");
}

static void
_wireless_gadget_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info)
{
   Evas_Event_Mouse_Down *ev = event_info;
   Instance *inst = data;
   Evas_Object *ctx, *box, *list, *toggle;
   int type;
   E_Zone *zone;
   const char *names[] =
   {
      "Ethernet",
      "Wifi",
      "Bluetooth",
      "Cellular",
   };

   if (ev->button != 1) return;
   for (type = 0; type < WIRELESS_SERVICE_TYPE_LAST; type++)
     if (obj == inst->icon[type])
       break;
   if (inst->popup.popup)
     {
        evas_object_hide(inst->popup.popup);
        evas_object_del(inst->popup.popup);
        if (inst->popup.type == type)
          return;
     }
   inst->popup.type = type;
   inst->popup.popup_items = eina_hash_pointer_new(NULL);
   ctx = elm_ctxpopup_add(e_comp->elm);
   elm_object_style_set(ctx, "noblock");

   box = elm_box_add(ctx);
   E_EXPAND(box);
   E_FILL(box);

   inst->popup.popup_list = list = elm_list_add(ctx);
   evas_object_data_set(list, "instance", inst);
   elm_list_mode_set(list, ELM_LIST_COMPRESS);
   E_EXPAND(list);
   E_FILL(list);
   _wireless_popup_list_populate(inst);
   elm_list_go(list);
   evas_object_show(list);
   elm_box_pack_end(box, list);
   toggle = elm_check_add(ctx);
   evas_object_show(toggle);
   elm_object_style_set(toggle, "toggle");
   elm_object_text_set(toggle, names[type]);
   elm_object_part_text_set(toggle, "on", "On");
   elm_object_part_text_set(toggle, "off", "Off");
   elm_check_state_set(toggle, wireless_type_enabled[type]);
   evas_object_smart_callback_add(toggle, "changed", _wireless_popup_toggle, inst);
   elm_box_pack_end(box, toggle);
   elm_object_content_set(ctx, box);
   z_gadget_util_ctxpopup_place(inst->box, ctx);
   evas_object_smart_callback_call(inst->box, "gadget_popup", ctx);
   inst->popup.popup = e_comp_object_util_add(ctx, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(inst->popup.popup, E_LAYER_MENU);
   e_comp_object_util_autoclose(inst->popup.popup, NULL, _wireless_popup_key, NULL);

   zone = e_zone_current_get();
   evas_object_resize(inst->popup.popup, zone->w / 5, zone->h / 4);
   evas_object_show(inst->popup.popup);
   evas_object_event_callback_add(inst->popup.popup, EVAS_CALLBACK_DEL, _wireless_popup_del, inst);
}

static Evas_Object *
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
   return lbl;
}

static const char *
_wifi_tooltip_method_name(void)
{
   const char *val = "Disabled";
   if (wireless_current[WIRELESS_SERVICE_TYPE_WIFI]->ipv6)
     {
        if (wireless_current[WIRELESS_SERVICE_TYPE_WIFI]->method == WIRELESS_NETWORK_IPV6_METHOD_MANUAL)
          val = "Manual";
        else if (wireless_current[WIRELESS_SERVICE_TYPE_WIFI]->method == WIRELESS_NETWORK_IPV6_METHOD_AUTO)
          val = "Auto";
        else if (wireless_current[WIRELESS_SERVICE_TYPE_WIFI]->method == WIRELESS_NETWORK_IPV6_METHOD_6TO4)
          val = "6to4";
     }
   else
     {
        if (wireless_current[WIRELESS_SERVICE_TYPE_WIFI]->method == WIRELESS_NETWORK_IPV4_METHOD_MANUAL)
          val = "Manual";
        else if (wireless_current[WIRELESS_SERVICE_TYPE_WIFI]->method == WIRELESS_NETWORK_IPV4_METHOD_DHCP)
          val = "DHCP";
     }
   return val;
}

static void
_wifi_tooltip_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   inst->tooltip.address = inst->tooltip.method = inst->tooltip.signal = NULL;
   inst->tooltip.type = -1;
}

static Evas_Object *
_wifi_tooltip(void *data, Evas_Object *obj EINA_UNUSED, Evas_Object *tooltip)
{
   Instance *inst = data;
   Evas_Object *tb;
   int row = 0;
   char buf[1024];
   int type = WIRELESS_SERVICE_TYPE_WIFI;

   if (!wireless_current[type]) return NULL;
   tb = elm_table_add(tooltip);
   elm_table_padding_set(tb, 5, 1);

   _wifi_tooltip_row(tb, "Name:", wireless_current[type]->wn->name, row++);
   inst->tooltip.method = _wifi_tooltip_row(tb, "Method:", _wifi_tooltip_method_name(), row++);

   inst->tooltip.address = _wifi_tooltip_row(tb, "Address:", wireless_current[type]->address, row++);
   snprintf(buf, sizeof(buf), "%u%%", wireless_current[type]->wn->strength);
   inst->tooltip.signal = _wifi_tooltip_row(tb, "Signal:", buf, row++);

   evas_object_event_callback_add(tb, EVAS_CALLBACK_DEL, _wifi_tooltip_del, inst);
   return tb;
}

static void
wireless_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   instances = eina_list_remove(instances, inst);
   evas_object_hide(inst->popup.popup);
   evas_object_del(inst->popup.popup);
   free(inst);
}

static void
_wireless_gadget_refresh(Instance *inst)
{
   Elm_Tooltip_Content_Cb tooltip_cb[] =
   {
     [WIRELESS_SERVICE_TYPE_ETHERNET] = NULL,
     [WIRELESS_SERVICE_TYPE_WIFI] = _wifi_tooltip,
     [WIRELESS_SERVICE_TYPE_BLUETOOTH] = NULL,
     [WIRELESS_SERVICE_TYPE_CELLULAR] = NULL,
   };
   int type;
   int avail = 0;

   for (type = 0; type < WIRELESS_SERVICE_TYPE_LAST; type++)
     {
        if (wireless_type_available[type])
          {
             if (!inst->icon[type])
               {
                  Evas_Object *g;

                  inst->icon[type] = g = elm_layout_add(inst->box);
                  E_EXPAND(g);
                  E_FILL(g);
                  e_theme_edje_object_set(g, NULL, wireless_theme_groups[type]);
                  if (tooltip_cb[type])
                    elm_object_tooltip_content_cb_set(g, tooltip_cb[type], inst, NULL);
                  evas_object_event_callback_add(g, EVAS_CALLBACK_MOUSE_DOWN, _wireless_gadget_mouse_down, inst);
               }
             _wifi_icon_init(inst->icon[type], wireless_current[type] ? wireless_current[type]->wn : NULL);
             evas_object_hide(inst->icon[type]);
             avail++;
          }
        else
          E_FREE_FUNC(inst->icon[type], evas_object_del);
     }
   elm_box_unpack_all(inst->box);
   type = WIRELESS_SERVICE_TYPE_ETHERNET;
   if (inst->icon[type])
     {
        /* only show ethernet if it's connected or there's no wifi available */
        if ((!inst->icon[WIRELESS_SERVICE_TYPE_WIFI]) ||
            (wireless_current[type] &&
            (wireless_current[type]->wn->state == WIRELESS_NETWORK_STATE_ONLINE)))
          {
             elm_box_pack_end(inst->box, inst->icon[type]);
             evas_object_show(inst->icon[type]);
          }
     }
   for (type = WIRELESS_SERVICE_TYPE_WIFI; type < WIRELESS_SERVICE_TYPE_LAST; type++)
     {
        if (!inst->icon[type]) continue;
        
        elm_box_pack_end(inst->box, inst->icon[type]);
        evas_object_show(inst->icon[type]);
     }
   if (inst->orient == Z_GADGET_SITE_ORIENT_VERTICAL)
     evas_object_size_hint_aspect_set(inst->box, EVAS_ASPECT_CONTROL_BOTH, 1, avail);
   else
     evas_object_size_hint_aspect_set(inst->box, EVAS_ASPECT_CONTROL_BOTH, avail, 1);
}

static Evas_Object *
wireless_create(Evas_Object *parent, int *id, Z_Gadget_Site_Orient orient)
{
   Evas_Object *g;
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->orient = orient;
   inst->popup.type = inst->tooltip.type = -1;
   inst->box = elm_box_add(parent);
   elm_box_horizontal_set(inst->box, orient != Z_GADGET_SITE_ORIENT_VERTICAL);
   elm_box_homogeneous_set(inst->box, 1);
   evas_object_event_callback_add(inst->box, EVAS_CALLBACK_DEL, wireless_del, inst);

   if (*id < 0)
     {
        inst->icon[WIRELESS_SERVICE_TYPE_WIFI] = g = elm_layout_add(inst->box);
        E_EXPAND(g);
        E_FILL(g);
        e_theme_edje_object_set(g, NULL, "e/modules/wireless/wifi");
        elm_object_signal_emit(g, "e,state,default", "e");
        _wifi_icon_signal(g, WIRELESS_NETWORK_STATE_ONLINE, 100);
        evas_object_size_hint_aspect_set(inst->box, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
     }
   else
     _wireless_gadget_refresh(inst);
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
wireless_service_type_available_set(Eina_Bool *avail)
{
   if (!memcmp(avail, &wireless_type_available, sizeof(wireless_type_available))) return;
   memcpy(&wireless_type_available, avail, WIRELESS_SERVICE_TYPE_LAST * sizeof(Eina_Bool));
   E_LIST_FOREACH(instances, _wireless_gadget_refresh);
}

EINTERN void
wireless_service_type_enabled_set(Eina_Bool *avail)
{
   if (!memcmp(avail, &wireless_type_enabled, sizeof(wireless_type_enabled))) return;
   memcpy(&wireless_type_enabled, avail, WIRELESS_SERVICE_TYPE_LAST * sizeof(Eina_Bool));
   E_LIST_FOREACH(instances, _wireless_gadget_refresh);
}

EINTERN void
wireless_wifi_current_networks_set(Wireless_Connection **current)
{
   Eina_List *l;
   Instance *inst;
   Wireless_Connection *prev[WIRELESS_SERVICE_TYPE_LAST] = {NULL};

   memcpy(&prev, &wireless_current, WIRELESS_SERVICE_TYPE_LAST * sizeof(void*));
   memcpy(&wireless_current, current, WIRELESS_SERVICE_TYPE_LAST * sizeof(void*));
   EINA_LIST_FOREACH(instances, l, inst)
     {
        int type;

        type = inst->popup.type;
        if (type > -1)
          {
             Elm_Object_Item *it;
             Evas_Object *icon;

             if (wireless_current[type])
               {
                  it = eina_hash_find(inst->popup.popup_items, &wireless_current[type]->wn);
                  icon = elm_object_item_content_get(it);
                  _wifi_icon_init(icon, wireless_current[type]->wn);
               }
             if (prev[type])
               {
                  it = eina_hash_find(inst->popup.popup_items, &prev[type]->wn);
                  if (it)
                    {
                       icon = elm_object_item_content_get(it);
                       _wifi_icon_init(icon, prev[type]->wn);
                    }
               }
          }
        for (type = 0; type < WIRELESS_SERVICE_TYPE_LAST; type++)
          {
             if (inst->icon[type] && wireless_current[type])
               _wifi_icon_init(inst->icon[type], wireless_current[type]->wn);
          }
        type = inst->tooltip.type;
        if (type < 0) continue;
        if (prev[type] &&
          ((!wireless_current[type]) ||
            ((wireless_current[type] != prev[type]) && (!eina_streq(wireless_current[type]->wn->name, prev[type]->wn->name)))))
          {
             elm_object_tooltip_hide(inst->icon[type]);
             continue;
          }
        if (inst->tooltip.method)
          elm_object_text_set(inst->tooltip.method, _wifi_tooltip_method_name());
        if (inst->tooltip.address)
          elm_object_text_set(inst->tooltip.address, wireless_current[type]->address);
        if (inst->tooltip.signal)
          {
             char buf[32];

             snprintf(buf, sizeof(buf), "%u%%", wireless_current[type]->wn->strength);
             elm_object_text_set(inst->tooltip.signal, buf);
          }
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
     if (inst->popup.popup)
       {
          elm_list_clear(inst->popup.popup_list);
          eina_hash_free_buckets(inst->popup.popup_items);
          _wireless_popup_list_populate(inst);
       }

   return prev;
}

EINTERN void
wireless_airplane_mode_set(Eina_Bool enabled)
{

}

static void
_wireless_auth_del(void *data, Evas_Object *popup)
{
   Wireless_Auth_Popup *p = data;

   if (!p->sent)
     p->cb(p->data, NULL);
   free(p);
   if (popup == wireless_auth_popup->popup)
     wireless_auth_popup = NULL;
   if (!wireless_auth_pending) return;
   wireless_auth_popup = eina_list_data_get(wireless_auth_pending);
   wireless_auth_pending = eina_list_remove_list(wireless_auth_pending, wireless_auth_pending);
   evas_object_show(wireless_auth_popup->popup);
   e_comp_object_util_autoclose(wireless_auth_popup->popup, _wireless_auth_del, _wireless_popup_key, wireless_auth_popup);
}

static void
_wireless_auth_send(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Wireless_Auth_Popup *p = data;
   Eina_Array *arr = NULL;
   Evas_Object *tb, *o;
   unsigned int row = 0;

   tb = evas_object_data_get(obj, "table");
   do
     {
        const char *txt;

        o = elm_table_child_get(tb, 0, row);
        if (!o) break;
        if (!arr) arr = eina_array_new(2);
        txt = elm_object_text_get(o);
        eina_array_push(arr, txt);
        o = elm_table_child_get(tb, 1, row);
        /* skip checkboxes */
        if (!strncmp(txt, "Pass", 4)) row++;
        eina_array_push(arr, elm_object_text_get(o));
        row++;
     } while (1);
   p->cb(p->data, arr);
   p->sent = 1;
   eina_array_free(arr);
}

static void
_wireless_auth_table_row(Evas_Object *tb, const char *name, Wireless_Auth_Popup *p, int *row)
{
   Evas_Object *lbl, *entry, *ck;
   char buf[1024];

   lbl = elm_label_add(tb);
   evas_object_show(lbl);
   E_ALIGN(lbl, 0, -1);
   elm_object_text_set(lbl, name);
   elm_table_pack(tb, lbl, 0, *row, 1, 1);

   entry = elm_entry_add(tb);
   evas_object_show(entry);
   E_ALIGN(entry, 0, -1);
   E_EXPAND(entry);
   elm_entry_single_line_set(entry, 1);
   evas_object_data_set(entry, "table", tb);
   evas_object_smart_callback_add(entry, "activated", _wireless_auth_send, p);
   elm_table_pack(tb, lbl, 1, *row, 1, 1);
   if (strncmp(name, "Pass", 4)) return;
   elm_entry_password_set(entry, 1);

   ck = elm_check_add(tb);
   evas_object_show(ck);
   E_ALIGN(ck, 0, -1);
   snprintf(buf, sizeof(buf), "Show %s", name);
   elm_object_text_set(ck, buf);
   elm_table_pack(tb, lbl, 0, ++(*row), 2, 1);
}

EINTERN void
wireless_authenticate(const Eina_Array *fields, Wireless_Auth_Cb cb, void *data)
{
   Evas_Object *popup, *tb, *lbl;
   Instance *inst;
   Eina_List *l;
   Eina_Iterator *it;
   const char *f;
   Wireless_Auth_Popup *p;
   int row = 0;

   p = E_NEW(Wireless_Auth_Popup, 1);
   p->cb = cb;
   p->data = data;
   EINA_LIST_FOREACH(instances, l, inst)
     if (inst->popup.popup)
       {
          evas_object_hide(inst->popup.popup);
          evas_object_del(inst->popup.popup);
       }

   popup = elm_popup_add(e_comp->elm);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   tb = elm_table_add(popup);
   evas_object_show(tb);
   elm_object_content_set(popup, tb);

   lbl = elm_label_add(popup);
   evas_object_show(lbl);
   elm_object_text_set(lbl, "Authentication Required");
   elm_table_pack(tb, lbl, 0, row++, 2, 1);

   it = eina_array_iterator_new(fields);
   EINA_ITERATOR_FOREACH(it, f)
     {
        _wireless_auth_table_row(tb, f, p, &row);
        row++;
     }
   popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   p->popup = popup;
   evas_object_resize(popup, e_zone_current_get()->w / 4, e_zone_current_get()->h / 3);
   evas_object_layer_set(popup, E_LAYER_MENU);
   e_comp_object_util_center(popup);
   if (wireless_auth_popup)
     wireless_auth_pending = eina_list_append(wireless_auth_pending, p);
   else
     {
        wireless_auth_popup = p;
        evas_object_show(popup);
        e_comp_object_util_autoclose(popup, _wireless_auth_del, _wireless_popup_key, p);
     }
}

EINTERN void
wireless_authenticate_cancel(void)
{
   if (!wireless_auth_popup) return;
   evas_object_hide(wireless_auth_popup->popup);
   evas_object_del(wireless_auth_popup->popup);
}
