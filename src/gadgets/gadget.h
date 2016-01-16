#ifndef Z_GADGET_H
# define Z_GADGET_H

#include "e.h"
#define Z_API __attribute__ ((visibility("default")))

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

typedef Evas_Object *(*Z_Gadget_Create_Cb)(Evas_Object *parent, int *id, Z_Gadget_Site_Orient orient);
typedef Evas_Object *(*Z_Gadget_Configure_Cb)(Evas_Object *gadget);
typedef void (*Z_Gadget_Style_Cb)(Evas_Object *owner, Eina_Stringshare *name, Evas_Object *g);

Z_API void z_gadget_init(void);
Z_API void z_gadget_shutdown(void);

Z_API Evas_Object *z_gadget_site_add(Z_Gadget_Site_Orient orient, const char *name);
Z_API Evas_Object *z_gadget_site_auto_add(Z_Gadget_Site_Orient orient, const char *name);
Z_API Z_Gadget_Site_Anchor z_gadget_site_anchor_get(Evas_Object *obj);
Z_API void z_gadget_site_owner_setup(Evas_Object *obj, Z_Gadget_Site_Anchor an, Z_Gadget_Style_Cb cb);
Z_API Z_Gadget_Site_Orient z_gadget_site_orient_get(Evas_Object *obj);
Z_API Z_Gadget_Site_Gravity z_gadget_site_gravity_get(Evas_Object *obj);
Z_API void z_gadget_site_gravity_set(Evas_Object *obj, Z_Gadget_Site_Gravity gravity);
Z_API void z_gadget_site_gadget_add(Evas_Object *obj, const char *type, Eina_Bool demo);
Z_API Eina_List *z_gadget_site_gadgets_list(Evas_Object *obj);


Z_API void z_gadget_configure_cb_set(Evas_Object *g, Z_Gadget_Configure_Cb cb);
Z_API void z_gadget_configure(Evas_Object *g);
Z_API Eina_Bool z_gadget_has_wizard(Evas_Object *g);
Z_API Evas_Object *z_gadget_site_get(Evas_Object *g);
Z_API Eina_Stringshare *z_gadget_type_get(Evas_Object *g);

Z_API void z_gadget_type_add(const char *type, Z_Gadget_Create_Cb callback);
Z_API void z_gadget_type_del(const char *type);
Z_API Eina_Iterator *z_gadget_type_iterator_get(void);

Z_API Evas_Object *z_gadget_util_layout_style_init(Evas_Object *g, Evas_Object *style);


Z_API Evas_Object *z_gadget_editor_add(Evas_Object *parent, Evas_Object *site);
#endif
