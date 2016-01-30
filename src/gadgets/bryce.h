#ifndef BRYCE_H
# define BRYCE_H

#include "e.h"

Z_API void z_bryce_init(void);
Z_API void z_bryce_shutdown(void);

Z_API Evas_Object *z_bryce_add(Evas_Object *parent, const char *name, Z_Gadget_Site_Orient orient, Z_Gadget_Site_Anchor an);
Z_API void z_bryce_orient(Evas_Object *bryce, Z_Gadget_Site_Orient orient, Z_Gadget_Site_Anchor an);
Z_API Evas_Object *z_bryce_site_get(Evas_Object *bryce);
Z_API void z_bryce_autosize_set(Evas_Object *bryce, Eina_Bool set);
Z_API void z_bryce_autohide_set(Evas_Object *bryce, Eina_Bool set);
Z_API Eina_Bool z_bryce_exists(Evas_Object *parent, Z_Gadget_Site_Orient orient, Z_Gadget_Site_Anchor an);
Z_API Eina_List *z_bryce_list(Evas_Object *parent);
Z_API void z_bryce_style_set(Evas_Object *bryce, const char *style);

Z_API Evas_Object *z_bryce_editor_add(Evas_Object *parent);

#endif
