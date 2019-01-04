#
# Makefile for the DAL (Display Abstract Layer), which is a  sub-component
# of the AMDGPU drm driver.
# It provides the HW control for display related functionalities.

AMDDALPATH = $(RELATIVE_AMD_DISPLAY_PATH)

subdir-ccflags-y += -I$(FULL_AMD_DISPLAY_PATH)/dc/inc/
subdir-ccflags-y += -I$(FULL_AMD_DISPLAY_PATH)/dc/inc/hw
subdir-ccflags-y += -I$(FULL_AMD_DISPLAY_PATH)/modules/inc
subdir-ccflags-y += -I$(FULL_AMD_DISPLAY_PATH)/modules/freesync
subdir-ccflags-y += -I$(FULL_AMD_DISPLAY_PATH)/modules/color

#TODO: remove when Timing Sync feature is complete
subdir-ccflags-y += -DBUILD_FEATURE_TIMING_SYNC=0

DAL_LIBS = amdgpu_dm dc	modules/freesync modules/color

AMD_DAL = $(addsuffix /Makefile, $(addprefix $(FULL_AMD_DISPLAY_PATH)/,$(DAL_LIBS)))

include $(AMD_DAL)
