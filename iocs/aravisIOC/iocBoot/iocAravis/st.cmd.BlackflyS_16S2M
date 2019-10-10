< envPaths
errlogInit(20000)

dbLoadDatabase("$(TOP)/dbd/ADAravisApp.dbd")
ADAravisApp_registerRecordDeviceDriver(pdbbase) 

# Name of camera as reported by arv-tool
epicsEnvSet("CAMERA_NAME", "FLIR-Blackfly S BFS-PGE-16S2M-19069593")

# The maximum image width; used for row profiles in the NDPluginStats plugin
epicsEnvSet("XSIZE",  "1440")

# The maximum image height; used for column profiles in the NDPluginStats plugin
epicsEnvSet("YSIZE",  "1080")

# Define NELEMENTS to be enough for a 1440x1080 mono image
epicsEnvSet("NELEMENTS", "1555200")

# Enable register caching
epicsEnvSet("ENABLE_CACHING", "1")

# The database file
epicsEnvSet("GENICAM_DB_FILE", "$(ADGENICAM)/db/PGR_BlackflyS_16S2M.template")

< st.cmd.base

