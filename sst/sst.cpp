/*
 * Copyright (C) 2009 Wind River Systems
 *      Author: Keun-O Park <keun-o.park@windriver.com>
 *              Ho-Eun Ryu <ho-eun.ryu@windriver.com>
 *              Min-Su Kim <min-su.kim@windriver.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <OMX_Core.h>

#include <cmodule.h>
#include <portaudio.h>
#include <componentbase.h>

#include <pv_omxcore.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <mixaudio.h>
#include <mixacpaac.h>
#include <mixacpmp3.h>

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "sst.h"

#define LOG_NDEBUG 0

#define LOG_TAG "mrst_sst"
#include <log.h>

/*
 * constructor & destructor
 */
MrstSstComponent::MrstSstComponent()
{
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = void)\n", __func__, __LINE__);
}

MrstSstComponent::~MrstSstComponent()
{
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = void)\n", __func__, __LINE__);
}

/* end of constructor & destructor */

/* core methods & helpers */
OMX_ERRORTYPE MrstSstComponent::ComponentAllocatePorts(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorUndefined;

    LOGV("%s(): enter\n", __func__);

    if (!strcmp(GetWorkingRole(), "audio_decoder.mp3"))
        ret = __AllocateMp3RolePorts(false);
    else if(!strcmp(GetWorkingRole(), "audio_decoder.aac"))
        ret = __AllocateAacRolePorts(false);

    LOGV("%s(): exit (ret = 0x%08x)\n", __func__, ret);
    return ret;
}


OMX_ERRORTYPE MrstSstComponent::__AllocateMp3RolePorts(bool isencoder)
{
    PortBase **ports;

    OMX_U32 mp3_port_index, pcm_port_index;
    OMX_DIRTYPE mp3_port_dir, pcm_port_dir;

    OMX_PORT_PARAM_TYPE portparam;
    OMX_U32 i;
    OMX_ERRORTYPE ret;

    LOGV("%s(): enter\n", __func__);

    ports = new PortBase *[NR_PORTS];
    if (!ports)
        return OMX_ErrorInsufficientResources;
    this->nr_ports = NR_PORTS;
    this->ports = ports;

    if (isencoder) {
        pcm_port_index = INPORT_INDEX;
        mp3_port_index = OUTPORT_INDEX;
        pcm_port_dir = OMX_DirInput;
        mp3_port_dir = OMX_DirOutput;
    }
    else {
        mp3_port_index = INPORT_INDEX;
        pcm_port_index = OUTPORT_INDEX;
        mp3_port_dir = OMX_DirInput;
        pcm_port_dir = OMX_DirOutput;
    }

    ret = __AllocateMp3Port(mp3_port_index, mp3_port_dir);
    if (ret != OMX_ErrorNone)
        goto free_ports;

    ret = __AllocatePcmPort(pcm_port_index, pcm_port_dir);
    if (ret != OMX_ErrorNone)
        goto free_mp3port;

    /* OMX_PORT_PARAM_TYPE */
    memset(&portparam, 0, sizeof(portparam));
    SetTypeHeader(&portparam, sizeof(portparam));
    portparam.nPorts = NR_PORTS;
    portparam.nStartPortNumber = INPORT_INDEX;

    memcpy(&this->portparam, &portparam, sizeof(portparam));
    /* end of OMX_PORT_PARAM_TYPE */

    coding_type = OMX_AUDIO_CodingMP3;
    codec_mode = isencoder ? MIX_CODING_ENCODE : MIX_CODING_DECODE;

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;

free_mp3port:
    delete ports[mp3_port_index];

free_ports:
    delete []ports;

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}


