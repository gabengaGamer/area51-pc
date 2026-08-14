//==============================================================================
//
//  x_profile.hpp
//
//==============================================================================

#ifndef X_PROFILE_HPP
#define X_PROFILE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "x_time.hpp"
#include "x_atomic.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum
{
    XPROFILE_NAME_LENGTH     = 64,
    XPROFILE_CATEGORY_LENGTH = 32,
    XPROFILE_FILE_LENGTH     = 96,
    XPROFILE_FUNCTION_LENGTH = 64,
    XPROFILE_VALUE_LENGTH    = 128
};

typedef u32 xprofile_id;

static const xprofile_id XPROFILE_INVALID_ID = 0xFFFFFFFFu;

enum xprofile_capture_mode
{
    XPROFILE_CAPTURE_OFF = 0,
    XPROFILE_CAPTURE_SUMMARY,
    XPROFILE_CAPTURE_TIMELINE
};

//------------------------------------------------------------------------------

enum xprofile_metric_type
{
    XPROFILE_METRIC_DURATION = 0,
    XPROFILE_METRIC_COUNTER,
    XPROFILE_METRIC_GAUGE,
    XPROFILE_METRIC_EVENT
};

//------------------------------------------------------------------------------

enum xprofile_unit
{
    XPROFILE_UNIT_NONE = 0,
    XPROFILE_UNIT_MILLISECONDS,
    XPROFILE_UNIT_BYTES,
    XPROFILE_UNIT_COUNT,
    XPROFILE_UNIT_PERCENT
};

//------------------------------------------------------------------------------

enum xprofile_timeline_event_type
{
    XPROFILE_TIMELINE_SCOPE_BEGIN = 0,
    XPROFILE_TIMELINE_SCOPE_END,
    XPROFILE_TIMELINE_VALUE,
    XPROFILE_TIMELINE_EVENT
};

//==============================================================================
//  DATA TYPES
//==============================================================================

struct xprofile_config
{
    u32                     FrameCapacity;
    u32                     MetricCapacity;
    u32                     PropertyCapacity;
    u32                     ThreadCapacity;
    u32                     ScopeDepth;
    u32                     EventsPerThread;
    u32                     TimelineEventCapacity;
    u32                     AutoReportFrameInterval;
    f64                     StutterThresholdMs;
    xprofile_capture_mode   CaptureMode;
    xbool                   FineTimingEnabled;

    xprofile_config( void );
};

//------------------------------------------------------------------------------

struct xprofile_metric_desc
{
    const char*          pName;
    const char*          pCategory;
    xprofile_metric_type Type;
    xprofile_unit        Unit;
    const char*          pFile;
    const char*          pFunction;
    u32                  Line;

    xprofile_metric_desc( void );
    xprofile_metric_desc( const char*          pMetricName,
                          const char*          pMetricCategory,
                          xprofile_metric_type MetricType,
                          xprofile_unit        MetricUnit );
};

//------------------------------------------------------------------------------

struct xprofile_metric_info
{
    xprofile_id          Id;
    char                 Name[XPROFILE_NAME_LENGTH];
    char                 Category[XPROFILE_CATEGORY_LENGTH];
    char                 File[XPROFILE_FILE_LENGTH];
    char                 Function[XPROFILE_FUNCTION_LENGTH];
    xprofile_metric_type Type;
    xprofile_unit        Unit;
    u32                  Line;
    u64                  RegisteredFrame;
};

//------------------------------------------------------------------------------

struct xprofile_metric_sample
{
    u64   FrameNumber;
    f64   Value;
    u32   CallCount;
    xbool Active;
};

//------------------------------------------------------------------------------

struct xprofile_frame_info
{
    u64   FrameNumber;
    xtick BeginTick;
    xtick EndTick;
    f64   DurationMs;
    u64   FirstTimelineSequence;
    u32   TimelineEventCount;
    u32   DroppedTimelineEventCount;
    xbool Stutter;
};

struct xprofile_thread_info
{
    s32  ThreadId;
    char Name[XPROFILE_NAME_LENGTH];
};

//------------------------------------------------------------------------------

