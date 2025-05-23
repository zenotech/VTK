// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkAbstractTransform.h"

#include "vtkDataArray.h"
#include "vtkDebugLeaks.h"
#include "vtkIndent.h"
#include "vtkLinearTransform.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkPoints.h"
#include "vtkSMPTools.h"

#include <mutex> // for std::mutex

VTK_ABI_NAMESPACE_BEGIN

class vtkAbstractTransform::vtkInternals
{
public:
  // We need to record the time of the last update, and we also need
  // to do mutex locking so updates don't collide.  These are private
  // because Update() is not virtual.
  // If DependsOnInverse is set, then this transform object will
  // check its inverse on every update, and update itself accordingly
  // if necessary.

  vtkTimeStamp UpdateTime;
  std::mutex UpdateMutex;
  std::mutex InverseMutex;

  // MyInverse is a transform which is the inverse of this one.

  vtkAbstractTransform* MyInverse;

  bool DependsOnInverse;
  bool InUpdate;
  bool InUnRegister;
};

//------------------------------------------------------------------------------
vtkAbstractTransform::vtkAbstractTransform()
{
  this->Internals = new vtkInternals;
  this->Internals->MyInverse = nullptr;
  this->Internals->DependsOnInverse = false;
  this->Internals->InUpdate = false;
  this->Internals->InUnRegister = false;
}

//------------------------------------------------------------------------------
vtkAbstractTransform::~vtkAbstractTransform()
{
  if (this->Internals->MyInverse)
  {
    this->Internals->MyInverse->Delete();
  }

  if (this->Internals)
  {
    delete this->Internals;
    this->Internals = nullptr;
  }
}

//------------------------------------------------------------------------------
void vtkAbstractTransform::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Inverse: (" << this->Internals->MyInverse << ")\n";
}

//------------------------------------------------------------------------------
void vtkAbstractTransform::TransformNormalAtPoint(
  const double point[3], const double in[3], double out[3])
{
  this->Update();

  double matrix[3][3];
  double coord[3];

  this->InternalTransformDerivative(point, coord, matrix);
  vtkMath::Transpose3x3(matrix, matrix);
  vtkMath::LinearSolve3x3(matrix, in, out);
  vtkMath::Normalize(out);
}

void vtkAbstractTransform::TransformNormalAtPoint(
  const float point[3], const float in[3], float out[3])
{
  double coord[3];
  double normal[3];

  coord[0] = point[0];
  coord[1] = point[1];
  coord[2] = point[2];

  normal[0] = in[0];
  normal[1] = in[1];
  normal[2] = in[2];

  this->TransformNormalAtPoint(coord, normal, normal);

  out[0] = static_cast<float>(normal[0]);
  out[1] = static_cast<float>(normal[1]);
  out[2] = static_cast<float>(normal[2]);
}

//------------------------------------------------------------------------------
void vtkAbstractTransform::TransformVectorAtPoint(
  const double point[3], const double in[3], double out[3])
{
  this->Update();

  double matrix[3][3];
  double coord[3];

  this->InternalTransformDerivative(point, coord, matrix);
  vtkMath::Multiply3x3(matrix, in, out);
}

void vtkAbstractTransform::TransformVectorAtPoint(
  const float point[3], const float in[3], float out[3])
{
  double coord[3];
  double vector[3];

  coord[0] = point[0];
  coord[1] = point[1];
  coord[2] = point[2];

  vector[0] = in[0];
  vector[1] = in[1];
  vector[2] = in[2];

  this->TransformVectorAtPoint(coord, vector, vector);

  out[0] = static_cast<float>(vector[0]);
  out[1] = static_cast<float>(vector[1]);
  out[2] = static_cast<float>(vector[2]);
}

//------------------------------------------------------------------------------
// Transform a series of points.
void vtkAbstractTransform::TransformPoints(vtkPoints* inPts, vtkPoints* outPts)
{
  this->Update();

  vtkIdType n = inPts->GetNumberOfPoints();
  vtkIdType m = outPts->GetNumberOfPoints();
  outPts->SetNumberOfPoints(m + n);

  vtkSMPTools::For(0, n,
    [&](vtkIdType ptId, vtkIdType endPtId)
    {
      double point[3];
      for (; ptId < endPtId; ++ptId)
      {
        inPts->GetPoint(ptId, point);
        this->InternalTransformPoint(point, point);
        outPts->SetPoint(m + ptId, point);
      }
    });
}