OMX_ERRORTYPE MrstSstComponent::__AllocateAacRolePorts(bool isencoder)
{
    PortBase **ports;

    OMX_U32 aac_port_index, pcm_port_index;
    OMX_DIRTYPE aac_port_dir, pcm_port_dir;

    OMX_PORT_PARAM_TYPE portparam;
    OMX_U32 i;
    OMX_ERRORTYPE ret;

    LOGV("%s(): enter\n", __func__);

    ports = new PortBase *[NR_PORTS];
    if (!ports)
        return OMX_ErrorInsufficientResources;
    this->nr_ports = NR_PORTS;
    this->ports = ports;

    if (isencoder) {
        pcm_port_index = INPORT_INDEX;
        aac_port_index = OUTPORT_INDEX;
        pcm_port_dir = OMX_DirInput;
        aac_port_dir = OMX_DirOutput;
    }
    else {
        aac_port_index = INPORT_INDEX;
        pcm_port_index = OUTPORT_INDEX;
        aac_port_dir = OMX_DirInput;
        pcm_port_dir = OMX_DirOutput;
    }

    ret = __AllocateAacPort(aac_port_index, aac_port_dir);
    if (ret != OMX_ErrorNone)
        goto free_ports;

    ret = __AllocatePcmPort(pcm_port_index, pcm_port_dir);
    if (ret != OMX_ErrorNone)
        goto free_aacport;

    /* OMX_PORT_PARAM_TYPE */
    memset(&portparam, 0, sizeof(portparam));
    SetTypeHeader(&portparam, sizeof(portparam));
    portparam.nPorts = NR_PORTS;
    portparam.nStartPortNumber = INPORT_INDEX;

    memcpy(&this->portparam, &portparam, sizeof(portparam));
    /* end of OMX_PORT_PARAM_TYPE */

    coding_type = OMX_AUDIO_CodingAAC;
    codec_mode = isencoder ? MIX_CODING_ENCODE : MIX_CODING_DECODE;

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;

free_aacport:
    delete ports[aac_port_index];

free_ports:
    delete []ports;

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::__AllocateMp3Port(OMX_U32 port_index,
                                                  OMX_DIRTYPE dir)
{
    PortMp3 *mp3port;

    OMX_PARAM_PORTDEFINITIONTYPE mp3portdefinition;
    OMX_AUDIO_PARAM_MP3TYPE mp3portparam;
    OMX_U32 i;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortMp3;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }

    mp3port = static_cast<PortMp3 *>(this->ports[port_index]);

    /* MP3 - OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&mp3portdefinition, 0, sizeof(mp3portdefinition));
    SetTypeHeader(&mp3portdefinition, sizeof(mp3portdefinition));
    mp3portdefinition.nPortIndex = port_index;
    mp3portdefinition.eDir = dir;
    if (dir == OMX_DirInput) {
        mp3portdefinition.nBufferCountActual = INPORT_MP3_ACTUAL_BUFFER_COUNT;
        mp3portdefinition.nBufferCountMin = INPORT_MP3_MIN_BUFFER_COUNT;
        mp3portdefinition.nBufferSize = INPORT_MP3_BUFFER_SIZE;
    }
    else {
        mp3portdefinition.nBufferCountActual = OUTPORT_MP3_ACTUAL_BUFFER_COUNT;
        mp3portdefinition.nBufferCountMin = OUTPORT_MP3_MIN_BUFFER_COUNT;
        mp3portdefinition.nBufferSize = OUTPORT_MP3_BUFFER_SIZE;
    }
    mp3portdefinition.bEnabled = OMX_TRUE;
    mp3portdefinition.bPopulated = OMX_FALSE;
    mp3portdefinition.eDomain = OMX_PortDomainAudio;
    mp3portdefinition.format.audio.cMIMEType = "audio/mpeg";
    mp3portdefinition.format.audio.pNativeRender = NULL;
    mp3portdefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
    mp3portdefinition.format.audio.eEncoding = OMX_AUDIO_CodingMP3;
    mp3portdefinition.bBuffersContiguous = OMX_FALSE;
    mp3portdefinition.nBufferAlignment = 0;

    mp3port->SetPortDefinition(&mp3portdefinition, true);

    /* end of MP3 - OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_AUDIO_PARAM_MP3TYPE */
    memset(&mp3portparam, 0, sizeof(mp3portparam));
    SetTypeHeader(&mp3portparam, sizeof(mp3portparam));
    mp3portparam.nPortIndex = port_index;
    mp3portparam.nChannels = 2;
    mp3portparam.nBitRate = 0;
    mp3portparam.nSampleRate = 0;
    mp3portparam.nAudioBandWidth = 0;
    mp3portparam.eChannelMode = OMX_AUDIO_ChannelModeStereo;
    mp3portparam.eFormat = OMX_AUDIO_MP3StreamFormatMP1Layer3;

    mp3port->SetPortMp3Param(&mp3portparam, true);
    /* end of OMX_AUDIO_PARAM_MP3TYPE */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstSstComponent::__AllocateAacPort(OMX_U32 port_index,
                                                  OMX_DIRTYPE dir)
{
    PortAac *aacport;

    OMX_PARAM_PORTDEFINITIONTYPE aacportdefinition;
    OMX_AUDIO_PARAM_AACPROFILETYPE aacportparam;
    OMX_U32 i;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortAac;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }

    aacport = static_cast<PortAac *>(this->ports[port_index]);

    /* AAC - OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&aacportdefinition, 0, sizeof(aacportdefinition));
    SetTypeHeader(&aacportdefinition, sizeof(aacportdefinition));
    aacportdefinition.nPortIndex = port_index;
    aacportdefinition.eDir = dir;
    if (dir == OMX_DirInput) {
        aacportdefinition.nBufferCountActual = INPORT_AAC_ACTUAL_BUFFER_COUNT;
        aacportdefinition.nBufferCountMin = INPORT_AAC_MIN_BUFFER_COUNT;
        aacportdefinition.nBufferSize = INPORT_AAC_BUFFER_SIZE;
    }
    else {
        aacportdefinition.nBufferCountActual = OUTPORT_AAC_ACTUAL_BUFFER_COUNT;
        aacportdefinition.nBufferCountMin = OUTPORT_AAC_MIN_BUFFER_COUNT;
        aacportdefinition.nBufferSize = OUTPORT_AAC_BUFFER_SIZE;
    }
    aacportdefinition.bEnabled = OMX_TRUE;
    aacportdefinition.bPopulated = OMX_FALSE;
    aacportdefinition.eDomain = OMX_PortDomainAudio;
    aacportdefinition.format.audio.cMIMEType = "audio/mpeg";
    aacportdefinition.format.audio.pNativeRender = NULL;
    aacportdefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
    aacportdefinition.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
    aacportdefinition.bBuffersContiguous = OMX_FALSE;
    aacportdefinition.nBufferAlignment = 0;

    aacport->SetPortDefinition(&aacportdefinition, true);

    /* end of AAC - OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_AUDIO_PARAM_AACPROFILETYPE */
    memset(&aacportparam, 0, sizeof(aacportparam));
    SetTypeHeader(&aacportparam, sizeof(aacportparam));
    aacportparam.nPortIndex = port_index;
    aacportparam.nChannels = 2;
    aacportparam.nBitRate = 0;
    aacportparam.nSampleRate = 0;
    aacportparam.nAudioBandWidth = 0;
    aacportparam.nFrameLength = 1024; /* default for LC */
    aacportparam.nAACtools = OMX_AUDIO_AACToolNone;
    aacportparam.nAACERtools = OMX_AUDIO_AACERNone;
    aacportparam.eAACProfile = OMX_AUDIO_AACObjectLC;
    aacportparam.eChannelMode = OMX_AUDIO_ChannelModeStereo;

    aacport->SetPortAacParam(&aacportparam, true);
    /* end of OMX_AUDIO_PARAM_AACPROFILETYPE */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstSstComponent::__AllocatePcmPort(OMX_U32 port_index,
                                                  OMX_DIRTYPE dir)
{
    PortPcm *pcmport;

    OMX_PARAM_PORTDEFINITIONTYPE pcmportdefinition;
    OMX_AUDIO_PARAM_PCMMODETYPE pcmportparam;
    OMX_U32 i;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortPcm;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }
    pcmport = static_cast<PortPcm *>(this->ports[port_index]);

    /* PCM - OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&pcmportdefinition, 0, sizeof(pcmportdefinition));
    SetTypeHeader(&pcmportdefinition, sizeof(pcmportdefinition));
    pcmportdefinition.nPortIndex = port_index;
    pcmportdefinition.eDir = dir;
    if (dir == OMX_DirInput) {
        pcmportdefinition.nBufferCountActual = INPORT_PCM_ACTUAL_BUFFER_COUNT;
        pcmportdefinition.nBufferCountMin = INPORT_PCM_MIN_BUFFER_COUNT;
        pcmportdefinition.nBufferSize = INPORT_PCM_BUFFER_SIZE;
    }
    else {
        pcmportdefinition.nBufferCountActual = OUTPORT_PCM_ACTUAL_BUFFER_COUNT;
        pcmportdefinition.nBufferCountMin = OUTPORT_PCM_MIN_BUFFER_COUNT;
        pcmportdefinition.nBufferSize = OUTPORT_PCM_BUFFER_SIZE;
    }
    pcmportdefinition.bEnabled = OMX_TRUE;
    pcmportdefinition.bPopulated = OMX_FALSE;
    pcmportdefinition.eDomain = OMX_PortDomainAudio;
    pcmportdefinition.format.audio.cMIMEType = "raw";
    pcmportdefinition.format.audio.pNativeRender = NULL;
    pcmportdefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
    pcmportdefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    pcmportdefinition.bBuffersContiguous = OMX_FALSE;
    pcmportdefinition.nBufferAlignment = 0;

    pcmport->SetPortDefinition(&pcmportdefinition, true);
    /* end of PCM - OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_AUDIO_PARAM_PCMMODETYPE */
    memset(&pcmportparam, 0, sizeof(pcmportparam));
    SetTypeHeader(&pcmportparam, sizeof(pcmportparam));
    pcmportparam.nPortIndex = port_index;
    pcmportparam.nChannels = 2;
    pcmportparam.eNumData = OMX_NumericalDataUnsigned;
    pcmportparam.eEndian = OMX_EndianLittle;
    pcmportparam.bInterleaved = OMX_FALSE;
    pcmportparam.nBitPerSample = 16;
    pcmportparam.nSamplingRate = 44100;
    pcmportparam.ePCMMode = OMX_AUDIO_PCMModeLinear;
    pcmportparam.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
    pcmportparam.eChannelMapping[1] = OMX_AUDIO_ChannelRF;

    pcmport->SetPortPcmParam(&pcmportparam, true);
    /* end of OMX_AUDIO_PARAM_PCMMODETYPE */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}