struct xprofile_property_info
{
    char Name[XPROFILE_NAME_LENGTH];
    char Value[XPROFILE_VALUE_LENGTH];
};

//------------------------------------------------------------------------------

struct xprofile_statistics
{
    u32 SampleCount;
    u32 ActiveSampleCount;
    u64 TotalCallCount;
    f64 ActivePercent;
    f64 CallsPerSample;
    f64 CallsPerActiveSample;
    f64 Average;
    f64 ActiveAverage;
    f64 Minimum;
    f64 Median;
    f64 P95;
    f64 P99;
    f64 Maximum;
};

//------------------------------------------------------------------------------

struct xprofile_timeline_event
{
    u64                          Sequence;
    u64                          FrameNumber;
    xtick                        Timestamp;
    s32                          ThreadId;
    u16                          Depth;
    xprofile_timeline_event_type Type;
    xprofile_id                  MetricId;
    f64                          Value;
};

//------------------------------------------------------------------------------

struct xprofile_capture_metric
{
    xprofile_metric_info   Info;
    xprofile_metric_sample Sample;
};

//------------------------------------------------------------------------------

struct xprofile_capture
{
    // Note: Array storage is owned by xprofiler and remains valid only for the duration of xprofile_sink::OnCapture().

    u32                            Generation;
    xprofile_frame_info            Frame;
    const xprofile_capture_metric* pMetrics;
    u32                            MetricCount;
    const xprofile_thread_info*    pThreads;
    u32                            ThreadCount;
    const xprofile_property_info*  pProperties;
    u32                            PropertyCount;
    const xprofile_timeline_event* pTimelineEvents;
    u32                            TimelineEventCount;
};

//==============================================================================
//  TYPED METRIC HANDLES
//==============================================================================

class xprofiler;

class xprofile_token
{
public:
                        xprofile_token( void );
                        xprofile_token( const xprofile_token& Token );
    xprofile_token&     operator=( const xprofile_token& Token );

private:
                        xprofile_token( const xprofile_metric_desc& Desc );

    xprofile_metric_desc    m_Desc;
    mutable x_atomic_u64    m_Resolved;

    friend class xprofiler;
};

//------------------------------------------------------------------------------

class xprofile_zone
{
public:
                    xprofile_zone( void );
    void            Record       ( xtick Ticks ) const;

private:
                    xprofile_zone( const xprofile_token& Token );

    xprofile_token  m_Token;

    friend class xprofiler;
};

//------------------------------------------------------------------------------

class xprofile_counter
{
public:
                    xprofile_counter( void );
    void            Add             ( f64 Value = 1.0 ) const;

private:
                    xprofile_counter( const xprofile_token& Token );

    xprofile_token  m_Token;

    friend class xprofiler;
};

//------------------------------------------------------------------------------

class xprofile_gauge
{
public:
                    xprofile_gauge( void );
    void            Set           ( f64 Value ) const;

private:
                    xprofile_gauge( const xprofile_token& Token );

    xprofile_token  m_Token;

    friend class xprofiler;
};

//------------------------------------------------------------------------------

class xprofile_event
{
public:
                    xprofile_event( void );
    void            Emit          ( f64 Value = 0.0 ) const;

private:
                    xprofile_event( const xprofile_token& Token );

    xprofile_token  m_Token;

    friend class xprofiler;
};

//==============================================================================
//  SNAPSHOTS AND SINKS
//==============================================================================

class xprofile_snapshot
{
public:
                            xprofile_snapshot       ( void );

