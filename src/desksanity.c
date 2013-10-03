#include "e_mod_main.h"

static void
_ds_flip(void *data, E_Desk *desk, Eina_Bool show)
{

}

EINTERN void
ds_init(void)
{
   e_desk_flip_cb_set(_ds_flip, NULL);
}

EINTERN void
ds_shutdown(void)
{
   e_desk_flip_cb_set(NULL, NULL);
}
