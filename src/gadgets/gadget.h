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
   Z_GADGET_SITE_GRAVITY_CENTER,
} Z_Gadget_Site_Gravity;

typedef enum
{
   Z_GADGET_SITE_ORIENT_NONE = 0,
   Z_GADGET_SITE_ORIENT_HORIZONTAL,
   Z_GADGET_SITE_ORIENT_VERTICAL,
} Z_Gadget_Site_Orient;

typedef enum
{
   Z_GADGET_SITE_ANCHOR_NONE = 0,
   Z_GADGET_SITE_ANCHOR_LEFT = (1 << 0),
   Z_GADGET_SITE_ANCHOR_RIGHT = (1 << 1),
   Z_GADGET_SITE_ANCHOR_TOP = (1 << 2),
   Z_GADGET_SITE_ANCHOR_BOTTOM = (1 << 3),
} Z_Gadget_Site_Anchor;

typedef Evas_Object *(*Z_Gadget_Create_Cb)(Evas_Object *parent, unsigned int *id, Z_Gadget_Site_Orient orient);
typedef Evas_Object *(*Z_Gadget_Configure_Cb)(Evas_Object *gadget);

Z_API Evas_Object *z_gadget_site_add(Evas_Object *parent, Z_Gadget_Site_Orient orient);
Z_API Z_Gadget_Site_Anchor z_gadget_site_anchor_get(Evas_Object *obj);
Z_API void z_gadget_site_anchor_set(Evas_Object *obj, Z_Gadget_Site_Anchor an);
Z_API Z_Gadget_Site_Orient z_gadget_site_orient_get(Evas_Object *obj);
Z_API Z_Gadget_Site_Gravity z_gadget_site_gravity_get(Evas_Object *obj);
Z_API void z_gadget_site_gadget_add(Evas_Object *obj, const char *type);


Z_API void z_gadget_configure_cb_set(Evas_Object *g, Z_Gadget_Configure_Cb cb);
Z_API void z_gadget_configure(Evas_Object *g);
Z_API Evas_Object *z_gadget_site_get(Evas_Object *g);


Z_API void z_gadget_type_add(const char *type, Z_Gadget_Create_Cb callback);
Z_API void z_gadget_type_del(const char *type);

#endif
