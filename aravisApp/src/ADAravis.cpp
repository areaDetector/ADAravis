/* ADAravis.cpp
 *
 * This is a driver for a GeniCam cameras using the aravis SDK. 
 * It is based on the aravisGigE driver by Tom Cobb.
 *
 * Author: Mark Rivers
 *         University of Chicago
 *
 */

/* System includes */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* EPICS includes */
#include <iocsh.h>
#include <alarm.h>
#include <epicsExit.h>
#include <epicsEndian.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <initHooks.h>

/* ADGenICam includes */
#include <ADGenICam.h>

/* aravis includes */
extern "C" {
    #include <arv.h>
}

#include <epicsExport.h>
#include <arvFeature.h>

#define DRIVER_VERSION "2.2.1"
// aravis does not define the Mono12p format yet.
#define ARV_PIXEL_FORMAT_MONO_12_P         ((ArvPixelFormat) 0x010c0047u)

/* number of raw buffers in our queue */
#define NRAW 20

/* driver name for asyn trace prints */
static const char *driverName = "ADAravis";

/* flag to say IOC is running */
static int iocRunning = 0;

/* lookup for binning mode strings */
struct bin_lookup {
    const char * mode;
    int binx, biny;
};
static const struct bin_lookup bin_lookup[] = {
    { "Binning1x1", 1, 1 },
    { "Binning1x2", 1, 2 },
    { "Binning1x4", 1, 4 },
    { "Binning2x1", 2, 1 },
    { "Binning2x2", 2, 2 },
    { "Binning2x4", 2, 4 },
    { "Binning4x1", 4, 1 },
    { "Binning4x2", 4, 2 },
    { "Binning4x4", 4, 4 }
};

/* lookup for pixel format types */
struct pix_lookup {
    ArvPixelFormat fmt;
    int colorMode, dataType, bayerFormat;
};

typedef enum {
    AravisConvertPixelFormatMono16Low,
    AravisConvertPixelFormatMono16High
} AravisConvertPixelFormat_t;

typedef enum {
    AravisShiftNone,
    AravisShiftLeft,
    AravisShiftRight
} AravisShift_t;

static const struct pix_lookup pix_lookup[] = {
    { ARV_PIXEL_FORMAT_MONO_8,        NDColorModeMono,  NDUInt8,  0           },
    { ARV_PIXEL_FORMAT_RGB_8_PACKED,  NDColorModeRGB1,  NDUInt8,  0           },
    { ARV_PIXEL_FORMAT_BAYER_GR_8,    NDColorModeBayer, NDUInt8,  NDBayerGRBG },
    { ARV_PIXEL_FORMAT_BAYER_RG_8,    NDColorModeBayer, NDUInt8,  NDBayerRGGB },
    { ARV_PIXEL_FORMAT_BAYER_GB_8,    NDColorModeBayer, NDUInt8,  NDBayerGBRG },
    { ARV_PIXEL_FORMAT_BAYER_BG_8,    NDColorModeBayer, NDUInt8,  NDBayerBGGR },
// For Int16, use Mono16 if available, otherwise Mono12
    { ARV_PIXEL_FORMAT_MONO_16,       NDColorModeMono,  NDUInt16, 0           },
// this doesn't work on Manta camers    { ARV_PIXEL_FORMAT_MONO_14,       NDColorModeMono,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_MONO_12,       NDColorModeMono,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_MONO_12_P,     NDColorModeMono,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_MONO_12_PACKED,NDColorModeMono,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_MONO_10,       NDColorModeMono,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_RGB_12_PACKED, NDColorModeRGB1,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_RGB_10_PACKED, NDColorModeRGB1,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_BAYER_GR_12,   NDColorModeBayer, NDUInt16, NDBayerGRBG },
    { ARV_PIXEL_FORMAT_BAYER_RG_12,   NDColorModeBayer, NDUInt16, NDBayerRGGB },
    { ARV_PIXEL_FORMAT_BAYER_GB_12,   NDColorModeBayer, NDUInt16, NDBayerGBRG },
    { ARV_PIXEL_FORMAT_BAYER_BG_12,   NDColorModeBayer, NDUInt16, NDBayerBGGR }
};

// Helper to ensure that GError is free'd
struct GErrorHelper {
    GError *err;
    GErrorHelper() :err(0) {}
    ~GErrorHelper() {
        if(err) g_error_free(err);
    }
    GError** get() {
        return &err;
    }
    operator GError*() const {
        return err;
    }
    GError* operator->() const {
        return err;
    }
};

/* Convert ArvBufferStatus enum to string */
const char * ArvBufferStatusToString( ArvBufferStatus buffer_status )
{
    const char  *   pString;
    switch( buffer_status )
    {
        default:
        case ARV_BUFFER_STATUS_UNKNOWN:         pString = "Unknown";        break;
        case ARV_BUFFER_STATUS_SUCCESS:         pString = "Success";        break;
        case ARV_BUFFER_STATUS_CLEARED:         pString = "Buffer Cleared"; break;
        case ARV_BUFFER_STATUS_TIMEOUT:         pString = "Timeout";        break;
        case ARV_BUFFER_STATUS_MISSING_PACKETS: pString = "Missing Pkts";   break;
        case ARV_BUFFER_STATUS_WRONG_PACKET_ID: pString = "Wrong Pkt ID";   break;
        case ARV_BUFFER_STATUS_SIZE_MISMATCH:   pString = "Image>bufSize";  break;
        case ARV_BUFFER_STATUS_FILLING:         pString = "Filling";        break;
        case ARV_BUFFER_STATUS_ABORTED:         pString = "Aborted";        break;
    }
    return pString;
}

