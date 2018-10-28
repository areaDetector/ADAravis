TOP=../..

include $(TOP)/configure/CONFIG

LIBRARY_IOC += ADAravis

# The following are compiled and added to the support library
ADAravis_SRCS += arvFeature.cpp
ADAravis_SRCS += ADAravis.cpp

DBD += ADAravisSupport.dbd

USR_INCLUDES +=  $(addprefix -I, $(GLIB_INCLUDE))

# We need to link against aravis
ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))
	USR_INCLUDES_Linux += -I$(TOP)/vendor/aravis-linux-x86_64/include/aravis-0.6
else ifeq (linux-x86, $(findstring linux-x86, $(T_A)))
	USR_INCLUDES_Linux += -I$(TOP)/vendor/aravis-linux-x86/include/aravis-0.6
endif

LIB_LIBS += ADGenICam

LIB_LIBS += glib-2.0

# TODO: should pick this up from the vendor directory
LIB_LIBS += aravis-0.6

include $(TOP)/configure/RULES
