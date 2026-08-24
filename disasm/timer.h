#pragma once
// Shim for the AAE z80 core. AAE drives its scheduled timers from CPU cycle
// consumption via this hook; this rig does its own pacing in ref.cpp, so the
// hook is a no-op.
static inline void timer_update(int cycles, int cpunum) { (void)cycles; (void)cpunum; }