/** Aravis GigE detector driver */
class ADAravis : public ADGenICam, epicsThreadRunable {
public:
    /* Constructor */
    ADAravis(const char *portName, const char *cameraName, int enableCaching,
                size_t maxMemory, int priority, int stackSize);

    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual GenICamFeature *createFeature(GenICamFeatureSet *set, 
                                          std::string const & asynName, asynParamType asynType, int asynIndex,
                                          std::string const & featureName, GCFeatureType_t featureType);
    virtual asynStatus startCapture();
    virtual asynStatus stopCapture();
    void report(FILE *fp, int details);

    /* This is the method we override from epicsThreadRunable */
    void run();

    /* These should be private, but are used in the aravis callback so must be public */
    epicsMessageQueueId msgQId;
    void newBufferCallback(ArvStream *stream);

    /** Used by epicsAtExit */
    ArvCamera *camera;

    /** Used by connection lost callback */
    int connectionValid;

protected:
    int AravisCompleted;
    #define FIRST_ARAVIS_CAMERA_PARAM AravisCompleted
    int AravisFailures;
    int AravisUnderruns;
    int AravisFrameRetention;
    int AravisMissingPkts;
    int AravisPktResend;
    int AravisPktTimeout;
    int AravisResentPkts;
    int AravisConvertPixelFormat;
    int AravisShiftDir;
    int AravisShiftBits;
    int AravisConnection;
    int AravisReset;
    #define LAST_ARAVIS_CAMERA_PARAM AravisReset

private:
    asynStatus allocBuffer();
    asynStatus processBuffer(ArvBuffer *buffer);
    asynStatus lookupColorMode(ArvPixelFormat fmt, int *colorMode, int *dataType, int *bayerFormat);
    asynStatus lookupPixelFormat(int colorMode, int dataType, int bayerFormat, ArvPixelFormat *fmt);
    asynStatus connectToCamera();
    asynStatus makeCameraObject();
    asynStatus makeStreamObject();

    ArvStream *stream;
    ArvDevice *device;
    ArvGc *genicam;
    char *cameraName;
    unsigned int featureIndex;
    int payload;
    int mEnableCaching;
    epicsThread pollingLoop;
};

GenICamFeature *ADAravis::createFeature(GenICamFeatureSet *set, 
                                        std::string const & asynName, asynParamType asynType, int asynIndex,
                                        std::string const & featureName, GCFeatureType_t featureType) {
    return new arvFeature(set, asynName, asynType, asynIndex, featureName, featureType, this->device);
}

/** Called by epicsAtExit to shutdown camera */
static void aravisShutdown(void* arg) {
    ADAravis *pPvt = (ADAravis *) arg;
    GErrorHelper err;
    ArvCamera *cam = pPvt->camera;
    printf("ADAravis: Stopping %s... ", pPvt->portName);
    arv_camera_stop_acquisition(cam, err.get());
    pPvt->connectionValid = 0;
    epicsThreadSleep(0.1);
    pPvt->camera = NULL;
    g_object_unref(cam);
    printf("ADAravis: OK\n");
}

/** Called by aravis when destroying a buffer with an NDArray wrapper */
static void destroyBuffer(gpointer data){
    NDArray *pRaw;
    if (data != NULL) {
        pRaw = (NDArray *) data;
        pRaw->release();
    }
}

/** Called by aravis when a new buffer is produced */
static void newBufferCallbackC(ArvStream *stream, ADAravis *pPvt) {
    pPvt->newBufferCallback(stream);
}

void ADAravis::newBufferCallback(ArvStream *stream) {
    ArvBuffer *buffer;
    int status;
    static int nConsecutiveBadFrames = 0;
    static const char *functionName = "newBufferCallback";

    buffer = arv_stream_try_pop_buffer(stream);
    if (buffer == NULL)    return;
    ArvBufferStatus buffer_status = arv_buffer_get_status(buffer);
    if (buffer_status == ARV_BUFFER_STATUS_SUCCESS /*|| buffer->status == ARV_BUFFER_STATUS_MISSING_PACKETS*/) {
        nConsecutiveBadFrames = 0;
        status = epicsMessageQueueTrySend(this->msgQId,
                &buffer,
                sizeof(&buffer));
        if (status) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s message queue full, dropped buffer\n", driverName, functionName);
            arv_stream_push_buffer (stream, buffer);
        }
    } else {
        arv_stream_push_buffer (stream, buffer);

        nConsecutiveBadFrames++;
        if ( nConsecutiveBadFrames < 10 )
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s bad frame status: %s\n", 
                driverName, functionName, ArvBufferStatusToString(buffer_status) );
        else if ( ((nConsecutiveBadFrames-10) % 1000) == 0 ) {
            static int  nBadFramesPrior = 0;
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s dad frame status: %s, %d msgs suppressed.\n", 
                driverName, functionName, ArvBufferStatusToString(buffer_status), (nConsecutiveBadFrames - nBadFramesPrior));
            nBadFramesPrior = nConsecutiveBadFrames;
        }
    }
}

