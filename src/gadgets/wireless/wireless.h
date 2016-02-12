#ifndef E_WIRELESS_H
# define E_WIRELESS_H

#include "e.h"
#include "gadget.h"

typedef enum
{
   WIRELESS_SERVICE_TYPE_NONE = -1,
   WIRELESS_SERVICE_TYPE_ETHERNET,
   WIRELESS_SERVICE_TYPE_WIFI,
   WIRELESS_SERVICE_TYPE_BLUETOOTH,
   WIRELESS_SERVICE_TYPE_CELLULAR,
} Wireless_Service_Type;

typedef enum
{
   WIRELESS_NETWORK_STATE_NONE,
   WIRELESS_NETWORK_STATE_CONFIGURING,
   WIRELESS_NETWORK_STATE_CONNECTED,
   WIRELESS_NETWORK_STATE_ONLINE,
   WIRELESS_NETWORK_STATE_FAILURE,
} Wireless_Network_State;

typedef enum
{
   WIRELESS_NETWORK_SECURITY_NONE = 0,
   WIRELESS_NETWORK_SECURITY_WEP = (1 << 0),
   WIRELESS_NETWORK_SECURITY_PSK = (1 << 1),
   WIRELESS_NETWORK_SECURITY_IEEE8021X = (1 << 2),
   WIRELESS_NETWORK_SECURITY_WPS = (1 << 3),
} Wireless_Network_Security;

typedef enum
{
   WIRELESS_NETWORK_IPV4_METHOD_OFF,
   WIRELESS_NETWORK_IPV4_METHOD_MANUAL,
   WIRELESS_NETWORK_IPV4_METHOD_DHCP,
} Wireless_Network_IPv4_Method;

typedef enum
{
   WIRELESS_NETWORK_IPV6_METHOD_OFF,
   WIRELESS_NETWORK_IPV6_METHOD_MANUAL,
   WIRELESS_NETWORK_IPV6_METHOD_AUTO,
   WIRELESS_NETWORK_IPV6_METHOD_6TO4,
} Wireless_Network_IPv6_Method;

typedef enum
{
   WIRELESS_NETWORK_IPV6_PRIVACY_DISABLED,
   WIRELESS_NETWORK_IPV6_PRIVACY_ENABLED,
   WIRELESS_NETWORK_IPV6_PRIVACY_PREFERRED,
} Wireless_Network_IPv6_Privacy;

typedef struct Wireless_Connection
{
   Eina_Stringshare *name;
   Wireless_Network_Security security;
   Wireless_Network_State state;
   Wireless_Service_Type type;
   uint8_t strength;
   unsigned int method;
   Eina_Stringshare *address;
   Eina_Stringshare *gateway;
   union
   {
      struct
      {
         Eina_Stringshare *netmask;
      } v4;
      struct
      {
         Eina_Stringshare *prefixlength;
         Wireless_Network_IPv6_Privacy privacy;
      } v6;
   } ip;
   Eina_Bool ipv6 : 1;
} Wireless_Connection;

extern Eldbus_Connection *dbus_conn;

EINTERN void wireless_wifi_network_state_set(Wireless_Network_State state);
EINTERN void wireless_wifi_current_network_set(Wireless_Connection *wn);
EINTERN Eina_Array *wireless_wifi_networks_set(Eina_Array *networks);
EINTERN void wireless_airplane_mode_set(Eina_Bool enabled);

#endif
