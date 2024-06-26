// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkActor.h"
#include "vtkCamera.h"
#include "vtkCompositeDataDisplayAttributes.h"
#include "vtkCompositeDataSet.h"
#include "vtkCompositePolyDataMapper.h"
#include "vtkCullerCollection.h"
#include "vtkInformation.h"
#include "vtkMath.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkNew.h"
#include "vtkPointDataToCellData.h"
#include "vtkProperty.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkRenderingOpenGLConfigure.h"
#include "vtkSmartPointer.h"
#include "vtkTimerLog.h"
#include "vtkTrivialProducer.h"

#include <vtkRegressionTestImage.h>
#include <vtkTestUtilities.h>

#include "vtkAppendPolyData.h"
#include "vtkCylinderSource.h"
#include "vtkElevationFilter.h"
#include "vtkExtractEdges.h"
#include "vtkPlaneSource.h"

int TestCompositePolyDataMapperMixedGeometryEdges(int argc, char* argv[])
{
  bool timeit = false;
  if (argc > 1 && argv[1] && !strcmp(argv[1], "-timeit"))
  {
    timeit = true;
  }

  vtkSmartPointer<vtkRenderWindow> win = vtkSmartPointer<vtkRenderWindow>::New();
  vtkSmartPointer<vtkRenderWindowInteractor> iren =
    vtkSmartPointer<vtkRenderWindowInteractor>::New();
  vtkSmartPointer<vtkRenderer> ren = vtkSmartPointer<vtkRenderer>::New();
  win->AddRenderer(ren);
  win->SetInteractor(iren);

  win->SetMultiSamples(0);

  vtkSmartPointer<vtkCompositePolyDataMapper> mapper =
    vtkSmartPointer<vtkCompositePolyDataMapper>::New();
  vtkNew<vtkCompositeDataDisplayAttributes> cdsa;
  mapper->SetCompositeDataDisplayAttributes(cdsa.GetPointer());

  int resolution = 18;
  vtkNew<vtkCylinderSource> cyl;
  cyl->CappingOn();
  cyl->SetRadius(0.2);
  cyl->SetHeight(0.8);
  cyl->SetResolution(resolution);

  vtkNew<vtkPlaneSource> plane;
  plane->SetResolution(resolution, resolution);
  plane->SetOrigin(-0.2, -0.2, 0.0);
  plane->SetPoint1(0.2, -0.2, 0.0);
  plane->SetPoint2(-0.2, 0.2, 0.0);

  vtkNew<vtkExtractEdges> extract;
  extract->SetInputConnection(plane->GetOutputPort());

  vtkNew<vtkAppendPolyData> append;
  append->SetUserManagedInputs(1);
  append->SetNumberOfInputs(2);
  append->SetInputConnectionByNumber(0, cyl->GetOutputPort());
  append->SetInputConnectionByNumber(1, extract->GetOutputPort());

  vtkNew<vtkElevationFilter> elev;
  elev->SetInputConnection(append->GetOutputPort());

  vtkNew<vtkPointDataToCellData> p2c;
  p2c->SetInputConnection(elev->GetOutputPort());
  p2c->PassPointDataOff();

  // build a composite dataset
  vtkNew<vtkMultiBlockDataSet> data;
  int blocksPerLevel[3] = { 1, 8, 16 };
  if (timeit)
  {
    blocksPerLevel[1] = 64;
    blocksPerLevel[2] = 256;
  }
  std::vector<vtkSmartPointer<vtkMultiBlockDataSet>> blocks;
  blocks.emplace_back(data.GetPointer());
  unsigned levelStart = 0;
  unsigned levelEnd = 1;
  int numLevels = sizeof(blocksPerLevel) / sizeof(blocksPerLevel[0]);
  int numLeaves = 0;
  int numNodes = 0;
  std::string blockName("Rolf");
  mapper->SetInputDataObject(data.GetPointer());
  for (int level = 1; level < numLevels; ++level)
  {
    int nblocks = blocksPerLevel[level];
    for (unsigned parent = levelStart; parent < levelEnd; ++parent)
    {
      blocks[parent]->SetNumberOfBlocks(nblocks);
      for (int block = 0; block < nblocks; ++block, ++numNodes)
      {
        if (level == numLevels - 1)
        {
          vtkNew<vtkPolyData> child;
          cyl->SetCenter(block * 0.25, -0.3, parent * 0.5);
          plane->SetCenter(block * 0.25, 0.5, parent * 0.5);
          elev->SetLowPoint(block * 0.25 - 0.2 + 0.2 * block / nblocks, -0.02, 0.0);
          elev->SetHighPoint(block * 0.25 + 0.1 + 0.2 * block / nblocks, 0.02, 0.0);
          p2c->Update();
          child->DeepCopy(p2c->GetOutput(0));
          blocks[parent]->SetBlock(block, (block % 2) ? nullptr : child.GetPointer());
          blocks[parent]->GetMetaData(block)->Set(vtkCompositeDataSet::NAME(), blockName.c_str());
          // test not setting it on some
          if (block % 11)
          {
            mapper->SetBlockVisibility(parent + numLeaves, (block % 7) != 0);
          }
          ++numLeaves;
        }
        else
        {
          vtkNew<vtkMultiBlockDataSet> child;
          blocks[parent]->SetBlock(block, child.GetPointer());
          blocks.emplace_back(child.GetPointer());
        }
      }
    }
    levelStart = levelEnd;
    levelEnd = static_cast<unsigned>(blocks.size());
  }

  mapper->SetScalarModeToUseCellData();

  vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
  actor->SetMapper(mapper);
  actor->GetProperty()->SetEdgeColor(1, 1, 1);
  actor->GetProperty()->EdgeVisibilityOn();
  ren->AddActor(actor);
  win->SetSize(400, 400);

  ren->RemoveCuller(ren->GetCullers()->GetLastItem());
  ren->ResetCamera();

  vtkSmartPointer<vtkTimerLog> timer = vtkSmartPointer<vtkTimerLog>::New();
  win->Render(); // get the window up

  // modify the data to force a rebuild of OpenGL structs
  // after rendering set one cylinder to white
  mapper->SetBlockColor(54, 1.0, 1.0, 1.0);
  mapper->SetBlockOpacity(54, 1.0);
  mapper->SetBlockVisibility(54, 1.0);

  timer->StartTimer();
  win->Render();
  timer->StopTimer();
  cout << "First frame time: " << timer->GetElapsedTime() << "\n";

  timer->StartTimer();

  int numFrames = (timeit ? 300 : 2);
  for (int i = 0; i <= numFrames; i++)
  {
    ren->GetActiveCamera()->Elevation(15.0 / numFrames);
    ren->GetActiveCamera()->Azimuth(-190.0 / numFrames);
    ren->GetActiveCamera()->Zoom(pow(1.5, 1.0 / numFrames));
    ren->GetActiveCamera()->Roll(5.0 / numFrames);
    win->Render();
  }

  timer->StopTimer();
  if (timeit)
  {
    double t = timer->GetElapsedTime();
    cout << "Avg Frame time: " << t / numFrames << " Frame Rate: " << numFrames / t << "\n";
  }

  int retVal = vtkRegressionTestImageThreshold(win.GetPointer(), 0.05);
  if (retVal == vtkRegressionTester::DO_INTERACTOR)
  {
    iren->Start();
  }

  return !retVal;
}
