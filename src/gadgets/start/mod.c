#include "e.h"

/* module setup */
E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Start"
};

E_API void *
e_modapi_init(E_Module *m)
{
   start_module = m;
   e_gadcon_provider_register(&_gadcon_class);
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   start_module = NULL;
   e_gadcon_provider_unregister(&_gadcon_class);
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   return 1;
}
