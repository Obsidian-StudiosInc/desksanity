#ifndef E_WIRELESS_H
# define E_WIRELESS_H

#include "e.h"
#include "gadget.h"

typedef enum Wifi_State
{
  WIFI_STATE_NONE,
  WIFI_STATE_ETHERNET,
  WIFI_STATE_WIFI,
} Wifi_State;

typedef enum
{
   WIFI_NETWORK_STATE_NONE,
   WIFI_NETWORK_STATE_CONFIGURING,
   WIFI_NETWORK_STATE_CONNECTED,
   WIFI_NETWORK_STATE_ONLINE,
   WIFI_NETWORK_STATE_FAILURE,
} Wifi_Network_State;

typedef struct Wifi_Network
{
   Eina_Stringshare *name;
   Eina_Array *security;
   Wifi_Network_State state;
   uint8_t strength;
} Wifi_Network;

extern Eldbus_Connection *dbus_conn;

EINTERN void wireless_wifi_state_set(Wifi_State state);
EINTERN void wireless_wifi_current_network_set(Wifi_Network *wn);
EINTERN Eina_Array *wireless_wifi_networks_set(Eina_Array *networks);
EINTERN void wireless_airplane_mode_set(Eina_Bool enabled);

#endif
