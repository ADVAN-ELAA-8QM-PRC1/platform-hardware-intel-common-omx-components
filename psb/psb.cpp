/*
 * Copyright (c) 2009-2010 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify, or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <OMX_Core.h>

#include <h264_parser.h>

#include <cmodule.h>
#include <portvideo.h>
#include <componentbase.h>

#include <pv_omxcore.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <mixdisplayx11.h>
#include <mixvideo.h>
#include <mixvideoformat_h264.h>
#include <mixvideoconfigparamsdec_h264.h>

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "psb.h"

//#define LOG_NDEBUG 0

#define LOG_TAG "mrst_psb"
#include <log.h>


#define PV_FULL_AVC_FRAME_MODE 0

#if PV_FULL_AVC_FRAME_MODE
 #define _MAX_NAL_PER_FRAME 100
#else
 #define _MAX_NAL_PER_FRAME 1
#endif

#define USE_G_CHIPSET_OVERLAY 0

/*
 * constructor & destructor
 */
MrstPsbComponent::MrstPsbComponent()
{
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit\n", __func__, __LINE__);
}

MrstPsbComponent::~MrstPsbComponent()
{
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit\n", __func__, __LINE__);
}

/* end of constructor & destructor */

/* core methods & helpers */
OMX_ERRORTYPE MrstPsbComponent::ComponentAllocatePorts(void)
{
    PortBase **ports;

    OMX_U32 codec_port_index, raw_port_index;
    OMX_DIRTYPE codec_port_dir, raw_port_dir;

    OMX_PORT_PARAM_TYPE portparam;

    const char *working_role;

    bool isencoder;

    OMX_ERRORTYPE ret = OMX_ErrorUndefined;

    LOGV("%s(): enter\n", __func__);

    ports = new PortBase *[NR_PORTS];
    if (!ports)
        return OMX_ErrorInsufficientResources;

    this->nr_ports = NR_PORTS;
    this->ports = ports;

    /* video_[encoder/decoder].[avc/whatever] */
    working_role = GetWorkingRole();
    working_role = strpbrk(working_role, "_");

    if (!strncmp(working_role, "_encoder", strlen("_encoder")))
        isencoder = true;
    else
        isencoder = false;

    if (isencoder) {
        raw_port_index = INPORT_INDEX;
        codec_port_index = OUTPORT_INDEX;
        raw_port_dir = OMX_DirInput;
        codec_port_dir = OMX_DirOutput;
    }
    else {
        codec_port_index = INPORT_INDEX;
        raw_port_index = OUTPORT_INDEX;
        codec_port_dir = OMX_DirInput;
        raw_port_dir = OMX_DirOutput;
    }

    working_role = strpbrk(working_role, ".");
    if (!working_role)
        return OMX_ErrorUndefined;
    working_role++;

    if (!strcmp(working_role, "avc")) {
        ret = __AllocateAvcPort(codec_port_index, codec_port_dir);
        coding_type = OMX_VIDEO_CodingAVC;
    } else
        ret = OMX_ErrorUndefined;

    if (ret != OMX_ErrorNone)
        goto free_ports;

    ret = __AllocateRawPort(raw_port_index, raw_port_dir);
    if (ret != OMX_ErrorNone)
        goto free_codecport;

    codec_mode = isencoder ? MIX_CODEC_MODE_ENCODE : MIX_CODEC_MODE_DECODE;

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
    coding_type = OMX_VIDEO_CodingUnused;

    delete []ports;
    ports = NULL;

    this->ports = NULL;
    this->nr_ports = 0;

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::__AllocateAvcPort(OMX_U32 port_index,
                                                  OMX_DIRTYPE dir)
{
    PortAvc *avcport;

    OMX_PARAM_PORTDEFINITIONTYPE avcportdefinition;
    OMX_VIDEO_PARAM_AVCTYPE avcportparam;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortAvc;
    if (!ports[port_index]) {
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
    if (dir == OMX_DirInput) {
        avcportdefinition.nBufferCountActual = INPORT_AVC_ACTUAL_BUFFER_COUNT;
        avcportdefinition.nBufferCountMin = INPORT_AVC_MIN_BUFFER_COUNT;
        avcportdefinition.nBufferSize =
            (INPORT_AVC_BUFFER_SIZE * _MAX_NAL_PER_FRAME);
    }
    else {
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
    avcportparam.eLevel = OMX_VIDEO_AVCLevel1;

    avcport->SetPortAvcParam(&avcportparam, true);
    /* end of OMX_VIDEO_PARAM_AVCTYPE */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::__AllocateRawPort(OMX_U32 port_index,
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
    rawportdefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    rawportdefinition.format.video.pNativeWindow = NULL;
    rawportdefinition.bBuffersContiguous = OMX_FALSE;
    rawportdefinition.nBufferAlignment = 0;

    rawport->SetPortDefinition(&rawportdefinition, true);

    /* end of OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_VIDEO_PARAM_PORTFORMATTYPE */
    rawvideoparam.nPortIndex = port_index;
    rawvideoparam.nIndex = 0;
    rawvideoparam.eCompressionFormat = OMX_VIDEO_CodingUnused;
    rawvideoparam.eColorFormat = OMX_COLOR_FormatYUV420Planar;

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
OMX_ERRORTYPE MrstPsbComponent::ComponentGetParameter(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter (index = 0x%08x)\n", __func__, nParamIndex);

    switch (nParamIndex) {
    case OMX_IndexParamVideoPortFormat: {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *p =
            (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortVideoParam(), sizeof(*p));
        break;
    }
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *p =
            (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAvc *port = NULL;

        if (strcmp(GetWorkingRole(), "video_decoder.avc")) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorUnsupportedIndex);
            return OMX_ErrorUnsupportedIndex;
        }

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAvc *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortAvcParam(), sizeof(*p));
        break;
    }
    case (OMX_INDEXTYPE) PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX: {
        PV_OMXComponentCapabilityFlagsType *p =
            (PV_OMXComponentCapabilityFlagsType *)pComponentParameterStructure;

        p->iIsOMXComponentMultiThreaded = OMX_TRUE;
        p->iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
        p->iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
#if PV_FULL_AVC_FRAME_MODE
        p->iOMXComponentSupportsMovableInputBuffers = OMX_FALSE;
        p->iOMXComponentUsesNALStartCodes = OMX_FALSE;
        p->iOMXComponentSupportsPartialFrames = OMX_FALSE;
        p->iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;
        p->iOMXComponentUsesFullAVCFrames = OMX_TRUE;
#else
        p->iOMXComponentSupportsMovableInputBuffers = OMX_TRUE;
        p->iOMXComponentUsesNALStartCodes = OMX_FALSE;
        p->iOMXComponentSupportsPartialFrames = OMX_TRUE;
        p->iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;
        p->iOMXComponentUsesFullAVCFrames = OMX_FALSE;
#endif
        break;
    }
    default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ComponentSetParameter(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter (index = 0x%08x)\n", __func__, nIndex);

    switch (nIndex) {
    case OMX_IndexParamVideoPortFormat: {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *p =
            (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
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
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *p =
            (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAvc *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAvc *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortAvcParam(p, false);
        break;
    }
    default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* Get/SetConfig */
OMX_ERRORTYPE MrstPsbComponent::ComponentGetConfig(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ComponentSetConfig(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* implement ComponentBase::Processor[*] */
OMX_ERRORTYPE MrstPsbComponent::ProcessorInit(void)
{
    MixVideo *mix = NULL;
    MixVideoInitParams *vip = NULL;
    MixParams *mvp = NULL;
    MixVideoConfigParams *vcp = NULL;
    MixDisplayX11 *display = NULL;
    MixVideoRenderParams *vrp = NULL;

    OMX_U32 port_index = (OMX_U32)-1;

    guint major, minor;

    OMX_ERRORTYPE oret = OMX_ErrorNone;
    MIX_RESULT mret;

    LOGV("%s(): enter\n", __func__);

    g_type_init();

    /*
     * common codes
     */
    mix = mix_video_new();
    if (!mix) {
        LOGE("%s(),%d: exit, mix_video_new failed", __func__, __LINE__);
        goto error_out;
    }

    mix_video_get_version(mix, &major, &minor);
    LOGV("MixVideo version: %d.%d", major, minor);

    /* decoder */
    if (codec_mode == MIX_CODEC_MODE_DECODE) {
        if (coding_type == OMX_VIDEO_CodingAVC)
            vcp = MIX_VIDEOCONFIGPARAMS(mix_videoconfigparamsdec_h264_new());

        mvp = MIX_PARAMS(mix_videodecodeparams_new());
        port_index = INPORT_INDEX;
    }

    if (!vcp || !mvp || (port_index == (OMX_U32)-1)) {
        LOGE("%s(),%d: exit, failed to allocate vcp, mvp, port_index\n",
             __func__, __LINE__);
        goto error_out;
    }

    oret = ChangeVcpWithPortParam(vcp, ports[port_index], NULL);
    if (oret != OMX_ErrorNone) {
        LOGE("%s(),%d: exit, ChangeVcpWithPortParam failed (ret == 0x%08x)\n",
             __func__, __LINE__, oret);
        goto error_out;
    }

    display = mix_displayx11_new();
    if (!display) {
        LOGE("%s(),%d: exit, mix_displayx11_new failed", __func__, __LINE__);
        goto error_out;
    }

    vip = mix_videoinitparams_new();
    if (!vip) {
        LOGE("%s(),%d: exit, mix_videoinitparams_new failed", __func__,
             __LINE__);
        goto error_out;
    }

    vrp = mix_videorenderparams_new();
    if (!vrp) {
        LOGE("%s(),%d: exit, mix_videorenderparams_new failed",
             __func__, __LINE__);
        goto error_out;
    }

    mret = mix_displayx11_set_drawable(display, 0xbcd);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_displayx11_set_drawable failed "
             "(ret == 0x%08x)", __func__, __LINE__, mret);
        goto error_out;
    }

    mret = mix_videoinitparams_set_display(vip, MIX_DISPLAY(display));
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_videoinitparams_set_display failed "
             "(ret == 0x%08x)", __func__, __LINE__, mret);
        goto error_out;
    }

    mret = mix_videorenderparams_set_display(vrp, MIX_DISPLAY(display));
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_videorenderparams_set_display "
             "(ret == 0x%08x)", __func__, __LINE__, mret);
        goto error_out;
    }

    mret = mix_video_initialize(mix, codec_mode, vip, NULL);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_video_initialize failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        goto error_out;
    }

    this->mix = mix;
    this->vip = vip;
    this->mvp = mvp;
    this->vrp = vrp;
    this->vcp = vcp;
    this->display = display;
    this->mixbuffer_in[0] = NULL;

    FrameCount = 0;

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, oret);
    return oret;

 error_out:
    mix_params_unref(mvp);
    mix_videorenderparams_unref(vrp);
    mix_videoconfigparams_unref(vcp);
    mix_displayx11_unref(display);
    mix_videoinitparams_unref(vip);
    mix_video_unref(mix);

    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorDeinit(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    mix_video_deinitialize(mix);

    mix_params_unref(mvp);
    mix_videorenderparams_unref(vrp);
    mix_videoconfigparams_unref(vcp);
    mix_displayx11_unref(display);
    mix_videoinitparams_unref(vip);
    mix_video_unref(mix);

    if (mixbuffer_in[0]) {
        mix_video_release_mixbuffer(mix, mixbuffer_in[0]);
        mixbuffer_in[0] = NULL;
    }

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorStart(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorStop(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorPause(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorResume(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* implement ComponentBase::ProcessorProcess */
OMX_ERRORTYPE MrstPsbComponent::ProcessorProcess(
    OMX_BUFFERHEADERTYPE **buffers,
    buffer_retain_t *retain,
    OMX_U32 nr_buffers)
{
    MixIOVec buffer_in, buffer_out;
    OMX_U32 outfilledlen = 0;
    OMX_S64 outtimestamp = 0;
    OMX_U32 outflags = 0;

    OMX_ERRORTYPE oret = OMX_ErrorNone;
    MIX_RESULT mret;

    LOGV("%s(): enter\n", __func__);

#if 0
    if (buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
        DumpBuffer(buffers[INPORT_INDEX], true);
    else
        DumpBuffer(buffers[INPORT_INDEX], false);
#endif

    LOGV_IF(buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_EOS,
            "%s(),%d: got OMX_BUFFERFLAG_EOS\n", __func__, __LINE__);

    if (!buffers[INPORT_INDEX]->nFilledLen) {
        LOGV("%s(),%d: input buffer's nFilledLen is zero\n",
             __func__, __LINE__);
        goto out;
    }

    buffer_in.data =
        buffers[INPORT_INDEX]->pBuffer + buffers[INPORT_INDEX]->nOffset;
    buffer_in.data_size = buffers[INPORT_INDEX]->nFilledLen;
    buffer_in.buffer_size = buffers[INPORT_INDEX]->nAllocLen;

    buffer_out.data =
        buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset;
    buffer_out.data_size = 0;
    buffer_out.buffer_size = buffers[OUTPORT_INDEX]->nAllocLen;
    mixiovec_out[0] = &buffer_out;

    /* only in case of decode mode */
    if ((buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) &&
        (buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_CODECCONFIG)) {
        if (coding_type == OMX_VIDEO_CodingAVC) {
            if (FrameCount == 0) {
                unsigned int width, height, stride, sliceheight;

                tBuff[0] = 0x00;
                tBuff[1] = 0x00;
                tBuff[2] = 0x00;
                tBuff[3] = 0x00;
                tBuff[4] = 0x03;
                tBuff[5] = 0x01;
                tBuff[6] = 0x00;
                tBuff[7] = 0x17;
                memcpy(tBuff+8, buffer_in.data, buffer_in.data_size);

                oret = ChangePortParamWithCodecData(buffer_in.data,
                                                    buffer_in.data_size,
                                                    ports);
                if (oret != OMX_ErrorNone) {
                    LOGE("%s(),%d: exit ChangePortParamWithCodecData failed "
                         "(ret:0x%08x)\n",
                         __func__, __LINE__, oret);
                    goto out;
                }

                oret = ChangeVcpWithPortParam(vcp,
                                              ports[INPORT_INDEX],
                                              NULL);
                if (oret != OMX_ErrorNone) {
                    LOGE("%s(),%d: exit ChangeVcpWithPortParam failed "
                         "(ret:0x%08x)\n", __func__, __LINE__, oret);
                    goto out;
                }

                oret = ChangeVrpWithPortParam(vrp,
                              static_cast<PortVideo *>(ports[INPORT_INDEX]));
                if (oret != OMX_ErrorNone) {
                    LOGE("%s(),%d: exit ChangeVrpWithPortParam failed "
                         "(ret:0x%08x)\n", __func__, __LINE__, oret);
                    goto out;
                }

                retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
                goto out;
            }
            else if (FrameCount == 1) {
                LOGV("%s,%d:\n", __func__, __LINE__);
                tBuff[31] = 0x01;
                tBuff[32] = 0x00;
                tBuff[33] = 0x04;
                memcpy(tBuff+8+23+3, buffer_in.data, buffer_in.data_size);
#if 0
                for(i = 0; i < 40; i+=8) {
                    LOGV("%02X %02X %02X %02X %02X %02X %02X %02X",
                         tBuff[i], tBuff[i+1], tBuff[i+2], tBuff[i+3],
                         tBuff[i+4], tBuff[i+5], tBuff[i+6], tBuff[i+7]);
                }
#endif

                buffer_in.data = tBuff;
                buffer_in.data_size = 38;

                mret = mix_videoconfigparamsdec_set_header(
                    MIX_VIDEOCONFIGPARAMSDEC(vcp), &buffer_in);
                if (mret != MIX_RESULT_SUCCESS) {
                    LOGE("%s(), %d: exit, "
                         "mix_videoconfigparamsdec_set_header failed "
                         "(ret:0x%08x)", __func__, __LINE__, mret);
                    oret = OMX_ErrorUndefined;
                    goto out;
                }
                mret = mix_video_configure(mix, vcp, NULL);
                if (mret != MIX_RESULT_SUCCESS) {
                    LOGE("%s(), %d: exit, mix_video_configure failed "
                         "(ret:0x%08x)", __func__, __LINE__, mret);
                    oret = OMX_ErrorUndefined;
                    goto out;
                }
                LOGV("%s(): mix video configured", __func__);

                /*
                 * port reconfigure
                 */
                ports[OUTPORT_INDEX]->ReportPortSettingsChanged();
                goto out;
            } /* FrameCount */
        } /* OMX_VIDEO_CodingAVC */
    } /* OMX_BUFFERFLAG_ENDOFFRAME && OMX_BUFFERFLAG_CODECCONFIG */

    /* get MixBuffer */
    mret = mix_video_get_mixbuffer(mix, &mixbuffer_in[0]);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d: exit, mix_video_get_mixbuffer failed (ret:0x%08x)",
                __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

    /* fill MixBuffer */
    mret = mix_buffer_set_data(mixbuffer_in[0],
                              buffer_in.data, buffer_in.data_size,
                              0, NULL);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d: exit, mix_buffer_set_data failed (ret:0x%08x)",
                __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

    if (codec_mode == MIX_CODEC_MODE_DECODE) {
        MixVideoFrame *frame;

        /* set timestamp */
        outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;
        mix_videodecodeparams_set_timestamp(MIX_VIDEODECODEPARAMS(mvp),
                                            outtimestamp);

    retry_decode:
        mret = mix_video_decode(mix,
                                mixbuffer_in, 1,
                                MIX_VIDEODECODEPARAMS(mvp));
        if (mret != MIX_RESULT_SUCCESS) {
            if (mret == MIX_RESULT_OUTOFSURFACES) {
                LOGV("%s(),%d: mix_video_decode() failed, "
                     "out of surfaces waits 10000 us and try again\n",
                     __func__, __LINE__);
                usleep(10000);
                goto retry_decode;
            }
            else if (mret == MIX_RESULT_DROPFRAME) {
                LOGE("%s(),%d: exit, mix_video_decode() failed, "
                     "frame dropped\n",
                     __func__, __LINE__);
                /* not an error */
                goto out;
            }
            else {
                LOGE("%s(),%d: exit, mix_video_decode() failed\n",
                     __func__, __LINE__);
                oret = OMX_ErrorUndefined;
                goto out;
            }
        }

        mret = mix_video_get_frame(mix, &frame);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(), %d mix_video_get_frame() failed (ret == 0x%08x)",
                 __func__, __LINE__, mret);
            oret = OMX_ErrorUndefined;
            goto out;
        }

#if !USE_G_CHIPSET_OVERLAY
        mret = mix_video_get_decoded_data(mix, mixiovec_out[0], vrp, frame);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(), %d: exit, mix_video_get_decoded_data failed "
                 "(ret == 0x%08x)\n", __func__, __LINE__, mret);
            oret = OMX_ErrorUndefined;
            goto release_frame;
        }

        outfilledlen = mixiovec_out[0]->data_size;
#else
 #error "the overlay feature needs implementing"
#endif

    release_frame:
        mret = mix_video_release_frame(mix, frame);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(), %d mix_video_release_frame() failed (ret == 0x%08x)",
                 __func__, __LINE__, mret);
            oret = OMX_ErrorUndefined;
            goto out;
        }

        outflags |= OMX_BUFFERFLAG_ENDOFFRAME;
    } /* MIX_CODEC_MODE_DECODE */

out:
    if (mixbuffer_in[0]) {
        mix_video_release_mixbuffer(mix, mixbuffer_in[0]);
        mixbuffer_in[0] = NULL;
    }

    buffers[OUTPORT_INDEX]->nFilledLen = outfilledlen;
    buffers[OUTPORT_INDEX]->nTimeStamp = outtimestamp;
    buffers[OUTPORT_INDEX]->nFlags = outflags;

    if ((oret == OMX_ErrorNone) &&
        (retain[INPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN))
        FrameCount++;

#if 0
    if (retain[OUTPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN)
        DumpBuffer(buffers[OUTPORT_INDEX], false);
#endif

    LOGV_IF(oret == OMX_ErrorNone,
            "%s(),%d: exit, done\n", __func__, __LINE__);
    return oret;
}

/* end of implement ComponentBase::Processor[*] */

/*
 * vcp setting helpers
 */

OMX_ERRORTYPE MrstPsbComponent::__AvcChangePortParamWithCodecData(
    const OMX_U8 *codec_data, OMX_U32 size, PortBase **ports)
{
    PortAvc *avcport = static_cast<PortAvc *>(ports[INPORT_INDEX]);
    PortVideo *rawport = static_cast<PortVideo *>(ports[OUTPORT_INDEX]);

    OMX_PARAM_PORTDEFINITIONTYPE avcpd, rawpd;

    unsigned int width, height, stride, sliceheight;

    int ret;

    ret = nal_sps_parse((OMX_U8 *)codec_data, size, &width, &height,
                        &stride, &sliceheight);
    if (ret != H264_STATUS_OK) {
        LOGE("%s(),%d: exit, nal_sps_parse failed (ret == 0x%08x)\n",
             __func__, __LINE__, ret);
        return OMX_ErrorBadParameter;
    }

    memcpy(&avcpd, avcport->GetPortDefinition(),
           sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

    if (avcpd.format.video.nFrameWidth != width) {
        LOGV("%s(): width : %lu != %d", __func__,
             avcpd.format.video.nFrameWidth, width);
        avcpd.format.video.nFrameWidth = width;
    }
    if (avcpd.format.video.nFrameHeight != height) {
        LOGV("%s(): height : %lu != %d", __func__,
             avcpd.format.video.nFrameHeight, height);
        avcpd.format.video.nFrameHeight = height;
    }
    if (avcpd.format.video.nStride != (OMX_S32)stride) {
        LOGV("%s(): stride : %lu != %d", __func__,
             avcpd.format.video.nStride, stride);
        avcpd.format.video.nStride = stride;
    }
    if (avcpd.format.video.nSliceHeight != sliceheight) {
        LOGV("%s(): sliceheight : %ld != %d", __func__,
             avcpd.format.video.nSliceHeight, sliceheight);
        avcpd.format.video.nSliceHeight = sliceheight;
    }

    memcpy(&rawpd, rawport->GetPortDefinition(),
           sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

    if (rawpd.format.video.nFrameWidth != width)
        rawpd.format.video.nFrameWidth = width;
    if (rawpd.format.video.nFrameHeight != height)
        rawpd.format.video.nFrameHeight = height;
    if (rawpd.format.video.nStride != (OMX_S32)stride)
        rawpd.format.video.nStride = stride;
    if (rawpd.format.video.nSliceHeight != sliceheight)
        rawpd.format.video.nSliceHeight = sliceheight;

    rawpd.nBufferSize = (stride * sliceheight * 3) >> 1;

    avcport->SetPortDefinition(&avcpd, true);
    rawport->SetPortDefinition(&rawpd, true);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::ChangePortParamWithCodecData(
    const OMX_U8 *codec_data,
    OMX_U32 size,
    PortBase **ports)
{
    OMX_ERRORTYPE ret = OMX_ErrorBadParameter;

    if (coding_type == OMX_VIDEO_CodingAVC) {
        ret = __AvcChangePortParamWithCodecData(codec_data, size, ports);
    }

    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::__AvcChangeVcpWithPortParam(
    MixVideoConfigParams *vcp, PortAvc *port, bool *vcp_changed)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (codec_mode == MIX_CODEC_MODE_DECODE) {
        MixVideoConfigParamsDec *config = MIX_VIDEOCONFIGPARAMSDEC(vcp);
        const OMX_PARAM_PORTDEFINITIONTYPE *pd = port->GetPortDefinition();

        if (config->frame_rate_num != (pd->format.video.xFramerate >> 16)) {
            LOGV("%s(): framerate : %u != %ld", __func__,
                 config->frame_rate_num, pd->format.video.xFramerate >> 16);
            mix_videoconfigparamsdec_set_frame_rate(config,
                                            pd->format.video.xFramerate >> 16,
                                            1);
            if (vcp_changed)
                *vcp_changed = true;
        }

        if ((config->picture_width != pd->format.video.nFrameWidth) ||
            (config->picture_height != pd->format.video.nFrameHeight)) {
            LOGV("%s(): width : %ld != %ld", __func__,
                 config->picture_width, pd->format.video.nFrameWidth);
            LOGV("%s(): height : %ld != %ld", __func__,
                 config->picture_height, pd->format.video.nFrameHeight);

            mix_videoconfigparamsdec_set_picture_res(config,
                                                pd->format.video.nFrameWidth,
                                                pd->format.video.nFrameHeight);
            if (vcp_changed)
                *vcp_changed = true;
        }

        /* common */

        /* mime type */
        mix_videoconfigparamsdec_set_mime_type(MIX_VIDEOCONFIGPARAMSDEC(vcp),
                                               "video/x-h264");
        /* fill discontinuity flag */
        mix_videodecodeparams_set_discontinuity(MIX_VIDEODECODEPARAMS(mvp),
                                                FALSE);
        /* set frame order mode */
        mix_videoconfigparamsdec_set_frame_order_mode(
            MIX_VIDEOCONFIGPARAMSDEC(vcp), MIX_FRAMEORDER_MODE_DECODEORDER);

        /* buffer pool size */
        mix_videoconfigparamsdec_set_buffer_pool_size(
            MIX_VIDEOCONFIGPARAMSDEC(vcp), 8);
        /* extra surface */
        mix_videoconfigparamsdec_set_extra_surface_allocation(
            MIX_VIDEOCONFIGPARAMSDEC(vcp), 4);
    }
    else {
        ret = OMX_ErrorBadParameter;
    }

    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ChangeVcpWithPortParam(
    MixVideoConfigParams *vcp,
    PortBase *port,
    bool *vcp_changed)
{
    OMX_ERRORTYPE ret;

    if (coding_type == OMX_VIDEO_CodingAVC)
        ret = __AvcChangeVcpWithPortParam(vcp,
                                          static_cast<PortAvc *>(port),
                                          vcp_changed);
    else
        ret = OMX_ErrorBadParameter;

    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ChangeVrpWithPortParam(
    MixVideoRenderParams *vrp,
    PortVideo *port)
{
    const OMX_PARAM_PORTDEFINITIONTYPE *pd = port->GetPortDefinition();
    MixRect src, dst;

    /* fill source, the video size */
    src.x = 0;
    src.y = 0;
    src.width = pd->format.video.nFrameWidth;
    src.height = pd->format.video.nFrameHeight;

    /* fill destination, the display size */
    dst.x = 0;
    dst.y = 0;
    dst.width = pd->format.video.nFrameWidth;
    dst.height = pd->format.video.nFrameHeight;

    mix_videorenderparams_set_src_rect(vrp, src);
    mix_videorenderparams_set_dest_rect(vrp, dst);
    mix_videorenderparams_set_clipping_rects(vrp, NULL, 0);

    return OMX_ErrorNone;
}

/* end of component methods & helpers */

/*
 * CModule Interface
 */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *g_name = (const char *)"OMX.Intel.Mrst.PSB";

static const char *g_roles[] =
{
    (const char *)"video_decoder.avc",
};

OMX_ERRORTYPE wrs_omxil_cmodule_ops_instantiate(OMX_PTR *instance)
{
    ComponentBase *cbase;

    cbase = new MrstPsbComponent;
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
