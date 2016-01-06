#include "gadget.h"

EINTERN Evas_Object *start_create(Evas_Object *parent, int *id EINA_UNUSED, Z_Gadget_Site_Orient orient);;

EINTERN void clock_init(void);
EINTERN void clock_shutdown(void);
EINTERN void ibar_init(void);

static Evas_Object *shelf;
static Evas_Object *site;

static void
_demo_style(Evas_Object *g, Eina_Stringshare *name)
{
   Evas_Object *ly;
   static int n;

   ly = elm_layout_add(site);
   if (n++ % 2)
     e_theme_edje_object_set(ly, NULL, "e/shelf/default/inset");
   else
     e_theme_edje_object_set(ly, NULL, "e/shelf/default/plain");
   z_gadget_util_layout_style_init(g, ly);
   elm_object_part_content_set(ly, "e.swallow.content", g);
}

EINTERN void
gadget_demo(void)
{
   Evas_Object *ly;

   if (!eina_streq(getenv("USER"), "zmike")) return;

   if (e_comp->w > 1200) return;
   z_gadget_type_add("Start", start_create);
   clock_init();
   ibar_init();

   ly = elm_layout_add(e_comp->elm);
   e_theme_edje_object_set(ly, NULL, "e/shelf/default/base");

   site = z_gadget_site_add(ly, Z_GADGET_SITE_ORIENT_HORIZONTAL);
   z_gadget_site_owner_set(site, Z_GADGET_SITE_ANCHOR_TOP, _demo_style);
   elm_object_part_content_set(ly, "e.swallow.content", site);
   elm_layout_signal_emit(ly, "e,state,orientation,top", "e");
   evas_object_geometry_set(ly, 0, 0, e_comp->w, 48);
   //evas_object_geometry_set(ly, 0, 0, 48, e_comp->h);
   evas_object_show(ly);
   shelf = e_comp_object_util_add(ly, E_COMP_OBJECT_TYPE_NONE);
   evas_object_data_set(shelf, "comp_skip", (void*)1);
   evas_object_layer_set(shelf, E_LAYER_POPUP);
   evas_object_lower(shelf);

   evas_object_clip_set(shelf, e_comp_zone_xy_get(0, 0)->bg_clip_object);

   z_gadget_site_gadget_add(site, "Start");
   z_gadget_site_gadget_add(site, "Clock");
   z_gadget_site_gadget_add(site, "IBar");
   z_gadget_site_gadget_add(site, "Clock");
   z_gadget_site_gadget_add(site, "IBar");
}
