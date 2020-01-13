//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------

//-----------------------------------
// Author: Florian Kuebler
//-----------------------------------
#pragma once

#include "PrintVar.h"
#include "LinuxPerfUtils.h"
#include "OrbitFunction.h"

using namespace LinuxPerfUtils;

class LinuxPerfEventVisitor;

class LinuxPerfEventVisitor;

// This base class will be used in order to do processing of
// different perf events using the visitor patter.
// The data of the perf events will be copied from the ring buffer
// via memcpy dirrectly into the concrete subclass (depending on the
// event typ).
// The target of the memcpy will be a field "ring_buffer_data" that
// must be present for the subclass at compile time.
// As the perf_event_open ringbuffer, is 8byte alligned, this field
// must also be extended with dummy bytes at the end of the record.

class LinuxPerfEvent
{
public:
    virtual uint64_t Timestamp() = 0;
    virtual void accept(LinuxPerfEventVisitor* a_Visitor) = 0;
};

struct __attribute__((__packed__)) perf_context_switch_event {
    struct perf_event_header header;
    struct perf_sample_id sample_id;
};

class LinuxContextSwitchEvent : public LinuxPerfEvent {
public: 
    perf_context_switch_event ring_buffer_data;

    virtual uint64_t Timestamp() override {
        return ring_buffer_data.sample_id.time;
    }

    virtual void accept(LinuxPerfEventVisitor* a_Visitor) override;

    uint32_t PID() { return ring_buffer_data.sample_id.pid; }
    uint32_t TID() { return ring_buffer_data.sample_id.tid; }
    uint32_t CPU() { return ring_buffer_data.sample_id.cpu; }
    bool IsSwitchOut() { return ring_buffer_data.header.misc & PERF_RECORD_MISC_SWITCH_OUT; }
    bool IsSwitchIn() { return !IsSwitchOut(); }
};

struct __attribute__((__packed__)) perf_context_switch_cpu_wide_event {
    struct perf_event_header header;
    uint32_t next_prev_pid;
    uint32_t next_prev_tid;
    struct perf_sample_id sample_id;
};

struct __attribute__((__packed__)) perf_sample_event {
    //struct perf_event_header header;
    uint64_t    sample_id;   /* if PERF_SAMPLE_IDENTIFIER */
    uint64_t    ip;          /* if PERF_SAMPLE_IP */
    uint32_t    pid, tid;    /* if PERF_SAMPLE_TID */
    uint64_t    time;        /* if PERF_SAMPLE_TIME */
    uint64_t    addr;        /* if PERF_SAMPLE_ADDR */
    uint64_t    id;          /* if PERF_SAMPLE_ID */
    uint64_t    stream_id;   /* if PERF_SAMPLE_STREAM_ID */
    uint32_t    cpu, res;    /* if PERF_SAMPLE_CPU */
    uint64_t    period;      /* if PERF_SAMPLE_PERIOD */
    // struct read_format v;    /* if PERF_SAMPLE_READ */
    // uint64_t    nr;          /* if PERF_SAMPLE_CALLCHAIN */
    // uint64_t    ips[nr];     /* if PERF_SAMPLE_CALLCHAIN */
    // uint32_t    size;        /* if PERF_SAMPLE_RAW */
    // char        data[size];  /* if PERF_SAMPLE_RAW */
    // uint64_t    bnr;         /* if PERF_SAMPLE_BRANCH_STACK */
    // struct perf_branch_entry lbr[bnr];
    //                          /* if PERF_SAMPLE_BRANCH_STACK */
    // uint64_t    abi;         /* if PERF_SAMPLE_REGS_USER */
    // uint64_t    regs[weight(mask)];
    //                          /* if PERF_SAMPLE_REGS_USER */
    // uint64_t    size;        /* if PERF_SAMPLE_STACK_USER */
    // char   data[size];       /* if PERF_SAMPLE_STACK_USER */
    // uint64_t    dyn_size;    /* if PERF_SAMPLE_STACK_USER &&
    //                             size != 0 */
    // uint64_t    weight;      /* if PERF_SAMPLE_WEIGHT */
    // uint64_t    data_src;    /* if PERF_SAMPLE_DATA_SRC */
    // uint64_t    transaction; /* if PERF_SAMPLE_TRANSACTION */
    // uint64_t    abi;         /* if PERF_SAMPLE_REGS_INTR */
    // uint64_t    regs[weight(mask)];
    //                          /* if PERF_SAMPLE_REGS_INTR */
};