/** Called by aravis when control signal is lost */
static void controlLostCallback(ArvDevice *device, ADAravis *pPvt) {
    pPvt->connectionValid = 0;
}

/** Init hook that sets iocRunning flag */
static void setIocRunningFlag(initHookState state) {
    switch(state) {
        case initHookAfterIocRunning:
            iocRunning = 1;
            break;
        default:
            break;
    }
}

/** Constructor for ADAravis; most parameters are simply passed to ADDriver::ADDriver.
  * After calling the base class constructor this method creates a thread to compute the GigE detector data,
  * and sets reasonable default values for parameters defined in this class, asynNDArrayDriver and ADDriver.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] cameraName The name of the camera, \<vendor\>-\<serial#\>, as returned by arv-show-devices
  * \param[in] enableCaching Flag to enable (1) or disable (0) register caching in aravis. 
               Performance is much better when caching is enable, but some cameras may not properly implement this.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is
  *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  */
ADAravis::ADAravis(const char *portName, const char *cameraName, int enableCaching,
                   size_t maxMemory, int priority, int stackSize)

    : ADGenICam(portName, maxMemory, priority, stackSize),
       camera(NULL),
       connectionValid(0),
       stream(NULL),
       device(NULL),
       genicam(NULL),
       payload(0),
       mEnableCaching(enableCaching),
       pollingLoop(*this, 
                   "aravisPoll", 
                   stackSize>0 ? stackSize : epicsThreadGetStackSize(epicsThreadStackMedium), 
                   epicsThreadPriorityHigh)
{
    const char *functionName = "ADAravis";
    char tempString[256];

    /* glib initialisation */
    //g_type_init ();

    /* Duplicate camera name so we can use it if we reconnect */
    this->cameraName = epicsStrDup(cameraName);

    /* Create a message queue to hold completed frames */
    this->msgQId = epicsMessageQueueCreate(NRAW, sizeof(ArvBuffer*));
    if (!this->msgQId) {
        printf("%s:%s: epicsMessageQueueCreate failure\n", driverName, functionName);
        return;
    }

    /* Create some custom parameters */
    createParam("ARAVIS_COMPLETED",      asynParamFloat64, &AravisCompleted);
    createParam("ARAVIS_FAILURES",       asynParamFloat64, &AravisFailures);
    createParam("ARAVIS_UNDERRUNS",      asynParamFloat64, &AravisUnderruns);
    createParam("ARAVIS_FRAME_RETENTION",asynParamInt32,   &AravisFrameRetention);
    createParam("ARAVIS_MISSING_PKTS",   asynParamInt32,   &AravisMissingPkts);
    createParam("ARAVIS_RESENT_PKTS",    asynParamInt32,   &AravisResentPkts);
    createParam("ARAVIS_PKT_RESEND",     asynParamInt32,   &AravisPktResend);
    createParam("ARAVIS_PKT_TIMEOUT",    asynParamInt32,   &AravisPktTimeout);
    createParam("ARAVIS_CONVERT_PIXEL_FORMAT", asynParamInt32,   &AravisConvertPixelFormat);
    createParam("ARAVIS_SHIFT_DIR",      asynParamInt32,   &AravisShiftDir);
    createParam("ARAVIS_SHIFT_BITS",     asynParamInt32,   &AravisShiftBits);
    createParam("ARAVIS_CONNECTION",     asynParamInt32,   &AravisConnection);
    createParam("ARAVIS_RESET",          asynParamInt32,   &AravisReset);

    /* Set some initial values for other parameters */
    setStringParam(NDDriverVersion, DRIVER_VERSION);
    sprintf(tempString, "%d.%d.%d", ARAVIS_MAJOR_VERSION, ARAVIS_MINOR_VERSION, ARAVIS_MICRO_VERSION);
    setStringParam(ADSDKVersion, tempString);
    setIntegerParam(ADReverseX, 0);
    setIntegerParam(ADReverseY, 0);
    setIntegerParam(ADImageMode, ADImageContinuous);
    setIntegerParam(ADNumImages, 100);
    setDoubleParam(AravisCompleted, 0);
    setDoubleParam(AravisFailures, 0);
    setDoubleParam(AravisUnderruns, 0);
    setIntegerParam(AravisFrameRetention, 100000);  // aravisGigE default 100ms
    setIntegerParam(AravisMissingPkts, 0);
    setIntegerParam(AravisPktResend, 1);
    setIntegerParam(AravisPktTimeout, 20000);       // aravisGigE default 20ms
    setIntegerParam(AravisResentPkts, 0);
    setIntegerParam(AravisConvertPixelFormat, AravisConvertPixelFormatMono16Low);
    setIntegerParam(AravisShiftDir, 0);
    setIntegerParam(AravisShiftBits, 4);
    setIntegerParam(AravisReset, 0);
    
    /* Enable the fake camera for simulations */
    arv_enable_interface ("Fake");

    /* Connect to the camera */
    this->featureIndex = 0;
    this->connectToCamera();

    /* Register the shutdown function for epicsAtExit */
    epicsAtExit(aravisShutdown, (void*)this);

    /* Register the pollingLoop to start after iocInit */
    initHookRegister(setIocRunningFlag);
    this->pollingLoop.start();
}



