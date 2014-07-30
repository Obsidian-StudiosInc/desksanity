#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <e.h>
#include <Efx.h>

#ifdef ENABLE_NLS
# include <libintl.h>
# define D_(str) dgettext(PACKAGE, str)
# define DP_(str, str_p, n) dngettext(PACKAGE, str, str_p, n)
#else
# define bindtextdomain(domain,dir)
# define bind_textdomain_codeset(domain,codeset)
# define D_(str) (str)
# define DP_(str, str_p, n) (str_p)
#endif

#define N_(str) (str)

#define MOD_CONFIG_FILE_EPOCH 0
#define MOD_CONFIG_FILE_GENERATION 1
#define MOD_CONFIG_FILE_VERSION ((MOD_CONFIG_FILE_EPOCH * 1000000) + MOD_CONFIG_FILE_GENERATION)

typedef enum
{
   DS_PAN, //slide desk in direction of flip
   DS_FADE_OUT, //current desk fades to transparent
   DS_FADE_IN, //new desk fades in from transparent
   DS_BATMAN, //adam west is calling
   DS_ZOOM_IN, //zoom in to new desk
   DS_ZOOM_OUT, //zoom out from old desk
   DS_GROW, //grow the view of the new desk based on flip direction
   DS_ROTATE_OUT, //spiral current desk out while shrinking
   DS_ROTATE_IN, //spiral new desk in while growing
   DS_SLIDE_SPLIT, //split screen in X parts and slide away based on flip direction
   DS_QUAD_SPLIT, //split screen into quads and move towards corners
   DS_QUAD_MERGE, //split screen into quads and move towards center
   DS_BLINK, //like blinking your eye
   DS_VIEWPORT, //current desk viewport shrinks towards 1x1 at center
   DS_LAST,
} DS_Type;

typedef struct Mod
{
   E_Config_Dialog *cfd;
   E_Module *module;
   Eina_Stringshare *edje_file;
} Mod;

typedef struct Config_Types
{
   Eina_Bool disable_PAN;
   Eina_Bool disable_FADE_OUT;
   Eina_Bool disable_FADE_IN;
   Eina_Bool disable_BATMAN;
   Eina_Bool disable_ZOOM_IN;
   Eina_Bool disable_ZOOM_OUT;
   Eina_Bool disable_GROW;
   Eina_Bool disable_ROTATE_OUT;
   Eina_Bool disable_ROTATE_IN;
   Eina_Bool disable_SLIDE_SPLIT;
   Eina_Bool disable_QUAD_SPLIT;
   Eina_Bool disable_QUAD_MERGE;
   Eina_Bool disable_BLINK;
   Eina_Bool disable_VIEWPORT;
} Config_Types;

typedef struct Config
{
   unsigned int config_version;
   Eina_Bool disable_ruler;
   Eina_Bool disable_maximize;
   Eina_Bool disable_transitions;
   unsigned int disabled_transition_count;
   Config_Types types;
} Config;


extern Mod *mod;
extern Config *ds_config;

EINTERN void ds_init(void);
EINTERN void ds_shutdown(void);

EINTERN void mr_shutdown(void);
EINTERN void mr_init(void);

EINTERN void maximize_init(void);
EINTERN void maximize_shutdown(void);


EINTERN void ds_config_init(void);
EINTERN void ds_config_shutdown(void);
#endif
