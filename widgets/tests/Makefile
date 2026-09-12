#!/usr/bin/make -f
# Makefile for DAF-Widgets #
# ------------------------ #
# Created by falkTX
#

DAF_DIR = ../../DAF
ifeq (,$(wildcard $(DAF_DIR)))
DAF_DIR = ../../dpf
endif

include $(DAF_DIR)/Makefile.base.mk

# ---------------------------------------------------------------------------------------------------------------------

BUILD_CXX_FLAGS += $(DGL_FLAGS) -DDONT_SET_USING_DGL_NAMESPACE
BUILD_CXX_FLAGS += -I$(DAF_DIR)/dgl -I../opengl

CAIRO_FLAGS += -DDGL_CAIRO -I../cairo
OPENGL_FLAGS += -DDGL_OPENGL -I../opengl

ifeq ($(WASM),true)
LINK_FLAGS += --shell-file=./emscripten/shell.html
LINK_FLAGS += -sEXPORTED_RUNTIME_METHODS=FS,cwrap
endif

# ---------------------------------------------------------------------------------------------------------------------

all: duskwidgets$(APP_EXT) imgui$(APP_EXT) opengl$(APP_EXT) textedit$(APP_EXT)

clean:
	rm -f *.d *.o *.js *.html *.wasm
	rm -f duskwidgets$(APP_EXT)
	rm -f imgui$(APP_EXT)
	rm -f opengl$(APP_EXT)
	rm -f textedit$(APP_EXT)

# ---------------------------------------------------------------------------------------------------------------------

.PHONY: $(DAF_DIR)/build/libdgl-cairo.a
.PHONY: $(DAF_DIR)/build/libdgl-opengl.a
.PHONY: $(DAF_DIR)/build/libdgl-stub.a
.PHONY: $(DAF_DIR)/build/libdgl-vulkan.a

$(DAF_DIR)/build/libdgl-cairo.a:
	$(MAKE) -C $(DAF_DIR)/dgl ../build/libdgl-cairo.a

$(DAF_DIR)/build/libdgl-opengl.a:
	$(MAKE) -C $(DAF_DIR)/dgl ../build/libdgl-opengl.a

$(DAF_DIR)/build/libdgl-stub.a:
	$(MAKE) -C $(DAF_DIR)/dgl ../build/libdgl-stub.a

$(DAF_DIR)/build/libdgl-vulkan.a:
	$(MAKE) -C $(DAF_DIR)/dgl ../build/libdgl-vulkan.a

# ---------------------------------------------------------------------------------------------------------------------

duskwidgets$(APP_EXT): duskwidgets.cpp.o duskwidgets-src.cpp.o imgui-src.cpp.o $(DAF_DIR)/build/libdgl-opengl.a
	@echo "Linking $@"
	$(SILENT)$(CXX) $^ $(LINK_FLAGS) $(DGL_SYSTEM_LIBS) $(OPENGL_LIBS) -o $@

imgui$(APP_EXT): imgui.cpp.o imgui-src.cpp.o $(DAF_DIR)/build/libdgl-opengl.a
	@echo "Linking $@"
	$(SILENT)$(CXX) $^ $(LINK_FLAGS) $(DGL_SYSTEM_LIBS) $(OPENGL_LIBS) -o $@

opengl$(APP_EXT): opengl.cpp.o imgui-src.cpp.o $(DAF_DIR)/build/libdgl-opengl.a
	@echo "Linking $@"
	$(SILENT)$(CXX) $^ $(LINK_FLAGS) $(DGL_SYSTEM_LIBS) $(OPENGL_LIBS) -o $@

textedit$(APP_EXT): textedit.cpp.o imgui-src.cpp.o $(DAF_DIR)/build/libdgl-opengl.a
	@echo "Linking $@"
	$(SILENT)$(CXX) $^ $(LINK_FLAGS) $(DGL_SYSTEM_LIBS) $(OPENGL_LIBS) -o $@

# ---------------------------------------------------------------------------------------------------------------------

duskwidgets.cpp.o: duskwidgets.cpp
	@echo "Compiling $<"
	$(SILENT)$(CXX) $< $(BUILD_CXX_FLAGS) $(OPENGL_FLAGS) -c -o $@

duskwidgets-src.cpp.o: ../opengl/DuskWidgets.cpp
	@echo "Compiling $<"
	$(SILENT)$(CXX) $< $(BUILD_CXX_FLAGS) $(OPENGL_FLAGS) -c -o $@

imgui.cpp.o: imgui.cpp
	@echo "Compiling $<"
	$(SILENT)$(CXX) $< $(BUILD_CXX_FLAGS) $(OPENGL_FLAGS) -c -o $@

imgui-src.cpp.o: imgui-src.cpp
	@echo "Compiling $<"
	$(SILENT)$(CXX) $< $(BUILD_CXX_FLAGS) $(OPENGL_FLAGS) -c -o $@

opengl.cpp.o: opengl.cpp
	@echo "Compiling $<"
	$(SILENT)$(CXX) $< $(BUILD_CXX_FLAGS) $(OPENGL_FLAGS) -c -o $@

textedit.cpp.o: textedit.cpp
	@echo "Compiling $<"
	$(SILENT)$(CXX) $< $(BUILD_CXX_FLAGS) $(OPENGL_FLAGS) -c -o $@

# ---------------------------------------------------------------------------------------------------------------------

-include cairo.cpp.d
-include duskwidgets.cpp.d
-include duskwidgets-src.cpp.d
-include imgui.cpp.d
-include imgui-src.cpp.d
-include opengl.cpp.d
-include textedit.cpp.d

# ---------------------------------------------------------------------------------------------------------------------
