
#include "CVars.h"
#include "HAL/IConsoleManager.h"

TAutoConsoleVariable<int32> CVarNDD_DebugCollisions(
		TEXT("r.NDD.DebugCollisions"),
		0,
		TEXT("Enables niagara destruction driver debug visualizations.\n")
		TEXT("<=0: OFF\n")
		TEXT(" 1: ON\n"),
		ECVF_SetByConsole);

TAutoConsoleVariable<int32> CVarNDD_DebugMaterial(
		TEXT("r.NDD.DebugMaterial"),
		0,
		TEXT("Enables use of a debug material on destructibles that shows bones in different colors.\n")
		TEXT("<=0: OFF\n")
		TEXT(" 1: ON\n"),
		ECVF_SetByConsole);