    xbool                   IsValid                 ( void ) const;
    u64                     GetCompletedFrameCount  ( void ) const;
    u32                     GetStoredFrameCount     ( void ) const;
    u32                     GetMetricCount          ( void ) const;
    u32                     GetThreadCount          ( void ) const;
    u32                     GetPropertyCount        ( void ) const;
    xbool                   GetFrameInfo            ( u32 NewestFrameOffset,
                                                      xprofile_frame_info& Info ) const;
    xbool                   GetThreadInfo           ( u32 ThreadIndex,
                                                      xprofile_thread_info& Info ) const;
    xbool                   GetPropertyInfo         ( u32 PropertyIndex,
                                                      xprofile_property_info& Info ) const;
    xbool                   GetMetricInfo           ( xprofile_id Id,
                                                      xprofile_metric_info& Info ) const;
    xbool                   GetMetricSample         ( xprofile_id Id,
                                                      u32 NewestFrameOffset,
                                                      xprofile_metric_sample& Sample ) const;
    xbool                   GetMetricStatistics     ( xprofile_id Id,
                                                      u32 NewestFrameCount,
                                                      xprofile_statistics& Statistics ) const;
    xbool                   GetTimelineEvent        ( u64 Sequence,
                                                      xprofile_timeline_event& Event ) const;

private:
                            xprofile_snapshot       ( const xprofiler* pOwner,
                                                      u32 Generation,
                                                      u64 CompletedFrameCount,
                                                      u32 MetricCount,
                                                      u32 ThreadCount,
                                                      u32 PropertyCount );

    const xprofiler*        m_pOwner;
    u32                     m_Generation;
    u64                     m_CompletedFrameCount;
    u32                     m_MetricCount;
    u32                     m_ThreadCount;
    u32                     m_PropertyCount;

    friend class xprofiler;
};

//------------------------------------------------------------------------------

class xprofile_sink
{
public:
                        xprofile_sink( void );
    virtual             ~xprofile_sink( void );
    xbool               Attach        ( void );
    void                Detach        ( void );
    xbool               IsAttached    ( void ) const;

protected:
    // Called once per completed frame after all producer data has been merged.
    virtual void        OnCapture     ( const xprofile_capture& Capture ) = 0;

private:
    xprofiler*          m_pOwner;

                        xprofile_sink( const xprofile_sink& );
    xprofile_sink&      operator=( const xprofile_sink& );

    friend class xprofiler;
};

//==============================================================================
//  PROFILER
//==============================================================================

class xprofiler
{
public:
    void                    Init                    ( const xprofile_config* pConfig = NULL );
    void                    Kill                    ( void );
    xbool                   IsInitialized           ( void ) const;

    void                    SetCaptureMode          ( xprofile_capture_mode Mode );
    xprofile_capture_mode   GetCaptureMode          ( void ) const;
    xbool                   IsFrameActive           ( void ) const;
    void                    SetFineTimingEnabled    ( xbool Enabled );
    xbool                   IsFineTimingEnabled     ( void ) const;
    void                    SetAutoReportInterval   ( u32 FrameInterval );
    void                    SetStutterThresholdMs   ( f64 ThresholdMs );
    void                    SetProperty             ( const char* pName, const char* pValue );

    xprofile_zone           RegisterZone            ( const char* pName,
                                                      const char* pCategory = "CPU" );
    xprofile_zone           RegisterZoneAt          ( const char* pName,
                                                      const char* pCategory,
                                                      const char* pFile,
                                                      const char* pFunction,
                                                      u32 Line );
    xprofile_counter        RegisterCounter         ( const char* pName,
                                                      const char* pCategory = "Counter" );
    xprofile_gauge          RegisterGauge           ( const char* pName,
                                                      xprofile_unit Unit = XPROFILE_UNIT_NONE,
                                                      const char* pCategory = "Gauge" );
    xprofile_event          RegisterEvent           ( const char* pName,
                                                      const char* pCategory = "Event" );

    void                    BeginFrame              ( void );
    void                    EndFrame                ( void );
    void                    CancelFrame             ( void );
    xbool                   BeginScope              ( const xprofile_zone& Zone );
    void                    EndScope                ( void );

    void                    RecordDynamicDuration   ( const char* pName,
                                                      xtick Ticks,
                                                      const char* pCategory = "CPU" );
    void                    AddDynamicCounter       ( const char* pName,
                                                      f64 Value = 1.0,
                                                      const char* pCategory = "Counter" );

    xprofile_snapshot       AcquireSnapshot         ( void ) const;
    void                    DumpReport              ( u32 NewestFrameCount = 0 ) const;

private:
                            xprofiler               ( void );
                           ~xprofiler               ( void );
                            xprofiler               ( const xprofiler& );
    xprofiler&              operator=               ( const xprofiler& );

    struct implementation;
    implementation*         m_pImplementation;

