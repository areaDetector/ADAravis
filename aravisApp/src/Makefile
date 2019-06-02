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

LIB_LIBS += glib-2.0

include $(TOP)/configure/RULES
