#define LOG_TAG "wrs-omx-stub-ve-avc-new-libva "
#include <utils/Log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>

#include <OMX_Core.h>
#include <OMX_Video.h>
#include <OMX_IndexExt.h>
#include <OMX_VideoExt.h>

#include <cmodule.h>
#include <portvideo.h>
#include <componentbase.h>

#ifdef COMPONENT_SUPPORT_OPENCORE
#include <pv_omxcore.h>
#include <pv_omxdefs.h>
#endif

#include <va/va.h>
#include <va/va_android.h>
#include "vabuffer.h"
#include "libinfodump.h"

#include "OMXAVCComponent.h"

//#define INFODUMP
#ifdef INFODUMP
int counter = 0;
#endif

static void DUMP_BUFFER(char* name, OMX_U8* buf, OMX_U32 len)
{
	int idx;
	int loglen = len < 16 ? len : 16;
	static char logbuf[4096];
	char *plogbuf = logbuf;

	for(idx = 0;idx<loglen;idx++) {
		sprintf(plogbuf, "0x%x ", ((unsigned char*)buf)[idx]);
		plogbuf = logbuf + strlen(logbuf);
	}
	plogbuf = 0;

	LOGV("%s(%p:%d): %s",name,  buf, len, logbuf);
}	


OMXAVCComponent::OMXAVCComponent():
	temp_coded_data_buffer(NULL),
	temp_coded_data_buffer_size(0)
{
    ENTER_FUN;

    int pret;

    config_lock = new pthread_mutex_t;
    pret = pthread_mutex_init(config_lock, NULL);
    assert(pret == 0);

    EXIT_FUN;
}

OMXAVCComponent::~OMXAVCComponent()
{
    ENTER_FUN;

    if (config_lock != NULL)
    {
        int pret;

	pret = pthread_mutex_destroy(config_lock);
	assert(pret == 0);

	delete config_lock;
    }

    EXIT_FUN;
}


/* core methods & helpers */
OMX_ERRORTYPE OMXAVCComponent::ComponentAllocatePorts(void)
{
    ENTER_FUN;

    PortBase **ports;

    OMX_U32 codec_port_index, raw_port_index;
    OMX_DIRTYPE codec_port_dir, raw_port_dir;

    OMX_PORT_PARAM_TYPE portparam;

    const char *working_role;

    bool isencoder;

    OMX_ERRORTYPE ret = OMX_ErrorUndefined;

    ports = new PortBase *[NR_PORTS];
    if (!ports)
        return OMX_ErrorInsufficientResources;

    this->nr_ports = NR_PORTS;
    this->ports = ports;

    /* video_[encoder/decoder].[avc/whatever] */

    raw_port_index = INPORT_INDEX;
    codec_port_index = OUTPORT_INDEX;
    raw_port_dir = OMX_DirInput;
    codec_port_dir = OMX_DirOutput;

    ret = __AllocateAVCPort(codec_port_index, codec_port_dir);
    if (ret != OMX_ErrorNone)
        goto free_codecport;

    ret = __AllocateRawPort(raw_port_index, raw_port_dir);
    if (ret != OMX_ErrorNone)
        goto free_codecport;

    /* OMX_PORT_PARAM_TYPE */
    memset(&portparam, 0, sizeof(portparam));
    SetTypeHeader(&portparam, sizeof(portparam));
    portparam.nPorts = NR_PORTS;
    portparam.nStartPortNumber = INPORT_INDEX;

    memcpy(&this->portparam, &portparam, sizeof(portparam));
    /* end of OMX_PORT_PARAM_TYPE */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;

free_codecport:
    delete ports[codec_port_index];
    ports[codec_port_index] = NULL;

free_ports:
    delete []ports;
    ports = NULL;

    this->ports = NULL;
    this->nr_ports = 0;

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXAVCComponent::__AllocateAVCPort(OMX_U32 port_index,
                                                    OMX_DIRTYPE dir)
{
    ENTER_FUN;
    PortAvc *avcport;

    OMX_PARAM_PORTDEFINITIONTYPE avcportdefinition;
    OMX_VIDEO_PARAM_AVCTYPE avcportparam;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortAvc;
    if (!ports[port_index])
    {
        LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }

    avcport = static_cast<PortAvc *>(this->ports[port_index]);

    /* OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&avcportdefinition, 0, sizeof(avcportdefinition));
    SetTypeHeader(&avcportdefinition, sizeof(avcportdefinition));
    avcportdefinition.nPortIndex = port_index;
    avcportdefinition.eDir = dir;
    if (dir == OMX_DirInput)
    {
        avcportdefinition.nBufferCountActual = INPORT_AVC_ACTUAL_BUFFER_COUNT;
        avcportdefinition.nBufferCountMin = INPORT_AVC_MIN_BUFFER_COUNT;
        avcportdefinition.nBufferSize = INPORT_AVC_BUFFER_SIZE;
    }
    else
    {
        avcportdefinition.nBufferCountActual = OUTPORT_AVC_ACTUAL_BUFFER_COUNT;
        avcportdefinition.nBufferCountMin = OUTPORT_AVC_MIN_BUFFER_COUNT;
        avcportdefinition.nBufferSize = OUTPORT_AVC_BUFFER_SIZE;
    }
    avcportdefinition.bEnabled = OMX_TRUE;
    avcportdefinition.bPopulated = OMX_FALSE;
    avcportdefinition.eDomain = OMX_PortDomainVideo;
    avcportdefinition.format.video.cMIMEType = (char *)"video/h264";
    avcportdefinition.format.video.pNativeRender = NULL;
    avcportdefinition.format.video.nFrameWidth = 176;
    avcportdefinition.format.video.nFrameHeight = 144;
    avcportdefinition.format.video.nStride = 0;
    avcportdefinition.format.video.nSliceHeight = 0;
    avcportdefinition.format.video.nBitrate = 64000;
    avcportdefinition.format.video.xFramerate = 15 << 16;
    avcportdefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    avcportdefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    avcportdefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    avcportdefinition.format.video.pNativeWindow = NULL;
    avcportdefinition.bBuffersContiguous = OMX_FALSE;
    avcportdefinition.nBufferAlignment = 0;

    avcport->SetPortDefinition(&avcportdefinition, true);

    /* end of OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_VIDEO_PARAM_AVCTYPE */
    memset(&avcportparam, 0, sizeof(avcportparam));
    SetTypeHeader(&avcportparam, sizeof(avcportparam));
    avcportparam.nPortIndex = port_index;

    avcportparam.eProfile = OMX_VIDEO_AVCProfileBaseline;
    avcportparam.eLevel = OMX_VIDEO_AVCLevel31;

    avcport->SetPortAvcParam(&avcportparam, true);
    /* end of OMX_VIDEO_PARAM_AVCTYPE */

    /* encoder */
    if (dir == OMX_DirOutput) {
        /* OMX_VIDEO_PARAM_BITRATETYPE */
        OMX_VIDEO_PARAM_BITRATETYPE bitrateparam;

        memset(&bitrateparam, 0, sizeof(bitrateparam));
        SetTypeHeader(&bitrateparam, sizeof(bitrateparam));

        bitrateparam.nPortIndex = port_index;
        bitrateparam.eControlRate = OMX_Video_ControlRateConstant;
        bitrateparam.nTargetBitrate = 192000;

        avcport->SetPortBitrateParam(&bitrateparam, true);

        /* OMX_VIDEO_CONFIG_PRI_INFOTYPE */
        OMX_VIDEO_CONFIG_PRI_INFOTYPE privateinfoparam;

        memset(&privateinfoparam, 0, sizeof(privateinfoparam));
        SetTypeHeader(&privateinfoparam, sizeof(privateinfoparam));

        privateinfoparam.nPortIndex = port_index;
        privateinfoparam.nCapacity = 0;
        privateinfoparam.nHolder = NULL;

        avcport->SetPortPrivateInfoParam(&privateinfoparam, true);

        /* end of OMX_VIDEO_PARAM_BITRATETYPE */
    }

#ifdef COMPONENT_SUPPORT_OPENCORE
    avcEncNaluFormatType = OMX_NaluFormatZeroByteInterleaveLength;  //opencore by default will accept this NALu format (via no extra param negotiation)
   
#else
    avcEncNaluFormatType = OMX_NaluFormatStartCodes;                //nal format only used for stagefright
#endif
    LOGE(" avcEncNaluFormatType = %d ", avcEncNaluFormatType);
    
    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;

}

OMX_ERRORTYPE OMXAVCComponent::__AllocateRawPort(OMX_U32 port_index,
                                                  OMX_DIRTYPE dir)
{
    PortVideo *rawport;

    OMX_PARAM_PORTDEFINITIONTYPE rawportdefinition;
    OMX_VIDEO_PARAM_PORTFORMATTYPE rawvideoparam;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortVideo;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }

    rawport = static_cast<PortVideo *>(this->ports[port_index]);

    /* OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&rawportdefinition, 0, sizeof(rawportdefinition));
    SetTypeHeader(&rawportdefinition, sizeof(rawportdefinition));
    rawportdefinition.nPortIndex = port_index;
    rawportdefinition.eDir = dir;
    if (dir == OMX_DirInput) {
        rawportdefinition.nBufferCountActual = INPORT_RAW_ACTUAL_BUFFER_COUNT;
        rawportdefinition.nBufferCountMin = INPORT_RAW_MIN_BUFFER_COUNT;
        rawportdefinition.nBufferSize = INPORT_RAW_BUFFER_SIZE;
    }
    else {
        rawportdefinition.nBufferCountActual = OUTPORT_RAW_ACTUAL_BUFFER_COUNT;
        rawportdefinition.nBufferCountMin = OUTPORT_RAW_MIN_BUFFER_COUNT;
        rawportdefinition.nBufferSize = OUTPORT_RAW_BUFFER_SIZE;
    }
    rawportdefinition.bEnabled = OMX_TRUE;
    rawportdefinition.bPopulated = OMX_FALSE;
    rawportdefinition.eDomain = OMX_PortDomainVideo;
    rawportdefinition.format.video.cMIMEType = (char *)"video/raw";
    rawportdefinition.format.video.pNativeRender = NULL;
    rawportdefinition.format.video.nFrameWidth = 176;
    rawportdefinition.format.video.nFrameHeight = 144;
    rawportdefinition.format.video.nStride = 176;
    rawportdefinition.format.video.nSliceHeight = 144;
    rawportdefinition.format.video.nBitrate = 64000;
    rawportdefinition.format.video.xFramerate = 15 << 16;
    rawportdefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    rawportdefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    rawportdefinition.format.video.eColorFormat =
        OMX_COLOR_FormatYUV420SemiPlanar;
    rawportdefinition.format.video.pNativeWindow = NULL;
    rawportdefinition.bBuffersContiguous = OMX_FALSE;
    rawportdefinition.nBufferAlignment = 0;

    rawport->SetPortDefinition(&rawportdefinition, true);

    /* end of OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_VIDEO_PARAM_PORTFORMATTYPE */
    rawvideoparam.nPortIndex = port_index;
    rawvideoparam.nIndex = 0;
    rawvideoparam.eCompressionFormat = OMX_VIDEO_CodingUnused;
    rawvideoparam.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;

    rawport->SetPortVideoParam(&rawvideoparam, true);

    /* end of OMX_VIDEO_PARAM_PORTFORMATTYPE */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}

