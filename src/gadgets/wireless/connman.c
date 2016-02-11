#include "wireless.h"

#define CONNMAN_BUS_NAME "net.connman"
#define CONNMAN_MANAGER_IFACE CONNMAN_BUS_NAME ".Manager"
#define CONNMAN_SERVICE_IFACE CONNMAN_BUS_NAME ".Service"
#define CONNMAN_TECHNOLOGY_IFACE CONNMAN_BUS_NAME ".Technology"
#define CONNMAN_TECHNOLOGY_PATH "/net/connman/technology/wifi"
#define CONNMAN_AGENT_IFACE "net.connman.Agent"
#define CONNMAN_AGENT_PATH "/org/enlightenment/connman/agent"

#undef DBG
#undef INF
#undef WRN
#undef ERR

#define DBG(...) EINA_LOG_DOM_DBG(_connman_log_dom, __VA_ARGS__)
#define INF(...) EINA_LOG_DOM_INFO(_connman_log_dom, __VA_ARGS__)
#define WRN(...) EINA_LOG_DOM_WARN(_connman_log_dom, __VA_ARGS__)
#define ERR(...) EINA_LOG_DOM_ERR(_connman_log_dom, __VA_ARGS__)

typedef enum
{
   CONNMAN_STATE_NONE = -1, /* All unknown states */
   CONNMAN_STATE_OFFLINE,
   CONNMAN_STATE_IDLE,
   CONNMAN_STATE_ASSOCIATION,
   CONNMAN_STATE_CONFIGURATION,
   CONNMAN_STATE_READY,
   CONNMAN_STATE_ONLINE,
   CONNMAN_STATE_DISCONNECT,
   CONNMAN_STATE_FAILURE,
} Connman_State;

typedef enum
{
   CONNMAN_SERVICE_TYPE_NONE = -1, /* All non-supported types */
   CONNMAN_SERVICE_TYPE_ETHERNET,
   CONNMAN_SERVICE_TYPE_WIFI,
   CONNMAN_SERVICE_TYPE_BLUETOOTH,
   CONNMAN_SERVICE_TYPE_CELLULAR,
} Connman_Service_Type;

typedef struct
{
   EINA_INLIST;
   const char *path;
   Eldbus_Proxy *proxy;

   /* Properties */
   Eina_Stringshare *name;
   Eina_Array *security;
   Connman_State state;
   Connman_Service_Type type;
   uint8_t strength;

   /* Private */
   struct
     {
        Eldbus_Pending *connect;
        Eldbus_Pending *disconnect;
        Eldbus_Pending *remov;
        void *data;
     } pending;
} Connman_Service;

typedef struct Connman_Field
{
   const char *name;

   const char *type;
   const char *requirement;
   const char *value;
   Eina_Array *alternates;
} Connman_Field;

static int _connman_log_dom = -1;

static Eldbus_Proxy *proxy_manager;
static Eldbus_Proxy *proxy_technology;

static Eldbus_Pending *pending_getservices;
static Eldbus_Pending *pending_getproperties_manager;

static Eina_Inlist *connman_services;
static unsigned int connman_services_count;
static Eldbus_Service_Interface *agent_iface;

static Connman_State connman_state;
static Eina_Bool connman_offline;
static Eina_Bool connman_powered;

static void
connman_update_networks(void)
{
   Eina_Array *arr;
   Connman_Service *cs;

   arr = eina_array_new(connman_services_count);
   EINA_INLIST_FOREACH(connman_services, cs)
     eina_array_push(arr, &cs->name);
   arr = wireless_wifi_networks_set(arr);
   eina_array_free(arr);
}

static void
connman_update_airplane_mode(Eina_Bool offline)
{
   wireless_airplane_mode_set(offline);
}

