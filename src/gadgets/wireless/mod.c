#include "e.h"

EINTERN Eldbus_Connection *dbus_conn;

EINTERN void
wireless_init(void)
{
   dbus_conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
   connman_init();
}

EINTERN void
wireless_shutdown(void)
{
   connman_shutdown();
}