/* end of core methods & helpers */

/*
 * component methods & helpers
 */
/* Get/SetParameter */
OMX_ERRORTYPE MrstSstComponent::ComponentGetParameter(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter (index = 0x%08x)\n", __func__, nParamIndex);

    switch (nParamIndex) {
    case OMX_IndexParamAudioPortFormat: {
        OMX_AUDIO_PARAM_PORTFORMATTYPE *p =
            (OMX_AUDIO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAudio *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAudio *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortAudioParam(), sizeof(*p));
        break;
    }
    case OMX_IndexParamAudioPcm: {
        OMX_AUDIO_PARAM_PCMMODETYPE *p =
            (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortPcm *port = NULL;

        if (strcmp(GetWorkingRole(), "audio_decoder.mp3") &&
            strcmp(GetWorkingRole(), "audio_decoder.aac")) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorUnsupportedIndex);
            return OMX_ErrorUnsupportedIndex;
        }

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortPcm *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortPcmParam(), sizeof(*p));
        break;
    }
    case OMX_IndexParamAudioMp3: {
        OMX_AUDIO_PARAM_MP3TYPE *p =
            (OMX_AUDIO_PARAM_MP3TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortMp3 *port = NULL;

        if (strcmp(GetWorkingRole(), "audio_decoder.mp3")) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorUnsupportedIndex);
            return OMX_ErrorUnsupportedIndex;
        }

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortMp3 *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortMp3Param(), sizeof(*p));
        break;
    }
    case OMX_IndexParamAudioAac: {
        OMX_AUDIO_PARAM_AACPROFILETYPE *p =
            (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAac *port = NULL;

        if (strcmp(GetWorkingRole(), "audio_decoder.aac")) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorUnsupportedIndex);
            return OMX_ErrorUnsupportedIndex;
        }

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAac *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortAacParam(), sizeof(*p));
        break;
    }
    case (OMX_INDEXTYPE) PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX: {
        PV_OMXComponentCapabilityFlagsType *p =
            (PV_OMXComponentCapabilityFlagsType *)pComponentParameterStructure;

        p->iIsOMXComponentMultiThreaded = OMX_TRUE;
        p->iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
        p->iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
        p->iOMXComponentSupportsMovableInputBuffers = OMX_TRUE;
        p->iOMXComponentUsesNALStartCodes = OMX_TRUE;
        p->iOMXComponentSupportsPartialFrames = OMX_FALSE;
        p->iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;
        p->iOMXComponentUsesFullAVCFrames = OMX_FALSE;
        break;
    }
    default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::ComponentSetParameter(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter (index = 0x%08x)\n", __func__, nIndex);

    switch (nIndex) {
    case OMX_IndexParamAudioPortFormat: {
        OMX_AUDIO_PARAM_PORTFORMATTYPE *p =
            (OMX_AUDIO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAudio *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortPcm *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortAudioParam(p, false);
        break;
    }
    case OMX_IndexParamAudioPcm: {
        OMX_AUDIO_PARAM_PCMMODETYPE *p =
            (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortPcm *port = NULL;

        if (strcmp(GetWorkingRole(), "audio_decoder.mp3") &&
            strcmp(GetWorkingRole(), "audio_decoder.aac")) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorUnsupportedIndex);
            return OMX_ErrorUnsupportedIndex;
        }

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortPcm *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortPcmParam(p, false);
        break;
    }
    case OMX_IndexParamAudioMp3: {
        OMX_AUDIO_PARAM_MP3TYPE *p =
            (OMX_AUDIO_PARAM_MP3TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortMp3 *port = NULL;

        if (strcmp(GetWorkingRole(), "audio_decoder.mp3")) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorUnsupportedIndex);
            return OMX_ErrorUnsupportedIndex;
        }

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortMp3 *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortMp3Param(p, false);
        break;
    }
    case OMX_IndexParamAudioAac: {
        OMX_AUDIO_PARAM_AACPROFILETYPE *p =
            (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAac *port = NULL;

        if (strcmp(GetWorkingRole(), "audio_decoder.aac")) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorUnsupportedIndex);
            return OMX_ErrorUnsupportedIndex;
        }

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAac *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortAacParam(p, false);
        break;
    }
    default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* Get/SetConfig */
