/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkPolyDataPlaneCutter.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkPolyDataPlaneCutter
 * @brief   threaded (high-performance) cutting of a vtkPolyData with a plane
 *
 * vtkPolyDataPlaneCutter cuts an input vtkPolyData with a plane to produce
 * an output vtkPolyData. (Here cutting means slicing through the polydata to
 * generates lines of intersection.) The input vtkPolyData must consist of
 * convex polygons - vertices, lines, and triangle strips are ignored. (Note:
 * use vtkTriangleFilter to triangulate non-convex input polygons if
 * necessary. If the input cells are non-convex, then the cutting operation
 * will likely produce erroneous results.)
 *
 * The main difference between this filter and other cutting filters is that
 * vtkPolyDataPlaneCutter is tuned for performance on vtkPolyData with convex
 * polygonal cells.
 *
 * @warning
 * This class has been threaded with vtkSMPTools. Using TBB or other
 * non-sequential type (set in the CMake variable
 * VTK_SMP_IMPLEMENTATION_TYPE) may improve performance significantly.
 *
 * @sa
 * vtkPlaneCutter vtkCutter vtkPolyDataPlaneClipper
 */

#ifndef vtkPolyDataPlaneCutter_h
#define vtkPolyDataPlaneCutter_h

#include "vtkFiltersCoreModule.h" // For export macro
#include "vtkPlane.h"             // For cutting plane
#include "vtkPolyDataAlgorithm.h"
#include "vtkSmartPointer.h" // For SmartPointer

class VTKFILTERSCORE_EXPORT vtkPolyDataPlaneCutter : public vtkPolyDataAlgorithm
{
public:
  ///@{
  /**
   * Standard construction, type, and print methods.
   */
  static vtkPolyDataPlaneCutter* New();
  vtkTypeMacro(vtkPolyDataPlaneCutter, vtkPolyDataAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) override;
  ///@}

  ///@{
  /**
   * Specify the plane (an implicit function) to perform the cutting. The
   * definition of the plane used to perform the cutting (i.e., its origin
   * and normal) is controlled via this instance of vtkPlane.
   */
  void SetPlane(vtkPlane*);
  vtkGetObjectMacro(Plane, vtkPlane);
  ///@}

  ///@{
  /**
   * Set/get the desired precision for the output points type. See the
   * documentation for the vtkAlgorithm::DesiredOutputPrecision enum for an
   * explanation of the available precision settings. OutputPointsPrecision
   * is DEFAULT_PRECISION by default.
   */
  vtkSetMacro(OutputPointsPrecision, int);
  vtkGetMacro(OutputPointsPrecision, int);
  ///@}

  /**
   * The modified time depends on the delegated cutting plane.
   */
  vtkMTimeType GetMTime() override;

  ///@{
  /**
   * Specify the number of input cells in a batch, where a batch defines
   * a subset of the input cells operated on during threaded
   * execution. Generally this is only used for debugging or performance
   * studies (since batch size affects the thread workload). By default,
   * the batch size is 10,000 cells.
   */
  vtkSetClampMacro(BatchSize, unsigned int, 1, VTK_INT_MAX);
  vtkGetMacro(BatchSize, unsigned int);
  ///@}

protected:
  vtkPolyDataPlaneCutter();
  ~vtkPolyDataPlaneCutter() override;

  vtkSmartPointer<vtkPlane> Plane;
  int OutputPointsPrecision;
  unsigned int BatchSize;

  // Pipeline-related methods
  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;

private:
  vtkPolyDataPlaneCutter(const vtkPolyDataPlaneCutter&) = delete;
  void operator=(const vtkPolyDataPlaneCutter&) = delete;
};

#endif