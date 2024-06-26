set(classes
  vtkBatchedSurfaceLICMapper
  vtkCompositeSurfaceLICMapper
  vtkCompositeSurfaceLICMapperDelegator
  vtkImageDataLIC2D
  vtkLineIntegralConvolution2D
  vtkStructuredGridLIC2D
  vtkSurfaceLICComposite
  vtkSurfaceLICInterface
  vtkSurfaceLICMapper
  vtkTextureIO)

set(nowrap_classes
  vtkLICNoiseHelper
  vtkSurfaceLICHelper)

set(headers
  vtkPainterCommunicator.h)

set(shader_files
  vtkLineIntegralConvolution2D_AAH.glsl
  vtkLineIntegralConvolution2D_AAV.glsl
  vtkLineIntegralConvolution2D_CE.glsl
  vtkLineIntegralConvolution2D_EE.glsl
  vtkLineIntegralConvolution2D_LIC0.glsl
  vtkLineIntegralConvolution2D_LICI.glsl
  vtkLineIntegralConvolution2D_LICN.glsl
  vtkLineIntegralConvolution2D_VT.glsl
  vtkStructuredGridLIC2D_fs.glsl
  vtkSurfaceLICInterface_CE.glsl
  vtkSurfaceLICInterface_DCpy.glsl
  vtkSurfaceLICInterface_SC.glsl)

set(shader_h_files)
set(shader_cxx_files)
foreach (shader_file IN LISTS shader_files)
  vtk_encode_string(
    INPUT         "${shader_file}"
    EXPORT_HEADER "vtkRenderingLICOpenGL2Module.h"
    EXPORT_SYMBOL "VTKRENDERINGLICOPENGL2_NO_EXPORT"
    HEADER_OUTPUT header
    SOURCE_OUTPUT source)
  list(APPEND shader_cxx_files ${source})
  list(APPEND shader_h_files ${header})
endforeach ()

vtk_module_add_module(VTK::RenderingLICOpenGL2
  CLASSES ${classes}
  NOWRAP_CLASSES ${nowrap_classes}
  SOURCES ${shader_cxx_files}
  HEADERS ${headers}
  PRIVATE_HEADERS ${shader_h_files})

if (VTK_RENDERINGPARALLELLIC_SURFACELICPAINTER_TIMER)
  vtk_module_definitions(VTK::RenderingLICOpenGL2
    PRIVATE
      vtkSurfaceLICPainterTIME)
endif ()
if (VTK_RENDERINGPARALLELLIC_LINEINTEGRALCONVOLUTION2D_TIMER)
  vtk_module_definitions(VTK::RenderingLICOpenGL2
    PRIVATE
      vtkLineIntegralConvolution2DTIME)
endif ()
vtk_add_test_mangling(VTK::RenderingLICOpenGL2)
