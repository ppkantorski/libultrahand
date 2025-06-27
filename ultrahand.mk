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
  $(ULTRA_DIR)/common \
  $(ULTRA_DIR)/libultra/source

INCLUDES += \
  $(ULTRA_DIR)/common \
  $(ULTRA_DIR)/libultra/include \
  $(ULTRA_DIR)/libtesla/include