/* end of core methods & helpers */

/*
 * component methods & helpers
 */
/* Get/SetParameter */
OMX_ERRORTYPE OMXAVCComponent::ComponentGetParameter(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter (index = 0x%08x)\n", __func__, nParamIndex);

    switch (nParamIndex) {
    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *p =
            (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone)
        {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
        {
            port = static_cast<PortVideo *>(ports[index]);
        }

        if (!port) 
        {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
         }

        memcpy(p, port->GetPortVideoParam(), sizeof(*p));

        break;
    }
    case OMX_IndexParamVideoAvc:
    {
        OMX_VIDEO_PARAM_AVCTYPE *p =
            (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAvc *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) 
        {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
        {
            port = static_cast<PortAvc *>(ports[index]);
        }

        if (!port)
        {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
         }

         memcpy(p, port->GetPortAvcParam(), sizeof(*p));
         break;
      }
     case OMX_IndexParamVideoBitrate: 
     {
        OMX_VIDEO_PARAM_BITRATETYPE *p =
            (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) 
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
        {
            port = static_cast<PortVideo *>(ports[index]);
        }

        if (!port)
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortBitrateParam(), sizeof(*p));
        break;
     }
#ifdef COMPONENT_SUPPORT_BUFFER_SHARING
#ifdef COMPONENT_SUPPORT_OPENCORE
     case OMX_IndexIntelPrivateInfo: 
     {
        OMX_VIDEO_CONFIG_PRI_INFOTYPE *p =
            (OMX_VIDEO_CONFIG_PRI_INFOTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone)
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
        {
            port = static_cast<PortVideo *>(ports[index]);
        }

        if (!port)
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortPrivateInfoParam(), sizeof(*p));
        break;
    }
#endif
#endif
#ifdef COMPONENT_SUPPORT_OPENCORE
    /* PVOpenCore */
    case (OMX_INDEXTYPE) PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX:
    {
        PV_OMXComponentCapabilityFlagsType *p =
            (PV_OMXComponentCapabilityFlagsType *)pComponentParameterStructure;

        p->iIsOMXComponentMultiThreaded = OMX_TRUE;
        p->iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
        p->iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
        p->iOMXComponentSupportsMovableInputBuffers = OMX_TRUE;
        p->iOMXComponentSupportsPartialFrames = OMX_FALSE;
        p->iOMXComponentCanHandleIncompleteFrames = OMX_FALSE;
        p->iOMXComponentUsesNALStartCodes = OMX_FALSE;
        p->iOMXComponentUsesFullAVCFrames = OMX_FALSE;

        break;
    }
#endif
    case OMX_IndexParamNalStreamFormat:
    case OMX_IndexParamNalStreamFormatSupported: 
    {
    	OMX_NALSTREAMFORMATTYPE *p =
           (OMX_NALSTREAMFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s() : OMX_IndexParamNalStreamFormat or OMX_IndexParamNalStreamFormatSupported");

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) 
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
        {
            port = static_cast<PortVideo *>(ports[index]);
        }

        if (!port) 
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        LOGV("%s() : about to get native or supported nal format", __func__);

        if (port->IsEnabled()) 
        {
#if 0
            OMX_STATETYPE state;
            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
#endif
            
	    if(nParamIndex == OMX_IndexParamNalStreamFormat) 
            {
                p->eNaluFormat = avcEncNaluFormatType;
                LOGV("OMX_IndexParamNalStreamFormat 0x%x", p->eNaluFormat);
            }
            else 
            {
                p->eNaluFormat = (OMX_NALUFORMATSTYPE)(OMX_NaluFormatStartCodes |
                    		      OMX_NaluFormatFourByteInterleaveLength |
                    		      OMX_NaluFormatZeroByteInterleaveLength);
                LOGV("OMX_IndexParamNalStreamFormatSupported 0x%x", p->eNaluFormat);
            }
         }
        break;
    }
    case OMX_IndexConfigVideoAVCIntraPeriod: 
    {
	 OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval =
		(OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentParameterStructure;
	 OMX_U32 index = pVideoIDRInterval->nPortIndex;

        LOGV("%s() : OMX_IndexConfigVideoAVCIntraPeriod", __func__);

	ret = CheckTypeHeader(pVideoIDRInterval, sizeof(*pVideoIDRInterval));
	if (ret != OMX_ErrorNone) 
        {
	    LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
	    return ret;
	}

	if (pVideoIDRInterval->nPortIndex != OUTPORT_INDEX) 
        {
		LOGV("Wrong port index");
		return OMX_ErrorBadPortIndex;
	}

	PortAvc *port = static_cast<PortAvc *> (ports[index]);
	if (!port)
        {
	    LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
	                  OMX_ErrorBadPortIndex);
	    return OMX_ErrorBadPortIndex;
	}

	if (port->IsEnabled()) 
        {
	    LOCK_CONFIG
	    RtCode aret;
	    aret = encoder->getConfig(&config);
	    assert(aret == SUCCESS);

            pVideoIDRInterval->nIDRPeriod = config.iDRInterval;
            pVideoIDRInterval->nPFrames = config.intraInterval;
	    UNLOCK_CONFIG

#if 0		//direct get the config
			ret = ComponentGetConfig(OMX_IndexConfigVideoAVCIntraPeriod,
				pComponentParameterStructure);
#endif
	}

	break;
     }
     case OMX_IndexParamVideoProfileLevelQuerySupported:
     {
         OMX_VIDEO_PARAM_PROFILELEVELTYPE *p =
             (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        PortAvc *port = NULL;

        OMX_U32 index = p->nPortIndex;
    

       LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) 
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
        {
            port = static_cast<PortAvc *>(ports[index]);
        }
        else
        {
            return OMX_ErrorBadParameter;
        }

        const OMX_VIDEO_PARAM_AVCTYPE *avcParam = port->GetPortAvcParam();

        p->eProfile = avcParam->eProfile;
        p->eLevel  = avcParam->eLevel;

        break;
     }

     default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXAVCComponent::ComponentSetParameter(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter (index = 0x%08x)\n", __func__, nIndex);

    switch (nIndex) {
    case OMX_IndexParamVideoPortFormat: 
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *p =
            (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone)
        {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
        {
            port = static_cast<PortVideo *>(ports[index]);
        }

        if (!port) 
        {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled())
        {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortVideoParam(p, false);
        break;
    }
    case OMX_IndexParamVideoAvc: 
    {
        OMX_VIDEO_PARAM_AVCTYPE *p =
            (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAvc *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) 
        {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
        {
            port = static_cast<PortAvc *>(ports[index]);
        }

        if (!port) 
        {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) 
        {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) 
            {
                LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortAvcParam(p, false);
        break;
    }
    case OMX_IndexParamVideoBitrate: 
    {
        OMX_VIDEO_PARAM_BITRATETYPE *p =
            (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone)
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
        { 
            port = static_cast<PortVideo *>(ports[index]);
        }

        if (!port) 
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) 
        {
            OMX_STATETYPE state;
            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) 
            {
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortBitrateParam(p, false);

        break;
    }
#ifdef COMPONENT_SUPPORT_BUFFER_SHARING
#ifdef COMPONENT_SUPPORT_OPENCORE
    case OMX_IndexIntelPrivateInfo: 
    {
        LOGV("OMX_IndexIntelPrivateInfo");
        OMX_VIDEO_CONFIG_PRI_INFOTYPE *p =
            (OMX_VIDEO_CONFIG_PRI_INFOTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) 
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
        {
            port = static_cast<PortVideo *>(ports[index]);
        }

        if (!port) 
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) 
        {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) 
            {
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortPrivateInfoParam(p, false);
        break;
    }
#endif
#endif
    case OMX_IndexParamVideoBytestream: 
    {
	OMX_VIDEO_PARAM_BYTESTREAMTYPE *p =
			(OMX_VIDEO_PARAM_BYTESTREAMTYPE *) pComponentParameterStructure;
	OMX_U32 index = p->nPortIndex;
	PortVideo *port = NULL;

	ret = CheckTypeHeader(p, sizeof(*p));
	if (ret != OMX_ErrorNone) 
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
	    return ret;
	}

	if (index < nr_ports)
	{   
             port = static_cast<PortVideo *> (ports[index]);
        }

	if (!port) 
        {
	     LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
			OMX_ErrorBadPortIndex);
	     return OMX_ErrorBadPortIndex;
	}

	if (port->IsEnabled()) 
        {
	   OMX_STATETYPE state;
	   CBaseGetState((void *) GetComponentHandle(), &state);
	   if (state != OMX_StateLoaded && state != OMX_StateWaitForResources) 
           {
	       LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
			OMX_ErrorIncorrectStateOperation);
               return OMX_ErrorIncorrectStateOperation;
	   }

	   if (p->bBytestream == OMX_TRUE) 
           {
		avcEncNaluFormatType = OMX_NaluFormatStartCodes;
	   }
           else 
           {
		avcEncNaluFormatType = OMX_NaluFormatZeroByteInterleaveLength;
	   }
	 }
	 
         break;

	}
	case OMX_IndexParamNalStreamFormatSelect: 
        {

           LOGV("%s() : OMX_IndexParamNalStreamFormatSelect", __func__);
	   OMX_NALSTREAMFORMATTYPE *p =
			(OMX_NALSTREAMFORMATTYPE *) pComponentParameterStructure;
	   OMX_U32 index = p->nPortIndex;
	   PortVideo *port = NULL;

	   ret = CheckTypeHeader(p, sizeof(*p));
	   if (ret != OMX_ErrorNone) 
           {
		LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
		return ret;
	   }

	  if (index < nr_ports)
	  {
 	       port = static_cast<PortVideo *> (ports[index]);
          }

	  if (!port) 
          {
		LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
				OMX_ErrorBadPortIndex);
		return OMX_ErrorBadPortIndex;
	  }

	 if (p->eNaluFormat == OMX_NaluFormatStartCodes || p->eNaluFormat
			== OMX_NaluFormatFourByteInterleaveLength || p->eNaluFormat
			== OMX_NaluFormatZeroByteInterleaveLength) 
         {

	    if (port->IsEnabled()) 
            {
		OMX_STATETYPE state;
		CBaseGetState((void *) GetComponentHandle(), &state);
		if (state != OMX_StateLoaded && state!= OMX_StateWaitForResources) 
                {
		    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
				OMX_ErrorIncorrectStateOperation);
		    return OMX_ErrorIncorrectStateOperation;
		}

		avcEncNaluFormatType = p->eNaluFormat;
                LOGV("OMX_IndexParamNalStreamFormatSelect : 0x%x", avcEncNaluFormatType);
	     }
	}
        else
        {
	    LOGV("OMX_IndexParamNalStreamFormatSelect : 0x%x - Not supported NAL foramt",
		p->eNaluFormat);
	   
           assert(0);
	//FIXME:no return codes are set here
	}
	
        break;
     }
    case OMX_IndexConfigVideoAVCIntraPeriod: 
    {

       LOGV("%s() : OMX_IndexConfigVideoAVCIntraPeriod", __func__);

       OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval =
		(OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentParameterStructure;
       if (!pVideoIDRInterval) 
       {
	   LOGE("NULL pointer");
	   return OMX_ErrorBadParameter;
       }

        OMX_U32 index = pVideoIDRInterval->nPortIndex;
	
 	if (pVideoIDRInterval->nPortIndex != OUTPORT_INDEX) 
        {
	    LOGV("Wrong port index");
	    return OMX_ErrorBadPortIndex;
	}

	PortAvc *port = static_cast<PortAvc *> (ports[index]);
	if (!port) 
        {
	    LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
			OMX_ErrorBadPortIndex);
	    return OMX_ErrorBadPortIndex;
	}

	if (!port->IsEnabled()) 
        {
                        LOGV("%s() : port is not enabled", __func__);
			return OMX_ErrorNotReady;
	}

	ret = CheckTypeHeader(pVideoIDRInterval, sizeof(*pVideoIDRInterval));
	if (ret != OMX_ErrorNone) 
        {
	    LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
	    return ret;
	}

	LOCK_CONFIG
	RtCode aret;
	aret = encoder->getConfig(&config);
	assert(aret == SUCCESS);
	    
	config.iDRInterval = pVideoIDRInterval->nIDRPeriod;
	config.intraInterval = pVideoIDRInterval->nPFrames;

	aret = encoder->setConfig(config);
	assert(aret == SUCCESS);
	UNLOCK_CONFIG

        LOGV("%s() : OMX_IndexConfigVideoAVCIntraPeriod : avcEncIDRPeriod = %d avcEncPFrames = %d",
                       __func__, config.iDRInterval, config.intraInterval);

	break;
     }

    default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* Get/SetConfig */
/* Get/SetConfig */
OMX_ERRORTYPE OMXAVCComponent::ComponentGetConfig(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    OMX_CONFIG_INTRAREFRESHVOPTYPE* pVideoIFrame;
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval;

    RtCode aret;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s() : nIndex = %d\n", __func__, nIndex);

     switch (nIndex)
     {
        case OMX_IndexConfigVideoAVCIntraPeriod:
        {
            pVideoIDRInterval = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentConfigStructure;
            if(!pVideoIDRInterval) {
                LOGE("NULL pointer");
                return OMX_ErrorBadParameter;
            }

            ret = CheckTypeHeader(pVideoIDRInterval, sizeof(*pVideoIDRInterval));
            if (ret != OMX_ErrorNone) {
                 LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
                 return ret;
            }
            if (pVideoIDRInterval->nPortIndex != OUTPORT_INDEX)
            {
                LOGV("Wrong port index");
                return OMX_ErrorBadPortIndex;
            }
            OMX_U32 index = pVideoIDRInterval->nPortIndex;
            PortAvc *port = static_cast<PortAvc *>(ports[index]);

	    LOCK_CONFIG
            RtCode aret;
	    aret = encoder->getConfig(&config);
	    assert(aret == SUCCESS);

            pVideoIDRInterval->nIDRPeriod = config.iDRInterval;
            pVideoIDRInterval->nPFrames = config.intraInterval;
	    UNLOCK_CONFIG

            LOGV("OMX_IndexConfigVideoAVCIntraPeriod : nIDRPeriod = %d, nPFrames = %d", pVideoIDRInterval->nIDRPeriod, pVideoIDRInterval->nPFrames);

            SetTypeHeader(pVideoIDRInterval, sizeof(OMX_VIDEO_CONFIG_AVCINTRAPERIOD));
        }
        break;

        default:
        {
            return OMX_ErrorUnsupportedIndex;
        }
    }
    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMXAVCComponent::ComponentSetConfig(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    OMX_CONFIG_INTRAREFRESHVOPTYPE* pVideoIFrame;
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s() : nIndex = %d\n", __func__, nParamIndex);

    switch (nParamIndex)
    {
        case OMX_IndexConfigVideoIntraVOPRefresh:
        {
            pVideoIFrame = (OMX_CONFIG_INTRAREFRESHVOPTYPE*) pComponentConfigStructure;
            ret = CheckTypeHeader(pVideoIFrame, sizeof(*pVideoIFrame));
            if (ret != OMX_ErrorNone) {
                 LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
                 return ret;
            }
            if (pVideoIFrame->nPortIndex != OUTPORT_INDEX)
            {
                LOGV("Wrong port index");
                return OMX_ErrorBadPortIndex;
            }

            LOGV("OMX_IndexConfigVideoIntraVOPRefresh");
            if(pVideoIFrame->IntraRefreshVOP == OMX_TRUE) {
                 LOGV("pVideoIFrame->IntraRefreshVOP == OMX_TRUE");

		 LOCK_CONFIG
	         encoder->refreshIDR();
	         UNLOCK_CONFIG
            }
        }
        break;

        case OMX_IndexConfigVideoAVCIntraPeriod:
        {
            pVideoIDRInterval = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentConfigStructure;

            ret = CheckTypeHeader(pVideoIDRInterval, sizeof(*pVideoIDRInterval));
            if (ret != OMX_ErrorNone) {
                 LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
                 return ret;
            }

            if (pVideoIDRInterval->nPortIndex != OUTPORT_INDEX)
            {
                LOGV("Wrong port index");
                return OMX_ErrorBadPortIndex;
            }
            OMX_U32 index = pVideoIDRInterval->nPortIndex;
            PortAvc *port = static_cast<PortAvc *>(ports[index]);

            LOGV("OMX_IndexConfigVideoAVCIntraPeriod : nIDRPeriod = %d, nPFrames = %d", pVideoIDRInterval->nIDRPeriod, pVideoIDRInterval->nPFrames);
	  
    	    LOCK_CONFIG
	    RtCode aret;
	    aret = encoder->getConfig(&config);
	    assert(aret == SUCCESS);
	    
	    config.iDRInterval = pVideoIDRInterval->nIDRPeriod;
	    config.intraInterval = pVideoIDRInterval->nPFrames;

	    aret = encoder->setDynamicConfig(config);
	    assert(aret == SUCCESS);
	    UNLOCK_CONFIG
        }
        break;

        default:
        {
            return OMX_ErrorUnsupportedIndex;
        }
    }
    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return OMX_ErrorNone;
}


/* end of component methods & helpers */

/*
 * implement ComponentBase::Processor[*]
 */
#define DUMP_VALUE(v) LOGE("%s = %d (0x%x)", #v, (unsigned int)(v),(unsigned int)(v))
OMX_ERRORTYPE OMXAVCComponent::ProcessorInit(void)
{
    RtCode aret = SUCCESS;
    OMX_ERRORTYPE oret = OMX_ErrorNone;

    OMX_U32 port_index = (OMX_U32)-1;

    LOGV("%s(): enter\n", __func__);

    encoder = new AVCEncoder();
    assert(encoder!=NULL);

    LOCK_CONFIG
    aret = encoder->getConfig(&config);
    assert(aret==SUCCESS);

    port_index = OUTPORT_INDEX;

    oret = ChangeAVCEncoderConfigWithPortParam(&config,
		    static_cast<PortVideo*>(ports[port_index])
		    );

	    
    aret = encoder->setConfig(config);
    assert(aret==SUCCESS);

    aret = encoder->getConfig(&config);
    assert(aret==SUCCESS);

    LOGV("Effective AVCEncoder Configuration ************");
    DUMP_VALUE(config.frameWidth);
    DUMP_VALUE(config.frameHeight);
    DUMP_VALUE(config.frameBitrate);
    DUMP_VALUE(config.intraInterval);
    DUMP_VALUE(config.iDRInterval);
    DUMP_VALUE(config.initialQp);
    DUMP_VALUE(config.minimalQp);
    DUMP_VALUE(config.rateControl);
    DUMP_VALUE(config.naluFormat);
    DUMP_VALUE(config.levelIDC);
    DUMP_VALUE(config.bShareBufferMode);
    DUMP_VALUE(config.nSlice);

    UNLOCK_CONFIG

    inframe_counter = 0;
    outframe_counter = 0;

    last_ts = 0;
    last_fps = 0.0;

    avc_enc_frame_size_left = 0;

    avc_enc_buffer = NULL;
    avc_enc_buffer_length = 0;
    avc_enc_buffer_offset = 0;

#ifdef INFODUMP
    libinfodump_init();
    counter = 0;
#endif
    
    assert(temp_coded_data_buffer == NULL);
    temp_coded_data_buffer_size = config.frameWidth *
	    config.frameHeight * 2;		//should be enough?
    temp_coded_data_buffer = new OMX_U8 [temp_coded_data_buffer_size];

    encoder->setObserver(this);
    encoder->enableTimingStatistics();
		
    aret = encoder->init();
    assert(aret==SUCCESS);

    encoder->resetStatistics(NULL);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, oret);
    return oret;
}

OMX_ERRORTYPE OMXAVCComponent::ProcessorDeinit(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    RtCode aret;

    LOGV("%s(): enter\n", __func__);

    //----------------------------------------------
    encoder->resetStatistics(&statistics);
    LOGV("Codec Statistics ********************************************");
		
    LOGV("time eclapsed = %f (ms)", statistics.timeEclapsed);		
    LOGV("#frames [encoded = %d|skipped = %d|total = %d]", statistics.frameEncoded,
				statistics.frameSkipped,
				statistics.frameTotal);
    LOGV("#I frames (%d IDR) = %d | #P frames = %d | #B frames %d", statistics.frameI + statistics.frameIDR,
				statistics.frameIDR,
				statistics.frameP,
				statistics.frameB);
    LOGV("average encoding time = %f (ms) - fps = %f", statistics.averEncode, 1000/statistics.averEncode);
    LOGV("average frame size = %f (Bytes) - bitrate = %f(Kbps)", statistics.averFrameSize, 
				statistics.averFrameSize * config.frameRate * 8 / 1024);
    LOGV("libva related details :");
    LOGV("average surface load time = %f (ms)", statistics.averSurfaceLoad);
    LOGV("average surface copy output time = %f (ms)", statistics.averCopyOutput);
    LOGV("average surface sync time = %f (ms)", statistics.averSurfaceSync);
    LOGV("average pure encoding time = %f (ms) - fps = %f", statistics.averEncode - 
				statistics.averSurfaceLoad,
				1000/(statistics.averEncode - statistics.averSurfaceLoad));
    //----------------------------------------------
    
    aret = encoder->deinit();
    assert(aret==SUCCESS);

    delete encoder;

    if (temp_coded_data_buffer != NULL) {
	    delete [] temp_coded_data_buffer;
    };

#ifdef INFODUMP
    libinfodump_destroy();
#endif

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXAVCComponent::ProcessorStart(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXAVCComponent::ProcessorStop(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    ports[INPORT_INDEX]->ReturnAllRetainedBuffers();

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXAVCComponent::ProcessorPause(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXAVCComponent::ProcessorResume(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* implement ComponentBase::ProcessorProcess */
OMX_ERRORTYPE OMXAVCComponent::ProcessorProcess(
    OMX_BUFFERHEADERTYPE **buffers,
    buffer_retain_t *retain,
    OMX_U32 nr_buffers)
{
    OMX_U32 outfilledlen = 0;
    OMX_S64 outtimestamp = 0;
    OMX_U32 outflags = 0;

    OMX_ERRORTYPE oret = OMX_ErrorNone;
    RtCode aret = SUCCESS;

    //intput buffer/output buffer
    void* buffer_in;
    int buffer_in_datasz;
    int buffer_in_buffersz;
    void* buffer_out;
    int buffer_out_datasz;
    int buffer_out_buffersz;

    AvcNaluType nalu_type;  
    OMX_U8 *nal,*nal_pps;
    OMX_U32 nal_len,nal_len_pps;

    LOGV("%s(): enter\n", __func__);

    LOGV("Dump Buffer Param (ProcessorInit) ***************************");
    LOGV("Input Buffer = %p, alloc = %d, offset = %d, filled = %d", 
		    buffers[INPORT_INDEX]->pBuffer,
		    buffers[INPORT_INDEX]->nAllocLen,
		    buffers[INPORT_INDEX]->nOffset,
		    buffers[INPORT_INDEX]->nFilledLen);    
    LOGV("Output Buffer = %p, alloc = %d, offset = %d, filled = %d", 
		    buffers[OUTPORT_INDEX]->pBuffer,
		    buffers[OUTPORT_INDEX]->nAllocLen,
		    buffers[OUTPORT_INDEX]->nOffset,
		    buffers[OUTPORT_INDEX]->nFilledLen);

    LOGV_IF(buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_EOS,
            "%s(),%d: got OMX_BUFFERFLAG_EOS\n", __func__, __LINE__);

    if (!buffers[INPORT_INDEX]->nFilledLen) {
        LOGV("%s(),%d: input buffer's nFilledLen is zero\n",
             __func__, __LINE__);
        goto out;
    }

    buffer_in =
        buffers[INPORT_INDEX]->pBuffer + buffers[INPORT_INDEX]->nOffset;

    buffer_in_datasz = buffers[INPORT_INDEX]->nFilledLen;
    buffer_in_buffersz = buffers[INPORT_INDEX]->nAllocLen - buffers[INPORT_INDEX]->nOffset;

    buffer_out =
        buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset;
    buffer_out_datasz = 0;
    buffer_out_buffersz = buffers[OUTPORT_INDEX]->nAllocLen - buffers[OUTPORT_INDEX]->nOffset;

    LOGV("INPUT TIMESTAMP = %ld", buffers[INPORT_INDEX]->nTimeStamp);


encode:
    assert(p->iOMXComponentUsesNALStartCodes == OMX_FALSE);
    assert(p->iOMXComponentUsesFullAVCFrames == OMX_FALSE);

    switch (avcEncNaluFormatType) {
         case OMX_NaluFormatStartCodes:
 
             if (avc_enc_frame_size_left == 0)
             {
		  in.len = static_cast<int>(buffer_in_datasz);	//caution here
		  out.len = static_cast<int>(temp_coded_data_buffer_size);	//caution here
		  in.buf = (unsigned char*)(buffer_in);
		  out.buf = (unsigned char*)(temp_coded_data_buffer);
			    
		  in.timestamp = buffers[INPORT_INDEX]->nTimeStamp;
		  in.bCompressed = false;

                  LOCK_CONFIG
                  aret = encoder->encode(&in, &out);
                  assert(aret==SUCCESS);
                  UNLOCK_CONFIG

                  last_out_timestamp = static_cast<OMX_S64>(out.timestamp);
                  last_out_frametype = out.frameType;

                  if (out.len == 0)
                  {
			retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
			retain[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;
			goto out;
                  }

    		 avc_enc_frame_size_left = out.len;
		 avc_enc_buffer = temp_coded_data_buffer;
		 avc_enc_buffer_offset = 0;
		 avc_enc_buffer_length = out.len;			 

		 startcode_type = DetectStartCodeType(avc_enc_buffer, avc_enc_buffer_length);
		 if (startcode_type != NAL_STARTCODE_4_BYTE)
                 {
			LOGE("invalid start code type");
			assert(0);
		 }

	     }

	    SplitNalByStartCode(avc_enc_buffer, avc_enc_buffer_length,  &nal, &nal_len, startcode_type);

            if(nal == NULL)	
            {
		  LOGE("nal is NULL");
	          assert(0);
            }

            avc_enc_frame_size_left -= nal_len;
            avc_enc_buffer += nal_len;
            avc_enc_buffer_length -= nal_len;
            avc_enc_buffer_offset += nal_len;
			    
            nalu_type = GetNaluType(&nal[0], startcode_type);

            if(nalu_type == SPS) //get pps 
            {
                SplitNalByStartCode(avc_enc_buffer, avc_enc_buffer_length,
				&nal_pps, &nal_len_pps, startcode_type);
				
            	nalu_type = GetNaluType(&nal_pps[0], startcode_type);

            	if(nalu_type != PPS)
            	{
                    LOGE("SPS and PPS not located togeter!");
		    assert(0);
            	}
				
            	avc_enc_frame_size_left -= nal_len_pps;
            	avc_enc_buffer += nal_len_pps;
            	avc_enc_buffer_length -= nal_len_pps;
            	avc_enc_buffer_offset += nal_len_pps;
				
            	outflags |= OMX_BUFFERFLAG_CODECCONFIG;
            	outfilledlen = nal_len + nal_len_pps;	

            }else
            {
                 outfilledlen = nal_len;
            }
    
               
	    memcpy(buffer_out, &nal[0], outfilledlen);	//start codes are not sent
			   		
            outtimestamp = last_out_timestamp;

            if (outfilledlen > 0)
            {
		    outflags |= OMX_BUFFERFLAG_ENDOFFRAME;				   
			  
		    if (last_out_frametype == I_FRAME || last_out_frametype == IDR_FRAME)
		    {
			outflags |= OMX_BUFFERFLAG_SYNCFRAME;
		    }

		     retain[OUTPORT_INDEX] = BUFFER_RETAIN_NOT_RETAIN;
	     } else 
	     {
		     retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
	     }

	     if (avc_enc_frame_size_left == 0)
	     {
			    //FIXME: CAUTION: libavce call back will 'return all retained buffer one time', pay attention to this
		     retain[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;
	      }else
	      {
		     retain[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
	      }
	   
	      break;
	    case OMX_NaluFormatFourByteInterleaveLength:
		    LOGV("FourByteInterleaveLength is not supported currently");
		    assert(0);
		    break;
	    case OMX_NaluFormatZeroByteInterleaveLength:
		    //load new coded data
		    if (avc_enc_frame_size_left == 0) {
			    in.len = static_cast<int>(buffer_in_datasz);	//caution here
			    out.len = static_cast<int>(temp_coded_data_buffer_size);	//caution here
			    in.buf = (unsigned char*)(buffer_in);
			    out.buf = (unsigned char*)(temp_coded_data_buffer);
			    
			    in.timestamp = buffers[INPORT_INDEX]->nTimeStamp;
			    in.bCompressed = false;

			    LOCK_CONFIG

		    	    aret = encoder->encode(&in, &out);
		    	    assert(aret==SUCCESS);

			    UNLOCK_CONFIG

		    	    last_out_timestamp = static_cast<OMX_S64>(out.timestamp);
			    last_out_frametype = out.frameType;

			    if (out.len == 0) {
				    retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
				    retain[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;
				    goto out;
			    };

    		            avc_enc_frame_size_left = out.len;
			    avc_enc_buffer = temp_coded_data_buffer;
			    avc_enc_buffer_offset = 0;
			    avc_enc_buffer_length = out.len;			 

			    startcode_type = DetectStartCodeType(avc_enc_buffer, avc_enc_buffer_length);
			    if (startcode_type == NAL_STARTCODE_INVALID) {
				    LOGV("invalid start code type");
				    assert(0);
			    };
/*
#ifdef INFODUMP
			    libinfodump_recordbuffer(counter++, 
					    avc_enc_buffer,
					    avc_enc_buffer_length
					    );
#endif
*/
		    };


		    SplitNalByStartCode(avc_enc_buffer, avc_enc_buffer_length,
				    &nal, &nal_len, startcode_type);

		    //DUMP_BUFFER("avc_encoder", avc_enc_buffer, avc_enc_buffer_length);
		    //DUMP_BUFFER("nal", nal, nal_len);

		    if (nal != NULL) {
			    if (startcode_type == NAL_STARTCODE_3_BYTE) {
				    assert(nal_len > 3);	//at least should be a start code len
			    } else {
				    assert(nal_len > 4);
			    };
			    	
			    avc_enc_frame_size_left -= nal_len;
			    avc_enc_buffer += nal_len;
			    avc_enc_buffer_length -= nal_len;
			    avc_enc_buffer_offset += nal_len;
			    
			    LOGV("buffer_out = %p, nal = %p, nal_len = %d", 
					    buffer_out,
					    &nal[0],
					    nal_len - 0);

			    if (startcode_type == NAL_STARTCODE_3_BYTE) {
			    	memcpy(buffer_out, &nal[3], nal_len - 3);	//start codes are not sent
			    	outfilledlen = nal_len - 3;
			    } else {
			    	memcpy(buffer_out, &nal[4], nal_len - 4);	//start codes are not sent
			    	outfilledlen = nal_len - 4;
			    };
			    outtimestamp = last_out_timestamp;

			    if (outfilledlen > 0) {
				    outflags |= OMX_BUFFERFLAG_ENDOFFRAME;	

				     nalu_type = GetNaluType(&nal[0], startcode_type);
				    
				    if (nalu_type == SPS || nalu_type == PPS) {
					    outflags |= OMX_BUFFERFLAG_CODECCONFIG;
				    };

				    /* dump nalu type & len */
				    switch (nalu_type) {
					    case SPS:
						    LOGV("Nalu : SPS, len = %d", nal_len);
						    break;
					    case PPS:
						    LOGV("Nalu : PPS, len = %d", nal_len);
						    break;
					    case CODED_SLICE_NON_IDR:
						    LOGV("Nalu : None IDR Slice, len = %d", nal_len);
						    break;
				       	    case CODED_SLICE_IDR:
						    LOGV("Nalu : IDR Slice, len = %d", nal_len);
						    break;
				    	    default:
					    	    LOGV("Nalu : Other (%d), len = %d", nalu_type, nal_len);
					    	    break;
			    	    }

				    if (last_out_frametype == I_FRAME ||
						    last_out_frametype == IDR_FRAME) {
					    outflags |= OMX_BUFFERFLAG_SYNCFRAME;
				    };


			    	    retain[OUTPORT_INDEX] = BUFFER_RETAIN_NOT_RETAIN;
		    	    } else {
			    	    retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
		    	    }


			    if (avc_enc_frame_size_left == 0) {		//FIXME: CAUTION: libavce call back will 'return all retained buffer one time', pay attention to this
				    retain[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;
			    } else {
				    retain[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
			    };

		    } else {
			    LOGV("[%s:%s:%d]", __FILE__, __func__, __LINE__);
			    assert(0);
		    };
		    break;
	    default:
		    LOGV("[%s:%s:%d] Unsupported Nalu format.",
				    __FILE__, __func__,
				    __LINE__);
		    assert(0);
		    break;
    }	    

out:
    if(retain[OUTPORT_INDEX] != BUFFER_RETAIN_GETAGAIN) {
        buffers[OUTPORT_INDEX]->nFilledLen = outfilledlen;
        buffers[OUTPORT_INDEX]->nTimeStamp = outtimestamp;
        buffers[OUTPORT_INDEX]->nFlags = outflags;
    
	LOGV("[GENERATE] output buffer (len = %d, timestamp = %ld, flag = %x", 
		    outfilledlen,
		    outtimestamp,
		    outflags);
    }

    if (retain[INPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN ||
        retain[INPORT_INDEX] == BUFFER_RETAIN_ACCUMULATE ) {
        inframe_counter++;
    }

    if (retain[OUTPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN)
        outframe_counter++;

    LOGV_IF(oret == OMX_ErrorNone,
            "%s(),%d: exit, done\n", __func__, __LINE__);

    return oret;
}


OMX_ERRORTYPE OMXAVCComponent::ChangeAVCEncoderConfigWithPortParam(
		CodecConfig* pconfig,
		PortVideo *port) 
{
    
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_VIDEO_CONTROLRATETYPE controlrate;

    const OMX_PARAM_PORTDEFINITIONTYPE *pd = port->GetPortDefinition();
    const OMX_VIDEO_PARAM_BITRATETYPE *bitrate =
            port->GetPortBitrateParam();

#ifdef COMPONENT_SUPPORT_OPENCORE
    pconfig->frameRate = static_cast<int>((pd->format.video.xFramerate >> 16));
#else   // stagefright set framerate in inputPort.hack code here

    PortVideo *input_port = static_cast<PortVideo*>(ports[INPORT_INDEX]);   
    const OMX_PARAM_PORTDEFINITIONTYPE *input_pd = input_port->GetPortDefinition();
    pconfig->frameRate = static_cast<int>((input_pd->format.video.xFramerate >> 16)); 
#endif

    pconfig->frameWidth = static_cast<int>(pd->format.video.nFrameWidth);
    pconfig->frameHeight = static_cast<int>(pd->format.video.nFrameHeight);
        
    pconfig->intraInterval = static_cast<int>(pconfig->frameRate / 2);	//just a init value
    pconfig->iDRInterval = -1;						//for opencore, will only accept -1 as iDRInterval

    pconfig->frameBitrate = bitrate->nTargetBitrate;
    
    if ((bitrate->eControlRate == OMX_Video_ControlRateVariable) ||
		    (bitrate->eControlRate ==
		     OMX_Video_ControlRateVariableSkipFrames)) {
	    pconfig->rateControl = RC_VBR;
    } else if ((bitrate->eControlRate ==
                      OMX_Video_ControlRateConstant) ||
                     (bitrate->eControlRate ==
                      OMX_Video_ControlRateConstantSkipFrames)) {
	    pconfig->rateControl = RC_CBR;
    } else {
	    pconfig->rateControl = RC_NONE;
    };

    pconfig->nSlice = 1;	//just hard coded for init value, could be adjusted later
    pconfig->initialQp = 24;
    pconfig->minimalQp = 1;

    pconfig->rateControl = RC_VBR;	//hard code as VBR

    const OMX_BOOL *isbuffersharing = port->GetPortBufferSharingInfo();
    const OMX_VIDEO_CONFIG_PRI_INFOTYPE *privateinfoparam = port->GetPortPrivateInfoParam();

    temp_port_private_info = NULL;
    if(*isbuffersharing == OMX_TRUE) {
	    LOGV("Enable buffer sharing mode");
	    if(privateinfoparam->nHolder != NULL) {
                int size = static_cast<int>(privateinfoparam->nCapacity);
                unsigned long* p = static_cast<unsigned long*>(privateinfoparam->nHolder);

                int i = 0;
                for(i = 0; i < size; i++)
		{
                    LOGV("@@@@@ nCapacity = %d, camera frame id array[%d] = 0x%08x @@@@@", size, i, p[i]);
		}
		
		temp_port_private_info = const_cast<OMX_VIDEO_CONFIG_PRI_INFOTYPE*>(privateinfoparam);

		pconfig->bShareBufferMode = true;
		pconfig->nShareBufferCount = size;
		pconfig->pShareBufferArray = p;

                //FIXME: CAUTION: free(privateinfoparam->nHolder);  ->  carried out in AVCEncoder's handleConfigChange() callback

            }
        } else {
	    	LOGV("Disable buffer sharing mode");
		pconfig->bShareBufferMode = false;
        }
    return ret;
}

OMX_ERRORTYPE OMXAVCComponent::ProcessorFlush(OMX_U32 port_index) {

	LOGV("port_index = %d Flushed!\n", port_index);
	
	if (port_index == INPORT_INDEX || port_index == OMX_ALL) {
                ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
	}

        return OMX_ErrorNone;
}

void OMXAVCComponent::handleEncoderCycleEnded(VASurfaceID surface,
					bool b_generated,
					bool b_share_buffer)
{
	LOGV("surface id =%x is released by encoder", surface);
	ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
};

void OMXAVCComponent::handleConfigChange(bool b_dynamic) {

	if (b_dynamic == false) {
		LOGV("avc encoder static config changed");

		if (temp_port_private_info != NULL) {
			free(temp_port_private_info->nHolder);
		}
	} else {
		LOGV("avc encoder dynamic config changed");
	};
};


NalStartCodeType OMXAVCComponent::DetectStartCodeType(OMX_U8* buf, OMX_U32 len) {
	OMX_U8* data = buf;
	OMX_U32 ofst = 0;

	while (ofst < len) {
		if (data[0] != 0x00) {
			break;
		}
		ofst ++;
		data ++;
	};
	
	if (data[0] == 0x01) {
		if (ofst == 2) {
			LOGV("3 byte len start code");
			return NAL_STARTCODE_3_BYTE;
		} else if (ofst == 3) {
			LOGV("4 byte len start code");
			return NAL_STARTCODE_4_BYTE;
		}
	};

	LOGV("invalid start code len");
	return NAL_STARTCODE_INVALID;
};

OMX_ERRORTYPE OMXAVCComponent::SplitNalByStartCode(OMX_U8* buf, OMX_U32 len,
		    OMX_U8** nalbuf, OMX_U32* nallen, 
		    NalStartCodeType startcode_type)
{
	if (buf == NULL || len == 0 ||
			nalbuf == NULL || nallen == NULL) {
		return OMX_ErrorBadParameter;
	}; 

	*nalbuf = NULL;
	*nallen = 0;

	OMX_U32 ofst = 0;
	OMX_U8 *data = buf;
	OMX_U8 *next_nalbuf;

	if (startcode_type == NAL_STARTCODE_3_BYTE) {
		while(ofst < len - 2) {
			if ( data[0] ==0x00 &&
					data[1] == 0x00 &&
					data[2] == 0x01 ) {
				*nalbuf = data;
				break;
			}
			data ++;
			ofst ++;
		};

		if (*nalbuf == NULL) {
			return OMX_ErrorNone;
		};

		data += 3;
		ofst += 3;
	
		next_nalbuf = NULL;

		while(ofst < len - 2) {
			if (data[0] == 0x00 &&
					data[1] == 0x00 &&
					data[2] == 0x01 ) {
				next_nalbuf = data;
				break;
			}
			data ++;
			ofst ++;
		}
	} else if (startcode_type == NAL_STARTCODE_4_BYTE) {
		while(ofst < len - 3) {
			if ( data[0] ==0x00 &&
					data[1] == 0x00 &&
					data[2] == 0x00 &&
					data[3] == 0x01) {
				*nalbuf = data;
				break;
			}
			data ++;
			ofst ++;
		};

		if (*nalbuf == NULL) {
			return OMX_ErrorNone;
		};

		data += 4;
		ofst += 4;
	
		next_nalbuf = NULL;

		while(ofst < len - 3) {
			if (data[0] == 0x00 &&
					data[1] == 0x00 &&
					data[2] == 0x00 &&
					data[3] == 0x01) {
				next_nalbuf = data;
				break;
			}
			data ++;
			ofst ++;
		}

	} else {
		return OMX_ErrorBadParameter;
	};

	if (next_nalbuf != NULL) {
		*nallen = next_nalbuf - *nalbuf;
	} else {
		*nallen = &buf[len - 1] - *nalbuf + 1;
	};

	return OMX_ErrorNone;
};

AvcNaluType OMXAVCComponent::GetNaluType(OMX_U8* nal,
		NalStartCodeType startcode_type)
{
	if (startcode_type == NAL_STARTCODE_3_BYTE) {
		return static_cast<AvcNaluType>(nal[3] & 0x1F);
	} else if (startcode_type == NAL_STARTCODE_4_BYTE) {
		return static_cast<AvcNaluType>(nal[4] & 0x1F);
	} else {
		return INVALID_NAL_TYPE;
	};
}

/*
 * CModule Interface
 */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *g_name = (const char *)"OMX.Intel.Mrst.PSB.ve.avc.libva";

static const char *g_roles[] =
{
    (const char *)"video_encoder.avc",
};

OMX_ERRORTYPE wrs_omxil_cmodule_ops_instantiate(OMX_PTR *instance)
{
    ComponentBase *cbase;

    cbase = new OMXAVCComponent;
    if (!cbase) {
        *instance = NULL;
        return OMX_ErrorInsufficientResources;
    }

    *instance = cbase;
    return OMX_ErrorNone;
}

struct wrs_omxil_cmodule_ops_s cmodule_ops = {
    instantiate: wrs_omxil_cmodule_ops_instantiate,
};

struct wrs_omxil_cmodule_s WRS_OMXIL_CMODULE_SYMBOL = {
    name: g_name,
    roles: &g_roles[0],
    nr_roles: ARRAY_SIZE(g_roles),
    ops: &cmodule_ops,
};