class LinuxSystemWideContextSwitchEvent : public LinuxPerfEvent {
public: 
    perf_context_switch_cpu_wide_event ring_buffer_data;

    virtual uint64_t Timestamp() override {
        return ring_buffer_data.sample_id.time;
    }

    virtual void accept(LinuxPerfEventVisitor* a_Visitor) override;

    uint32_t PrevPID()
    {
        if (IsSwitchOut())
            return ring_buffer_data.sample_id.pid;
        else
            return ring_buffer_data.next_prev_pid;
    }
    uint32_t PrevTID()
    {
        if (IsSwitchOut())
            return ring_buffer_data.sample_id.tid;
        else
            return ring_buffer_data.next_prev_tid;
    }
    uint32_t NextPID()
    { 
        if (IsSwitchOut())
            return ring_buffer_data.next_prev_pid;
        else
            return ring_buffer_data.sample_id.pid;
    }
    uint32_t NextTID()
    {
        if (IsSwitchOut())
            return ring_buffer_data.next_prev_tid;
        else
            return ring_buffer_data.sample_id.tid;
    }
    bool IsSwitchOut() { return ring_buffer_data.header.misc & PERF_RECORD_MISC_SWITCH_OUT; }
    bool IsSwitchIn() { return !IsSwitchOut(); }

    uint32_t CPU() { return ring_buffer_data.sample_id.cpu; }
};

struct __attribute__((__packed__)) perf_fork_exit_event {
    struct perf_event_header header;
    uint32_t    pid, ppid;
    uint32_t    tid, ptid;
    uint64_t    time;
    struct perf_sample_id sample_id;
};

class LinuxForkEvent : public LinuxPerfEvent {
public: 
    perf_fork_exit_event ring_buffer_data;

    virtual uint64_t Timestamp() override {
        return ring_buffer_data.time;
    }

    virtual void accept(LinuxPerfEventVisitor* a_Visitor) override;

    uint32_t PID() { return ring_buffer_data.pid; }
    uint32_t ParentPID() { return ring_buffer_data.ppid; }
    uint32_t TID() { return ring_buffer_data.tid; }
    uint32_t ParentTID() { return ring_buffer_data.ptid; }
};

struct __attribute__((__packed__)) perf_lost_event 
{
    struct perf_event_header header;
    uint64_t    id;
    uint64_t    lost;
    struct perf_sample_id sample_id;
};

class LinuxPerfLostEvent : public LinuxPerfEvent {
public:
    perf_lost_event ring_buffer_data;

    virtual uint64_t Timestamp() override {
        return ring_buffer_data.sample_id.time;
    }

    virtual void accept(LinuxPerfEventVisitor* a_Visitor) override;

    uint64_t Lost() {
        return ring_buffer_data.lost;
    }
};

template<typename perf_record_data_t>
class LinuxPerfEventRecord : public LinuxPerfEvent {
public:
    perf_record_data_t ring_buffer_data;

    virtual uint64_t Timestamp() override {
        return ring_buffer_data.basic_sample_data.time;
    }

    uint32_t PID() { return ring_buffer_data.basic_sample_data.pid; }
    uint32_t TID() { return ring_buffer_data.basic_sample_data.tid; }
    uint32_t CPU() { return ring_buffer_data.basic_sample_data.cpu; }
};  

template<typename perf_record_data_t>
class AbstractLinuxUprobeEvent : public LinuxPerfEventRecord<perf_record_data_t>
{
public: 
    Function* GetFunction() { return m_Function; }
    void SetFunction(Function* a_Function) { m_Function = a_Function; }
private: 
    Function* m_Function = nullptr;
};

struct perf_empty_record
{
    struct perf_event_header header;
    struct perf_sample_id basic_sample_data;
};

struct perf_record_with_stack
{
    struct perf_event_header header;
    struct perf_sample_id basic_sample_data;
    struct perf_sample_regs_user register_data;
    struct perf_sample_stack_user stack_data;
};

class LinuxUprobeEvent : public AbstractLinuxUprobeEvent<perf_empty_record>
{
public:
    void accept(LinuxPerfEventVisitor* a_Visitor) override;
};

class LinuxUprobeEventWithStack : public AbstractLinuxUprobeEvent<perf_record_with_stack>
{
public:
    void accept(LinuxPerfEventVisitor* a_Visitor) override;
};

class LinuxUretprobeEvent : public AbstractLinuxUprobeEvent<perf_empty_record>
{
public:
    void accept(LinuxPerfEventVisitor* a_Visitor) override;
};