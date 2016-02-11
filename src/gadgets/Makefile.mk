AM_CPPFLAGS += \
-Isrc/gadgets

module_la_SOURCES += \
src/gadgets/bryce.c \
src/gadgets/bryce_config.c \
src/gadgets/bryce.h \
src/gadgets/core.c \
src/gadgets/site_config.c \
src/gadgets/demo.c \
src/gadgets/gadget.h \
src/gadgets/start/start.c \
src/gadgets/clock/clock.c \
src/gadgets/clock/config.c \
src/gadgets/clock/clock.h \
src/gadgets/clock/mod.c \
src/gadgets/clock/time.c \
src/gadgets/ibar/ibar.c \
src/gadgets/ibar/ibar.h \
src/gadgets/wireless/connman.c \
src/gadgets/wireless/mod.c \
src/gadgets/wireless/wireless.c \
src/gadgets/wireless/wireless.h
