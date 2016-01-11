#ifndef BRYCE_H
# define BRYCE_H

Z_API Evas_Object *z_bryce_add(Evas_Object *parent);
Z_API Evas_Object *z_bryce_site_get(Evas_Object *bryce);
Z_API void z_bryce_autosize_set(Evas_Object *bryce, Eina_Bool set);
Z_API void z_bryce_autohide_set(Evas_Object *bryce, Eina_Bool set);

#endif