asynStatus ADAravis::makeCameraObject() {
    const char *functionName = "makeCameraObject";

    GErrorHelper err;
    /* remove old camera if it exists */
    if (this->camera != NULL) {
        g_object_unref(this->camera);
        this->camera = NULL;
    }
    /* remove ref to device and genicam */
    this->device = NULL;
    this->genicam = NULL;

    /* connect to camera */
    printf ("ADAravis: Looking for camera '%s'... \n", this->cameraName);
    this->camera = arv_camera_new (this->cameraName, err.get());
    if (this->camera == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: No camera found, err=%s\n",
                    driverName, functionName, err->message);
        return asynError;
    }
    /* Store device */
    this->device = arv_camera_get_device(this->camera);
    if (this->device == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: No device associated with camera\n",
                    driverName, functionName);
        return asynError;
    }
    if (ARV_IS_GV_DEVICE(this->device)) {
        // Automatically determine optimum packet size
        arv_gv_device_auto_packet_size(ARV_GV_DEVICE(this->device), err.get());
        // Uncomment this line to set jumbo packets
        //arv_gv_device_set_packet_size(ARV_GV_DEVICE(this->device), 9000);
    }
    /* Store genicam */
    this->genicam = arv_device_get_genicam (this->device);
    if (this->genicam == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: No genicam element associated with camera\n",
                    driverName, functionName);
        return asynError;
    }
    /* Set the cache policy */
    arv_gc_set_register_cache_policy(this->genicam, mEnableCaching ? ARV_REGISTER_CACHE_POLICY_ENABLE : ARV_REGISTER_CACHE_POLICY_DISABLE);

    return asynSuccess;
}

asynStatus ADAravis::makeStreamObject() {
    const char *functionName = "makeStreamObject";
    asynStatus status = asynSuccess;
    GErrorHelper err;
    
    /* remove old stream if it exists */
    if (this->stream != NULL) {
        arv_stream_set_emit_signals (this->stream, FALSE);
        g_object_unref(this->stream);
        this->stream = NULL;
    }
    this->stream = arv_camera_create_stream (this->camera, NULL, NULL, err.get());
    if (this->stream == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Making stream failed, err=%s, retrying in 5s...\n",
                    driverName, functionName, err->message);
        epicsThreadSleep(5);
        /* make the camera object */
        status = this->makeCameraObject();
        if (status != asynSuccess) return (asynStatus) status;
        /* Make the stream */
        this->stream = arv_camera_create_stream (this->camera, NULL, NULL, err.get());
    }
    if (this->stream == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Making stream failed, err=%s\n",
                    driverName, functionName, err->message);
        return asynError;
    }
    
    if (ARV_IS_GV_DEVICE(this->stream)) {
        /* configure the stream */
        // Available stream options:
        //  socket-buffer:      ARV_GV_STREAM_SOCKET_BUFFER_FIXED, ARV_GV_STREAM_SOCKET_BUFFER_AUTO, defaults to auto which follows arvgvbuffer size
        //  socket-buffer-size: 64 bit int, Defaults to -1
        //  packet-resend:      ARV_GV_STREAM_PACKET_RESEND_NEVER, ARV_GV_STREAM_PACKET_RESEND_ALWAYS, defaults to always
        //  packet-timeout:     64 bit int, units us, ARV_GV_STREAM default 40000
        //  frame-retention:    64 bit int, units us, ARV_GV_STREAM default 200000
    
        epicsInt32      FrameRetention, PktResend, PktTimeout;
        getIntegerParam(AravisFrameRetention,  &FrameRetention);
        getIntegerParam(AravisPktResend,       &PktResend);
        getIntegerParam(AravisPktTimeout,      &PktTimeout);
        g_object_set (ARV_GV_STREAM (this->stream),
                  "packet-resend",      (guint64) PktResend,
                  "packet-timeout",     (guint64) PktTimeout,
                  "frame-retention",    (guint64) FrameRetention,
                  NULL);
    }

    // Enable callback on new buffers
    arv_stream_set_emit_signals (this->stream, TRUE);
    g_signal_connect (this->stream, "new-buffer", G_CALLBACK (newBufferCallbackC), this);
    return asynSuccess;
}


