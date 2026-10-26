#include "stdafx.h"

CNicBase::fnNicBaseThisVoid CNicBase::NicBaseInitialize = NULL;
CNicBase::fnNicBaseThisVoid CNicBase::NicBaseShutdown = NULL;
CNicBase::fnNicBaseThisTimerStart CNicBase::NicBaseTimerStart = NULL;
CNicBase::fnNicBaseThisStats CNicBase::NicBaseAddInternalStats = NULL;
CNicBase::fnNicBaseThisCEnetAddr CNicBase::NicBaseIsSubscribedMcastAddr = NULL;
CNicBase::fnNicBaseThisPrintStats CNicBase::NicBasePrintStats = NULL;
CNicBase::fnNicBaseThisVoid CNicBase::NicBaseTakeLock = NULL;
CNicBase::fnNicBaseThisVoid CNicBase::NicBaseTakeLockAtRaisedIrql = NULL;

VOID CNicBase::ResolveFunctions() {
    const USBENET_KERNEL_ADDRESSES* Kernel = Main::Kernel;

    NicBaseInitialize = reinterpret_cast<fnNicBaseThisVoid>(Kernel->NicBaseInitialize);
    NicBaseShutdown = reinterpret_cast<fnNicBaseThisVoid>(Kernel->NicBaseShutdown);
    NicBaseTimerStart = reinterpret_cast<fnNicBaseThisTimerStart>(Kernel->NicBaseTimerStart);
    NicBaseAddInternalStats = reinterpret_cast<fnNicBaseThisStats>(Kernel->NicBaseAddInternalStats);
    NicBaseIsSubscribedMcastAddr = reinterpret_cast<fnNicBaseThisCEnetAddr>(Kernel->NicBaseIsSubscribedMcastAddr);

    if (Main::Devkit) {
        NicBasePrintStats = reinterpret_cast<fnNicBaseThisPrintStats>(Kernel->NicBasePrintStats);
        NicBaseTakeLock = reinterpret_cast<fnNicBaseThisVoid>(Kernel->NicBaseTakeLock);
        NicBaseTakeLockAtRaisedIrql = reinterpret_cast<fnNicBaseThisVoid>(Kernel->NicBaseTakeLockAtRaisedIrql);
    }
}