static void
connman_update_state(Connman_State state)
{
   Wifi_State wifi_state;

   if (connman_state == state) return;
   connman_state = state;
   switch (connman_state)
     {
      case CONNMAN_STATE_ASSOCIATION:
      case CONNMAN_STATE_CONFIGURATION:
        wifi_state = WIFI_NETWORK_STATE_CONFIGURING;
        break;
      case CONNMAN_STATE_READY:
        wifi_state = WIFI_NETWORK_STATE_CONNECTED;
        break;
      case CONNMAN_STATE_ONLINE:
        wifi_state = WIFI_NETWORK_STATE_ONLINE;
        break;
      case CONNMAN_STATE_FAILURE:
        wifi_state = WIFI_NETWORK_STATE_FAILURE;
        break;
      case CONNMAN_STATE_NONE:
      case CONNMAN_STATE_OFFLINE:
      case CONNMAN_STATE_IDLE:
      case CONNMAN_STATE_DISCONNECT:
      default:
        wifi_state = WIFI_NETWORK_STATE_NONE;
     }
   wireless_wifi_state_set(wifi_state);
}

static Connman_State
str_to_state(const char *s)
{
   if (!strcmp(s, "offline"))
     return CONNMAN_STATE_OFFLINE;
   if (!strcmp(s, "idle"))
     return CONNMAN_STATE_IDLE;
   if (!strcmp(s, "association"))
     return CONNMAN_STATE_ASSOCIATION;
   if (!strcmp(s, "configuration"))
     return CONNMAN_STATE_CONFIGURATION;
   if (!strcmp(s, "ready"))
     return CONNMAN_STATE_READY;
   if (!strcmp(s, "online"))
     return CONNMAN_STATE_ONLINE;
   if (!strcmp(s, "disconnect"))
     return CONNMAN_STATE_DISCONNECT;
   if (!strcmp(s, "failure"))
     return CONNMAN_STATE_FAILURE;

   ERR("Unknown state %s", s);
   return CONNMAN_STATE_NONE;
}

static Connman_Service_Type
str_to_type(const char *s)
{
   if (!strcmp(s, "ethernet"))
     return CONNMAN_SERVICE_TYPE_ETHERNET;
   else if (!strcmp(s, "wifi"))
     return CONNMAN_SERVICE_TYPE_WIFI;
   else if (!strcmp(s, "bluetooth"))
     return CONNMAN_SERVICE_TYPE_BLUETOOTH;
   else if (!strcmp(s, "cellular"))
     return CONNMAN_SERVICE_TYPE_CELLULAR;

   DBG("Unknown type %s", s);
   return CONNMAN_SERVICE_TYPE_NONE;
}

static void
_connman_service_security_array_clean(Connman_Service *cs)
{
   const char *item;
   Eina_Array_Iterator itr;
   unsigned int i;

   EINA_ARRAY_ITER_NEXT(cs->security, i, item, itr)
     eina_stringshare_del(item);

   eina_array_clean(cs->security);
}

static void
_connman_service_free(Connman_Service *cs)
{
   Eldbus_Object *obj;

   if (!cs) return;

   if (cs->pending.connect)
     {
        eldbus_pending_cancel(cs->pending.connect);
        free(cs->pending.data);
     }
   else if (cs->pending.disconnect)
     {
        eldbus_pending_cancel(cs->pending.disconnect);
        free(cs->pending.data);
     }
   else if (cs->pending.remov)
     {
        eldbus_pending_cancel(cs->pending.remov);
        free(cs->pending.data);
     }

   eina_stringshare_del(cs->name);
   _connman_service_security_array_clean(cs);
   eina_array_free(cs->security);
   eina_stringshare_del(cs->path);
   obj = eldbus_proxy_object_get(cs->proxy);
   eldbus_proxy_unref(cs->proxy);
   eldbus_object_unref(obj);

   free(cs);
}

static void
_connman_service_parse_prop_changed(Connman_Service *cs, const char *prop_name, Eldbus_Message_Iter *value)
{
   DBG("service %p %s prop %s", cs, cs->path, prop_name);

   if (!strcmp(prop_name, "State"))
     {
        const char *state;
        EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value,
                                                                     "s",
                                                                     &state));
        cs->state = str_to_state(state);
        DBG("New state: %s %d", state, cs->state);
     }
   else if (!strcmp(prop_name, "Name"))
     {
        const char *name;
        EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value,
                                                                     "s",
                                                                     &name));
        eina_stringshare_replace(&cs->name, name);
        DBG("New name: %s", cs->name);
     }
   else if (!strcmp(prop_name, "Type"))
     {
        const char *type;
        EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value,
                                                                     "s",
                                                                     &type));
        cs->type = str_to_type(type);
        DBG("New type: %s %d", type, cs->type);
     }
   else if (!strcmp(prop_name, "Strength"))
     {
        uint8_t strength;
        EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value,
                                                                     "y",
                                                                     &strength));
        cs->strength = strength;
        DBG("New strength: %d", strength);
     }
   else if (!strcmp(prop_name, "Security"))
     {
        const char *s;

        DBG("Old security count: %d",
            cs->security ? eina_array_count(cs->security) : 0);
        if (cs->security)
          _connman_service_security_array_clean(cs);
        else
          cs->security = eina_array_new(5);
        
        while (eldbus_message_iter_get_and_next(value, 's', &s))
          {
             eina_array_push(cs->security, eina_stringshare_add(s));
             DBG("Push %s", s);
          }
        DBG("New security count: %d", eina_array_count(cs->security));
     }
}