asynStatus ADAravis::connectToCamera() {
    const char *functionName = "connectToCamera";
    int status = asynSuccess;
    GErrorHelper err;

    /* stop old camera if it exists */
    this->connectionValid = 0;
    if (this->camera != NULL) {
        arv_camera_stop_acquisition(this->camera, err.get());
    }

    /* Tell areaDetector it is no longer acquiring */
    setIntegerParam(ADAcquire, 0);

    /* make the camera object */
    status = this->makeCameraObject();
    if (status) return (asynStatus) status;

    /* Make sure it's stopped */
    arv_camera_stop_acquisition(this->camera, err.get());
    status |= setIntegerParam(ADStatus, ADStatusIdle);
    
    /* Check the tick frequency */
    if (ARV_IS_GV_DEVICE(this->device)) {
        guint64 freq = arv_gv_device_get_timestamp_tick_frequency(ARV_GV_DEVICE(this->device), err.get());
        printf("ADAravis: Your tick frequency is %" G_GUINT64_FORMAT "\n", freq);
        if (freq > 0) {
            printf("So your timestamp resolution is %f ns\n", 1.e9/freq);
        } else {
            printf("So your camera doesn't provide timestamps. Using system clock instead\n");
        }
    }
    
    /* Make the stream */
    status = this->makeStreamObject();
    if (status) return (asynStatus) status;    
    
    /* connect connection lost signal to camera */
    g_signal_connect (this->device, "control-lost", G_CALLBACK (controlLostCallback), this);


    /* Report if anything has failed */
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Unable to get all camera parameters\n",
                    driverName, functionName);
    }

    /* Mark connection valid again */
    this->connectionValid = 1;

    return (asynStatus) status;
}

/** Called when asyn clients call pasynInt32->write().
  * This function performs actions for some parameters, including ADAcquire, ADColorMode, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus ADAravis::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    epicsInt32 rbv;
    const char  *reasonName = "unknownReason";
    //static const char *functionName = "writeInt32";
    getParamName(0, function, &reasonName);

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    getIntegerParam(function, &rbv);
    status = setIntegerParam(function, value);

    /* If we have no camera, then just fail */
    if (function == AravisReset) {
        status = this->connectToCamera();
    } else if (this->camera == NULL || this->connectionValid != 1) {
        if (rbv != value)
            setIntegerParam(ADStatus, ADStatusDisconnected);
        status = asynError;
    } else if (function == AravisConnection) {
        if (this->connectionValid != 1) status = asynError;
    } else if (function == AravisFrameRetention || function == AravisPktResend || function == AravisPktTimeout ||
               function == AravisShiftDir || function == AravisShiftBits || function == AravisConvertPixelFormat) {
        /* just write the value for these as they get fetched via getIntegerParam when needed */
    } else if ((function < FIRST_ARAVIS_CAMERA_PARAM) || (function > LAST_ARAVIS_CAMERA_PARAM)) {
        /* If this parameter belongs to a base class call its method */
        /* GenICam parameters are created after this constructor runs, so they are higher numbers */
        status = ADGenICam::writeInt32(pasynUser, value);
    /* generic feature lookup */
    } else {
           status = asynError;
    }

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();

    if (function != AravisConnection)
    {
        /* Report any errors */
        if (status)
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s:writeInt32 error, status=%d function=%d %s, value=%d\n",
                  driverName, status, function, reasonName, value);
        else
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                  "%s:writeInt32: function=%d %s, value=%d\n",
                  driverName, function, reasonName, value);
    }
    return status;
}

/** Report status of the driver.
  * Prints details about the driver if details>0.
  * It then calls the ADDriver::report() method.
  * \param[in] fp File pointed passed by caller where the output is written to.
  * \param[in] details If >0 then driver details are printed.
  */
void ADAravis::report(FILE *fp, int details)
{

    fprintf(fp, "Aravis GigE detector %s\n", this->portName);
    if (details > 0) {
        int nx, ny, dataType;
        getIntegerParam(ADSizeX, &nx);
        getIntegerParam(ADSizeY, &ny);
        getIntegerParam(NDDataType, &dataType);
        fprintf(fp, "  NX, NY:            %d  %d\n", nx, ny);
        fprintf(fp, "  Data type:         %d\n", dataType);
    }
    /* Invoke the base class method */
    ADGenICam::report(fp, details);
}


/** Allocate an NDArray and prepare a buffer that is passed to the stream
    this->camera exists, lock taken */
asynStatus ADAravis::allocBuffer() {
    const char *functionName = "allocBuffer";
    ArvBuffer *buffer;
    NDArray *pRaw;
    size_t bufferDims[2] = {1,1};

    /* check stream exists */
    if (this->stream == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Cannot allocate buffer on a NULL stream\n",
                    driverName, functionName);
        return asynError;
    }

    pRaw = this->pNDArrayPool->alloc(2, bufferDims, NDInt8, this->payload, NULL);
    if (pRaw==NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error allocating raw buffer\n",
                    driverName, functionName);
        return asynError;
    }

    buffer = arv_buffer_new_full(this->payload, pRaw->pData, (void *)pRaw, destroyBuffer);
    arv_stream_push_buffer (this->stream, buffer);
    return asynSuccess;
}

/** Check what event we have, and deal with new frames.
    this->camera exists, lock not taken */
