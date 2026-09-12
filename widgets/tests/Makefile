#!/usr/bin/make -f
# Makefile for DAF-Widgets #
# ------------------------ #
# Created by falkTX
#

DAF_DIR = ../..

include $(DAF_DIR)/Makefile.base.mk

# ---------------------------------------------------------------------------------------------------------------------

BUILD_CXX_FLAGS += $(DGL_FLAGS) -DDONT_SET_USING_DGL_NAMESPACE
BUILD_CXX_FLAGS += -I$(DAF_DIR)/dgl -I../imgui -I../dusk

OPENGL_FLAGS += -DDGL_OPENGL

# ---------------------------------------------------------------------------------------------------------------------

all: duskwidgets$(APP_EXT) imgui$(APP_EXT)

clean:
	rm -f *.d *.o *.js *.html *.wasm
	rm -f duskwidgets$(APP_EXT)
	rm -f imgui$(APP_EXT)

# ---------------------------------------------------------------------------------------------------------------------

.PHONY: $(DAF_DIR)/build/libdgl-opengl.a

$(DAF_DIR)/build/libdgl-opengl.a:
	$(MAKE) -C $(DAF_DIR)/dgl ../build/libdgl-opengl.a

# ---------------------------------------------------------------------------------------------------------------------

duskwidgets$(APP_EXT): duskwidgets.cpp.o duskwidgets-src.cpp.o imgui-src.cpp.o $(DAF_DIR)/build/libdgl-opengl.a
	@echo "Linking $@"
	$(SILENT)$(CXX) $^ $(LINK_FLAGS) $(DGL_SYSTEM_LIBS) $(OPENGL_LIBS) -o $@

imgui$(APP_EXT): imgui.cpp.o imgui-src.cpp.o $(DAF_DIR)/build/libdgl-opengl.a
	@echo "Linking $@"
	$(SILENT)$(CXX) $^ $(LINK_FLAGS) $(DGL_SYSTEM_LIBS) $(OPENGL_LIBS) -o $@

# ---------------------------------------------------------------------------------------------------------------------

duskwidgets.cpp.o: duskwidgets.cpp
	@echo "Compiling $<"
	$(SILENT)$(CXX) $< $(BUILD_CXX_FLAGS) $(OPENGL_FLAGS) -c -o $@

duskwidgets-src.cpp.o: ../dusk/DuskWidgets.cpp
	@echo "Compiling $<"
	$(SILENT)$(CXX) $< $(BUILD_CXX_FLAGS) $(OPENGL_FLAGS) -c -o $@

imgui.cpp.o: imgui.cpp
	@echo "Compiling $<"
	$(SILENT)$(CXX) $< $(BUILD_CXX_FLAGS) $(OPENGL_FLAGS) -c -o $@

imgui-src.cpp.o: imgui-src.cpp
	@echo "Compiling $<"
	$(SILENT)$(CXX) $< $(BUILD_CXX_FLAGS) $(OPENGL_FLAGS) -c -o $@

# ---------------------------------------------------------------------------------------------------------------------

-include duskwidgets.cpp.d
-include duskwidgets-src.cpp.d
-include imgui.cpp.d
-include imgui-src.cpp.d

# ---------------------------------------------------------------------------------------------------------------------

# The gallery needs a display; CI only builds it.
run: all

.PHONY: all clean run
