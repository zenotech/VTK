vtk_module_test_data(
  Data/degenerated-hexahedrons.vtu
  Data/explicitStructuredGrid.vtu
  Data/ghost_cells.vtu
  Data/quadraticTetra01.vtu
  Data/quadratic_tets_with_ghost_cells_0.vtu)

if (NOT vtk_testing_cxx_disabled)
  add_subdirectory(Cxx)
endif ()

if (VTK_WRAP_PYTHON)
  vtk_module_test_data(
    Data/RectGrid2.vtk
    Data/combq.bin
    Data/combxyz.bin
    Data/office.binary.vtk)

  add_subdirectory(Python)
endif ()
