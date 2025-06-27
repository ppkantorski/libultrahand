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

# Cross-platform OS detection and command setup
ifeq ($(OS),Windows_NT)
    # Windows
    #DETECTED_OS := Windows
    # Use Windows command syntax
    RENAME_CMD = if exist "$(1)" ren "$(1)" "$(notdir $(1)).disabled"
    # Windows path handling - convert forward slashes to backslashes for commands
    WIN_PATH = $(subst /,\,$(1))
else
    # Unix-like systems (Linux, macOS, etc.)
    #DETECTED_OS := Unix
    RENAME_CMD = if [ -f "$(1)" ]; then mv -f "$(1)" "$(1).disabled"; fi
endif

# Cross-platform file renaming function
define rename_file
$(shell $(call RENAME_CMD,$(1)))
endef

# Alternative cross-platform approach using multiple fallback commands
define safe_rename
$(shell mv -f "$(1)" "$(1).disabled" 2>/dev/null || \
        (if exist "$(call WIN_PATH,$(1))" ren "$(call WIN_PATH,$(1))" "$(notdir $(1)).disabled") 2>nul || \
        true)
endef

# Disable other cJSON headers (cross-platform version)
ifneq (,$(wildcard $(ULTRA_DIR)/external/cJSON/*.h))
  $(foreach file,$(wildcard $(ULTRA_DIR)/external/cJSON/*.h), \
    $(eval base := $(notdir $(file))) \
    $(if $(filter $(base),cJSON.h),, \
      $(call rename_file,$(file))) \
  )
endif

ifneq (,$(wildcard $(ULTRA_DIR)/external/cJSON/*.c))
  $(foreach file,$(wildcard $(ULTRA_DIR)/external/cJSON/*.c), \
    $(eval base := $(notdir $(file))) \
    $(if $(filter $(base),cJSON.c),, \
      $(call rename_file,$(file))) \
  )
endif