static void
_connman_service_prop_dict_changed(Connman_Service *cs, Eldbus_Message_Iter *props)
{
   Eldbus_Message_Iter *dict;

   while (eldbus_message_iter_get_and_next(props, 'e', &dict))
     {
        char *name;
        Eldbus_Message_Iter *var;

        if (eldbus_message_iter_arguments_get(dict, "sv", &name, &var))
          _connman_service_parse_prop_changed(cs, name, var);
     }
}

static void
_connman_service_property(void *data, const Eldbus_Message *msg)
{
   Connman_Service *cs = data;
   Eldbus_Message_Iter *var;
   const char *name;

   if (eldbus_message_arguments_get(msg, "sv", &name, &var))
     _connman_service_parse_prop_changed(cs, name, var);
}

static void
_connman_service_new(const char *path, Eldbus_Message_Iter *props)
{
   Connman_Service *cs;
   Eldbus_Object *obj;

   cs = E_NEW(Connman_Service, 1);
   cs->path = eina_stringshare_add(path);

   obj = eldbus_object_get(dbus_conn, CONNMAN_BUS_NAME, path);
   cs->proxy = eldbus_proxy_get(obj, CONNMAN_SERVICE_IFACE);
   eldbus_proxy_signal_handler_add(cs->proxy, "PropertyChanged",
                                  _connman_service_property, cs);

   _connman_service_prop_dict_changed(cs, props);
   connman_services = eina_inlist_append(connman_services, EINA_INLIST_GET(cs));
   connman_services_count++;
   DBG("Added service: %p %s", cs, path);
}

static void
_connman_manager_agent_register(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{

}

static void
_connman_technology_parse_wifi_prop_changed(const char *name, Eldbus_Message_Iter *value)
{
   if (!strcmp(name, "Powered"))
     {
        Eina_Bool powered;

        eldbus_message_iter_arguments_get(value, "b", &powered);
        //connman_update_
     }
}

static void
_connman_technology_event_property(void *data EINA_UNUSED, const Eldbus_Message *msg)
{
   Eldbus_Message_Iter *var;
   const char *name;

   if (!eldbus_message_arguments_get(msg, "sv", &name, &var))
     ERR("Could not parse message %p", msg);
   else
     _connman_technology_parse_wifi_prop_changed(name, var);
}

static Eina_Bool
_connman_manager_parse_prop_changed(const char *name, Eldbus_Message_Iter *value)
{
   if (!strcmp(name, "State"))
     {
        const char *state;

        if (!eldbus_message_iter_arguments_get(value, "s", &state))
          return EINA_FALSE;
        DBG("New state: %s", state);
        connman_update_state(str_to_state(state));
     }
   else if (!strcmp(name, "OfflineMode"))
     {
        Eina_Bool offline;
        if (!eldbus_message_iter_arguments_get(value, "b", &offline))
          return EINA_FALSE;
        connman_update_airplane_mode(offline);
     }
   else
     return EINA_FALSE;

   return EINA_TRUE;
}

static void
_connman_manager_getproperties(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   Eldbus_Message_Iter *array, *dict;
   const char *name, *text;

   pending_getproperties_manager = NULL;
   if (eldbus_message_error_get(msg, &name, &text))
     {
        ERR("Could not get properties. %s: %s", name, text);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "a{sv}", &array))
     {
        ERR("Error getting arguments.");
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'e', &dict))
     {
        const char *key;
        Eldbus_Message_Iter *var;

        if (eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          _connman_manager_parse_prop_changed(key, var);
     }
}

