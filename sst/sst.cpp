/*
 * Copyright (c) 2009 Wind River Systems, Inc.
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

#include <audio_parser.h>

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

//#define LOG_NDEBUG 0

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
    PortBase **ports;

    OMX_U32 codec_port_index, pcm_port_index;
    OMX_DIRTYPE codec_port_dir, pcm_port_dir;

    OMX_PORT_PARAM_TYPE portparam;

    bool isencoder;
    const char *working_role;

    OMX_U32 i;
    OMX_ERRORTYPE ret;

    LOGV("%s(): enter\n", __func__);

    ports = new PortBase *[NR_PORTS];
    if (!ports)
        return OMX_ErrorInsufficientResources;

    this->nr_ports = NR_PORTS;
    this->ports = ports;

    working_role = GetWorkingRole();
    if (!strncmp(working_role, "audio_decoder", strlen("audio_decoder")))
        isencoder = false;
    else
        isencoder = true;

    if (isencoder) {
        pcm_port_index = INPORT_INDEX;
        codec_port_index = OUTPORT_INDEX;
        pcm_port_dir = OMX_DirInput;
        codec_port_dir = OMX_DirOutput;
    }
    else {
        codec_port_index = INPORT_INDEX;
        pcm_port_index = OUTPORT_INDEX;
        codec_port_dir = OMX_DirInput;
        pcm_port_dir = OMX_DirOutput;
    }

    working_role = strpbrk(working_role, ".");
    if (!working_role)
        return OMX_ErrorUndefined;
    working_role++;

    if (!strcmp(working_role, "mp3")) {
        ret = __AllocateMp3Port(codec_port_index, codec_port_dir);
        coding_type = OMX_AUDIO_CodingMP3;
    }
    else if(!strcmp(working_role, "aac")) {
        ret = __AllocateAacPort(codec_port_index, codec_port_dir);
        coding_type = OMX_AUDIO_CodingAAC;
    }
    else
        ret = OMX_ErrorUndefined;

    if (ret != OMX_ErrorNone)
        goto free_ports;

    ret = __AllocatePcmPort(pcm_port_index, pcm_port_dir);
    if (ret != OMX_ErrorNone)
        goto free_codecport;

    /* OMX_PORT_PARAM_TYPE */
    memset(&portparam, 0, sizeof(portparam));
    SetTypeHeader(&portparam, sizeof(portparam));
    portparam.nPorts = NR_PORTS;
    portparam.nStartPortNumber = INPORT_INDEX;

    memcpy(&this->portparam, &portparam, sizeof(portparam));
    /* end of OMX_PORT_PARAM_TYPE */

    codec_mode = isencoder ? MIX_CODING_ENCODE : MIX_CODING_DECODE;

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;

free_codecport:
    delete ports[codec_port_index];
    ports[codec_port_index] = NULL;