void ADAravis::run() {
    epicsTimeStamp lastFeatureGet;
    int numImagesCounter, imageMode, numImages, acquire;
    const char *functionName = "run";
    ArvBuffer *buffer;

    /* Wait for database to be up */
    while (!iocRunning) {
        epicsThreadSleep(0.1);
    }

    /* Loop forever */
    epicsTimeGetCurrent(&lastFeatureGet);
    while (1) {
        /* Wait 5ms for an array to arrive from the queue */
        if (epicsMessageQueueReceiveWithTimeout(this->msgQId, &buffer, sizeof(&buffer), 0.005) == -1) {
        } else {
            /* Got a buffer, so lock up and process it */
            this->lock();
            getIntegerParam(ADAcquire, &acquire);
            if (acquire) {
                this->processBuffer(buffer);
                /* free memory */
                g_object_unref(buffer);
                /* See if acquisition is done */
                getIntegerParam(ADNumImages, &numImages);
                getIntegerParam(ADNumImagesCounter, &numImagesCounter);
                getIntegerParam(ADImageMode, &imageMode);
                if ((imageMode == ADImageSingle) ||
                    ((imageMode == ADImageMultiple) &&
                     (numImagesCounter >= numImages))) {
                    this->stopCapture();
                    // Want to make sure we're idle before we callback on ADAcquire
                    callParamCallbacks();
                    setIntegerParam(ADAcquire, 0);
                    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                          "%s:%s: acquisition completed\n", driverName, functionName);
                } else {
                    /* Allocate the new raw buffer we use to compute images. */
                    this->allocBuffer();
                }
            } else {
                // We recieved a buffer that we didn't request
                g_object_unref(buffer);
            }
            this->unlock();
        }
    }
}

asynStatus ADAravis::processBuffer(ArvBuffer *buffer) {
    int arrayCallbacks, imageCounter, numImages, numImagesCounter, imageMode;
    int colorMode, dataType, bayerFormat;
    size_t expected_size;
    int xDim=0, yDim=1, binX, binY, shiftDir, shiftBits;
    double acquirePeriod;
    const char *functionName = "processBuffer";
    guint64 n_completed_buffers, n_failures, n_underruns;
    NDArray *pRaw;
    bool releaseArray = false;

    /* Get the current parameters */
    getIntegerParam(NDArrayCounter, &imageCounter);
    getIntegerParam(ADNumImages, &numImages);
    getIntegerParam(ADNumImagesCounter, &numImagesCounter);
    getIntegerParam(ADImageMode, &imageMode);
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    getDoubleParam(ADAcquirePeriod, &acquirePeriod);
    getIntegerParam(AravisShiftDir, &shiftDir); 
    getIntegerParam(AravisShiftBits, &shiftBits); 
    /* The buffer structure does not contain the binning, get that from param lib,
     * but it could be wrong for this frame if recently changed */
    getIntegerParam(ADBinX, &binX);
    getIntegerParam(ADBinY, &binY);
    /* Report a new frame with the counters */
    imageCounter++;
    numImagesCounter++;
    setIntegerParam(NDArrayCounter, imageCounter);
    setIntegerParam(ADNumImagesCounter, numImagesCounter);
    if (imageMode == ADImageMultiple) {
        setDoubleParam(ADTimeRemaining, (numImages - numImagesCounter) * acquirePeriod);
    }
    /* find the buffer */

    pRaw = (NDArray *) arv_buffer_get_user_data(buffer);
    if (pRaw == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: where did this buffer come from?\n",
                driverName, functionName);
        return asynError;
    }
