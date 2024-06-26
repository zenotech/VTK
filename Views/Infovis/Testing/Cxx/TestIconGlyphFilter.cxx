// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright 2007 Sandia Corporation
// SPDX-License-Identifier: LicenseRef-BSD-3-Clause-Sandia-USGov

#include <vtkIconGlyphFilter.h>

#include <vtkApplyIcons.h>
#include <vtkDoubleArray.h>
#include <vtkFollower.h>
#include <vtkGraphLayoutView.h>
#include <vtkGraphMapper.h>
#include <vtkImageData.h>
#include <vtkMutableUndirectedGraph.h>
#include <vtkPNGReader.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderedGraphRepresentation.h>
#include <vtkRenderer.h>
#include <vtkTexture.h>

#include <vtkRegressionTestImage.h>
#include <vtkTestUtilities.h>

int TestIconGlyphFilter(int argc, char* argv[])
{
  char* fname = vtkTestUtilities::ExpandDataFileName(argc, argv, "Data/Tango/TangoIcons.png");

  int imageDims[3];

  vtkPNGReader* imageReader = vtkPNGReader::New();

  imageReader->SetFileName(fname);
  imageReader->Update();

  delete[] fname;

  imageReader->GetOutput()->GetDimensions(imageDims);

  vtkMutableUndirectedGraph* graph = vtkMutableUndirectedGraph::New();
  vtkPoints* points = vtkPoints::New();
  vtkDoubleArray* pointData = vtkDoubleArray::New();
  pointData->SetNumberOfComponents(3);
  points->SetData(static_cast<vtkDataArray*>(pointData));
  graph->SetPoints(points);

  vtkIntArray* iconIndex = vtkIntArray::New();
  iconIndex->SetName("IconIndex");
  iconIndex->SetNumberOfComponents(1);

  graph->GetVertexData()->SetScalars(iconIndex);

  graph->AddVertex();
  points->InsertNextPoint(0.0, 0.0, 0.0);
  graph->AddVertex();
  points->InsertNextPoint(2.0, 0.0, 0.0);
  graph->AddVertex();
  points->InsertNextPoint(3.0, 0.0, 0.0);
  graph->AddVertex();
  points->InsertNextPoint(2.0, 2.5, 0.0);
  graph->AddVertex();
  points->InsertNextPoint(0.0, -2.0, 0.0);
  graph->AddVertex();
  points->InsertNextPoint(2.0, -1.5, 0.0);
  graph->AddVertex();
  points->InsertNextPoint(-1.0, 2.0, 0.0);
  graph->AddVertex();
  points->InsertNextPoint(3.0, 0.0, 0.0);

  graph->AddEdge(0, 1);
  graph->AddEdge(1, 2);
  graph->AddEdge(2, 3);
  graph->AddEdge(3, 4);
  graph->AddEdge(4, 5);
  graph->AddEdge(5, 6);
  graph->AddEdge(6, 7);
  graph->AddEdge(7, 0);

  iconIndex->InsertNextTuple1(1);
  iconIndex->InsertNextTuple1(4);
  iconIndex->InsertNextTuple1(26);
  iconIndex->InsertNextTuple1(17);
  iconIndex->InsertNextTuple1(0);
  iconIndex->InsertNextTuple1(5);
  iconIndex->InsertNextTuple1(1);
  iconIndex->InsertNextTuple1(29);

  vtkGraphLayoutView* view = vtkGraphLayoutView::New();
  view->DisplayHoverTextOff();
  view->SetRepresentationFromInput(graph);
  view->SetLayoutStrategyToSimple2D();
  view->ResetCamera();

  vtkTexture* texture = vtkTexture::New();
  texture->SetInputConnection(imageReader->GetOutputPort());
  view->SetIconTexture(texture);
  int size[] = { 24, 24 };
  view->SetIconSize(size);
  vtkRenderedGraphRepresentation* rep =
    static_cast<vtkRenderedGraphRepresentation*>(view->GetRepresentation());
  rep->UseVertexIconTypeMapOff();
  rep->SetVertexSelectedIcon(12);
  rep->SetVertexIconSelectionModeToSelectedIcon();
  rep->VertexIconVisibilityOn();
  rep->SetVertexIconArrayName(iconIndex->GetName());
  rep->SetLayoutStrategyToPassThrough();

  view->GetRenderWindow()->SetSize(500, 500);

  view->GetInteractor()->Initialize();
  view->Render();
  int retVal = vtkRegressionTestImageThreshold(view->GetRenderWindow(), 0.05);
  if (retVal == vtkRegressionTester::DO_INTERACTOR)
  {
    view->GetInteractor()->Start();
  }

  imageReader->Delete();
  iconIndex->Delete();
  graph->Delete();
  points->Delete();
  pointData->Delete();
  view->Delete();
  texture->Delete();

  return !retVal;
}
