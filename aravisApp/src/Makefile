TOP=../..

include $(TOP)/configure/CONFIG

LIBRARY_IOC_Linux += ADAravis

# The following are compiled and added to the support library
ADAravis_SRCS += arvFeature.cpp
ADAravis_SRCS += ADAravis.cpp

DBD += ADAravisSupport.dbd

USR_INCLUDES +=  $(addprefix -I, $(GLIB_INCLUDE))

ifdef ARAVIS_INCLUDE
  USR_INCLUDES += $(addprefix -I, $(ARAVIS_INCLUDE))
endif

LIB_LIBS += ADGenICam

ifdef ARAVIS_LIB
  aravis-0.8_DIR = $(ARAVIS_LIB)
  LIB_LIBS     += aravis-0.8
else
  LIB_SYS_LIBS += aravis-0.8
endif

LIB_SYS_LIBS += gio-2.0 gobject-2.0 gthread-2.0 glib-2.0
LIB_SYS_LIBS += usb-1.0

include $(TOP)/configure/RULES
