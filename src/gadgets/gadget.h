#ifndef Z_GADGET_H
# define Z_GADGET_H

#include "e.h"
#define Z_API __attribute__ ((visibility("default")))

#define Z_GADGET_TYPE 0xE31337

typedef enum
{
   Z_GADGET_SITE_GRAVITY_NONE = 0,
   Z_GADGET_SITE_GRAVITY_LEFT,
   Z_GADGET_SITE_GRAVITY_RIGHT,
   Z_GADGET_SITE_GRAVITY_TOP,
   Z_GADGET_SITE_GRAVITY_BOTTOM,
} Z_Gadget_Site_Gravity;

typedef Evas_Object *(*Z_Gadget_Create_Cb)(Evas_Object *parent, unsigned int *id);

Z_API Evas_Object *z_gadget_site_add(Evas_Object *parent, Z_Gadget_Site_Gravity gravity);
Z_API Z_Gadget_Site_Gravity z_gadget_site_gravity_get(Evas_Object *obj);
Z_API void z_gadget_site_gadget_add(Evas_Object *obj, const char *type);
Z_API Evas_Object *z_gadget_site_get(Evas_Object *g);
Z_API void z_gadget_type_add(const char *type, Z_Gadget_Create_Cb callback);
Z_API void z_gadget_type_del(const char *type);

#endif
