#ifndef CLOCK_H
#define CLOCK_H

#include "e.h"
#include "gadget.h"

#define _(X) (X)
#define N_(X) (X)

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init     (E_Module *m);
E_API int   e_modapi_shutdown (E_Module *m);
E_API int   e_modapi_save     (E_Module *m);

typedef struct _Config Config;
typedef struct _Config_Item Config_Item;
typedef struct _Instance Instance;

struct _Config
{
  Eina_List *items;

  E_Module *module;
  E_Config_Dialog *config_dialog;
};

struct _Config_Item
{
  int id;
  struct {
      int start, len; // 0->6 0 == sun, 6 == sat, number of days
   } weekend;
   struct {
      int start; // 0->6 0 == sun, 6 == sat
   } week;
   int digital_clock;
   int digital_24h;
   int show_seconds;
   int show_date;
   Eina_Bool changed;
};


struct _Instance
{
   Evas_Object     *o_clock, *o_table, *o_popclock, *o_cal;
   Evas_Object  *popup;

   int              madj;

   char             year[8];
   char             month[64];
   const char      *daynames[7];
   unsigned char    daynums[7][6];
   Eina_Bool        dayweekends[7][6];
   Eina_Bool        dayvalids[7][6];
   Eina_Bool        daytoday[7][6];
   Config_Item     *cfg;
};

EINTERN Evas_Object *config_clock(Evas_Object *g);
void e_int_clock_instances_redo(Eina_Bool all);

EINTERN void time_daynames_clear(Instance *inst);
EINTERN void time_string_format(Instance *inst, char *buf, int bufsz);
EINTERN void time_instance_update(Instance *inst);
EINTERN void time_init(void);
EINTERN void time_shutdown(void);

EINTERN Evas_Object *clock_create(Evas_Object *parent, int *id, Z_Gadget_Site_Orient orient);
EINTERN void clock_popup_new(Instance *inst);

extern Config *clock_config;
extern Eina_List *clock_instances;

#endif
