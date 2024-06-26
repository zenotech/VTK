// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkDataObjectAlgorithm.h"

#include "vtkCommand.h"
#include "vtkDataObject.h"
#include "vtkDataObjectTypes.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkDataObjectAlgorithm);

//------------------------------------------------------------------------------
vtkDataObjectAlgorithm::vtkDataObjectAlgorithm()
{
  // by default assume filters have one input and one output
  // subclasses that deviate should modify this setting
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

//------------------------------------------------------------------------------
vtkDataObjectAlgorithm::~vtkDataObjectAlgorithm() = default;

//------------------------------------------------------------------------------
void vtkDataObjectAlgorithm::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//------------------------------------------------------------------------------
vtkDataObject* vtkDataObjectAlgorithm::GetOutput()
{
  return this->GetOutput(0);
}

//------------------------------------------------------------------------------
vtkDataObject* vtkDataObjectAlgorithm::GetOutput(int port)
{
  return this->GetOutputDataObject(port);
}

//------------------------------------------------------------------------------
void vtkDataObjectAlgorithm::SetOutput(vtkDataObject* d)
{
  this->GetExecutive()->SetOutputData(0, d);
}

//------------------------------------------------------------------------------
vtkDataObject* vtkDataObjectAlgorithm::GetInput()
{
  return this->GetInput(0);
}

//------------------------------------------------------------------------------
vtkDataObject* vtkDataObjectAlgorithm::GetInput(int port)
{
  if (this->GetNumberOfInputConnections(port) < 1)
  {
    return nullptr;
  }
  return this->GetExecutive()->GetInputData(port, 0);
}

//------------------------------------------------------------------------------
vtkTypeBool vtkDataObjectAlgorithm::ProcessRequest(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  // generate the data
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_DATA()))
  {
    return this->RequestData(request, inputVector, outputVector);
  }

  if (request->Has(vtkStreamingDemandDrivenPipeline::REQUEST_UPDATE_EXTENT()))
  {
    return this->RequestUpdateExtent(request, inputVector, outputVector);
  }

  if (request->Has(vtkStreamingDemandDrivenPipeline::REQUEST_UPDATE_TIME()))
  {
    return this->RequestUpdateTime(request, inputVector, outputVector);
  }

  // Create data object output
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_DATA_OBJECT()))
  {
    return this->RequestDataObject(request, inputVector, outputVector);
  }

  // execute information
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_INFORMATION()))
  {
    return this->RequestInformation(request, inputVector, outputVector);
  }

  return this->Superclass::ProcessRequest(request, inputVector, outputVector);
}

//------------------------------------------------------------------------------
int vtkDataObjectAlgorithm::FillOutputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  // now add our info
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkDataObject");
  return 1;
}

//------------------------------------------------------------------------------
int vtkDataObjectAlgorithm::FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
  return 1;
}

//------------------------------------------------------------------------------
int vtkDataObjectAlgorithm::RequestInformation(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** vtkNotUsed(inputVector), vtkInformationVector* vtkNotUsed(outputVector))
{
  // do nothing let subclasses handle it
  return 1;
}

//------------------------------------------------------------------------------
void vtkDataObjectAlgorithm::SetInputData(vtkDataObject* input)
{
  this->SetInputData(0, input);
}

//------------------------------------------------------------------------------
void vtkDataObjectAlgorithm::SetInputData(int index, vtkDataObject* input)
{
  this->SetInputDataInternal(index, input);
}

//------------------------------------------------------------------------------
void vtkDataObjectAlgorithm::AddInputData(vtkDataObject* input)
{
  this->AddInputData(0, input);
}

//------------------------------------------------------------------------------
void vtkDataObjectAlgorithm::AddInputData(int index, vtkDataObject* input)
{
  this->AddInputDataInternal(index, input);
}

//------------------------------------------------------------------------------
bool vtkDataObjectAlgorithm::SetOutputDataObject(
  int dataType, vtkInformation* outInfo, bool exact /*=false*/)
{
  if (!outInfo)
  {
    return false;
  }

  auto dobj = vtkDataObject::GetData(outInfo);
  if (dobj == nullptr ||
    (!exact && vtkDataObjectTypes::TypeIdIsA(dobj->GetDataObjectType(), dataType)) ||
    (exact && dobj->GetDataObjectType() != dataType))
  {
    dobj = vtkDataObjectTypes::NewDataObject(dataType);
    if (!dobj)
    {
      return false;
    }

    outInfo->Set(vtkDataObject::DATA_OBJECT(), dobj);
    outInfo->Set(vtkDataObject::DATA_EXTENT_TYPE(), dobj->GetExtentType());
    dobj->FastDelete();
  }
  return true;
}
VTK_ABI_NAMESPACE_END
