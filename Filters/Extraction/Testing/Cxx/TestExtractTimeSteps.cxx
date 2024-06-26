// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkExtractTimeSteps.h"

#include "vtkDataObject.h"
#include "vtkExodusIIReader.h"
#include "vtkInformation.h"
#include "vtkNew.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTestUtilities.h"

#include <cmath>

enum
{
  TEST_PASSED_RETVAL = 0,
  TEST_FAILED_RETVAL = 1
};

const double e = 1e-5;

int TestExtractTimeSteps(int argc, char* argv[])
{
  char* fname = vtkTestUtilities::ExpandDataFileName(argc, argv, "Data/can.ex2");
  vtkNew<vtkExodusIIReader> reader;
  reader->SetFileName(fname);
  delete[] fname;

  vtkNew<vtkExtractTimeSteps> extractor;
  extractor->SetInputConnection(reader->GetOutputPort());
  extractor->GenerateTimeStepIndices(0, 30, 5);
  extractor->AddTimeStepIndex(30);
  extractor->AddTimeStepIndex(35);
  extractor->AddTimeStepIndex(30);
  extractor->AddTimeStepIndex(40);
  extractor->AddTimeStepIndex(43);

  int numSteps = extractor->GetNumberOfTimeSteps();
  if (numSteps != 10)
  {
    std::cout << "vtkExtractTimeSteps add time-steps failed" << std::endl;
    return TEST_FAILED_RETVAL;
  }

  int tsteps[10];
  extractor->GetTimeStepIndices(tsteps);
  extractor->ClearTimeStepIndices();
  extractor->SetTimeStepIndices(numSteps, tsteps);
  extractor->Update();

  double expected[10] = { 0.0000, 0.0005, 0.0010, 0.0015, 0.0020, 0.0025, 0.0030, 0.0035, 0.0040,
    0.0043 };
  double* result = nullptr;

  vtkInformation* info = extractor->GetOutputInformation(0);
  if (info->Has(vtkStreamingDemandDrivenPipeline::TIME_STEPS()))
  {
    if (info->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS()) != 10)
    {
      std::cout << "got incorrect number of time steps" << std::endl;
      return TEST_FAILED_RETVAL;
    }
    result = info->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
  }

  if (!result)
  {
    std::cout << "result has no time steps" << std::endl;
    return TEST_FAILED_RETVAL;
  }
  else
  {
    for (int i = 0; i < 10; ++i)
    {
      if (std::fabs(expected[i] - result[i]) > e)
      {
        std::cout << "extracted time steps values do not match" << std::endl;
        return TEST_FAILED_RETVAL;
      }
    }
  }

  extractor->UseRangeOn();
  extractor->SetRange(4, 27);
  extractor->SetTimeStepInterval(3);
  extractor->Update();
  // This should pull out 4, 7, 10, 13, 16, 19, 22, 25

  double expected2[8] = { 0.0004, 0.0007, 0.0010, 0.0013, 0.0016, 0.0019, 0.0022, 0.0025 };

  info = extractor->GetOutputInformation(0);
  if (info->Has(vtkStreamingDemandDrivenPipeline::TIME_STEPS()))
  {
    if (info->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS()) != 8)
    {
      std::cout << "got incorrect number of time steps for use range test" << std::endl;
      return TEST_FAILED_RETVAL;
    }
    result = info->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
  }

  if (!result)
  {
    std::cout << "result has no time steps" << std::endl;
    return TEST_FAILED_RETVAL;
  }
  else
  {
    for (int i = 0; i < 8; ++i)
    {
      if (std::fabs(expected2[i] - result[i]) > e)
      {
        std::cout << expected2[i] << " " << result[i] << std::endl;
        std::cout << "extracted time steps values do not match for use range test" << std::endl;
        return TEST_FAILED_RETVAL;
      }
    }
  }

  extractor->UpdateTimeStep(0.0020);
  std::cout << extractor->GetExecutive()->GetClassName() << std::endl;
  info = extractor->GetOutput()->GetInformation();
  double t = info->Get(vtkDataObject::DATA_TIME_STEP());
  if (std::fabs(0.0019 - t) > e)
  {
    std::cout << "vtkExtractTimeSteps returned wrong time step when intermediate time given"
              << std::endl;
    std::cout << "When asked for timestep " << 0.0020 << " it resulted in time: " << t << std::endl;
    return TEST_FAILED_RETVAL;
  }

  return TEST_PASSED_RETVAL;
}
