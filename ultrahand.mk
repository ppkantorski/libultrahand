#--------------------------------------------------------------------------------- 
# Ultrahand Library Configuration
# Auto-detects libultrahand directory location
#--------------------------------------------------------------------------------- 

# Assume TOPDIR is the root project directory where you run make
TOPDIR ?= $(CURDIR)

# Get the absolute path of this .mk file directory
ULTRA_ABS := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

# Convert absolute path to relative path from TOPDIR
ULTRA_DIR := $(subst $(TOPDIR)/,,$(ULTRA_ABS))

# If ULTRA_DIR equals ULTRA_ABS, then ULTRA_ABS is outside TOPDIR,
# fallback to just ULTRA_ABS (rare case)
ifeq ($(ULTRA_DIR),$(ULTRA_ABS))
  ULTRA_DIR := $(ULTRA_ABS)
endif


# Now add folder paths relative to TOPDIR (or absolute if fallback)
SOURCES += \
  $(ULTRA_DIR)/external/cJSON \
  $(ULTRA_DIR)/libultra/source

INCLUDES += \
  $(ULTRA_DIR)/external/cJSON \
  $(ULTRA_DIR)/libultra/include \
  $(ULTRA_DIR)/libtesla/include

# Disable other cJSON headers
ifneq (,$(wildcard $(ULTRA_DIR)/external/cJSON/*.h))
  $(foreach file,$(wildcard $(ULTRA_DIR)/external/cJSON/*.h), \
    $(eval base := $(notdir $(file))) \
    $(if $(filter $(base),cJSON.h),, \
      $(shell mv -f $(file) $(file).disabled)) \
  )
endif
ifneq (,$(wildcard $(ULTRA_DIR)/external/cJSON/*.c))
  $(foreach file,$(wildcard $(ULTRA_DIR)/external/cJSON/*.c), \
    $(eval base := $(notdir $(file))) \
    $(if $(filter $(base),cJSON.c),, \
      $(shell mv -f $(file) $(file).disabled)) \
  )
endif