    xprofile_id             ResolveToken            ( const xprofile_token& Token );
    void                    RecordDuration          ( const xprofile_token& Token, xtick Ticks );
    void                    CounterAdd              ( const xprofile_token& Token, f64 Value );
    void                    GaugeSet                ( const xprofile_token& Token, f64 Value );
    void                    EventEmit               ( const xprofile_token& Token, f64 Value );
    xbool                   AttachSink              ( xprofile_sink& Sink );
    void                    DetachSink              ( xprofile_sink& Sink );

    xbool                   IsSnapshotValid         ( const xprofile_snapshot& Snapshot ) const;
    u32                     GetStoredFrameCount     ( const xprofile_snapshot& Snapshot ) const;
    xbool                   GetFrameInfo            ( const xprofile_snapshot& Snapshot,
                                                      u32 NewestFrameOffset,
                                                      xprofile_frame_info& Info ) const;
    xbool                   GetThreadInfo           ( const xprofile_snapshot& Snapshot,
                                                      u32 ThreadIndex,
                                                      xprofile_thread_info& Info ) const;
    xbool                   GetPropertyInfo         ( const xprofile_snapshot& Snapshot,
                                                      u32 PropertyIndex,
                                                      xprofile_property_info& Info ) const;
    xbool                   GetMetricInfo           ( const xprofile_snapshot& Snapshot,
                                                      xprofile_id Id,
                                                      xprofile_metric_info& Info ) const;
    xbool                   GetMetricSample         ( const xprofile_snapshot& Snapshot,
                                                      xprofile_id Id,
                                                      u32 NewestFrameOffset,
                                                      xprofile_metric_sample& Sample ) const;
    xbool                   GetMetricStatistics     ( const xprofile_snapshot& Snapshot,
                                                      xprofile_id Id,
                                                      u32 NewestFrameCount,
                                                      xprofile_statistics& Statistics ) const;
    xbool                   GetTimelineEvent        ( const xprofile_snapshot& Snapshot,
                                                      u64 Sequence,
                                                      xprofile_timeline_event& Event ) const;

    friend class xprofile_zone;
    friend class xprofile_counter;
    friend class xprofile_gauge;
    friend class xprofile_event;
    friend class xprofile_snapshot;
    friend class xprofile_sink;
    friend xprofiler& x_GetProfiler( void );
};

xprofiler& x_GetProfiler( void );

//==============================================================================
//  SCOPED API
//==============================================================================

class xprofile_scope
{
public:
    explicit            xprofile_scope( const xprofile_zone& Zone );
                       ~xprofile_scope( void );

private:
    xbool               m_Active;

                        xprofile_scope( const xprofile_scope& );
    xprofile_scope&     operator=( const xprofile_scope& );
};

//------------------------------------------------------------------------------

#define X_PROFILE_JOIN_IMPL(a,b) a##b
#define X_PROFILE_JOIN(a,b)      X_PROFILE_JOIN_IMPL(a,b)

#if X_PROFILE
    #define X_PROFILE_SCOPE(Name)                                                        \
        static xprofile_zone X_PROFILE_JOIN(_xProfileZone_,__LINE__) =                  \
            x_GetProfiler().RegisterZoneAt( (Name), "CPU", __FILE__, __FUNCTION__, __LINE__ ); \
        xprofile_scope X_PROFILE_JOIN(_xProfileScope_,__LINE__)(                        \
            X_PROFILE_JOIN(_xProfileZone_,__LINE__) )

    #define X_PROFILE_SCOPE_CATEGORY(Category,Name)                                      \
        static xprofile_zone X_PROFILE_JOIN(_xProfileZone_,__LINE__) =                  \
            x_GetProfiler().RegisterZoneAt( (Name), (Category), __FILE__, __FUNCTION__, __LINE__ ); \
        xprofile_scope X_PROFILE_JOIN(_xProfileScope_,__LINE__)(                        \
            X_PROFILE_JOIN(_xProfileZone_,__LINE__) )
#else
    #define X_PROFILE_SCOPE(Name)                    ((void)0)
    #define X_PROFILE_SCOPE_CATEGORY(Category,Name)  ((void)0)
#endif

//==============================================================================
#endif // X_PROFILE_HPP
//==============================================================================