//------------------------------------------------------------------------------
// Transform the normals and vectors using the derivative of the
// transformation.  Either inNms or inVrs can be set to nullptr.
// Normals are multiplied by the inverse transpose of the transform
// derivative, while vectors are simply multiplied by the derivative.
// Note that the derivative of the inverse transform is simply the
// inverse of the derivative of the forward transform.

void vtkAbstractTransform::TransformPointsNormalsVectors(vtkPoints* inPts, vtkPoints* outPts,
  vtkDataArray* inNms, vtkDataArray* outNms, vtkDataArray* inVrs, vtkDataArray* outVrs,
  int nOptionalVectors, vtkDataArray** inVrsArr, vtkDataArray** outVrsArr)
{
  this->Update();

  vtkIdType n = inPts->GetNumberOfPoints();
  vtkIdType m = outPts->GetNumberOfPoints();
  outPts->SetNumberOfPoints(m + n);
  if (inVrs)
  {
    outVrs->SetNumberOfTuples(m + n);
  }
  if (inVrsArr)
  {
    for (int iArr = 0; iArr < nOptionalVectors; iArr++)
    {
      outVrsArr[iArr]->SetNumberOfTuples(m + n);
    }
  }
  if (inNms)
  {
    outNms->SetNumberOfTuples(m + n);
  }

  vtkSMPTools::For(0, n,
    [&](vtkIdType ptId, vtkIdType endPtId)
    {
      double matrix[3][3];
      double point[3];
      for (; ptId < endPtId; ++ptId)
      {
        inPts->GetPoint(ptId, point);
        this->InternalTransformDerivative(point, point, matrix);
        outPts->SetPoint(m + ptId, point);

        if (inVrs)
        {
          inVrs->GetTuple(ptId, point);
          vtkMath::Multiply3x3(matrix, point, point);
          outVrs->SetTuple(m + ptId, point);
        }
        if (inVrsArr)
        {
          for (int iArr = 0; iArr < nOptionalVectors; iArr++)
          {
            inVrsArr[iArr]->GetTuple(ptId, point);
            vtkMath::Multiply3x3(matrix, point, point);
            outVrsArr[iArr]->SetTuple(m + ptId, point);
          }
        }
        if (inNms)
        {
          inNms->GetTuple(ptId, point);
          vtkMath::Transpose3x3(matrix, matrix);
          vtkMath::LinearSolve3x3(matrix, point, point);
          vtkMath::Normalize(point);
          outNms->SetTuple(m + ptId, point);
        }
      }
    });
}

//------------------------------------------------------------------------------
vtkAbstractTransform* vtkAbstractTransform::GetInverse()
{
  auto& internals = *(this->Internals);
  internals.InverseMutex.lock();
  if (internals.MyInverse == nullptr)
  {
    // we create a circular reference here, it is dealt with in UnRegister
    internals.MyInverse = this->MakeTransform();
    internals.MyInverse->SetInverse(this);
  }
  internals.InverseMutex.unlock();
  return internals.MyInverse;
}

//------------------------------------------------------------------------------
void vtkAbstractTransform::SetInverse(vtkAbstractTransform* transform)
{
  auto& internals = *(this->Internals);
  if (internals.MyInverse == transform)
  {
    return;
  }

  // check type first
  if (!transform->IsA(this->GetClassName()))
  {
    vtkErrorMacro("SetInverse: requires a " << this->GetClassName() << ", a "
                                            << transform->GetClassName() << " is not compatible.");
    return;
  }

  if (transform->CircuitCheck(this))
  {
    vtkErrorMacro("SetInverse: this would create a circular reference.");
    return;
  }

  if (internals.MyInverse)
  {
    internals.MyInverse->Delete();
  }

  transform->Register(this);
  internals.MyInverse = transform;

  // we are now a special 'inverse transform'
  internals.DependsOnInverse = (transform != nullptr);

  this->Modified();
}

