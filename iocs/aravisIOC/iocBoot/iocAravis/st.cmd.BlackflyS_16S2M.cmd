< envPaths
errlogInit(20000)

dbLoadDatabase("$(TOP)/dbd/ADAravisApp.dbd")
ADAravisApp_registerRecordDeviceDriver(pdbbase) 

# Name of camera as reported by arv-tool-0.6
epicsEnvSet("CAMERA_NAME", "FLIR-Blackfly S BFS-PGE-70S7M-19069593")
epicsEnvSet("GENICAM_DB_FILE", "$(ADGENICAM)/db/PGR_BlackflyS_16S2M.template")

< st.cmd.base