static void
_connman_manager_getservices(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   Eldbus_Message_Iter *array, *s;
   const char *name, *text;

   pending_getservices = NULL;
   if (eldbus_message_error_get(msg, &name, &text))
     {
        ERR("Could not get services. %s: %s", name, text);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "a(oa{sv})", &array))
     {
        ERR("Error getting array");
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'r', &s))
     {
        const char *path;
        Eldbus_Message_Iter *inner_array;

        if (!eldbus_message_iter_arguments_get(s, "oa{sv}", &path, &inner_array))
          continue;

        _connman_service_new(path, inner_array);
     }
   connman_update_networks();
}

static void
_connman_manager_event_services(void *data EINA_UNUSED, const Eldbus_Message *msg)
{
   Eldbus_Message_Iter *changed, *removed, *s;
   const char *path;
   Eina_Bool update = EINA_FALSE;

   if (pending_getservices) return;

   if (!eldbus_message_arguments_get(msg, "a(oa{sv})ao", &changed, &removed))
     {
        ERR("Error getting arguments");
        return;
     }

   while (eldbus_message_iter_get_and_next(removed, 'o', &path))
     {
        Connman_Service *cs;

        EINA_INLIST_FOREACH(connman_services, cs)
          {
             if (!eina_streq(cs->path, path)) continue;
             connman_services = eina_inlist_remove(connman_services, EINA_INLIST_GET(cs));
             DBG("Removed service: %p %s", cs, path);
             connman_services_count--;
             _connman_service_free(cs);
             update = EINA_TRUE;
             break;
          }
     }

   while (eldbus_message_iter_get_and_next(changed, 'r', &s))
     {
        Connman_Service *cs;
        Eldbus_Message_Iter *array;
        Eina_Bool found = EINA_FALSE;

        if (!eldbus_message_iter_arguments_get(s, "oa{sv}", &path, &array))
          continue;

        update = EINA_TRUE;
        EINA_INLIST_FOREACH(connman_services, cs)
          {
             if (!eina_streq(path, cs->path)) continue;
             _connman_service_prop_dict_changed(cs, array);
             DBG("Changed service: %p %s", cs, path);
             break;
          }
        if (!found)
          _connman_service_new(path, array);
     }
   if (update)
     connman_update_networks();
}

static void
_connman_manager_event_property(void *data EINA_UNUSED, const Eldbus_Message *msg)
{
   Eldbus_Message_Iter *var;
   const char *name;

   if (pending_getproperties_manager) return;
   if (!eldbus_message_arguments_get(msg, "sv", &name, &var))
     {
        ERR("Could not parse message %p", msg);
        return;
     }

   _connman_manager_parse_prop_changed(name, var);
}

static Eldbus_Message *
_connman_agent_release(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   Eldbus_Message *reply;

   DBG("Agent released");

   reply = eldbus_message_method_return_new(msg);
#warning FIXME
   //if (agent->dialog)
     //e_object_del(E_OBJECT(agent->dialog));

   return reply;
}

static Eldbus_Message *
_connman_agent_report_error(const Eldbus_Service_Interface *iface EINA_UNUSED,
                    const Eldbus_Message *msg EINA_UNUSED)
{
#warning FIXME
   return NULL;
}

static Eldbus_Message *
_connman_agent_request_browser(const Eldbus_Service_Interface *iface EINA_UNUSED,
                       const Eldbus_Message *msg EINA_UNUSED)
{
#warning FIXME
   return NULL;
}

static Eina_Bool
_parse_field_value(Connman_Field *field, const char *key,
                   Eldbus_Message_Iter *value, const char *signature)
{
   if (!strcmp(key, "Type"))
     {
        EINA_SAFETY_ON_FALSE_RETURN_VAL(signature[0] == 's', EINA_FALSE);
        eldbus_message_iter_basic_get(value, &field->type);
        return EINA_TRUE;
     }

   if (!strcmp(key, "Requirement"))
     {
        EINA_SAFETY_ON_FALSE_RETURN_VAL(signature[0] == 's', EINA_FALSE);
        eldbus_message_iter_basic_get(value, &field->requirement);
        return EINA_TRUE;
     }

   if (!strcmp(key, "Alternates"))
     {
        EINA_SAFETY_ON_FALSE_RETURN_VAL(signature[0] == 'a', EINA_FALSE);
        /* ignore alternates */
        return EINA_TRUE;
     }

   if (!strcmp(key, "Value"))
     {
        EINA_SAFETY_ON_FALSE_RETURN_VAL(signature[0] == 's', EINA_FALSE);
        eldbus_message_iter_basic_get(value, &field->value);
        return EINA_TRUE;
     }

   DBG("Ignored unknown argument: %s", key);
   return EINA_FALSE;
}