//  printf("callb buffer: %p, pRaw[%d]: %p, pData %p\n", buffer, i, pRaw, pRaw->pData);
    int pixel_format = arv_buffer_get_image_pixel_format(buffer);
    int width = arv_buffer_get_image_width(buffer);
    int height = arv_buffer_get_image_height(buffer);
    int x_offset = arv_buffer_get_image_x(buffer);
    int y_offset = arv_buffer_get_image_y(buffer);
    size_t size = 0;
    arv_buffer_get_data(buffer, &size);
    
    //  Print the first 16 bytes of the buffer in hex
    //for (int i=0; i<16; i++) printf("%x ", ((epicsUInt8 *)pRaw->pData)[i]); printf("\n");

    // If the pixel format is Mono12p or Mono12Packed we need to do the conversion to UInt16 here
    if ((pixel_format == ARV_PIXEL_FORMAT_MONO_12_P) || 
       ( pixel_format == ARV_PIXEL_FORMAT_MONO_12_PACKED)) {
        //epicsTimeStamp tstart, tend;
        //epicsTimeGetCurrent(&tstart);
        int convertFormat;
        getIntegerParam(AravisConvertPixelFormat, &convertFormat);
        bool leftShift = (convertFormat == AravisConvertPixelFormatMono16High);
        NDArray *pIn = pRaw;
        size_t bufferDims[2] = {(size_t)width, (size_t)height};
        pRaw = this->pNDArrayPool->alloc(2, bufferDims, NDUInt16, 0, NULL);
        if (pixel_format == ARV_PIXEL_FORMAT_MONO_12_P) {
            decompressMono12p(width*height, leftShift, (epicsUInt8 *)pIn->pData, (epicsUInt16 *)pRaw->pData);
        } else {
            decompressMono12Packed(width*height, leftShift, (epicsUInt8 *)pIn->pData, (epicsUInt16 *)pRaw->pData);
        }
        //epicsTimeGetCurrent(&tend);
        //printf("Time to convert Mono12 = %f\n", epicsTimeDiffInSeconds(&tend, &tstart));
        size = width * height * sizeof(epicsUInt16);
        releaseArray = true;
    }
    //  Print the first 8 pixels of the buffer in decimal
    //for (int i=0; i<8; i++) printf("%u ", ((epicsUInt16 *)pRaw->pData)[i]); printf("\n");

    /* Put the frame number and time stamp into the buffer */
    pRaw->uniqueId = imageCounter;
    pRaw->timeStamp = arv_buffer_get_timestamp(buffer) / 1.e9;

    /* Update the areaDetector timeStamp */
    updateTimeStamp(&pRaw->epicsTS);

    /* Get any attributes that have been defined for this driver */
    this->getAttributes(pRaw->pAttributeList);

    /* Annotate it with its dimensions */
    if (this->lookupColorMode(pixel_format, &colorMode, &dataType, &bayerFormat) != asynSuccess) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: unknown pixel format %d\n",
                    driverName, functionName, pixel_format);
        return asynError;
    }
    pRaw->pAttributeList->add("BayerPattern", "Bayer Pattern", NDAttrInt32, &bayerFormat);
    pRaw->pAttributeList->add("ColorMode", "Color Mode", NDAttrInt32, &colorMode);
    pRaw->dataType = (NDDataType_t) dataType;
    setIntegerParam(NDArraySizeX, width);
    setIntegerParam(NDArraySizeY, height);
    setIntegerParam(NDArraySize, (int)size);
    setIntegerParam(NDDataType,dataType);
    switch (colorMode) {
        case NDColorModeMono:
        case NDColorModeBayer:
            xDim = 0;
            yDim = 1;
            pRaw->ndims = 2;
            expected_size = width * height;
            break;
        case NDColorModeRGB1:
            xDim = 1;
            yDim = 2;
            pRaw->ndims = 3;
            pRaw->dims[0].size    = 3;
            pRaw->dims[0].offset  = 0;
            pRaw->dims[0].binning = 1;
            expected_size = width * height * 3;
            break;
        default:
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                        "%s:%s: unknown colorMode %d\n",
                        driverName, functionName, colorMode);
            return asynError;
    }
    pRaw->dims[xDim].size    = width;
    pRaw->dims[xDim].offset  = x_offset;
    pRaw->dims[xDim].binning = binX;
    pRaw->dims[yDim].size    = height;
    pRaw->dims[yDim].offset  = y_offset;
    pRaw->dims[yDim].binning = binY;

    /* If we are 16 bit, shift by the correct amount */
    if (pRaw->dataType == NDUInt16) {
        expected_size *= 2;
        uint16_t *array = (uint16_t *) pRaw->pData;
        if (shiftDir == AravisShiftLeft) {
            for (unsigned int ib = 0; ib < size / 2; ib++) {
                array[ib] = array[ib] << shiftBits;
            }
        } else if (shiftDir == AravisShiftRight) {
            for (unsigned int ib = 0; ib < size / 2; ib++) {
                array[ib] = array[ib] >> shiftBits;
            }
        }
    }

    if (expected_size != size) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: w: %d, h: %d, size: %zu, expected_size: %zu\n",
                    driverName, functionName, width, height, size, expected_size);
        return asynError;
    }

    /* this is a good image, so callback on it */
    if (arrayCallbacks) {
        /* Call the NDArray callback */
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
             "%s:%s: calling imageData callback\n", driverName, functionName);
        doCallbacksGenericPointer(pRaw, NDArrayData, 0);
    }
    
    if (releaseArray) {
        pRaw->release();
    }

    /* Report statistics */
    if (this->stream != NULL) {
        arv_stream_get_statistics(this->stream, &n_completed_buffers, &n_failures, &n_underruns);
        setDoubleParam(AravisCompleted, (double) n_completed_buffers);
        setDoubleParam(AravisFailures, (double) n_failures);
        setDoubleParam(AravisUnderruns, (double) n_underruns);

        if (ARV_IS_GV_DEVICE(this->stream)) {
            guint64 n_resent_pkts, n_missing_pkts;
            arv_gv_stream_get_statistics(ARV_GV_STREAM(this->stream), &n_resent_pkts, &n_missing_pkts);
            setIntegerParam(AravisResentPkts,  (epicsInt32) n_resent_pkts);
            setIntegerParam(AravisMissingPkts, (epicsInt32) n_missing_pkts);
        }
    }

    /* Call the callbacks to update any changes */
    callParamCallbacks();
    return asynSuccess;
}

asynStatus ADAravis::stopCapture() {
    /* Stop the camera */
    arv_camera_stop_acquisition(this->camera, NULL);
    setIntegerParam(ADStatus, ADStatusIdle);
    /* Tear down the old stream and make a new one */
    return this->makeStreamObject();
}

