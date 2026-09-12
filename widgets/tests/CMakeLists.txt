daf__add_dgl_opengl3(TRUE TRUE TRUE)
daf__add_widgets_dusk()
daf__add_widgets_imgui(dgl-opengl3)

add_executable(daf-widgets-gallery duskwidgets.cpp)
target_compile_definitions(daf-widgets-gallery PRIVATE
  DAF_WIDGETS_GALLERY_LINKED DONT_SET_USING_DGL_NAMESPACE)
target_link_libraries(daf-widgets-gallery PRIVATE daf-widgets-imgui)
