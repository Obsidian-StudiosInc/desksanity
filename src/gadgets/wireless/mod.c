#include "e.h"

EINTERN void wireless_gadget_init(void);
EINTERN void wireless_gadget_shutdown(void);

EINTERN void connman_init(void);
EINTERN void connman_shutdown(void);

EINTERN Eldbus_Connection *dbus_conn;

EINTERN void
wireless_init(void)
{
   dbus_conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
   connman_init();
   wireless_gadget_init();
}

EINTERN void
wireless_shutdown(void)
{
   wireless_gadget_shutdown();
   connman_shutdown();
   E_FREE_FUNC(dbus_conn, eldbus_connection_unref);
}