OMX_ERRORTYPE MrstSstComponent::ComponentGetConfig(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::ComponentSetConfig(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* implement ComponentBase::Processor[*] */
OMX_ERRORTYPE MrstSstComponent::ProcessorInit(void)
{
    MixAudio *mix;
    MixAudioConfigParams *acp;
    MixIOVec *mixio;
    OMX_ERRORTYPE oret = OMX_ErrorNone;
    MIX_RESULT mret;

    LOGV("%s(): enter\n", __func__);

    g_type_init();

    /* set default parameters */
    if (coding_type == OMX_AUDIO_CodingMP3)
        acp = MIX_AUDIOCONFIGPARAMS(mix_acp_mp3_new());
    else if (coding_type == OMX_AUDIO_CodingAAC)
        acp = MIX_AUDIOCONFIGPARAMS(mix_acp_aac_new());
    else {
        LOGE("%s(),%d: exit, unkown role (ret == 0x%08x)\n",
             __func__, __LINE__, OMX_ErrorInvalidState);
        return OMX_ErrorInvalidState;
    }

    if (codec_mode == MIX_CODING_DECODE)
        MIX_ACP_DECODEMODE(acp) = MIX_DECODE_DIRECTRENDER;
    /*
    else if (codec_mode == MIX_CODING_ENCODE)
        ;
    */

    mret = mix_acp_set_streamname(acp, GetWorkingRole());
    if (!MIX_SUCCEEDED(mret)) {
        LOGE("%s(),%d: exit, mix_acp_set_streamname failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        mix_params_unref(MIX_PARAMS(acp));
        return OMX_ErrorInvalidState;
    }

    mix = mix_audio_new();
    mret = mix_audio_initialize(mix, codec_mode, NULL, NULL);
    if (!(MIX_SUCCEEDED(mret))) {
        LOGE("%s(),%d: exit, mix_audio_initialize failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        mix_params_unref(MIX_PARAMS(acp));
        return OMX_ErrorInvalidState;
    }

    mixio = (MixIOVec *)malloc(sizeof(MixIOVec));
    if (!mixio) {
        LOGE("%s(),%d: exit, failed to allocate mbuffer (ret == 0x%08x)",
             __func__, __LINE__, mret);
        mix_params_unref(MIX_PARAMS(acp));
        mix_audio_unref(mix);
        return OMX_ErrorInvalidState;
    }

    this->mix = mix;
    this->acp = acp;
    this->mixio = mixio;

    ibuffercount = 0;

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, oret);
    return oret;
}

OMX_ERRORTYPE MrstSstComponent::ProcessorDeinit(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    mix_audio_stop_drop(mix);

    mix_audio_deinitialize(mix);

    mix_acp_unref(acp);
    mix_audio_unref(mix);

    free(mixio);

    ibuffercount = 0;

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::ProcessorStart(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::ProcessorStop(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    mix_audio_stop_drop(mix);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::ProcessorPause(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::ProcessorResume(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* implement ComponentBase::ProcessorProcess */
void MrstSstComponent::ProcessorProcess(
    OMX_BUFFERHEADERTYPE **buffers,
    bool *retain,
    OMX_U32 nr_buffers)
{
    OMX_U32 outfilledlen = 0;
    OMX_S64 outtimestamp = 0;
    MIX_RESULT mret;

    LOGV("%s(): enter\n", __func__);

    if (!buffers[INPORT_INDEX]->nFilledLen) {
        LOGE("%s(),%d: exit input buffer's nFilledLen is zero (ret = void)\n",
             __func__, __LINE__);
        return;
    }

    mixio->data = buffers[INPORT_INDEX]->pBuffer +
        buffers[INPORT_INDEX]->nOffset;
    mixio->size = buffers[INPORT_INDEX]->nFilledLen;

    if (coding_type == OMX_AUDIO_CodingMP3) {
        MIX_ACP_NUM_CHANNELS(acp) = 2;
        MIX_ACP_BITRATE(acp) = 128000;
        MIX_ACP_SAMPLE_FREQ(acp) = 44100;
        mix_acp_set_op_align(acp, MIX_ACP_OUTPUT_ALIGN_LSB);
        mix_acp_set_bps(acp, MIX_ACP_BPS_16);

        MIX_ACP_MP3_CRC(acp) = 0;
        MIX_ACP_MP3_MPEG_FORMAT(acp) = 1;
        MIX_ACP_MP3_MPEG_LAYER(acp) = 3;
    }
    /*
    else if (coding_type == OMX_AUDIO_CodingMP3) {
        ;
    }
    */
    else {
        LOGE("%s(),%d: exit, unkown mix acp\n", __func__, __LINE__);
        return;
    }

    if (!ibuffercount) {
        mret = mix_audio_configure(mix, acp, NULL);
        if (!MIX_SUCCEEDED(mret)) {
            LOGE("%s(),%d: exit, mix_audio_configure failed (ret == 0x%08x)",
                 __func__, __LINE__, mret);
            return;
        }

        mret = mix_audio_start(mix);
        if (!MIX_SUCCEEDED(mret)) {
            LOGE("%s(),%d: faild to mix_audio_start (ret == 0x%08x)",
                 __func__, __LINE__, mret);
            return;
        }
    }
    ibuffercount++;

    if (codec_mode == MIX_CODING_DECODE) {
        mret = mix_audio_decode(mix, (const MixIOVec *)mixio, 1, NULL, NULL);
        if (!MIX_SUCCEEDED(mret)) {
            LOGV("_decode returns fail. Error code:0x%08x", mret);
            return;
        }
        mix_audio_get_timestamp(mix, (OMX_U64 *)&outtimestamp);
    }
    /*
    else {
        ;
    }
    */

    buffers[OUTPORT_INDEX]->nFilledLen = outfilledlen;
    buffers[OUTPORT_INDEX]->nTimeStamp = outtimestamp;

    buffers[INPORT_INDEX]->nFilledLen = 0;

    LOGV("%s(),%d: exit (ret = void)\n", __func__, __LINE__);
}

/* end of implement ComponentBase::Processor[*] */

/* end of component methods & helpers */

/*
 * CModule Interface
 */
static const OMX_STRING g_roles[] =
{
    "audio_decoder.mp3",
    //"audio_decoder.aac",
};

static const OMX_STRING g_compname = "OMX.Intel.MrstSST";

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

OMX_ERRORTYPE omx_component_module_instantiate(OMX_PTR *instance)
{
    ComponentBase *cbase;

    cbase = new MrstSstComponent;
    if (!cbase) {
        *instance = NULL;
        return OMX_ErrorInsufficientResources;
    }

    *instance = cbase;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_component_module_query_name(OMX_STRING name, OMX_U32 len)
{
    if (!name)
        return OMX_ErrorBadParameter;

    strncpy(name, g_compname, len);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_component_module_query_roles(OMX_U32 *nr_roles,
                                               OMX_U8 **roles)
{
    return ComponentBase::QueryRolesHelper(ARRAY_SIZE(g_roles),
                                           (const OMX_U8 **)g_roles,
                                           nr_roles, roles);
}
