/*
 * Copyright (c) 2009 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify, or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

#ifndef __WRS_OMXIL_INTEL_MRST_PSB
#define __WRS_OMXIL_INTEL_MRST_PSB

#include <OMX_Core.h>
#include <OMX_Component.h>

#include <cmodule.h>
#include <portbase.h>
#include <componentbase.h>

//#define PV_FULL_AVC_FRAME_MODE

class MrstPsbComponent : public ComponentBase
{
public:
    /*
     * constructor & destructor
     */
    MrstPsbComponent();
    ~MrstPsbComponent();

private:
    /*
     * component methods & helpers
     */
    /* implement ComponentBase::ComponentAllocatePorts */
    virtual OMX_ERRORTYPE ComponentAllocatePorts(void);

    OMX_ERRORTYPE __AllocateAvcPort(OMX_U32 port_index, OMX_DIRTYPE dir);
    OMX_ERRORTYPE __AllocateRawPort(OMX_U32 port_index, OMX_DIRTYPE dir);

    /* implement ComponentBase::ComponentGet/SetPatameter */
    virtual OMX_ERRORTYPE
        ComponentGetParameter(OMX_INDEXTYPE nParamIndex,
                              OMX_PTR pComponentParameterStructure);
    virtual OMX_ERRORTYPE
        ComponentSetParameter(OMX_INDEXTYPE nIndex,
                              OMX_PTR pComponentParameterStructure);

    /* implement ComponentBase::ComponentGet/SetConfig */
    virtual OMX_ERRORTYPE
        ComponentGetConfig(OMX_INDEXTYPE nIndex,
                           OMX_PTR pComponentConfigStructure);
    virtual OMX_ERRORTYPE
        ComponentSetConfig(OMX_INDEXTYPE nIndex,
                           OMX_PTR pComponentConfigStructure);

    /* implement ComponentBase::Processor[*] */
    virtual OMX_ERRORTYPE ProcessorInit(void);  /* Loaded to Idle */
    virtual OMX_ERRORTYPE ProcessorDeinit(void);/* Idle to Loaded */
    virtual OMX_ERRORTYPE ProcessorStart(void); /* Idle to Executing/Pause */
    virtual OMX_ERRORTYPE ProcessorStop(void);  /* Executing/Pause to Idle */
    virtual OMX_ERRORTYPE ProcessorPause(void); /* Executing to Pause */
    virtual OMX_ERRORTYPE ProcessorResume(void);/* Pause to Executing */
    virtual void ProcessorProcess(OMX_BUFFERHEADERTYPE **buffers,
                                  buffer_retain_t *retain,
                                  OMX_U32 nr_buffers);

    /* end of component methods & helpers */

    OMX_VIDEO_CODINGTYPE coding_type;
    bool isencoder;

    /* mix video */
    MixVideo *mix_video;
    MixDrmParams *mix_drm;
    MixVideoInitParams *init_params;
    MixVideoDecodeParams *mix_decode_params;
    MixVideoConfigParamsDec *config_params;
    MixVideoRenderParams *mix_video_render_params;
    MixVideoConfigParamsDecH264 *configH264;
    MixDisplayX11 *mix_display_x11;

    MixIOVec *mixio;
    MixBuffer *mix_buffer;

    unsigned char tBuff[4096];
    int FrameCount;

    /* constant */
    /* ports */
    const static OMX_U32 NR_PORTS = 2;
    const static OMX_U32 INPORT_INDEX = 0;
    const static OMX_U32 OUTPORT_INDEX = 1;

    /* buffer */
    /* FIXME */
    const static OMX_U32 INPORT_AVC_ACTUAL_BUFFER_COUNT = 5;
    const static OMX_U32 INPORT_AVC_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_AVC_BUFFER_SIZE = 40960;
    const static OMX_U32 OUTPORT_RAW_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 OUTPORT_RAW_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_RAW_BUFFER_SIZE = 409600;
    const static OMX_U32 INPORT_RAW_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 INPORT_RAW_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_RAW_BUFFER_SIZE = 409600;
    const static OMX_U32 OUTPORT_AVC_ACTUAL_BUFFER_COUNT = 5;
    const static OMX_U32 OUTPORT_AVC_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_AVC_BUFFER_SIZE = 40960;
};

#endif /* __WRS_OMXIL_INTEL_MRST_PSB */
