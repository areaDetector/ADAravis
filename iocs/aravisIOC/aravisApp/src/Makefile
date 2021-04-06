TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

PROD_NAME = ADAravisApp
PROD_IOC_Linux  += $(PROD_NAME)
PROD_IOC_Darwin += $(PROD_NAME)

# <name>.dbd will be created from <name>Include.dbd
DBD += $(PROD_NAME).dbd
$(PROD_NAME)_DBD += ADAravisSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
PROD_SRCS += $(PROD_NAME)_registerRecordDeviceDriver.cpp $(PROD_NAME)Main.cpp

# Add locally compiled object code
PROD_LIBS += ADAravis
PROD_LIBS += ADGenICam
ifdef ARAVIS_LIB
  aravis-0.8_DIR = $(ARAVIS_LIB)
  PROD_LIBS     += aravis-0.8
else
  PROD_SYS_LIBS += aravis-0.8
endif

PROD_SYS_LIBS += gio-2.0 gobject-2.0 gthread-2.0 glib-2.0
PROD_SYS_LIBS += usb-1.0

include $(ADCORE)/ADApp/commonDriverMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

