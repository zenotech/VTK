set(Module_SRCS
  vtkAMRDataSetCache.cxx
  vtkAMRBaseParticlesReader.cxx
  vtkAMRBaseReader.cxx
  vtkAMREnzoParticlesReader.cxx
  vtkAMREnzoReader.cxx
  vtkAMREnzoReaderInternal.cxx
  vtkAMRFlashParticlesReader.cxx
  vtkAMRFlashReader.cxx
  vtkAMRFlashReaderInternal.cxx
  )

set_source_files_properties(
  vtkAMRBaseReader
  vtkAMRBaseParticlesReader
  ABSTRACT
  )

set_source_files_properties(
  vtkAMREnzoReaderInternal
  vtkAMRFlashReaderInternal
  PROPERTIES
    WRAP_EXCLUDE 1
    WRAP_EXCLUDE_PYTHON 1
  )

include(vtkMPI)

vtk_module_library(vtkIOAMR ${Module_SRCS})
if(PARAVIEW_USE_MPI)
    vtk_mpi_link(vtkIOAMR)
endif()
