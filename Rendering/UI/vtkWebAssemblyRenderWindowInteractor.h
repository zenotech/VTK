// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkWebAssemblyRenderWindowInteractor
 * @brief   Handles user interaction in web browsers.
 *
 * The class is implemented using SDL2 and emscripten APIs.
 * The SDL2 library is an implementation detail and may be changed
 * in the future to use WASI or other APIs.
 *
 * Contrary to the documentation of `Start`, this interactor's event loop
 * does not block in order to return control to the browser so that it can render graphics, UI, etc.
 * See https://emscripten.org/docs/api_reference/emscripten.h.html#c.emscripten_set_main_loop
 */

#ifndef vtkWebAssemblyRenderWindowInteractor_h
#define vtkWebAssemblyRenderWindowInteractor_h

#ifndef __EMSCRIPTEN__
#error "vtkWebAssemblyRenderWindowInteractor requires the Emscripten SDK"
#endif

#include "vtkDeprecation.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderingUIModule.h" // For export macro
#include <map>                    // for ivar

VTK_ABI_NAMESPACE_BEGIN
class VTKRENDERINGUI_EXPORT vtkWebAssemblyRenderWindowInteractor : public vtkRenderWindowInteractor
{
public:
  /**
   * Construct object so that light follows camera motion.
   */
  static vtkWebAssemblyRenderWindowInteractor* New();

  vtkTypeMacro(vtkWebAssemblyRenderWindowInteractor, vtkRenderWindowInteractor);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Initialize the event handler
   */
  void Initialize() override;

  /**
   * Process all user-interaction, timer events and return.
   * If there are no events, this method returns immediately.
   */
  void ProcessEvents() override;

  /**
   * This function is called on 'q','e' keypress if exitmethod is not
   * specified and should be overridden by platform dependent subclasses
   * to provide a termination procedure if one is required.
   */
  void TerminateApp() override;

  /**
   * These methods correspond to the Exit, User and Pick
   * callbacks. They allow for the Style to invoke them.
   */
  void ExitCallback() override;

  /**
   * Specify the selector of the canvas element in the DOM.
   * Note that the current implementation of canvas in SDL2
   * is hardcoded to a DOM element with id="canvas".
   * Any other values are NOT supported.
   */
  vtkGetStringMacro(CanvasSelector);
  vtkSetStringMacro(CanvasSelector);

  /**
   * When true (default), the style of the parent element of canvas will be adjusted
   * allowing the canvas to take up entire space of the parent.
   */
  vtkGetMacro(ExpandCanvasToContainer, bool);
  vtkSetMacro(ExpandCanvasToContainer, bool);
  vtkBooleanMacro(ExpandCanvasToContainer, bool);

protected:
  vtkWebAssemblyRenderWindowInteractor();
  ~vtkWebAssemblyRenderWindowInteractor() override;

  bool ProcessEvent(void* event);

  ///@{
  /**
   * Internal methods for creating and destroying timers that must be
   * implemented by subclasses. InternalCreateTimer() returns a
   * platform-specific timerId and InternalDestroyTimer() returns
   * non-zero value on success.
   */
  int InternalCreateTimer(int timerId, int timerType, unsigned long duration) override;
  int InternalDestroyTimer(int platformTimerId) override;
  ///@}

  /**
   * Sets up resize observer on the parent element of canvas.
   * The resize observer will update the interactor's window size
   * with the canvas dimensions.
   * When ExpandCanvasToContainer is true, the canvas style and parent style
   * are also initialized correctly.
   */
  void InitializeCanvasElement();

  std::map<int, int> VTKToPlatformTimerMap;

  /**
   * This will start up the event loop without blocking the main thread.
   */
  void StartEventLoop() override;

  char* CanvasSelector = nullptr;
  bool ExpandCanvasToContainer = true;

private:
  vtkWebAssemblyRenderWindowInteractor(const vtkWebAssemblyRenderWindowInteractor&) = delete;
  void operator=(const vtkWebAssemblyRenderWindowInteractor&) = delete;

  bool StartedMessageLoop = false;
  bool ResizeObserverInstalled = false;
};

VTK_ABI_NAMESPACE_END
#endif
