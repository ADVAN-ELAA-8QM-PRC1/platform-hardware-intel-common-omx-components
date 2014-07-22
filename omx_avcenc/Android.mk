ifeq ($(strip $(BOARD_USES_WRS_OMXIL_CORE)),true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        src/AVCEncoder.cpp \
	src/OMXAVCComponent.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libwrs_omxil_intel_mrst_psb_stub_ve_avc_new

LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
	libwrs_omxil_common \
	liblog \
        libva \
	libva-android \
        libva-tpi \
        libwrs_omxil_base_encoder
	

LOCAL_STATIC_LIBRARIES := \
	libinfodump

LOCAL_C_INCLUDES := \
	$(WRS_OMXIL_CORE_ROOT)/utils/inc \
	$(WRS_OMXIL_CORE_ROOT)/base/inc \
	$(WRS_OMXIL_CORE_ROOT)/core/inc/khronos/openmax/include \
	$(PV_INCLUDES) \
	$(LIBVA_TOP) \
	$(LIBINFODUMP_TOP) \
	$(LIBBASECODEC_TOP)/inc  \
        $(LOCAL_PATH)/inc

ifeq ($(strip $(COMPONENT_SUPPORT_BUFFER_SHARING)),true)
LOCAL_CFLAGS += -DCOMPONENT_SUPPORT_BUFFER_SHARING
endif
ifeq ($(strip $(COMPONENT_SUPPORT_OPENCORE)),true)
LOCAL_CFLAGS += -DCOMPONENT_SUPPORT_OPENCORE
endif

include $(BUILD_SHARED_LIBRARY)
endif

# mpeg4Encoder-test =============================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
		src/AVCEncoder-test.cpp

  
LOCAL_CFLAGS += -DANDROID -DLINUX -DDXVA_VERSION -march=i686 -msse

LOCAL_C_INCLUDES :=  \
	        $(LIBVA_TOP) \
		$(LOCAL_PATH)/inc\
        	$(LIBBASECODEC_TOP)/inc

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := AVCEncoder-test

LOCAL_STATIC_LIBRARIES :=

LOCAL_SHARED_LIBRARIES := libwrs_omxil_base_encoder libva libutils libwrs_omxil_intel_mrst_psb_stub_ve_avc_new

include $(BUILD_EXECUTABLE)
