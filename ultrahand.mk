#--------------------------------------------------------------------------------- 
# Ultrahand Library Configuration
# This file should be included after LOCAL_LIBS is defined
#--------------------------------------------------------------------------------- 

ifndef LOCAL_LIBS
$(error LOCAL_LIBS must be defined before including ultrahand.mk)
endif

# Add libultrahand sources to SOURCES
SOURCES += $(LOCAL_LIBS)/libultrahand/miniz
SOURCES += $(LOCAL_LIBS)/libultrahand/libultra/source

# Add libultrahand includes to INCLUDES
INCLUDES += $(LOCAL_LIBS)/libultrahand
INCLUDES += $(LOCAL_LIBS)/libultrahand/miniz
INCLUDES += $(LOCAL_LIBS)/libultrahand/libultra/include
INCLUDES += $(LOCAL_LIBS)/libultrahand/libtesla/include