free_ports:
    coding_type = OMX_AUDIO_CodingUnused;

    delete []ports;
    ports = NULL;

    this->ports = NULL;
    this->nr_ports = 0;

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
    mp3portdefinition.format.audio.cMIMEType = (char *)"audio/mpeg";
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
    aacportdefinition.format.audio.cMIMEType = (char *)"audio/mp4";
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
    pcmportdefinition.format.audio.cMIMEType = (char *)"raw";
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
    MixIOVec *mixio_in, *mixio_out;
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
        oret = OMX_ErrorInvalidState;
        goto free_acp;
    }

    mix = mix_audio_new();
    mret = mix_audio_initialize(mix, codec_mode, NULL, NULL);
    if (!(MIX_SUCCEEDED(mret))) {
        LOGE("%s(),%d: exit, mix_audio_initialize failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorInvalidState;
        goto free_acp;
    }

    mixio_in = (MixIOVec *)malloc(sizeof(MixIOVec));
    if (!mixio_in) {
        LOGE("%s(),%d: exit, failed to allocate mbuffer (ret == 0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorInvalidState;
        goto deinitialize_mix;
    }

    mixio_out = (MixIOVec *)malloc(sizeof(MixIOVec));
    if (!mixio_out) {
        LOGE("%s(),%d: exit, failed to allocate mbuffer (ret == 0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorInvalidState;
        goto free_mixio_in;
    }

    this->mix = mix;
    this->acp = acp;
    this->mixio_in = mixio_in;
    this->mixio_out = mixio_out;

    LOGV("%s(),%d: exit, done\n", __func__, __LINE__);
    return OMX_ErrorNone;

free_mixio_in:
    free(mixio_in);

deinitialize_mix:
    mix_audio_deinitialize(mix);
    mix_audio_unref(mix);

free_acp:
    mix_params_unref(MIX_PARAMS(acp));

    LOGV("%s(),%d: exit, (ret = 0x%08x)\n", __func__, __LINE__, oret);
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

    free(mixio_in);
    free(mixio_out);

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

    //mix_audio_pause(mix);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::ProcessorResume(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    //mix_audio_resume(mix);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* implement ComponentBase::ProcessorProcess */
void MrstSstComponent::ProcessorProcess(
    OMX_BUFFERHEADERTYPE **buffers,
    buffer_retain_t *retain,
    OMX_U32 nr_buffers)
{
    OMX_U32 outfilledlen = 0;
    OMX_S64 outtimestamp = 0;

    MixStreamState mstream_state = MIX_STREAM_NULL;
    MixState mstate;
    bool acp_changed = false;

    MIX_RESULT mret;

    LOGV("%s(): enter\n", __func__);

#if !LOG_NDEBUG
    DumpBuffer(buffers[INPORT_INDEX], false);
#endif

    if (!buffers[INPORT_INDEX]->nFilledLen) {
        LOGE("%s(),%d: exit, input buffer's nFilledLen is zero (ret = void)\n",
             __func__, __LINE__);
        return;
    }

    mixio_in->data = buffers[INPORT_INDEX]->pBuffer +
        buffers[INPORT_INDEX]->nOffset;
    mixio_in->size = buffers[INPORT_INDEX]->nFilledLen;

    mixio_out->data = buffers[OUTPORT_INDEX]->pBuffer +
        buffers[OUTPORT_INDEX]->nOffset;
    mixio_out->size = buffers[OUTPORT_INDEX]->nAllocLen;

    if (codec_mode == MIX_CODING_DECODE) {
        if (coding_type == OMX_AUDIO_CodingMP3)
            mret = ChangeAcpWithConfigHeader(mixio_in->data, &acp_changed);
        else if (coding_type == OMX_AUDIO_CodingAAC) {
            if (buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
                mret = ChangeAcpWithConfigHeader(mixio_in->data, &acp_changed);
            else
                mret = MIX_RESULT_SUCCESS;
        }
        else {
            LOGE("%s(),%d: exit, unknown coding type (0x%08x)\n",
                 __func__, __LINE__, coding_type);
            mret = MIX_RESULT_FAIL;
        }

        if (!MIX_SUCCEEDED(mret)) {
            LOGE("%s(),%d: exit, ret == 0x%08x\n", __func__, __LINE__, mret);
            return;
        }

        mix_audio_get_state(mix, &mstate);
        if (mstate == MIX_STATE_CONFIGURED)
            mix_audio_get_stream_state(mix, &mstream_state);

        if (acp_changed) {
            if ((mstream_state != MIX_STREAM_NULL) &&
                (mstream_state != MIX_STREAM_STOPPED))
                mix_audio_stop_drain(mix);

            mret = mix_audio_configure(mix, acp, NULL);
            if (!MIX_SUCCEEDED(mret)) {
                LOGE("%s(),%d: exit, mix_audio_configure failed "
                     "(ret == 0x%08x)", __func__, __LINE__, mret);
                return;
            }
        }

        mix_audio_get_stream_state(mix, &mstream_state);
        if (mstream_state != MIX_STREAM_PLAYING) {
            LOGV("%s(): mix current stream state = %d, call mix_audio_start\n",
                 __func__, mstream_state);
            mret = mix_audio_start(mix);
            if (!MIX_SUCCEEDED(mret)) {
                LOGE("%s(),%d: faild to mix_audio_start (ret == 0x%08x)",
                     __func__, __LINE__, mret);
                return;
            }
        }
    }
    /*
    else {
        ;
    }
    */

    if (codec_mode == MIX_CODING_DECODE) {
        OMX_U64 consumed = 0, produced = 0;

        if (buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
            outfilledlen = 0;
            outtimestamp = 0;
            retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
            goto out;
        }

        mret = mix_audio_decode(mix,
                                (const MixIOVec *)mixio_in, 1, &consumed,
                                mixio_out, 1, &produced);
        if (!MIX_SUCCEEDED(mret)) {
            LOGE("%s(), %d: exit, mix_audio_decode failed (ret == 0x%08x)",
                 __func__, __LINE__, mret);
            return;
        }

        buffers[INPORT_INDEX]->nFilledLen -= (OMX_U32)consumed;
        if (buffers[INPORT_INDEX]->nFilledLen) {
            buffers[INPORT_INDEX]->nOffset += (OMX_U32)consumed;
            retain[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
            /* FIXME */
            outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;

            LOGD("%s(): input buffer NOT fully consumed %lu bytes consumed,"
                 "%lu bytes remained\n", __func__, (OMX_U32)consumed,
                 buffers[INPORT_INDEX]->nFilledLen);
        }
        else {
            buffers[INPORT_INDEX]->nOffset = 0;
            outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;

            LOGV("%s(): %lu bytes fully consumed\n", __func__,
                 (OMX_U32)consumed);
        }

        outfilledlen = produced;
        LOGV("%s(): %lu bytes produced\n", __func__, outfilledlen);
    }
    /*
    else {
        ;
    }
    */

out:
    buffers[OUTPORT_INDEX]->nFilledLen = outfilledlen;
    buffers[OUTPORT_INDEX]->nTimeStamp = outtimestamp;

#if !LOG_NDEBUG
//    if (retain[OUTPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN)
//        DumpBuffer(buffers[OUTPORT_INDEX], false);
#endif

    LOGV("%s(),%d: exit, done\n", __func__, __LINE__);
}

/* end of implement ComponentBase::Processor[*] */

/* end of component methods & helpers */

/*
 * parser wrappers
 */
static inline MIX_RESULT __Mp3ChangeAcpWithConfigHeader(
    MixAudioConfigParams *acp, const unsigned char *buffer, bool *acp_changed)
{
    int version, layer, crc, bitrate, samplingrate, channel,
        mode_extension, frame_length, frame_duration;
    int ret;

    if (!acp_changed)
        return MIX_RESULT_FAIL;

    ret = mp3_header_parse(buffer,
                           &version, &layer,
                           &crc, &bitrate,
                           &samplingrate, &channel,
                           &mode_extension, &frame_length, &frame_duration);
    if (ret)
        return MIX_RESULT_FAIL;

    if (version == MP3_HEADER_VERSION_1)
        version = 1;
    else if ((version == MP3_HEADER_VERSION_2) ||
             (version == MP3_HEADER_VERSION_25))
        version = 2;
    else
        return MIX_RESULT_FAIL;

    if (layer == MP3_HEADER_LAYER_1)
        layer = 1;
    else if (layer == MP3_HEADER_LAYER_2)
        layer = 2;
    else if (layer == MP3_HEADER_LAYER_3)
        layer = 3;
    else
        return MIX_RESULT_FAIL;


    if (crc == MP3_HEADER_CRC_PROTECTED)
        crc = 1;
    else if (crc == MP3_HEADER_NOT_PROTECTED)
        crc = 0;
    else
        return MIX_RESULT_FAIL;

    if ((channel == MP3_HEADER_STEREO) ||
        (channel == MP3_HEADER_JOINT_STEREO) ||
        (channel == MP3_HEADER_DUAL_CHANNEL))
        channel = 2;
    else if (channel == MP3_HEADER_SINGLE_CHANNEL)
        channel = 1;
    else
        return MIX_RESULT_FAIL;

    if ((MIX_ACP_NUM_CHANNELS(acp) != channel) ||
        (MIX_ACP_BITRATE(acp) != bitrate) ||
        (MIX_ACP_SAMPLE_FREQ(acp) != samplingrate) ||
        (MIX_ACP_MP3_CRC(acp) != crc) ||
        (MIX_ACP_MP3_MPEG_FORMAT(acp) != version) ||
        (MIX_ACP_MP3_MPEG_LAYER(acp) != layer)) {

        *acp_changed = true;

        LOGV_IF(MIX_ACP_NUM_CHANNELS(acp) != channel,
                "%s(): channel : %d != %d\n", __func__,
                MIX_ACP_NUM_CHANNELS(acp), channel);
        LOGV_IF(MIX_ACP_BITRATE(acp) != bitrate,
                "%s(): bitrate : %d != %d\n", __func__,
                MIX_ACP_BITRATE(acp), bitrate);
        LOGV_IF(MIX_ACP_SAMPLE_FREQ(acp) != samplingrate,
                "%s(): frequency : %d != %d\n", __func__,
                MIX_ACP_SAMPLE_FREQ(acp), samplingrate);
        LOGV_IF(MIX_ACP_MP3_CRC(acp) != crc,
                "%s(): crc : %d != %d\n", __func__, MIX_ACP_MP3_CRC(acp), crc);
        LOGV_IF(MIX_ACP_MP3_MPEG_FORMAT(acp) != version,
                "%s(): version : %d != %d\n", __func__,
                MIX_ACP_MP3_MPEG_FORMAT(acp), version);
        LOGV_IF(MIX_ACP_MP3_MPEG_LAYER(acp) != layer,
                "%s(): layer : %d != %d\n", __func__,
                MIX_ACP_MP3_MPEG_LAYER(acp), layer);

        MIX_ACP_NUM_CHANNELS(acp) = channel;
        MIX_ACP_BITRATE(acp) = bitrate;
        MIX_ACP_SAMPLE_FREQ(acp) = samplingrate;

        MIX_ACP_MP3_CRC(acp) = crc;
        MIX_ACP_MP3_MPEG_FORMAT(acp) = version;
        MIX_ACP_MP3_MPEG_LAYER(acp) = layer;

        mix_acp_set_op_align(acp, MIX_ACP_OUTPUT_ALIGN_LSB);
        mix_acp_set_bps(acp, MIX_ACP_BPS_16);

        LOGV("%s(): mp3 configration parameter has been changed\n", __func__);
        LOGV("%s():   format : %d\n", __func__, MIX_ACP_MP3_MPEG_FORMAT(acp));
        LOGV("%s():   layer : %d\n", __func__, MIX_ACP_MP3_MPEG_LAYER(acp));
        LOGV("%s():   crc : %d\n", __func__, MIX_ACP_MP3_CRC(acp));
        LOGV("%s():   frequency : %d\n", __func__, MIX_ACP_SAMPLE_FREQ(acp));
        LOGV("%s():   bitrate : %d\n", __func__, MIX_ACP_BITRATE(acp));
        LOGV("%s():   channel : %d\n", __func__, MIX_ACP_NUM_CHANNELS(acp));
    }

    return MIX_RESULT_SUCCESS;
}

static inline MIX_RESULT __AacChangeAcpWithConfigHeader(
    MixAudioConfigParams *acp, const unsigned char *buffer, bool *acp_changed)
{
    int aot, frequency, channel;
    int ret;

    if (!acp_changed)
        return MIX_RESULT_FAIL;

    ret = audio_specific_config_parse(buffer,
                                      &aot, &frequency, &channel);
    if (ret)
        return MIX_RESULT_FAIL;

    if (aot < 1 || aot > 4)
        return MIX_RESULT_FAIL;

    if (MIX_ACP_NUM_CHANNELS(acp) != channel) {
        LOGV("%s(): channel : %d != %d\n", __func__, MIX_ACP_NUM_CHANNELS(acp),
             channel);

        MIX_ACP_NUM_CHANNELS(acp) = channel;
        *acp_changed = true;
    }

    if (MIX_ACP_SAMPLE_FREQ(acp) != frequency) {
        LOGV("%s(): samplingrate : %d != %d\n", __func__,
             MIX_ACP_SAMPLE_FREQ(acp), frequency);

        MIX_ACP_SAMPLE_FREQ(acp) = frequency;
        *acp_changed = true;
    }

    MIX_ACP_AAC_MPEG_ID(acp) = MIX_AAC_MPEG_2_ID;
    MIX_ACP_AAC_CRC(acp) = 0; /* 0:disabled, 1:enabled */
    MIX_ACP_AAC_AOT(acp) = aot; /* 1:Main, 2:LC, 3:SSR, 4:SBR */
    mix_acp_aac_set_bit_stream_format(MIX_AUDIOCONFIGPARAMSAAC(acp),
                                      MIX_AAC_BS_RAW);
    /* 0:Main, 1:LC, 2:SSR, 3:SBR */
    mix_acp_aac_set_aac_profile(MIX_AUDIOCONFIGPARAMSAAC(acp),
                                (MixAACProfile)(aot - 1));
    /* 0:CBR, 1:VBR */
    mix_acp_aac_set_bit_rate_type(MIX_AUDIOCONFIGPARAMSAAC(acp),
                                  (MixACPBitrateType)0);

    if (*acp_changed) {
        LOGV("%s(): audio configration parameter has been chagned\n",
             __func__);
        LOGV("%s():   aot : %d\n", __func__, MIX_ACP_AAC_AOT(acp));
        LOGV("%s():   frequency : %d\n", __func__, MIX_ACP_SAMPLE_FREQ(acp));
        LOGV("%s():   channel : %d\n", __func__, MIX_ACP_NUM_CHANNELS(acp));
    }

    return MIX_RESULT_SUCCESS;
}

MIX_RESULT MrstSstComponent::ChangeAcpWithConfigHeader(
    const unsigned char *buffer,
    bool *acp_changed)
{
    MIX_RESULT ret;

    if (coding_type == OMX_AUDIO_CodingMP3)
        ret = __Mp3ChangeAcpWithConfigHeader(acp, buffer, acp_changed);
    else if (coding_type == OMX_AUDIO_CodingAAC)
        ret = __AacChangeAcpWithConfigHeader(acp, buffer, acp_changed);
    else
        return -1;

    if (!MIX_SUCCEEDED(ret))
        return ret;

    mix_acp_set_op_align(acp, MIX_ACP_OUTPUT_ALIGN_LSB);
    mix_acp_set_bps(acp, MIX_ACP_BPS_16);

    return MIX_RESULT_SUCCESS;
}

/* end of parser wrappers */

/*
 * CModule Interface
 */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *g_name = (const char *)"OMX.Intel.Mrst.SST";

static const char *g_roles[] =
{
    (const char *)"audio_decoder.mp3",
    (const char *)"audio_decoder.aac",
};

OMX_ERRORTYPE wrs_omxil_cmodule_ops_instantiate(OMX_PTR *instance)
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

struct wrs_omxil_cmodule_ops_s cmodule_ops = {
    instantiate: wrs_omxil_cmodule_ops_instantiate,
};

struct wrs_omxil_cmodule_s WRS_OMXIL_CMODULE_SYMBOL = {
    name: g_name,
    roles: &g_roles[0],
    nr_roles: ARRAY_SIZE(g_roles),
    ops: &cmodule_ops,
};
