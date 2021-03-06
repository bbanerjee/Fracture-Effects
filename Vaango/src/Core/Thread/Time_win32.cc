/*
 * The MIT License
 *
 * Copyright (c) 2013-2014 Callaghan Innovation, New Zealand
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * The MIT License
 *
 * Copyright (c) 1997-2012 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */




/*
 *  Time_win32.cc: Generic unix implementation of the Time class
 *
 *  Written by:
 *   Author: Steve Parker
 *   Department of Computer Science
 *   University of Utah
 *   Date: June 1997
 *
 */


#include <Core/Thread/Time.h>
#include <Core/Thread/Thread.h>
#include <Core/Thread/ThreadError.h>
#include <cstdio>
#include <windows.h>
#include <Mmsystem.h> // windows time functions
#include <cerrno>

static bool initialized=false;
static DWORD start_time; // measured in milliseconds

using namespace Uintah;

void
Time::initialize()
{
  initialized=true;
  timeBeginPeriod(1); // give the timer millisecond accuracy
  start_time = timeGetTime();
}

double
Time::secondsPerTick()
{
  return 1.e-3;
}

double
Time::currentSeconds()
{
  if(!initialized)
    initialize();
  DWORD now_time = timeGetTime();

  return ((double)(now_time - start_time))*1.e-3; 
}

Time::SysClock
Time::currentTicks()
{ 
  if(!initialized)
    initialize();
  DWORD now_time = timeGetTime();

  return now_time - start_time;
}

double
Time::ticksPerSecond()
{
    return 1000;
}

void
Time::waitUntil(double seconds)
{
    waitFor(seconds-currentSeconds());
}

void
Time::waitFor(double seconds)
{
  if(!initialized) 
    initialize();

  if(seconds<=0)
    return;

  int oldstate=Thread::couldBlock("Timed wait");
  Sleep(seconds*1000); // Sleep is in milliseconds
  Thread::couldBlockDone(oldstate);
}

void
Time::waitUntil(SysClock time)
{
    waitFor(time-currentTicks());
}

void
Time::waitFor(SysClock time)
{
  if(!initialized)
    initialize();
  if(time<=0)
    return;
  int oldstate=Thread::couldBlock("Timed wait");
  Sleep(time);
  Thread::couldBlockDone(oldstate);
}