//------------------------------------------------------------------------------
void vtkAbstractTransform::DeepCopy(vtkAbstractTransform* transform)
{
  // check whether we're trying to copy a transform to itself
  if (transform == this)
  {
    return;
  }

  // check to see if the transform is the same type as this one
  if (!transform->IsA(this->GetClassName()))
  {
    vtkErrorMacro("DeepCopy: can't copy a " << transform->GetClassName() << " into a "
                                            << this->GetClassName() << ".");
    return;
  }

  if (transform->CircuitCheck(this))
  {
    vtkErrorMacro("DeepCopy: this would create a circular reference.");
    return;
  }

  // call InternalDeepCopy for subtype
  this->InternalDeepCopy(transform);

  this->Modified();
}

//------------------------------------------------------------------------------
void vtkAbstractTransform::Update()
{
  auto& internals = *(this->Internals);

  // locking is required to ensure that the class is thread-safe
  internals.UpdateMutex.lock();
  internals.InUpdate = true;

  // check to see if we are a special 'inverse' transform
  if (internals.DependsOnInverse &&
    internals.MyInverse->GetMTime() >= internals.UpdateTime.GetMTime())
  {
    vtkDebugMacro("Updating transformation from its inverse");
    this->InternalDeepCopy(internals.MyInverse);
    this->Inverse();
    vtkDebugMacro("Calling InternalUpdate on the transformation");
    this->InternalUpdate();
  }
  // otherwise just check our MTime against our last update
  else if (this->GetMTime() >= internals.UpdateTime.GetMTime())
  {
    // do internal update for subclass
    vtkDebugMacro("Calling InternalUpdate on the transformation");
    this->InternalUpdate();
  }

  internals.InUpdate = false;
  internals.UpdateTime.Modified();
  internals.UpdateMutex.unlock();
}

//------------------------------------------------------------------------------
int vtkAbstractTransform::CircuitCheck(vtkAbstractTransform* transform)
{
  auto& internals = *(this->Internals);
  return (transform == this ||
    (internals.DependsOnInverse && internals.MyInverse->CircuitCheck(transform)));
}

//------------------------------------------------------------------------------
// Need to check inverse's MTime if we are an inverse transform
vtkMTimeType vtkAbstractTransform::GetMTime()
{
  auto& internals = *(this->Internals);
  vtkMTimeType mtime = this->vtkObject::GetMTime();
  if (internals.DependsOnInverse)
  {
    vtkMTimeType inverseMTime = internals.MyInverse->GetMTime();
    if (inverseMTime > mtime)
    {
      return inverseMTime;
    }
  }

  return mtime;
}

//------------------------------------------------------------------------------
// During an update, we don't want to generate ModifiedEvent because code
// observing the event might modify the transform while the transform's
// update is in progress (leading to corrupt state, deadlocks, infinite
// recursion, or other nastiness).
void vtkAbstractTransform::Modified()
{
  if (!this->Internals->InUpdate)
  {
    this->Superclass::Modified();
  }
}

