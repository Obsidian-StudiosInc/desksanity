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

typedef struct Mod
{
   E_Config_Dialog *cfd;
   E_Module *module;
} Mod;

typedef struct Config
{
   unsigned int config_version;
} Config;

extern Mod *mod;
extern Config *ds_config;

EINTERN void ds_init(void);
EINTERN void ds_shutdown(void);

EINTERN void mr_shutdown(void);
EINTERN void mr_init(void);

#endif