asynStatus ADAravis::startCapture() {
    int imageMode, numImages;
    GErrorHelper err;
    const char *functionName = "start";
    
    getIntegerParam(ADImageMode, &imageMode);

    if (imageMode == ADImageSingle) {
        arv_camera_set_acquisition_mode(this->camera, ARV_ACQUISITION_MODE_SINGLE_FRAME, err.get());
    } else if (imageMode == ADImageMultiple) {
        if (mGCFeatureSet.getByName("AcquisitionFrameCount")) {
            getIntegerParam(ADNumImages, &numImages);
            arv_device_set_integer_feature_value(this->device, "AcquisitionFrameCount", numImages, err.get());
        }
        arv_camera_set_acquisition_mode(this->camera, ARV_ACQUISITION_MODE_MULTI_FRAME, err.get());
    } else {
        arv_camera_set_acquisition_mode(this->camera, ARV_ACQUISITION_MODE_CONTINUOUS, err.get());
    }
    setIntegerParam(ADNumImagesCounter, 0);
    setIntegerParam(ADStatus, ADStatusAcquire);

    /* fill the queue */
    this->payload = arv_camera_get_payload(this->camera, err.get());
    for (int i=0; i<NRAW; i++) {
        if (this->allocBuffer() != asynSuccess) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                        "%s:%s: allocBuffer returned error\n",
                        driverName, functionName);
            return asynError;
        }
    }

    // Start the camera acquiring
    arv_camera_start_acquisition (this->camera, err.get());
    return asynSuccess;
}

/** Lookup a colorMode, dataType and bayerFormat from an ArvPixelFormat */
asynStatus ADAravis::lookupColorMode(ArvPixelFormat fmt, int *colorMode, int *dataType, int *bayerFormat) {
    const char *functionName = "lookupColorMode";
    const int N = sizeof(pix_lookup) / sizeof(struct pix_lookup);
    for (int i = 0; i < N; i ++)
        if (pix_lookup[i].fmt == fmt) {
            *colorMode   = pix_lookup[i].colorMode;
            *dataType    = pix_lookup[i].dataType;
            *bayerFormat = pix_lookup[i].bayerFormat;
            return asynSuccess;
        }
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: Could not find a match for pixel format: %d\n",
                driverName, functionName, fmt);
    return asynError;
}

/** Lookup an ArvPixelFormat from a colorMode, dataType and bayerFormat */
asynStatus ADAravis::lookupPixelFormat(int colorMode, int dataType, int bayerFormat, ArvPixelFormat *fmt) {
    const char *functionName = "lookupPixelFormat";
    GErrorHelper err;
    const int N = sizeof(pix_lookup) / sizeof(struct pix_lookup);
    ArvGcNode *node = arv_gc_get_node(genicam, "PixelFormat");
    for (int i = 0; i < N; i ++)
        if (colorMode   == pix_lookup[i].colorMode &&
            dataType    == pix_lookup[i].dataType &&
            bayerFormat == pix_lookup[i].bayerFormat) {
            if (ARV_IS_GC_ENUMERATION (node)) {
                /* Check if the pixel format is supported by the camera */
                ArvGcEnumeration *enumeration = (ARV_GC_ENUMERATION (node));
                const GSList *iter;
                for (iter = arv_gc_enumeration_get_entries (enumeration); iter != NULL; iter = iter->next) {
                    if (arv_gc_feature_node_is_available(ARV_GC_FEATURE_NODE(iter->data), err.get()) &&
                            arv_gc_enum_entry_get_value(ARV_GC_ENUM_ENTRY(iter->data), err.get()) == pix_lookup[i].fmt) {
                        *fmt = pix_lookup[i].fmt;
                        return asynSuccess;
                    }
                }
            } else {
                /* No PixelFormat node to check against */
                *fmt = pix_lookup[i].fmt;
                return asynSuccess;
            }
        }
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: Could not find a match for colorMode: %d, dataType: %d, bayerFormat: %d\n",
                driverName, functionName, colorMode, dataType, bayerFormat);
    return asynError;
}


/** Configuration command, called directly or from iocsh */
extern "C" int ADAravisConfig(const char *portName, const char *cameraName, int enableCaching,
                              size_t maxMemory, int priority, int stackSize)
{
    new ADAravis(portName, cameraName, enableCaching, maxMemory, priority, stackSize);
    return(asynSuccess);
}

/** Code for iocsh registration */
static const iocshArg ADAravisConfigArg0 = {"Port name", iocshArgString};
static const iocshArg ADAravisConfigArg1 = {"Camera name", iocshArgString};
static const iocshArg ADAravisConfigArg2 = {"enable caching", iocshArgInt};
static const iocshArg ADAravisConfigArg3 = {"maxMemory", iocshArgInt};
static const iocshArg ADAravisConfigArg4 = {"priority", iocshArgInt};
static const iocshArg ADAravisConfigArg5 = {"stackSize", iocshArgInt};
static const iocshArg * const ADAravisConfigArgs[] =  {&ADAravisConfigArg0,
                                                       &ADAravisConfigArg1,
                                                       &ADAravisConfigArg2,
                                                       &ADAravisConfigArg3,
                                                       &ADAravisConfigArg4,
                                                       &ADAravisConfigArg5};
static const iocshFuncDef configADAravis = {"aravisConfig", 6, ADAravisConfigArgs};
static void configADAravisCallFunc(const iocshArgBuf *args)
{
    ADAravisConfig(args[0].sval, args[1].sval, args[2].ival, 
                   args[3].ival, args[4].ival, args[5].ival);
}


static void ADAravisRegister(void)
{

    iocshRegister(&configADAravis, configADAravisCallFunc);
}

extern "C" {
    epicsExportRegistrar(ADAravisRegister);
}
