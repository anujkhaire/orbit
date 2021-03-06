// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Core.h"
#include "Message.h"
#include "Profiling.h"
#include "ScopeTimer.h"
#include "Threading.h"

class TcpClient;
class Message;
struct ContextSwitch;

//-----------------------------------------------------------------------------
class TimerManager {
 public:
  TimerManager(bool a_IsClient = false);
  ~TimerManager();

  void StartRecording();
  void StopRecording();
  void Stop();

  void StartClient();
  void StopClient();

  void AddTimer(const Timer& a_Timer);
  void AddContextSwitch(const ContextSwitch& a_CS);

  void ConsumeTimers();
  void SendTimers();
  bool HasQueuedEntries() const { return m_NumQueuedEntries > 0; }
  void FlushQueue();

 public:
  AutoResetEvent m_ConditionVariable;

  std::atomic<bool> m_Paused;
  std::atomic<bool> m_IsFull;
  std::atomic<bool> m_IsRecording;
  std::atomic<bool> m_ExitRequested;
  std::atomic<bool> m_FlushRequested;
  std::atomic<int> m_NumQueuedEntries;
  std::atomic<int> m_NumQueuedTimers;
  std::atomic<int> m_NumQueuedMessages;
  std::atomic<int> m_TimerIndex;
  std::atomic<int> m_NumTimersFromPreviousSession;
  std::atomic<int> m_NumFlushedTimers;

  int m_ThreadCounter;
  LockFreeQueue<Timer> m_LockFreeQueue;
  LockFreeQueue<Message> m_LockFreeMessageQueue;
  std::thread consumerThread_;
  bool m_IsClient = false;

  typedef std::function<void(Timer&)> TimerAddedCallback;
  std::vector<TimerAddedCallback> m_TimerAddedCallbacks;

  typedef std::function<void(const struct ContextSwitch&)>
      ContextSwitchAddedCallback;
  ContextSwitchAddedCallback m_ContextSwitchAddedCallback;
};

//-----------------------------------------------------------------------------
extern std::unique_ptr<TimerManager> GTimerManager;

//-----------------------------------------------------------------------------
struct ScopeStartRecording {
  ScopeStartRecording() { GTimerManager->StartRecording(); }
  ~ScopeStartRecording() { GTimerManager->StopRecording(); }
};