//------------------------------------------------------------------------------
// We need to handle the circular reference between a transform and its
// inverse.
void vtkAbstractTransform::UnRegister(vtkObjectBase* o)
{
  auto& internals = *(this->Internals);
  if (internals.InUnRegister)
  { // we don't want to go into infinite recursion...
    vtkDebugMacro(<< "UnRegister: circular reference eliminated");
    --this->ReferenceCount;
    return;
  }

  // check to see if the only reason our reference count is not 1
  // is the circular reference from MyInverse
  if (internals.MyInverse && this->ReferenceCount == 2 &&
    internals.MyInverse->Internals->MyInverse == this && internals.MyInverse->ReferenceCount == 1)
  { // break the cycle
    vtkDebugMacro(<< "UnRegister: eliminating circular reference");
    internals.InUnRegister = true;
    internals.MyInverse->UnRegister(this);
    internals.MyInverse = nullptr;
    internals.InUnRegister = false;
  }

  this->vtkObject::UnRegister(o);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// All of the following methods are for vtkTransformConcatenation
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// A very, very minimal transformation
class vtkSimpleTransform : public vtkLinearTransform
{
public:
  vtkTypeMacro(vtkSimpleTransform, vtkLinearTransform);
  static vtkSimpleTransform* New() { VTK_STANDARD_NEW_BODY(vtkSimpleTransform); }
  vtkAbstractTransform* MakeTransform() override { return vtkSimpleTransform::New(); }
  void Inverse() override
  {
    this->Matrix->Invert();
    this->Modified();
  }

protected:
  vtkSimpleTransform() = default;
  vtkSimpleTransform(const vtkSimpleTransform&);
  vtkSimpleTransform& operator=(const vtkSimpleTransform&);
};

//------------------------------------------------------------------------------
vtkTransformConcatenation::vtkTransformConcatenation()
{
  this->PreMatrix = nullptr;
  this->PostMatrix = nullptr;
  this->PreMatrixTransform = nullptr;
  this->PostMatrixTransform = nullptr;

  this->PreMultiplyFlag = 1;
  this->InverseFlag = 0;

  this->NumberOfTransforms = 0;
  this->NumberOfPreTransforms = 0;
  this->MaxNumberOfTransforms = 0;

  // The transform list is the list of the transforms to be concatenated.
  this->TransformList = nullptr;
}

//------------------------------------------------------------------------------
vtkTransformConcatenation::~vtkTransformConcatenation()
{
  if (this->NumberOfTransforms > 0)
  {
    for (int i = 0; i < this->NumberOfTransforms; i++)
    {
      vtkTransformPair* tuple = &this->TransformList[i];
      if (tuple->ForwardTransform)
      {
        tuple->ForwardTransform->Delete();
      }
      if (tuple->InverseTransform)
      {
        tuple->InverseTransform->Delete();
      }
    }
  }
  delete[] this->TransformList;
}

//------------------------------------------------------------------------------
void vtkTransformConcatenation::Concatenate(vtkAbstractTransform* trans)
{
  // in case either PreMatrix or PostMatrix is going to be pushed
  // into the concatenation from their position at the end
  if (this->PreMultiplyFlag && this->PreMatrix)
  {
    this->PreMatrix = nullptr;
    this->PreMatrixTransform = nullptr;
  }
  else if (!this->PreMultiplyFlag && this->PostMatrix)
  {
    this->PostMatrix = nullptr;
    this->PostMatrixTransform = nullptr;
  }

  vtkTransformPair* transList = this->TransformList;
  int n = this->NumberOfTransforms;
  this->NumberOfTransforms++;

  // check to see if we need to allocate more space
  if (this->NumberOfTransforms > this->MaxNumberOfTransforms)
  {
    int nMax = this->MaxNumberOfTransforms + 5;
    transList = new vtkTransformPair[nMax];
    for (int i = 0; i < n; i++)
    {
      transList[i].ForwardTransform = this->TransformList[i].ForwardTransform;
      transList[i].InverseTransform = this->TransformList[i].InverseTransform;
    }
    delete[] this->TransformList;
    this->TransformList = transList;
    this->MaxNumberOfTransforms = nMax;
  }

  // add the transform either the beginning or end of the list,
  // according to flags
  if (this->PreMultiplyFlag ^ this->InverseFlag)
  {
    for (int i = n; i > 0; i--)
    {
      transList[i].ForwardTransform = transList[i - 1].ForwardTransform;
      transList[i].InverseTransform = transList[i - 1].InverseTransform;
    }
    n = 0;
    this->NumberOfPreTransforms++;
  }

  trans->Register(nullptr);

  if (this->InverseFlag)
  {
    transList[n].ForwardTransform = nullptr;
    transList[n].InverseTransform = trans;
  }
  else
  {
    transList[n].ForwardTransform = trans;
    transList[n].InverseTransform = nullptr;
  }
}

//------------------------------------------------------------------------------
void vtkTransformConcatenation::Concatenate(const double elements[16])
{
  // concatenate the matrix with either the Pre- or PostMatrix
  if (this->PreMultiplyFlag)
  {
    if (this->PreMatrix == nullptr)
    {
      // add the matrix to the concatenation
      vtkSimpleTransform* mtrans = vtkSimpleTransform::New();
      this->Concatenate(mtrans);
      mtrans->Delete();
      this->PreMatrixTransform = mtrans;
      this->PreMatrix = mtrans->GetMatrix();
    }
    vtkMatrix4x4::Multiply4x4(*this->PreMatrix->Element, elements, *this->PreMatrix->Element);
    this->PreMatrix->Modified();
    this->PreMatrixTransform->Modified();
  }
  else
  {
    if (this->PostMatrix == nullptr)
    {
      // add the matrix to the concatenation
      vtkSimpleTransform* mtrans = vtkSimpleTransform::New();
      this->Concatenate(mtrans);
      mtrans->Delete();
      this->PostMatrixTransform = mtrans;
      this->PostMatrix = mtrans->GetMatrix();
    }
    vtkMatrix4x4::Multiply4x4(elements, *this->PostMatrix->Element, *this->PostMatrix->Element);
    this->PostMatrix->Modified();
    this->PostMatrixTransform->Modified();
  }
}

//------------------------------------------------------------------------------
void vtkTransformConcatenation::Translate(double x, double y, double z)
{
  if (x == 0.0 && y == 0.0 && z == 0.0)
  {
    return;
  }

  double matrix[4][4];
  vtkMatrix4x4::Identity(*matrix);

  matrix[0][3] = x;
  matrix[1][3] = y;
  matrix[2][3] = z;

  this->Concatenate(*matrix);
}

//------------------------------------------------------------------------------
void vtkTransformConcatenation::Rotate(double angle, double x, double y, double z)
{
  double matrix[4][4];
  vtkMatrix4x4::MatrixFromRotation(angle, x, y, z, *matrix);
  this->Concatenate(*matrix);
}

//------------------------------------------------------------------------------
void vtkTransformConcatenation::Scale(double x, double y, double z)
{
  if (x == 1.0 && y == 1.0 && z == 1.0)
  {
    return;
  }

  double matrix[4][4];
  vtkMatrix4x4::Identity(*matrix);

  matrix[0][0] = x;
  matrix[1][1] = y;
  matrix[2][2] = z;

  this->Concatenate(*matrix);
}

//------------------------------------------------------------------------------
void vtkTransformConcatenation::Inverse()
{
  // invert the matrices
  if (this->PreMatrix)
  {
    this->PreMatrix->Invert();
    this->PreMatrixTransform->Modified();
    int i = (this->InverseFlag ? this->NumberOfTransforms - 1 : 0);
    this->TransformList[i].SwapForwardInverse();
  }

  if (this->PostMatrix)
  {
    this->PostMatrix->Invert();
    this->PostMatrixTransform->Modified();
    int i = (this->InverseFlag ? 0 : this->NumberOfTransforms - 1);
    this->TransformList[i].SwapForwardInverse();
  }

  // swap the pre- and post-matrices
  vtkMatrix4x4* tmp = this->PreMatrix;
  vtkAbstractTransform* tmp2 = this->PreMatrixTransform;
  this->PreMatrix = this->PostMatrix;
  this->PreMatrixTransform = this->PostMatrixTransform;
  this->PostMatrix = tmp;
  this->PostMatrixTransform = tmp2;

  // what used to be pre-transforms are now post-transforms
  this->NumberOfPreTransforms = this->NumberOfTransforms - this->NumberOfPreTransforms;

  this->InverseFlag = !this->InverseFlag;
}

//------------------------------------------------------------------------------
void vtkTransformConcatenation::Identity()
{
  // forget the Pre- and PostMatrix
  this->PreMatrix = nullptr;
  this->PostMatrix = nullptr;
  this->PreMatrixTransform = nullptr;
  this->PostMatrixTransform = nullptr;

  // delete all the transforms
  if (this->NumberOfTransforms > 0)
  {
    for (int i = 0; i < this->NumberOfTransforms; i++)
    {
      vtkTransformPair* tuple = &this->TransformList[i];
      if (tuple->ForwardTransform)
      {
        tuple->ForwardTransform->Delete();
        tuple->ForwardTransform = nullptr;
      }
      if (tuple->InverseTransform)
      {
        tuple->InverseTransform->Delete();
        tuple->InverseTransform = nullptr;
      }
    }
  }
  this->NumberOfTransforms = 0;
  this->NumberOfPreTransforms = 0;
}

//------------------------------------------------------------------------------
vtkAbstractTransform* vtkTransformConcatenation::GetTransform(int i)
{
  // we walk through the list in reverse order if InverseFlag is set
  if (this->InverseFlag)
  {
    int j = this->NumberOfTransforms - i - 1;
    vtkTransformPair* tuple = &this->TransformList[j];
    // if inverse is nullptr, then get it from the forward transform
    if (tuple->InverseTransform == nullptr)
    {
      tuple->InverseTransform = tuple->ForwardTransform->GetInverse();
      tuple->InverseTransform->Register(nullptr);
    }
    return tuple->InverseTransform;
  }
  else
  {
    vtkTransformPair* tuple = &this->TransformList[i];
    // if transform is nullptr, then get it from its inverse
    if (tuple->ForwardTransform == nullptr)
    {
      tuple->ForwardTransform = tuple->InverseTransform->GetInverse();
      tuple->ForwardTransform->Register(nullptr);
    }
    return tuple->ForwardTransform;
  }
}

//------------------------------------------------------------------------------
vtkMTimeType vtkTransformConcatenation::GetMaxMTime()
{
  vtkMTimeType result = 0;
  vtkMTimeType mtime;

  for (int i = 0; i < this->NumberOfTransforms; i++)
  {
    vtkTransformPair* tuple = &this->TransformList[i];
    if (tuple->ForwardTransform)
    {
      mtime = tuple->ForwardTransform->GetMTime();
    }
    else
    {
      mtime = tuple->InverseTransform->GetMTime();
    }

    if (mtime > result)
    {
      result = mtime;
    }
  }

  return result;
}

//------------------------------------------------------------------------------
void vtkTransformConcatenation::DeepCopy(vtkTransformConcatenation* concat)
{
  // allocate a larger list if necessary
  if (this->MaxNumberOfTransforms < concat->NumberOfTransforms)
  {
    int newMax = concat->NumberOfTransforms;
    vtkTransformPair* newList = new vtkTransformPair[newMax];
    // copy items onto new list
    int i = 0;
    for (; i < this->NumberOfTransforms; i++)
    {
      newList[i].ForwardTransform = this->TransformList[i].ForwardTransform;
      newList[i].InverseTransform = this->TransformList[i].InverseTransform;
    }
    for (; i < concat->NumberOfTransforms; i++)
    {
      newList[i].ForwardTransform = nullptr;
      newList[i].InverseTransform = nullptr;
    }
    delete[] this->TransformList;
    this->MaxNumberOfTransforms = newMax;
    this->TransformList = newList;
  }

  // save the PreMatrix and PostMatrix in case they can be reused
  vtkSimpleTransform* oldPreMatrixTransform = nullptr;
  vtkSimpleTransform* oldPostMatrixTransform = nullptr;

  if (this->PreMatrix)
  {
    vtkTransformPair* tuple;
    if (this->InverseFlag)
    {
      tuple = &this->TransformList[this->NumberOfTransforms - 1];
      tuple->SwapForwardInverse();
    }
    else
    {
      tuple = &this->TransformList[0];
    }
    tuple->ForwardTransform = nullptr;
    if (tuple->InverseTransform)
    {
      tuple->InverseTransform->Delete();
      tuple->InverseTransform = nullptr;
    }
    oldPreMatrixTransform = static_cast<vtkSimpleTransform*>(this->PreMatrixTransform);
    this->PreMatrixTransform = nullptr;
    this->PreMatrix = nullptr;
  }

  if (this->PostMatrix)
  {
    vtkTransformPair* tuple;
    if (this->InverseFlag)
    {
      tuple = &this->TransformList[0];
      tuple->SwapForwardInverse();
    }
    else
    {
      tuple = &this->TransformList[this->NumberOfTransforms - 1];
    }
    tuple->ForwardTransform = nullptr;
    if (tuple->InverseTransform)
    {
      tuple->InverseTransform->Delete();
      tuple->InverseTransform = nullptr;
    }
    oldPostMatrixTransform = static_cast<vtkSimpleTransform*>(this->PostMatrixTransform);
    this->PostMatrixTransform = nullptr;
    this->PostMatrix = nullptr;
  }

  // the PreMatrix and PostMatrix transforms must be DeepCopied,
  // not copied by reference, so adjust the copy loop accordingly
  int i = 0;
  int n = concat->NumberOfTransforms;
  if (concat->PreMatrix)
  {
    if (concat->InverseFlag)
    {
      n--;
    }
    else
    {
      i++;
    }
  }
  if (concat->PostMatrix)
  {
    if (concat->InverseFlag)
    {
      i++;
    }
    else
    {
      n--;
    }
  }

  // copy the transforms by reference
  for (; i < n; i++)
  {
    vtkTransformPair* pair = &this->TransformList[i];
    vtkTransformPair* pair2 = &concat->TransformList[i];

    if (pair->ForwardTransform != pair2->ForwardTransform)
    {
      if (pair->ForwardTransform && i < this->NumberOfTransforms)
      {
        pair->ForwardTransform->Delete();
      }
      pair->ForwardTransform = pair2->ForwardTransform;
      if (pair->ForwardTransform)
      {
        pair->ForwardTransform->Register(nullptr);
      }
    }
    if (pair->InverseTransform != pair2->InverseTransform)
    {
      if (pair->InverseTransform && i < this->NumberOfTransforms)
      {
        pair->InverseTransform->Delete();
      }
      pair->InverseTransform = pair2->InverseTransform;
      if (pair->InverseTransform)
      {
        pair->InverseTransform->Register(nullptr);
      }
    }
  }

  // delete surplus items from the list
  for (i = concat->NumberOfTransforms; i < this->NumberOfTransforms; i++)
  {
    if (this->TransformList[i].ForwardTransform)
    {
      this->TransformList[i].ForwardTransform->Delete();
      this->TransformList[i].ForwardTransform = nullptr;
    }
    if (this->TransformList[i].InverseTransform)
    {
      this->TransformList[i].InverseTransform->Delete();
      this->TransformList[i].InverseTransform = nullptr;
    }
  }

  // make a DeepCopy of the PreMatrix transform
  if (concat->PreMatrix)
  {
    i = (concat->InverseFlag ? concat->NumberOfTransforms - 1 : 0);
    vtkTransformPair* pair = &this->TransformList[i];
    vtkSimpleTransform* mtrans;

    if (concat->InverseFlag == this->InverseFlag)
    {
      mtrans = (oldPreMatrixTransform ? oldPreMatrixTransform : vtkSimpleTransform::New());
      oldPreMatrixTransform = nullptr;
    }
    else
    {
      mtrans = (oldPostMatrixTransform ? oldPostMatrixTransform : vtkSimpleTransform::New());
      oldPostMatrixTransform = nullptr;
    }

    this->PreMatrix = mtrans->GetMatrix();
    this->PreMatrix->DeepCopy(concat->PreMatrix);
    this->PreMatrixTransform = mtrans;
    this->PreMatrixTransform->Modified();

    if (pair->ForwardTransform)
    {
      pair->ForwardTransform->Delete();
      pair->ForwardTransform = nullptr;
    }
    if (pair->InverseTransform)
    {
      pair->InverseTransform->Delete();
      pair->InverseTransform = nullptr;
    }

    if (concat->InverseFlag)
    {
      pair->ForwardTransform = nullptr;
      pair->InverseTransform = this->PreMatrixTransform;
    }
    else
    {
      pair->ForwardTransform = this->PreMatrixTransform;
      pair->InverseTransform = nullptr;
    }
  }

  // make a DeepCopy of the PostMatrix transform
  if (concat->PostMatrix)
  {
    i = (concat->InverseFlag ? 0 : concat->NumberOfTransforms - 1);
    vtkTransformPair* pair = &this->TransformList[i];
    vtkSimpleTransform* mtrans;

    if (concat->InverseFlag == this->InverseFlag)
    {
      mtrans = (oldPostMatrixTransform ? oldPostMatrixTransform : vtkSimpleTransform::New());
      oldPostMatrixTransform = nullptr;
    }
    else
    {
      mtrans = (oldPreMatrixTransform ? oldPreMatrixTransform : vtkSimpleTransform::New());
      oldPreMatrixTransform = nullptr;
    }

    this->PostMatrix = mtrans->GetMatrix();
    this->PostMatrix->DeepCopy(concat->PostMatrix);
    this->PostMatrixTransform = mtrans;
    this->PostMatrixTransform->Modified();

    if (pair->ForwardTransform)
    {
      pair->ForwardTransform->Delete();
      pair->ForwardTransform = nullptr;
    }
    if (pair->InverseTransform)
    {
      pair->InverseTransform->Delete();
      pair->InverseTransform = nullptr;
    }
    if (concat->InverseFlag)
    {
      pair->ForwardTransform = nullptr;
      pair->InverseTransform = this->PostMatrixTransform;
    }
    else
    {
      pair->ForwardTransform = this->PostMatrixTransform;
      pair->InverseTransform = nullptr;
    }
  }

  // delete the old PreMatrix and PostMatrix transforms if not reused
  if (oldPreMatrixTransform)
  {
    oldPreMatrixTransform->Delete();
  }
  if (oldPostMatrixTransform)
  {
    oldPostMatrixTransform->Delete();
  }

  // copy misc. ivars
  this->InverseFlag = concat->InverseFlag;
  this->PreMultiplyFlag = concat->PreMultiplyFlag;

  this->NumberOfTransforms = concat->NumberOfTransforms;
  this->NumberOfPreTransforms = concat->NumberOfPreTransforms;
}

//------------------------------------------------------------------------------
void vtkTransformConcatenation::PrintSelf(ostream& os, vtkIndent indent)
{
  os << indent << "InverseFlag: " << this->InverseFlag << "\n";
  os << indent << (this->PreMultiplyFlag ? "PreMultiply\n" : "PostMultiply\n");
  os << indent << "NumberOfPreTransforms: " << this->GetNumberOfPreTransforms() << "\n";
  os << indent << "NumberOfPostTransforms: " << this->GetNumberOfPostTransforms() << "\n";
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// All of the following methods are for vtkTransformConcatenationStack
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
vtkTransformConcatenationStack::vtkTransformConcatenationStack()
{
  this->StackSize = 0;
  this->StackBottom = nullptr;
  this->Stack = nullptr;
}

//------------------------------------------------------------------------------
vtkTransformConcatenationStack::~vtkTransformConcatenationStack()
{
  int n = static_cast<int>(this->Stack - this->StackBottom);
  for (int i = 0; i < n; i++)
  {
    this->StackBottom[i]->Delete();
  }

  delete[] this->StackBottom;
}

//------------------------------------------------------------------------------
void vtkTransformConcatenationStack::Pop(vtkTransformConcatenation** concat)
{
  // if we're at the bottom of the stack, don't pop
  if (this->Stack == this->StackBottom)
  {
    return;
  }

  // get the previous PreMultiplyFlag
  vtkTypeBool preMultiplyFlag = (*concat)->GetPreMultiplyFlag();

  // delete the previous item
  (*concat)->Delete();

  // pop new item off the stack
  *concat = *--this->Stack;

  // re-set the PreMultiplyFlag
  (*concat)->SetPreMultiplyFlag(preMultiplyFlag);
}

//------------------------------------------------------------------------------
void vtkTransformConcatenationStack::Push(vtkTransformConcatenation** concat)
{
  // check stack size and grow if necessary
  if ((this->Stack - this->StackBottom) == this->StackSize)
  {
    int newStackSize = this->StackSize + 10;
    vtkTransformConcatenation** newStackBottom = new vtkTransformConcatenation*[newStackSize];
    for (int i = 0; i < this->StackSize; i++)
    {
      newStackBottom[i] = this->StackBottom[i];
    }
    delete[] this->StackBottom;
    this->StackBottom = newStackBottom;
    this->Stack = this->StackBottom + this->StackSize;
    this->StackSize = newStackSize;
  }

  // add item to the stack
  *this->Stack++ = *concat;

  // make a copy of that item the current item
  *concat = vtkTransformConcatenation::New();
  (*concat)->DeepCopy(*(this->Stack - 1));
}

//------------------------------------------------------------------------------
void vtkTransformConcatenationStack::DeepCopy(vtkTransformConcatenationStack* stack)
{
  int n = static_cast<int>(stack->Stack - stack->StackBottom);
  int m = static_cast<int>(this->Stack - this->StackBottom);

  // check to see if we have to grow the stack
  if (n > this->StackSize)
  {
    int newStackSize = n + n % 10;
    vtkTransformConcatenation** newStackBottom = new vtkTransformConcatenation*[newStackSize];
    for (int j = 0; j < m; j++)
    {
      newStackBottom[j] = this->StackBottom[j];
    }
    delete[] this->StackBottom;
    this->StackBottom = newStackBottom;
    this->Stack = this->StackBottom + this->StackSize;
    this->StackSize = newStackSize;
  }

  // delete surplus items
  for (int l = n; l < m; l++)
  {
    (*--this->Stack)->Delete();
  }

  // allocate new items
  for (int i = m; i < n; i++)
  {
    *this->Stack++ = vtkTransformConcatenation::New();
  }

  // deep copy the items
  for (int k = 0; k < n; k++)
  {
    this->StackBottom[k]->DeepCopy(stack->StackBottom[k]);
  }
}
VTK_ABI_NAMESPACE_END