static Eina_Bool
_parse_field(Connman_Field *field, Eldbus_Message_Iter *value,
             const char *signature EINA_UNUSED)
{
   Eldbus_Message_Iter *array, *dict;

   eldbus_message_iter_arguments_get(value, "a{sv}", &array);
   EINA_SAFETY_ON_NULL_RETURN_VAL(array, EINA_FALSE);

   while (eldbus_message_iter_get_and_next(array, 'e', &dict))
     {
        Eldbus_Message_Iter *var;
        const char *key;
        char *sig2;

        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          return EINA_FALSE;
        sig2 = eldbus_message_iter_signature_get(var);
        if (!sig2)
          return EINA_FALSE;

        if (!_parse_field_value(field, key, var, sig2))
          {
             free(sig2);
             return EINA_FALSE;
          }
        free(sig2);
     }

   return EINA_TRUE;
}

static Eldbus_Message *
_connman_agent_request_input(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
#warning FIXME
#if 0
   const Eina_List *l;
   Eldbus_Message_Iter *array, *dict;
   const char *path;
   /* Discard previous requests */
   // if msg is the current agent msg? eek.
   if (agent->msg == msg) return NULL;

   if (agent->msg)
     eldbus_message_unref(agent->msg);
   agent->msg = eldbus_message_ref((Eldbus_Message *)msg);

   EINA_LIST_FOREACH(ctxt->instances, l, inst)
     econnman_popup_del(inst);

   if (agent->dialog)
     e_object_del(E_OBJECT(agent->dialog));
   agent->dialog = _dialog_new(agent);
   EINA_SAFETY_ON_NULL_GOTO(agent->dialog, err);

   if (!eldbus_message_arguments_get(msg, "oa{sv}", &path, &array))
     goto err;

   while (eldbus_message_iter_get_and_next(array, 'e', &dict))
     {
        Eldbus_Message_Iter *var;
        char *signature;
        Connman_Field field = { NULL };

        if (!eldbus_message_iter_arguments_get(dict, "sv", &field.name, &var))
          goto err;
        signature = eldbus_message_iter_signature_get(var);
        if (!signature) goto err;

        if (!_parse_field(&field, var, signature))
          {
             free(signature);
             goto err;
          }
        free(signature);

        DBG("AGENT Got field:\n"
            "\tName: %s\n"
            "\tType: %s\n"
            "\tRequirement: %s\n"
            "\tAlternates: (omit array)\n"
            "\tValue: %s",
            field.name, field.type, field.requirement, field.value);

        _dialog_field_add(agent, &field);
     }

   return NULL;

err:
   eldbus_message_unref((Eldbus_Message *)msg);
   agent->msg = NULL;
   WRN("Failed to parse msg");
#endif
   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
_connman_agent_cancel(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   Eldbus_Message *reply = eldbus_message_method_return_new(msg);

   DBG("Agent canceled");
#warning FIXME
   //if (agent->dialog)
     //e_object_del(E_OBJECT(agent->dialog));

   return reply;
}

static const Eldbus_Method methods[] = {
   { "Release", NULL, NULL, _connman_agent_release, 0 },
   {
    "ReportError", ELDBUS_ARGS({"o", "service"}, {"s", "error"}), NULL,
    _connman_agent_report_error, 0
   },
   //{
    //"ReportPeerError", ELDBUS_ARGS({"o", "peer"}, {"s", "error"}), NULL,
    //_connman_agent_report_peer_error, 0
   //},
   {
    "RequestBrowser", ELDBUS_ARGS({"o", "service"}, {"s", "url"}), NULL,
     _connman_agent_request_browser, 0
   },
   {
    "RequestInput", ELDBUS_ARGS({"o", "service"}, {"a{sv}", "fields"}),
    ELDBUS_ARGS({"a{sv}", ""}), _connman_agent_request_input, 0
   },
   //{
    //"RequestPeerAuthorization", ELDBUS_ARGS({"o", "peer"}, {"a{sv}", "fields"}),
    //ELDBUS_ARGS({"a{sv}", ""}), _connman_agent_request_peer_auth, 0
   //},
   { "Cancel", NULL, NULL, _connman_agent_cancel, 0 },
   { NULL, NULL, NULL, NULL, 0 }
};

static const Eldbus_Service_Interface_Desc desc = {
   CONNMAN_AGENT_IFACE, methods, NULL, NULL, NULL, NULL
};

static void
_connman_start(void)
{
   Eldbus_Object *obj;

   obj = eldbus_object_get(dbus_conn, CONNMAN_BUS_NAME, "/");
   proxy_manager = eldbus_proxy_get(obj, CONNMAN_MANAGER_IFACE);
   obj = eldbus_object_get(dbus_conn, CONNMAN_BUS_NAME, CONNMAN_TECHNOLOGY_PATH);
   proxy_technology = eldbus_proxy_get(obj, CONNMAN_TECHNOLOGY_IFACE);

   eldbus_proxy_signal_handler_add(proxy_manager, "PropertyChanged",
                                   _connman_manager_event_property, NULL);
   eldbus_proxy_signal_handler_add(proxy_manager, "ServicesChanged",
                                   _connman_manager_event_services, NULL);
   eldbus_proxy_signal_handler_add(proxy_technology, "PropertyChanged",
                                   _connman_technology_event_property, NULL);
   
   /*
    * PropertyChanged signal in service's path is guaranteed to arrive only
    * after ServicesChanged above. So we only add the handler later, in a per
    * service manner.
    */
   pending_getservices = eldbus_proxy_call(proxy_manager, "GetServices", _connman_manager_getservices,
     NULL, -1, "");
   pending_getproperties_manager = eldbus_proxy_call(proxy_manager, "GetProperties", _connman_manager_getproperties,
     NULL, -1, "");
   eldbus_proxy_call(proxy_technology, "Scan", NULL, NULL, -1, "");

   agent_iface = eldbus_service_interface_register(dbus_conn, CONNMAN_AGENT_PATH, &desc);
}

static void
_connman_end(void)
{
   Eldbus_Object *obj;

   eldbus_proxy_call(proxy_manager, "UnregisterAgent", NULL, NULL, -1, "o", CONNMAN_AGENT_PATH);

   while (connman_services)
     {
        Connman_Service *cs = EINA_INLIST_CONTAINER_GET(connman_services,
                                                        Connman_Service);
        connman_services = eina_inlist_remove(connman_services, connman_services);
        _connman_service_free(cs);
     }

   E_FREE_FUNC(pending_getservices, eldbus_pending_cancel);
   E_FREE_FUNC(pending_getproperties_manager, eldbus_pending_cancel);

   obj = eldbus_proxy_object_get(proxy_manager);
   E_FREE_FUNC(proxy_manager, eldbus_proxy_unref);
   eldbus_object_unref(obj);
   obj = eldbus_proxy_object_get(proxy_technology);
   E_FREE_FUNC(proxy_technology, eldbus_proxy_unref);
   eldbus_object_unref(obj);
}

static void
_connman_name_owner_changed(void *data EINA_UNUSED, const char *bus EINA_UNUSED, const char *from EINA_UNUSED, const char *to)
{
   if (to[0])
     _connman_start();
   else
     _connman_end();
}

EINTERN void
connman_init(void)
{
   if (_connman_log_dom > -1) return;
   eldbus_name_owner_changed_callback_add(dbus_conn, CONNMAN_BUS_NAME,
                                         _connman_name_owner_changed,
                                         NULL, EINA_TRUE);
   _connman_log_dom = eina_log_domain_register("wireless.connman", EINA_COLOR_ORANGE);
}

EINTERN void
connman_shutdown(void)
{
   E_FREE_FUNC(agent_iface, eldbus_service_object_unregister);
   _connman_end();
   eldbus_name_owner_changed_callback_del(dbus_conn, CONNMAN_BUS_NAME, _connman_name_owner_changed, NULL);
   eina_log_domain_unregister(_connman_log_dom);
   _connman_log_dom = -1;
}
