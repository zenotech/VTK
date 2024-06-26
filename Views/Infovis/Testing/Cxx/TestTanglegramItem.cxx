// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkDataSetAttributes.h"
#include "vtkDoubleArray.h"
#include "vtkMutableDirectedGraph.h"
#include "vtkNew.h"
#include "vtkStringArray.h"
#include "vtkTable.h"
#include "vtkTanglegramItem.h"
#include "vtkTree.h"

#include "vtkContextActor.h"
#include "vtkContextInteractorStyle.h"
#include "vtkContextMouseEvent.h"
#include "vtkContextScene.h"
#include "vtkContextTransform.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"

#include "vtkRegressionTestImage.h"

//------------------------------------------------------------------------------
int TestTanglegramItem(int argc, char* argv[])
{
  // tree #1
  vtkNew<vtkMutableDirectedGraph> graph1;
  vtkIdType root = graph1->AddVertex();
  vtkIdType internalOne = graph1->AddChild(root);
  vtkIdType internalTwo = graph1->AddChild(internalOne);
  vtkIdType a = graph1->AddChild(internalTwo);
  vtkIdType b = graph1->AddChild(internalTwo);
  vtkIdType c = graph1->AddChild(internalOne);

  vtkNew<vtkDoubleArray> weights;
  weights->SetNumberOfTuples(5);
  weights->SetValue(graph1->GetEdgeId(root, internalOne), 1.0f);
  weights->SetValue(graph1->GetEdgeId(internalOne, internalTwo), 2.0f);
  weights->SetValue(graph1->GetEdgeId(internalTwo, a), 1.0f);
  weights->SetValue(graph1->GetEdgeId(internalTwo, b), 1.0f);
  weights->SetValue(graph1->GetEdgeId(internalOne, c), 3.0f);

  weights->SetName("weight");
  graph1->GetEdgeData()->AddArray(weights);

  vtkNew<vtkStringArray> names1;
  names1->SetNumberOfTuples(6);
  names1->SetValue(a, "cat");
  names1->SetValue(b, "dog");
  names1->SetValue(c, "human");

  names1->SetName("node name");
  graph1->GetVertexData()->AddArray(names1);

  vtkNew<vtkDoubleArray> nodeWeights;
  nodeWeights->SetNumberOfTuples(6);
  nodeWeights->SetValue(root, 0.0f);
  nodeWeights->SetValue(internalOne, 1.0f);
  nodeWeights->SetValue(internalTwo, 3.0f);
  nodeWeights->SetValue(a, 4.0f);
  nodeWeights->SetValue(b, 4.0f);
  nodeWeights->SetValue(c, 4.0f);
  nodeWeights->SetName("node weight");
  graph1->GetVertexData()->AddArray(nodeWeights);

  // tree #2
  vtkNew<vtkMutableDirectedGraph> graph2;
  root = graph2->AddVertex();
  internalOne = graph2->AddChild(root);
  internalTwo = graph2->AddChild(internalOne);
  a = graph2->AddChild(internalTwo);
  b = graph2->AddChild(internalTwo);
  c = graph2->AddChild(internalOne);

  weights->SetName("weight");
  graph2->GetEdgeData()->AddArray(weights);

  vtkNew<vtkStringArray> names2;
  names2->SetNumberOfTuples(6);
  names2->SetValue(a, "dog food");
  names2->SetValue(b, "cat food");
  names2->SetValue(c, "steak");

  names2->SetName("node name");
  graph2->GetVertexData()->AddArray(names2);

  graph2->GetVertexData()->AddArray(nodeWeights);

  // set up correspondence table: who eats what
  vtkNew<vtkTable> table;
  vtkNew<vtkStringArray> eaters;
  vtkNew<vtkDoubleArray> hungerForSteak;
  hungerForSteak->SetName("steak");
  vtkNew<vtkDoubleArray> hungerForDogFood;
  hungerForDogFood->SetName("dog food");
  vtkNew<vtkDoubleArray> hungerForCatFood;
  hungerForCatFood->SetName("cat food");

  eaters->SetNumberOfTuples(3);
  hungerForSteak->SetNumberOfTuples(3);
  hungerForDogFood->SetNumberOfTuples(3);
  hungerForCatFood->SetNumberOfTuples(3);

  eaters->SetValue(0, "human");
  eaters->SetValue(1, "dog");
  eaters->SetValue(2, "cat");

  hungerForSteak->SetValue(0, 2.0);
  hungerForSteak->SetValue(1, 1.0);
  hungerForSteak->SetValue(2, 1.0);

  hungerForDogFood->SetValue(0, 0.0);
  hungerForDogFood->SetValue(1, 2.0);
  hungerForDogFood->SetValue(2, 0.0);

  hungerForCatFood->SetValue(0, 0.0);
  hungerForCatFood->SetValue(1, 1.0);
  hungerForCatFood->SetValue(2, 2.0);

  table->AddColumn(eaters);
  table->AddColumn(hungerForSteak);
  table->AddColumn(hungerForDogFood);
  table->AddColumn(hungerForCatFood);

  vtkNew<vtkContextActor> actor;

  vtkNew<vtkTree> tree1;
  tree1->ShallowCopy(graph1);

  vtkNew<vtkTree> tree2;
  tree2->ShallowCopy(graph2);

  vtkNew<vtkTanglegramItem> tanglegram;
  tanglegram->SetTree1(tree1);
  tanglegram->SetTree2(tree2);
  tanglegram->SetTable(table);
  tanglegram->SetTree1Label("Diners");
  tanglegram->SetTree2Label("Meals");

  vtkNew<vtkContextTransform> trans;
  trans->SetInteractive(true);
  trans->AddItem(tanglegram);
  // center the item within the render window
  trans->Translate(20, 75);
  trans->Scale(1.25, 1.25);
  actor->GetScene()->AddItem(trans);

  vtkNew<vtkRenderer> renderer;
  renderer->SetBackground(1.0, 1.0, 1.0);

  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->SetSize(400, 200);
  renderWindow->AddRenderer(renderer);
  renderer->AddActor(actor);
  actor->GetScene()->SetRenderer(renderer);

  vtkNew<vtkContextInteractorStyle> interactorStyle;
  interactorStyle->SetScene(actor->GetScene());

  vtkNew<vtkRenderWindowInteractor> interactor;
  interactor->SetInteractorStyle(interactorStyle);
  interactor->SetRenderWindow(renderWindow);
  renderWindow->SetMultiSamples(0);
  renderWindow->Render();

  int retVal = vtkRegressionTestImageThreshold(renderWindow, 0.05);
  if (retVal == vtkRegressionTester::DO_INTERACTOR)
  {
    renderWindow->Render();
    interactor->Start();
    retVal = vtkRegressionTester::PASSED;
  }
  return !retVal;
}
