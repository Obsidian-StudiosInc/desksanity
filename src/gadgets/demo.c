#include "gadget.h"
#include "bryce.h"

EINTERN Evas_Object *start_create(Evas_Object *parent, int *id EINA_UNUSED, Z_Gadget_Site_Orient orient);;

EINTERN void clock_init(void);
EINTERN void clock_shutdown(void);
EINTERN void ibar_init(void);

EINTERN void
gadget_demo(void)
{
   Evas_Object *b, *site;

   if (!eina_streq(getenv("USER"), "zmike")) return;

   if (e_comp->w > 1200) return;
   z_gadget_type_add("Start", start_create);
   clock_init();
   ibar_init();

   b = z_bryce_add(e_comp->elm);
   site = z_bryce_site_get(b);

   z_gadget_site_gadget_add(site, "Start", 0);
   z_gadget_site_gadget_add(site, "Clock", 0);
   z_gadget_site_gadget_add(site, "IBar", 0);
   z_bryce_autosize_set(b, 1);
   z_bryce_autohide_set(b, 1);
}
