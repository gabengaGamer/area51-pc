{*******************************************************}
{                                                       }
{ Miles Sound System interface to Delphi.               }
{ Copyright (c) 1991-1999 RAD Game Tools, Inc.          }
{                                                       }
{*******************************************************}

unit MSS16;

interface

uses
  mmsystem;

const

{ Miles Sound System Library to use }

MSS_lib = 'MSS16';       { 16-bit MSS library }

DIG_F_16BITS_MASK        = 1;
DIG_F_STEREO_MASK        = 2;
DIG_F_ADPCM_MASK         = 4;

{ PCM data formats }
DIG_F_MONO_8             = 0;        { PCM data formats }
DIG_F_MONO_16            = (DIG_F_16BITS_MASK);
DIG_F_STEREO_8           = (DIG_F_STEREO_MASK);
DIG_F_STEREO_16          = (DIG_F_STEREO_MASK or DIG_F_16BITS_MASK);
DIG_F_ADPCM_MONO_16      = (DIG_F_ADPCM_MASK or DIG_F_16BITS_MASK);
DIG_F_ADPCM_STEREO_16    = (DIG_F_ADPCM_MASK or DIG_F_16BITS_MASK or DIG_F_STEREO_MASK);
DIG_F_USING_ASI          = 16;

MIDI_NULL_DRIVER         = -2;

{ PCM flags }
DIG_PCM_SIGN            = $0001;
DIG_PCM_ORDER           = $0002;

SMP_FREE        =  $0001;    { Sample is available for allocation }

SMP_DONE        =  $0002;    { Sample has finished playing, or has }
                             { never been started }

SMP_PLAYING     =  $0004;    { Sample is playing }

SMP_STOPPED     =  $0008;    { Sample has been stopped }

SMP_PLAYINGBUTRELEASED = $0010; { Sample is playing, but digital handle }
                                { has been temporarily released }

SEQ_FREE        =  $0001;    { Sequence is available for allocation }

SEQ_DONE        =  $0002;    { Sequence has finished playing, or has }
                             { never been started }

SEQ_PLAYING     =  $0004;    { Sequence is playing }

SEQ_STOPPED     =  $0008;    { Sequence has been stopped }

SEQ_PLAYINGBUTRELEASED = $0010; { Sequence is playing, but MIDI handle }
                                { has been temporarily released }

AILDS_RELINQUISH = 0;         { App returns control of secondary buffer }
AILDS_SEIZE      = 1;         { App takes control of secondary buffer }
AILDS_SEIZE_LOOP = 2;         { App wishes to loop the secondary buffer }

DIG_RESAMPLING_TOLERANCE  = 0;

DIG_MIXER_CHANNELS       =  1;

DIG_DEFAULT_VOLUME       =  2;

MDI_SERVICE_RATE          = 3;

MDI_SEQUENCES             = 4;

MDI_DEFAULT_VOLUME        = 5;

MDI_QUANT_ADVANCE         = 6;

MDI_ALLOW_LOOP_BRANCHING  = 7;

MDI_DEFAULT_BEND_RANGE    = 8;

MDI_DOUBLE_NOTE_OFF       = 9;

DIG_ENABLE_RESAMPLE_FILTER= 31;

DIG_DECODE_BUFFER_SIZE    = 32;

DIG_USE_MSS_MIXER         = 33;

DIG_DS_FRAGMENT_SIZE      = 34;

DIG_DS_FRAGMENT_CNT       = 35;

DIG_DS_MIX_FRAGMENT_CNT   = 42;

DIG_DS_USE_PRIMARY        = 36;

MDI_SYSEX_BUFFER_SIZE     = 10;

DIG_OUTPUT_BUFFER_SIZE    = 11;

AIL_MM_PERIOD             = 12;

AIL_TIMERS                = 13;

DIG_MIN_CHAIN_ELEMENT_SIZE= 14;

DIG_USE_WAVEOUT           = 15;

DIG_DS_SECONDARY_SIZE     = 16;

DIG_DS_SAMPLE_CEILING     = 17;

AIL_LOCK_PROTECTION       = 18;

AIL_WIN32S_CALLBACK_SIZE  = 19;

DLS_TIMEBASE              = 20;

DLS_VOICE_LIMIT           = 21;

DLS_BANK_SELECT_ALIAS     = 22;

DLS_STREAM_BOOTSTRAP      = 23;     { Don't submit first stream buffer }

DLS_VOLUME_BOOST          = 24;

DLS_ENABLE_FILTERING      = 25;     { No filtering by default }

DLS_ENABLE_GLOBAL_REVERB  = 26;     { Global reverb disabled by default }

AIL_ENABLE_MMX_SUPPORT    = 27;     { Enable MMX support if present }

DLS_GM_PASSTHROUGH        = 28;     { Pass unrecognized traffic on to }

DLS_GLOBAL_REVERB_LEVEL   = 29;     { Reverb level 0-127 }

DLS_GLOBAL_REVERB_TIME    = 30;     { Reverb time in stream buffers }

DLS_ADPCM_TO_ASI_THRESHOLD= 39;

DIG_REVERB_BUFFER_SIZE    = 40;     { No reverb support by default }

DIG_INPUT_LATENCY         = 41;     { Use >= 250-millisecond input buffers }

DIG_USE_WAVEIN            = 43;

FILE_READ_WITH_SIZE =-1;

RETAIN_DLS_COLLECTION   =$000000001;
RETURN_TO_BOOTUP_STATE  =$000000002;
RETURN_TO_GM_ONLY_STATE =$000000004;
DLS_COMPACT_MEMORY      =$000000008;

AIL_DLS_UNLOAD_MINE =0;
AIL_DLS_UNLOAD_ALL  =1;

AIL_QUICK_USE_WAVEOUT    =2;
AIL_QUICK_MIDI_AND_DLS   =2;
AIL_QUICK_DLS_ONLY       =3;
AIL_QUICK_MIDI_AND_VORTEX_DLS     =4;
AIL_QUICK_MIDI_AND_SONICVIBES_DLS =5;

AIL_QUICK_XMIDI_TYPE     =1;
AIL_QUICK_DIGITAL_TYPE   =2;
AIL_QUICK_DLS_XMIDI_TYPE =3;
AIL_QUICK_MPEG_DIGITAL_TYPE=4;

AILFILTERDLS_USINGLIST =1;

AILMIDITOXMI_USINGLIST =1;
AILMIDITOXMI_TOLERANT  =2;

AILMIDILIST_ROLANDSYSEX =1;
AILMIDILIST_ROLANDUN    =2;
AILMIDILIST_ROLANDAB    =4;

AILDLSLIST_ARTICULATION =1;
AILDLSLIST_DUMP_WAVS    =2;

AILFILETYPE_UNKNOWN   = 0;
AILFILETYPE_PCM_WAV   = 1;
AILFILETYPE_ADPCM_WAV = 2;
AILFILETYPE_OTHER_WAV = 3;
AILFILETYPE_VOC       = 4;
AILFILETYPE_MIDI      = 5;
AILFILETYPE_XMIDI     = 6;
AILFILETYPE_XMIDI_DLS = 7;
AILFILETYPE_XMIDI_MLS = 8;
AILFILETYPE_DLS       = 9;
AILFILETYPE_MLS       =10;
AILFILETYPE_MPEG_L1_AUDIO=11;
AILFILETYPE_MPEG_L2_AUDIO=12;
AILFILETYPE_MPEG_L3_AUDIO=13;


RIB_NOERR            =0;
RIB_NOT_ALL_AVAILABLE=1;
RIB_NOT_FOUND        =2;
RIB_OUT_OF_MEM       =3;

HINTENUM_FIRST=0;
HPROENUM_FIRST=0;

PROVIDER_NAME    =-100;  { RIB_STRING name of decoder }
PROVIDER_VERSION =-101;  { RIB_HEX BCD version number }

AIL_ASI_VERSION =1;
AIL_ASI_REVISION=0;

ASI_NOERR                  = 0;   { Success -- no error }
ASI_NOT_ENABLED            = 1;   { ASI not enabled }
ASI_ALREADY_STARTED        = 2;   { ASI already started }
ASI_INVALID_PARAM          = 3;   { Invalid parameters used }
ASI_INTERNAL_ERR           = 4;   { Internal error in ASI driver }
ASI_OUT_OF_MEM             = 5;   { Out of system RAM }
ASI_ERR_NOT_IMPLEMENTED    = 6;   { Feature not implemented }
ASI_NOT_FOUND              = 7;   { ASI supported device not found }
ASI_NOT_INIT               = 8;   { ASI not initialized }
ASI_CLOSE_ERR              = 9;   { ASI not closed correctly }

M3D_NOERR                  = 0;   { Success -- no error }
M3D_NOT_ENABLED            = 1;   { M3D not enabled }
M3D_ALREADY_STARTED        = 2;   { M3D already started }
M3D_INVALID_PARAM          = 3;   { Invalid parameters used }
M3D_INTERNAL_ERR           = 4;   { Internal error in ASI driver }
M3D_OUT_OF_MEM             = 5;   { Out of system RAM }
M3D_ERR_NOT_IMPLEMENTED    = 6;   { Feature not implemented }
M3D_NOT_FOUND              = 7;   { M3D supported device not found }
M3D_NOT_INIT               = 8;   { M3D not initialized }
M3D_CLOSE_ERR              = 9;   { M3D not closed correctly }

AIL_LENGTHY_INIT           =0;
AIL_LENGTHY_SET_PREFERENCE =1;
AIL_LENGTHY_UPDATE         =2;
AIL_LENGTHY_DONE           =3;

RIB_NONE   =0; { No type }
RIB_CUSTOM =1; { Used for pointers to application-specific structures }
RIB_DEC    =2; { Used for 32-bit integer values to be reported in decimal }
RIB_HEX    =3; { Used for 32-bit integer values to be reported in hex }
RIB_FLOAT  =4; { Used for 32-bit single-precision FP values }
RIB_PERCENT=5; { Used for 32-bit single-precision FP values to be reported as percentages }
RIB_BOOL   =6; { Used for Boolean-constrained integer values to be reported as TRUE or FALSE }
RIB_STRING =7; { Used for pointers to null-terminated ASCII strings }

RIB_FUNCTION = 0;
RIB_ATTRIBUTE =1; { Attribute: read-only data type used for status/info communication }
RIB_PREFERENCE=2; { Preference: read/write data type used to control behavior }

DP_ASI_DECODER   =0; { Must be "ASI codec stream" provider }
DP_POST_DECODER  =1; { Must be "MSS pipeline filter" provider }
DP_MERGE         =2; { Must be "MSS mixer" provider }
N_SAMPLE_STAGES  =3; { Placeholder for end of list (= # of valid stages) }
SAMPLE_ALL_STAGES=4; { Used to signify all pipeline stages, for shutdown }

DP_FLUSH         =0; { Must be "MSS mixer" provider }
DP_COPY          =1; { Must be "MSS mixer" provider }
DP_POST_COPY     =2; { Must be "MSS pipeline filter" provider }
N_DIGDRV_STAGES  =3; { Placeholder for end of list (= # of valid stages) }
DIGDRV_ALL_STAGES=4; { Used to signify all pipeline stages, for shutdown }

EAX_ENVIRONMENT_GENERIC          = 0; { factory default }
EAX_ENVIRONMENT_PADDEDCELL       = 1;
EAX_ENVIRONMENT_ROOM             = 2; { standard environments }
EAX_ENVIRONMENT_BATHROOM         = 3;
EAX_ENVIRONMENT_LIVINGROOM       = 4;
EAX_ENVIRONMENT_STONEROOM        = 5;
EAX_ENVIRONMENT_AUDITORIUM       = 6;
EAX_ENVIRONMENT_CONCERTHALL      = 7;
EAX_ENVIRONMENT_CAVE             = 8;
EAX_ENVIRONMENT_ARENA            = 9;
EAX_ENVIRONMENT_HANGAR           =10;
EAX_ENVIRONMENT_CARPETEDHALLWAY  =11;
EAX_ENVIRONMENT_HALLWAY          =12;
EAX_ENVIRONMENT_STONECORRIDOR    =13;
EAX_ENVIRONMENT_ALLEY            =14;
EAX_ENVIRONMENT_FOREST           =15;
EAX_ENVIRONMENT_CITY             =16;
EAX_ENVIRONMENT_MOUNTAINS        =17;
EAX_ENVIRONMENT_QUARRY           =18;
EAX_ENVIRONMENT_PLAIN            =19;
EAX_ENVIRONMENT_PARKINGLOT       =20;
EAX_ENVIRONMENT_SEWERPIPE        =21;
EAX_ENVIRONMENT_UNDERWATER       =22;
EAX_ENVIRONMENT_DRUGGED          =23;
EAX_ENVIRONMENT_DIZZY            =24;
EAX_ENVIRONMENT_PSYCHOTIC        =25;
EAX_ENVIRONMENT_COUNT            =26; { total number of environments }

EAX_REVERBMIX_USEDISTANCE=1.0;

type

S32  = LongInt;
U32  = S32;
S16  = SmallInt;
U16  = Word;
S8   = ShortInt;
U8   = Byte;
PU32 = ^U32;
PS32 = ^S32;
F32  = single;
F64  = double;

AUDIO=record
  tag:S32;
end;
TIMER=record
  tag:S32;
end;
DIGDRIVER=record
  tag:S32;
end;
SAMPLE=record
  tag:S32;
end;
MDIDRIVER=record
  tag:S32;
end;
SEQUENCE=record
  tag:S32;
end;
WAVESYNTH=record
  tag:S32;
end;
REDBOOK=record
  tag:S32;
end;
STREAM=record
  tag:S32;
end;
DLSDEVICE=record
  tag:S32;
end;
DLSFILEID=record
  tag:S32;
end;

AILDLSINFO=record
  Description:array[0..127] of char;
  MaxDLSMemory:S32;
  CurrentDLSMemory:S32;
  LargestSize:S32;
  GMAvailable:S32;
  GMBankSize:S32;
end;

AILSOUNDINFO=record
  format:S32;
  data_ptr:PCHAR;
  data_len:U32;
  rate:U32;
  bits:S32;
  channels:S32;
  samples:U32;
  block_size:U32;
  initial_ptr:PCHAR;
end;

HAUDIO                  = ^AUDIO;
HTIMER                  = ^TIMER;
HDIGDRIVER              = ^DIGDRIVER;
HSAMPLE                 = ^SAMPLE;
HMDIDRIVER              = ^MDIDRIVER;
HSEQUENCE               = ^SEQUENCE;
HWAVESYNTH              = ^WAVESYNTH;
HREDBOOK                = ^REDBOOK;
HSTREAM                 = ^STREAM;
HDLSDEVICE              = ^DLSDEVICE;
HDLSFILEID              = ^DLSFILEID;
AILLPDIRECTSOUND        = pchar;
AILLPDIRECTSOUNDBUFFER  = pchar;

RIBRESULT=S32;
HPROVIDER=U32;
HATTRIB=U32;
HINTENUM=U32;
HPROENUM=U32;
RIB_DATA_SUBTYPE=U32;
RIB_ENTRY_TYPE=U32;

SAMPLESTAGE=U32;
DIDRVSTAGE=U32;

RIB_INTERFACE_ENTRY=record
  ribtype:RIB_ENTRY_TYPE;
  entry_name:pchar;
  token:U32;
  subtype:RIB_DATA_SUBTYPE;
end;

HASISTREAM=S32;
ASIRESULT=S32;

H3DPOBJECT=pchar;
H3DSAMPLE=H3DPOBJECT;

M3DRESULT=S32;

M3DPROVIDER=record
  place:S32;
end;

ASISTAGE=record
  place:S32;
end;

MIXSTAGE=record
  place:S32;
end;

FLTSTAGE=record
  place:S32;
end;

DPINFO=record
  active:S32;
  provider:HPROVIDER;
  stagetype:S32;
end;

ADPCMDATA=record
  blocksize:U32;
  extrasamples:U32;
  blockleft:U32;
  step:U32;
  savesrc:U32;
  sample:U32;
  destend:U32;
  srcend:U32;
  samplesL:U32;
  samplesR:U32;
  moresamples:array[0..15] of U32;
end;

AILMIXINFO=record { used for AIL_process }
  Info:AILSOUNDINFO;
  mss_adpcm:ADPCMDATA;
  src_fract:U32;
  left_val:S32;
  right_val:S32;
end;

PAILMIXINFO=^AILMIXINFO;

{ Callback function types }

AILLENGTHYCB      = function(state:U32;user:U32):S32;
AILCODECSETPREFCB = function(pref:pchar;value:U32):S32;

AILTIMERCB   = procedure(user : U32);
AILSAMPLECB  = procedure(sample : HSAMPLE);
AILEVENTCB   = function(hmi : HMDIDRIVER;seq : HSEQUENCE; status : S32;
                        data_1 : S32; data_2 : S32): S32;
AILTIMBRECB  = function(hmi : HMDIDRIVER; bank : S32; patch : S32): S32;
AILPREFIXCB  = function(seq : HSEQUENCE; log : S32; data : S32): S32;
AILTRIGGERCB = procedure(seq : HSEQUENCE; log : S32; data : S32);
AILBEATCB    = procedure(hmi : HMDIDRIVER; seq : HSEQUENCE; beat : S32;
                         measure : S32);
AILSEQUENCECB = procedure(seq : HSEQUENCE);
AILSTREAMCB  = procedure(stream : HSTREAM);

{ Function Prototypes. }

{ VOID pointers in the MSSW.H are pointers to CHAR in this file. }

function  AIL_us_count     : S32; far;

function  AIL_mem_alloc_lock (size : U32): PCHAR; far;

procedure AIL_mem_free_lock  (ptr : PCHAR); far;

function  AIL_file_error     : S32; far;

function  AIL_file_size      (filename : PCHAR):S32; far;

function  AIL_file_read      (filename : PCHAR; dest : PCHAR): PCHAR; far;

function  AIL_file_write     (filename : PCHAR; buf : PCHAR; len : U32): S32; far;

function  AIL_WAV_file_write (filename : PCHAR; buf : PCHAR; len : U32; rate:S32; format:S32): S32; far;

function  AIL_quick_startup (use_digital : S32 ; use_MIDI : S32 ;
                             output_rate : U32 ; output_bits : S32 ;
                             output_channels : S32 ): S32; far;

procedure AIL_quick_handles( var dig:HDIGDRIVER; var mdi:HMDIDRIVER; var dls:HDLSDEVICE); far;

procedure AIL_quick_shutdown ; far;

function  AIL_quick_load (filename : PCHAR): HAUDIO; far;

function  AIL_quick_load_mem (buffer : PCHAR): HAUDIO; far;

function  AIL_quick_copy (audio : HAUDIO): HAUDIO; far;

procedure AIL_quick_unload (audio : HAUDIO); far;

function  AIL_quick_play (audio : HAUDIO; loop_count : U32): S32; far;

procedure AIL_quick_halt (audio : HAUDIO); far;

function  AIL_quick_status (audio : HAUDIO): S32; far;

function  AIL_quick_load_and_play (filename : PCHAR;
                                   loop_count : U32;
                                   wait_request : S32): HAUDIO; far;

procedure AIL_quick_set_volume (audio : HAUDIO; volume : S32; extravol : S32); far;

procedure AIL_quick_set_speed  (audio : HAUDIO; speed : S32); far;

procedure AIL_quick_set_ms_position (audio : HAUDIO; milliseconds : S32); far;

procedure AIL_quick_set_reverb (audio :HAUDIO;
                                reverb_level:F32;
                                reverb_reflect_time:F32;
                                reverb_decay_time:F32); far;

function  AIL_quick_ms_position (audio : HAUDIO): S32; far;

function  AIL_quick_ms_length (audio : HAUDIO): S32; far;

function  AIL_quick_type (audio : HAUDIO): S32; far;

function AIL_set_redist_directory(dir:PCHAR): PCHAR; far;

function  AIL_startup    : S32; far;

procedure AIL_shutdown   ; far;

function  AIL_get_preference (number : U32): S32; far;

function  AIL_set_preference (number : U32; value : S32): S32; far;

function  AIL_last_error : PCHAR; far;

procedure AIL_set_error  (error_msg : PCHAR); far;

procedure AIL_lock       ; far;

procedure AIL_unlock     ; far;

procedure AIL_delay      (intervals : S32); far;

function  AIL_background : S32; far;

function  AIL_register_timer (fn : AILTIMERCB): HTIMER; far;

function  AIL_set_timer_user (timer : HTIMER; user : U32): U32; far;

procedure AIL_set_timer_period (timer : HTIMER ; microseconds : U32); far;

procedure AIL_set_timer_frequency (timer : HTIMER; hertz : U32); far;

procedure AIL_set_timer_divisor (timer : HTIMER; PIT_divisor : U32); far;

procedure AIL_start_timer (timer : HTIMER); far;

procedure AIL_start_all_timers ; far;

procedure AIL_stop_timer (timer : HTIMER); far;

procedure AIL_stop_all_timers ; far;

procedure AIL_release_timer_handle (timer : HTIMER); far;

procedure AIL_release_all_timers ; far;

function  AIL_waveOutOpen (var drvr : HDIGDRIVER;
                           lphWaveOut : PHWAVEOUT;
                           wDeviceID : U32 ;
                           lpFormat : PWAVEFORMAT): S32; far;

procedure AIL_waveOutClose (drvr : HDIGDRIVER); far;

function AIL_digital_CPU_percent    (drvr : HDIGDRIVER): S32; far;

function AIL_digital_latency        (drvr : HDIGDRIVER): S32; far;

function AIL_digital_handle_release (drvr : HDIGDRIVER): S32; far;

function AIL_digital_handle_reacquire (drvr : HDIGDRIVER): S32; far;

procedure AIL_serve; far;

function  AIL_allocate_sample_handle (dig : HDIGDRIVER): HSAMPLE; far;

function AIL_allocate_file_sample  (dig : HDIGDRIVER;
                                    file_image : PCHAR;
                                    block : S32): HSAMPLE; far;

procedure AIL_release_sample_handle (S : HSAMPLE); far;

procedure AIL_init_sample (S : HSAMPLE); far;

function  AIL_set_sample_file (S : HSAMPLE; file_image : PCHAR;
                               block : S32): S32; far;

function  AIL_set_named_sample_file (S : HSAMPLE; file_ext:pchar;file_image :PCHAR; file_size:U32;
                               block : S32): S32; far;

procedure AIL_set_sample_address (S :HSAMPLE; start : PCHAR;
                                  len : U32); far;

procedure AIL_set_sample_type (S : HSAMPLE; format : S32;
                               flags : U32); far;

procedure AIL_start_sample (S : HSAMPLE); far;

procedure AIL_stop_sample (S : HSAMPLE); far;

procedure AIL_resume_sample (S : HSAMPLE); far;

procedure AIL_end_sample (S : HSAMPLE); far;

procedure AIL_set_sample_playback_rate (S : HSAMPLE;
                                        playback_rate : S32); far;

procedure AIL_set_sample_volume (S : HSAMPLE; volume : S32); far;

procedure AIL_set_sample_pan (S : HSAMPLE; pan : S32); far;

procedure AIL_set_sample_adpcm_block_size (S : HSAMPLE; blocksize : S32); far;

procedure AIL_set_sample_loop_count (S : HSAMPLE; loop_count : S32); far;

procedure AIL_set_sample_loop_block (S : HSAMPLE;
                                     loop_start_offset : S32;
                                     loop_end_offset : S32); far;

function  AIL_sample_status (S : HSAMPLE): U32; far;

function  AIL_sample_playback_rate  (S : HSAMPLE): S32; far;

function  AIL_sample_volume         (S : HSAMPLE): S32; far;

function  AIL_sample_pan            (S : HSAMPLE): S32; far;

function  AIL_sample_loop_count     (S : HSAMPLE): S32; far;

procedure AIL_set_digital_master_volume (dig : HDIGDRIVER;
                                         master_volume : S32); far;

function  AIL_digital_master_volume (dig : HDIGDRIVER): S32; far;

function AIL_minimum_sample_buffer_size (dig : HDIGDRIVER;
                                        playback_rate : S32;
                                        format : S32): S32; far;

function  AIL_sample_buffer_ready (S : HSAMPLE): S32; far;

procedure AIL_load_sample_buffer (S : HSAMPLE;
                                  buff_num : U32;
                                  buffer : PCHAR;
                                  len : U32); far;

function  AIL_sample_buffer_info (S : HSAMPLE;
                                  pos0 : PU32;
                                  len0 : PU32;
                                  pos1 : PU32;
                                  len1 : PU32): S32; far;

procedure AIL_set_sample_position (S : HSAMPLE;
                                   pos : U32); far;

function  AIL_sample_position (S : HSAMPLE): U32; far;

procedure AIL_set_sample_ms_position (S : HSAMPLE;
                                   milliseconds : U32); far;

procedure AIL_sample_ms_position (S : HSAMPLE; var total_ms:S32;var cur_ms:S32); far;

function  AIL_sample_granularity    (S : HSAMPLE): S32; far;

function  AIL_register_SOB_callback (S : HSAMPLE;
                                     SOB : AILSAMPLECB): AILSAMPLECB; far;

function  AIL_register_EOB_callback (S : HSAMPLE;
                                     EOB : AILSAMPLECB): AILSAMPLECB; far;

function  AIL_register_EOS_callback (S : HSAMPLE;
                                     EOS : AILSAMPLECB): AILSAMPLECB; far;

function  AIL_register_EOF_callback (S : HSAMPLE;
                                     EOFILE : AILSAMPLECB): AILSAMPLECB; far;

procedure AIL_set_sample_user_data (S : HSAMPLE;
                                    index : U32;
                                    value : S32); far;

function  AIL_sample_user_data (S : HSAMPLE; index : U32): S32; far;

function AIL_active_sample_count (dig : HDIGDRIVER): S32; far;

procedure AIL_digital_configuration (dig : HDIGDRIVER;
                                     rate : PS32;
                                     format : PS32;
                                     str : PCHAR); far;

function  AIL_set_direct_buffer_control (S : HSAMPLE;
                                         command : U32): S32; far;

procedure AIL_get_DirectSound_info (S : HSAMPLE;
                                    lplpDS : AILLPDIRECTSOUND;
                                    lplpDSB : AILLPDIRECTSOUNDBUFFER); far;

procedure AIL_set_sample_reverb(S:HSAMPLE;
                                reverb_level:F32;
                                reverb_reflect_time:F32;
                                reverb_decay_time:F32); far;

procedure AIL_sample_reverb    (S:HSAMPLE;
                                var reverb_level:F32;
                                var reverb_reflect_time:F32;
                                var reverb_decay_time:F32); far;

procedure AIL_set_stream_reverb(S:HSTREAM;
                                reverb_level:F32;
                                reverb_reflect_time:F32;
                                reverb_decay_time:F32); far;

procedure AIL_stream_reverb    (S:HSTREAM;
                                var reverb_level:F32;
                                var reverb_reflect_time:F32;
                                var reverb_decay_time:F32); far;


function  AIL_midiOutOpen(var drvr : HMDIDRIVER;
                          lphMidiOut : PHMIDIOUT;
                          dwDeviceID : U32 ): S32; far;

procedure AIL_midiOutClose (mdi : HMDIDRIVER); far;

function AIL_MIDI_handle_release (drvr : HMDIDRIVER): S32; far;

function AIL_MIDI_handle_reacquire (drvr : HMDIDRIVER): S32; far;

function  AIL_allocate_sequence_handle (mdi : HMDIDRIVER): HSEQUENCE; far;

procedure AIL_release_sequence_handle (S : HSEQUENCE); far;

function  AIL_init_sequence (S : HSEQUENCE;
                             start : PCHAR;
                             sequence_num : S32): S32; far;

procedure AIL_start_sequence        (S : HSEQUENCE); far;

procedure AIL_stop_sequence         (S : HSEQUENCE); far;

procedure AIL_resume_sequence       (S : HSEQUENCE); far;

procedure AIL_end_sequence          (S : HSEQUENCE); far;

procedure AIL_set_sequence_tempo (S : HSEQUENCE;
                                  tempo : S32;
                                  milliseconds : S32); far;

procedure AIL_set_sequence_volume (S : HSEQUENCE;
                                   volume : S32;
                                   milliseconds : S32); far;

procedure AIL_set_sequence_loop_count (S : HSEQUENCE;
                                       loop_count : S32); far;

function AIL_sequence_status       (S : HSEQUENCE):U32; far;

function AIL_sequence_tempo        (S : HSEQUENCE):S32; far;

function AIL_sequence_volume       (S : HSEQUENCE):S32; far;

function AIL_sequence_loop_count   (S : HSEQUENCE):S32; far;

procedure AIL_set_XMIDI_master_volume (mdi : HMDIDRIVER;
                                       master_volume : S32); far;

function  AIL_XMIDI_master_volume (mdi : HMDIDRIVER): S32; far;

function  AIL_active_sequence_count (mdi : HMDIDRIVER): S32; far;

function  AIL_controller_value (S : HSEQUENCE;
                                channel : S32;
                                controller_num : S32): S32; far;

function  AIL_channel_notes (S : HSEQUENCE;
                             channel : S32): S32; far;

procedure AIL_sequence_position (S : HSEQUENCE;
                                 beat : PS32;
                                 measure : PS32); far;

procedure AIL_set_sequence_ms_position(S:HSEQUENCE; milliseconds:S32); far;

procedure AIL_sequence_ms_position (S : HSEQUENCE; var total_ms:S32;var cur_ms:S32); far;

procedure AIL_branch_index (S : HSEQUENCE;
                            marker : U32); far;

function  AIL_register_prefix_callback (S : HSEQUENCE;
                                        callback : AILPREFIXCB): AILPREFIXCB; far;

function  AIL_register_trigger_callback (S : HSEQUENCE;
                                         callback:AILTRIGGERCB): AILTRIGGERCB; far;

function  AIL_register_sequence_callback (S : HSEQUENCE;
                                          callback:AILSEQUENCECB): AILSEQUENCECB; far;

function  AIL_register_beat_callback   (S : HSEQUENCE;
                                        callback:AILBEATCB): AILBEATCB; far;

function  AIL_register_event_callback (mdi : HMDIDRIVER;
                                       callback : AILEVENTCB): AILEVENTCB; far;

function  AIL_register_timbre_callback (mdi : HMDIDRIVER;
                                        callback : AILTIMBRECB): AILTIMBRECB; far;

procedure AIL_set_sequence_user_data (S : HSEQUENCE;
                                      index : U32;
                                      value : S32); far;

function  AIL_sequence_user_data (S : HSEQUENCE;
                                  index : U32): S32; far;

procedure AIL_register_ICA_array (S : HSEQUENCE;
                                  arr : PCHAR); far;

function  AIL_lock_channel (mdi : HMDIDRIVER): S32; far;

procedure AIL_release_channel (mdi : HMDIDRIVER;
                               channel : S32); far;

procedure AIL_map_sequence_channel (S : HSEQUENCE;
                                    seq_channel : S32;
                                    new_channel : S32); far;

function  AIL_true_sequence_channel (S : HSEQUENCE;
                                     seq_channel : S32): S32; far;

procedure AIL_send_channel_voice_message (mdi : HMDIDRIVER;
                                          S : HSEQUENCE;
                                          status : S32;
                                          data_1 : S32;
                                          data_2 : S32); far;

procedure AIL_send_sysex_message (mdi : HMDIDRIVER;
                                  buffer : PCHAR); far;

function  AIL_create_wave_synthesizer (dig : HDIGDRIVER;
                                       mdi : HMDIDRIVER;
                                       wave_lib : PCHAR;
                                       polyphony : S32): HWAVESYNTH; far;

procedure AIL_destroy_wave_synthesizer (W : HWAVESYNTH); far;

function  AIL_redbook_open(which : U32): HREDBOOK; far;

function  AIL_redbook_open_drive(c : S32): HREDBOOK; far;

procedure AIL_redbook_close(hand : HREDBOOK); far;

procedure AIL_redbook_eject(hand : HREDBOOK); far;

procedure AIL_redbook_retract(hand : HREDBOOK); far;

function  AIL_redbook_status(hand : HREDBOOK): U32; far;

function  AIL_redbook_tracks(hand : HREDBOOK): U32; far;

function  AIL_redbook_track(hand : HREDBOOK): U32; far;

procedure AIL_redbook_track_info(hand : HREDBOOK; tracknum : U32;
                                 startmsec : PU32; endmsec : PU32); far;

function  AIL_redbook_id(hand : HREDBOOK): U32; far;

function  AIL_redbook_position(hand : HREDBOOK): U32; far;

function  AIL_redbook_play(hand : HREDBOOK;startmsec : U32; endmsec : U32): U32; far;

function  AIL_redbook_stop(hand : HREDBOOK): U32; far;

function  AIL_redbook_pause(hand : HREDBOOK): U32; far;

function  AIL_redbook_resume(hand : HREDBOOK): U32; far;

function  AIL_redbook_volume(hand : HREDBOOK):S32; far;

function  AIL_redbook_set_volume(hand:HREDBOOK;volume:S32):S32; far;



function  AIL_open_stream(dig:HDIGDRIVER; filename:pchar; stream_mem:S32):HSTREAM; far;

procedure AIL_close_stream(stream:HSTREAM); far;

function  AIL_service_stream(stream:HSTREAM; fillup:S32):S32; far;

procedure AIL_start_stream(stream:HSTREAM); far;

procedure AIL_pause_stream(stream:HSTREAM; onoff:S32); far;

procedure AIL_set_stream_volume(stream:HSTREAM; volume:S32); far;

procedure AIL_set_stream_pan(stream:HSTREAM; pan:S32); far;

function  AIL_stream_volume(stream:HSTREAM):S32; far;

function  AIL_stream_pan(stream:HSTREAM):S32; far;

procedure AIL_set_stream_playback_rate(stream:HSTREAM; rate:S32); far;

function  AIL_stream_playback_rate(stream:HSTREAM):S32; far;

function  AIL_stream_loop_count(stream:HSTREAM):S32; far;

procedure AIL_set_stream_loop_count(stream:HSTREAM; count:S32); far;

procedure AIL_set_stream_loop_block(stream:HSTREAM; loop_start,loop_end:S32); far;

function  AIL_stream_status(stream:HSTREAM):S32; far;

procedure AIL_set_stream_position(stream:HSTREAM; offset:S32); far;

function  AIL_stream_position(stream:HSTREAM):S32; far;

procedure AIL_set_stream_ms_position(stream:HSTREAM; milliseconds:S32); far;

procedure AIL_stream_ms_position (S : HSTREAM; var total_ms:S32;var cur_ms:S32); far;

procedure AIL_stream_info(stream:HSTREAM; datarate:PS32; sndtype:PS32; length:PS32; memory:PS32); far;

function  AIL_register_stream_callback(stream:HSTREAM; callback:AILSTREAMCB):AILSTREAMCB; far;

procedure AIL_auto_service_stream(stream:HSTREAM; onoff:S32); far;

function  AIL_stream_user_data(stream:HSTREAM; index:U32):S32; far;

procedure AIL_set_stream_user_data(stream:HSTREAM; index:U32; value:S32); far;

function AIL_DLS_open(mdi:HMDIDRIVER; dig:HDIGDRIVER;libname:PCHAR;
                      flags, rate:U32; bits, channels:S32):HDLSDEVICE; far;

procedure AIL_DLS_close(dls:HDLSDEVICE;flags:U32); far;

function AIL_DLS_load_file(dls:HDLSDEVICE;filename:PCHAR;flags:U32):HDLSFILEID; far;

function AIL_DLS_load_memory(dls:HDLSDEVICE;buffer:PCHAR;flags:U32):HDLSFILEID; far;

procedure AIL_DLS_unload(dls:HDLSDEVICE;dlsid:HDLSFILEID); far;

procedure AIL_DLS_compact(dls:HDLSDEVICE); far;

procedure AIL_DLS_get_info(dls:HDLSDEVICE;var info:AILDLSINFO; var PercentCPU:S32); far;

procedure AIL_DLS_set_reverb  (dls:HDLSDEVICE;
                               reverb_level:F32;
                               reverb_reflect_time:F32;
                               reverb_decay_time:F32); far;

procedure AIL_DLS_get_reverb  (dls:HDLSDEVICE;
                               var reverb_level:F32;
                               var reverb_reflect_time:F32;
                               var reverb_decay_time:F32); far;

function AIL_WAV_info(data:PCHAR; var info:AILSOUNDINFO):S32; far;

function AIL_size_processed_digital_audio(dest_rate:U32;dest_format:U32;num_srcs:S32;src:PAILMIXINFO):S32; far;

function AIL_process_digital_audio(dest_buf:pchar;dest_size:S32;dest_rate:U32;dest_format:U32;num_srcs:S32;
           src:PAILMIXINFO):S32; far;

function AIL_compress_ADPCM(var info:AILSOUNDINFO;var outdata:PCHAR;var outsize:U32):S32; far;

function AIL_decompress_ADPCM(var info:AILSOUNDINFO; var outdata:PCHAR; var outsize:U32):S32; far;

function AIL_compress_ASI(var info:AILSOUNDINFO;filename:pchar;var outdata:PCHAR;var outsize:U32;
           lengthy:AILLENGTHYCB):S32; far;

function AIL_decompress_ASI(var info:AILSOUNDINFO;insize:U32;filename:pchar; var outdata:PCHAR;
           var outsize:U32;lengthy:AILLENGTHYCB):S32; far;

function AIL_compress_DLS(dls:PCHAR;var mls:PCHAR;var mlssize:U32;lengthy:AILLENGTHYCB):S32; far;

function AIL_merge_DLS_with_XMI(xmi:PCHAR;dls:PCHAR;var mss:PCHAR;var msssize:U32):S32; far;

function AIL_extract_DLS( source_image:PCHAR;source_size:U32;
                          var XMI_output_data:PCHAR; var XMI_output_size:U32;
                          var DLS_output_data:PCHAR; var DLS_output_size:U32;lengthy:AILLENGTHYCB):S32; far;

function AIL_filter_DLS_with_XMI(xmi:PCHAR;dls:PCHAR;
                                 var DLS_output_data:PCHAR; var DLS_output_size:U32;
                                 flags:S32;lengthy:AILLENGTHYCB):S32; far;

function AIL_MIDI_to_XMI       (MIDI:PCHAR;MIDI_size:U32;
                                var XMI_output_data:PCHAR; var XMI_output_size:U32;
                                flags:S32):S32; far;

function AIL_list_MIDI         (MIDI:PCHAR;MIDI_size:U32;
                                var lst_data:PCHAR; var lst_size:U32;
                                flags:S32):S32; far;

function AIL_list_DLS          (DLS:PCHAR;
                                var lst_data:PCHAR; var lst_size:U32;
                                flags:S32):S32; far;

function AIL_file_type(data:PCHAR;size:U32):S32; far;

function AIL_find_DLS    ( source_image:PCHAR;source_size:U32;
                          var XMI_output_data:PCHAR; var XMI_output_size:U32;
                          var DLS_output_data:PCHAR; var DLS_output_size:U32):S32; far;

implementation




function  AIL_us_count     : S32; external MSS_lib;

function  AIL_mem_alloc_lock (size : U32): PCHAR; external MSS_lib;

procedure AIL_mem_free_lock  (ptr : PCHAR); external MSS_lib;

function  AIL_file_error     : S32; external MSS_lib;

function  AIL_file_size      (filename : PCHAR):S32; external MSS_lib;

function  AIL_file_read      (filename : PCHAR; dest : PCHAR): PCHAR; external MSS_lib;

function  AIL_WAV_file_write (filename : PCHAR; buf : PCHAR; len : U32; rate:S32; format:S32): S32; external MSS_lib;

function  AIL_file_write     (filename : PCHAR; buf : PCHAR; len : U32): S32; external MSS_lib;

function  AIL_quick_startup (use_digital : S32 ; use_MIDI : S32 ;
                             output_rate : U32 ; output_bits : S32 ;
                             output_channels : S32 ): S32; external MSS_lib;

procedure AIL_quick_handles( var dig:HDIGDRIVER; var mdi:HMDIDRIVER; var dls:HDLSDEVICE);
          external MSS_lib;

procedure AIL_quick_shutdown ; external MSS_lib;

function  AIL_quick_load (filename : PCHAR): HAUDIO; external MSS_lib;

function  AIL_quick_load_mem (buffer : PCHAR): HAUDIO; external MSS_lib;

function  AIL_quick_copy (audio : HAUDIO): HAUDIO; external MSS_lib;

procedure AIL_quick_unload (audio : HAUDIO); external MSS_lib;

function  AIL_quick_play (audio : HAUDIO; loop_count : U32): S32; external MSS_lib;

procedure AIL_quick_halt (audio : HAUDIO); external MSS_lib;

function  AIL_quick_status (audio : HAUDIO): S32; external MSS_lib;

function  AIL_quick_load_and_play (filename : PCHAR;
                                   loop_count : U32;
                                   wait_request : S32): HAUDIO; external MSS_lib;

procedure AIL_quick_set_volume (audio : HAUDIO; volume : S32; extravol : S32); external MSS_lib;

procedure AIL_quick_set_speed  (audio : HAUDIO; speed : S32); external MSS_lib;

procedure AIL_quick_set_ms_position (audio : HAUDIO; milliseconds : S32); external MSS_lib;

procedure AIL_quick_set_reverb (audio :HAUDIO;
                                reverb_level:F32;
                                reverb_reflect_time:F32;
                                reverb_decay_time:F32); external MSS_lib;

function  AIL_quick_ms_position (audio : HAUDIO): S32; external MSS_lib;

function  AIL_quick_ms_length (audio : HAUDIO): S32; external MSS_lib;

function  AIL_quick_type (audio : HAUDIO): S32; external MSS_lib;

function AIL_set_redist_directory(dir:PCHAR): PCHAR; external MSS_lib;

function  AIL_startup    : S32; external MSS_lib;

procedure AIL_shutdown   ; external MSS_lib;

function  AIL_get_preference (number : U32): S32; external MSS_lib;

function  AIL_set_preference (number : U32; value : S32): S32; external MSS_lib;

function  AIL_last_error : PCHAR; external MSS_lib;

procedure AIL_set_error  (error_msg : PCHAR); external MSS_lib;

procedure AIL_lock       ; external MSS_lib;

procedure AIL_unlock     ; external MSS_lib;

procedure AIL_delay      (intervals : S32); external MSS_lib;

function  AIL_background : S32; external MSS_lib;

function  AIL_register_timer (fn : AILTIMERCB): HTIMER; external MSS_lib;

function  AIL_set_timer_user (timer : HTIMER; user : U32): U32; external MSS_lib;

procedure AIL_set_timer_period (timer : HTIMER ; microseconds : U32); external MSS_lib;

procedure AIL_set_timer_frequency (timer : HTIMER; hertz : U32); external MSS_lib;

procedure AIL_set_timer_divisor (timer : HTIMER; PIT_divisor : U32); external MSS_lib;

procedure AIL_start_timer (timer : HTIMER); external MSS_lib;

procedure AIL_start_all_timers ; external MSS_lib;

procedure AIL_stop_timer (timer : HTIMER); external MSS_lib;

procedure AIL_stop_all_timers ; external MSS_lib;

procedure AIL_release_timer_handle (timer : HTIMER); external MSS_lib;

procedure AIL_release_all_timers ; external MSS_lib;

function  AIL_waveOutOpen (var drvr : HDIGDRIVER;
                           lphWaveOut : PHWAVEOUT;
                           wDeviceID : U32 ;
                           lpFormat : PWAVEFORMAT): S32; external MSS_lib;

procedure AIL_waveOutClose (drvr : HDIGDRIVER); external MSS_lib;

function AIL_digital_CPU_percent    (drvr : HDIGDRIVER): S32; external MSS_lib;

function AIL_digital_latency        (drvr : HDIGDRIVER): S32; external MSS_lib;

function AIL_digital_handle_release (drvr : HDIGDRIVER): S32; external MSS_lib;

function AIL_digital_handle_reacquire (drvr : HDIGDRIVER): S32; external MSS_lib;

procedure AIL_serve; external MSS_lib;

function  AIL_allocate_sample_handle (dig : HDIGDRIVER): HSAMPLE; external MSS_lib;

function AIL_allocate_file_sample  (dig : HDIGDRIVER;
                                    file_image : PCHAR;
                                    block : S32): HSAMPLE; external MSS_lib;

procedure AIL_release_sample_handle (S : HSAMPLE); external MSS_lib;

procedure AIL_init_sample (S : HSAMPLE); external MSS_lib;

function  AIL_set_sample_file (S : HSAMPLE; file_image : PCHAR;
                               block : S32): S32; external MSS_lib;

function  AIL_set_named_sample_file (S : HSAMPLE; file_ext:pchar;file_image :PCHAR; file_size:U32;
                               block : S32): S32; external MSS_lib;

procedure AIL_set_sample_address (S :HSAMPLE; start : PCHAR;
                                  len : U32); external MSS_lib;

procedure AIL_set_sample_type (S : HSAMPLE; format : S32;
                               flags : U32); external MSS_lib;

procedure AIL_start_sample (S : HSAMPLE); external MSS_lib;

procedure AIL_stop_sample (S : HSAMPLE); external MSS_lib;

procedure AIL_resume_sample (S : HSAMPLE); external MSS_lib;

procedure AIL_end_sample (S : HSAMPLE); external MSS_lib;

procedure AIL_set_sample_playback_rate (S : HSAMPLE;
                                        playback_rate : S32); external MSS_lib;

procedure AIL_set_sample_volume (S : HSAMPLE; volume : S32); external MSS_lib;

procedure AIL_set_sample_pan (S : HSAMPLE; pan : S32); external MSS_lib;

procedure AIL_set_sample_adpcm_block_size (S : HSAMPLE; blocksize : S32); external MSS_lib;

procedure AIL_set_sample_loop_count (S : HSAMPLE; loop_count : S32); external MSS_lib;

procedure AIL_set_sample_loop_block (S : HSAMPLE;
                                     loop_start_offset : S32;
                                     loop_end_offset : S32); external MSS_lib;

function  AIL_sample_status (S : HSAMPLE): U32; external MSS_lib;

function  AIL_sample_playback_rate  (S : HSAMPLE): S32; external MSS_lib;

function  AIL_sample_volume         (S : HSAMPLE): S32; external MSS_lib;

function  AIL_sample_pan            (S : HSAMPLE): S32; external MSS_lib;

function  AIL_sample_loop_count     (S : HSAMPLE): S32; external MSS_lib;

procedure AIL_set_digital_master_volume (dig : HDIGDRIVER;
                                         master_volume : S32); external MSS_lib;

function  AIL_digital_master_volume (dig : HDIGDRIVER): S32; external MSS_lib;

function AIL_minimum_sample_buffer_size (dig : HDIGDRIVER;
                                        playback_rate : S32;
                                        format : S32): S32; external MSS_lib;

function  AIL_sample_buffer_ready (S : HSAMPLE): S32; external MSS_lib;

procedure AIL_load_sample_buffer (S : HSAMPLE;
                                  buff_num : U32;
                                  buffer : PCHAR;
                                  len : U32); external MSS_lib;

function  AIL_sample_buffer_info (S : HSAMPLE;
                                  pos0 : PU32;
                                  len0 : PU32;
                                  pos1 : PU32;
                                  len1 : PU32): S32; external MSS_lib;

procedure AIL_set_sample_position (S : HSAMPLE;
                                   pos : U32); external MSS_lib;

function  AIL_sample_position (S : HSAMPLE): U32; external MSS_lib;

procedure AIL_set_sample_ms_position (S : HSAMPLE;
                                   milliseconds : U32); external MSS_lib;

procedure AIL_sample_ms_position (S : HSAMPLE; var total_ms:S32;var cur_ms:S32); external MSS_lib;

function  AIL_sample_granularity    (S : HSAMPLE): S32; external MSS_lib;

function  AIL_register_SOB_callback (S : HSAMPLE;
                                     SOB : AILSAMPLECB): AILSAMPLECB; external MSS_lib;

function  AIL_register_EOB_callback (S : HSAMPLE;
                                     EOB : AILSAMPLECB): AILSAMPLECB; external MSS_lib;

function  AIL_register_EOS_callback (S : HSAMPLE;
                                     EOS : AILSAMPLECB): AILSAMPLECB; external MSS_lib;

function  AIL_register_EOF_callback (S : HSAMPLE;
                                     EOFILE : AILSAMPLECB): AILSAMPLECB; external MSS_lib;

procedure AIL_set_sample_user_data (S : HSAMPLE;
                                    index : U32;
                                    value : S32); external MSS_lib;

function  AIL_sample_user_data (S : HSAMPLE; index : U32): S32; external MSS_lib;

function AIL_active_sample_count (dig : HDIGDRIVER): S32; external MSS_lib;

procedure AIL_digital_configuration (dig : HDIGDRIVER;
                                     rate : PS32;
                                     format : PS32;
                                     str : PCHAR); external MSS_lib;

function  AIL_set_direct_buffer_control (S : HSAMPLE;
                                         command : U32): S32; external MSS_lib;

procedure AIL_get_DirectSound_info (S : HSAMPLE;
                                    lplpDS : AILLPDIRECTSOUND;
                                    lplpDSB : AILLPDIRECTSOUNDBUFFER); external MSS_lib;

procedure AIL_set_sample_reverb(S:HSAMPLE;
                                reverb_level:F32;
                                reverb_reflect_time:F32;
                                reverb_decay_time:F32); external MSS_lib;

procedure AIL_sample_reverb    (S:HSAMPLE;
                                var reverb_level:F32;
                                var reverb_reflect_time:F32;
                                var reverb_decay_time:F32); external MSS_lib;

procedure AIL_set_stream_reverb(S:HSTREAM;
                                reverb_level:F32;
                                reverb_reflect_time:F32;
                                reverb_decay_time:F32); external MSS_lib;

procedure AIL_stream_reverb    (S:HSTREAM;
                                var reverb_level:F32;
                                var reverb_reflect_time:F32;
                                var reverb_decay_time:F32); external MSS_lib;


function  AIL_midiOutOpen(var drvr : HMDIDRIVER;
                          lphMidiOut : PHMIDIOUT;
                          dwDeviceID : U32 ): S32; external MSS_lib;

procedure AIL_midiOutClose (mdi : HMDIDRIVER); external MSS_lib;

function AIL_MIDI_handle_release (drvr : HMDIDRIVER): S32; external MSS_lib;

function AIL_MIDI_handle_reacquire (drvr : HMDIDRIVER): S32; external MSS_lib;

function  AIL_allocate_sequence_handle (mdi : HMDIDRIVER): HSEQUENCE; external MSS_lib;

procedure AIL_release_sequence_handle (S : HSEQUENCE); external MSS_lib;

function  AIL_init_sequence (S : HSEQUENCE;
                             start : PCHAR;
                             sequence_num : S32): S32; external MSS_lib;

procedure AIL_start_sequence        (S : HSEQUENCE); external MSS_lib;

procedure AIL_stop_sequence         (S : HSEQUENCE); external MSS_lib;

procedure AIL_resume_sequence       (S : HSEQUENCE); external MSS_lib;

procedure AIL_end_sequence          (S : HSEQUENCE); external MSS_lib;

procedure AIL_set_sequence_tempo (S : HSEQUENCE;
                                  tempo : S32;
                                  milliseconds : S32); external MSS_lib;

procedure AIL_set_sequence_volume (S : HSEQUENCE;
                                   volume : S32;
                                   milliseconds : S32); external MSS_lib;

procedure AIL_set_sequence_loop_count (S : HSEQUENCE;
                                       loop_count : S32); external MSS_lib;

function AIL_sequence_status       (S : HSEQUENCE):U32; external MSS_lib;

function AIL_sequence_tempo        (S : HSEQUENCE):S32; external MSS_lib;

function AIL_sequence_volume       (S : HSEQUENCE):S32; external MSS_lib;

function AIL_sequence_loop_count   (S : HSEQUENCE):S32; external MSS_lib;

procedure AIL_set_XMIDI_master_volume (mdi : HMDIDRIVER;
                                       master_volume : S32); external MSS_lib;

function  AIL_XMIDI_master_volume (mdi : HMDIDRIVER): S32; external MSS_lib;

function  AIL_active_sequence_count (mdi : HMDIDRIVER): S32; external MSS_lib;

function  AIL_controller_value (S : HSEQUENCE;
                                channel : S32;
                                controller_num : S32): S32; external MSS_lib;

function  AIL_channel_notes (S : HSEQUENCE;
                             channel : S32): S32; external MSS_lib;

procedure AIL_sequence_position (S : HSEQUENCE;
                                 beat : PS32;
                                 measure : PS32); external MSS_lib;

procedure AIL_set_sequence_ms_position (S : HSEQUENCE;
                                   milliseconds : U32); external MSS_lib;

procedure AIL_sequence_ms_position (S : HSEQUENCE; var total_ms:S32;var cur_ms:S32); external MSS_lib;

procedure AIL_branch_index (S : HSEQUENCE;
                            marker : U32); external MSS_lib;

function  AIL_register_prefix_callback (S : HSEQUENCE;
                                        callback : AILPREFIXCB): AILPREFIXCB; external MSS_lib;

function  AIL_register_trigger_callback (S : HSEQUENCE;
                                         callback:AILTRIGGERCB): AILTRIGGERCB; external MSS_lib;

function  AIL_register_sequence_callback (S : HSEQUENCE;
                                          callback:AILSEQUENCECB): AILSEQUENCECB; external MSS_lib;

function  AIL_register_beat_callback   (S : HSEQUENCE;
                                        callback:AILBEATCB): AILBEATCB; external MSS_lib;

function  AIL_register_event_callback (mdi : HMDIDRIVER;
                                       callback : AILEVENTCB): AILEVENTCB; external MSS_lib;

function  AIL_register_timbre_callback (mdi : HMDIDRIVER;
                                        callback : AILTIMBRECB): AILTIMBRECB; external MSS_lib;

procedure AIL_set_sequence_user_data (S : HSEQUENCE;
                                      index : U32;
                                      value : S32); external MSS_lib;

function  AIL_sequence_user_data (S : HSEQUENCE;
                                  index : U32): S32; external MSS_lib;

procedure AIL_register_ICA_array (S : HSEQUENCE;
                                  arr : PCHAR); external MSS_lib;

function  AIL_lock_channel (mdi : HMDIDRIVER): S32; external MSS_lib;

procedure AIL_release_channel (mdi : HMDIDRIVER;
                               channel : S32); external MSS_lib;

procedure AIL_map_sequence_channel (S : HSEQUENCE;
                                    seq_channel : S32;
                                    new_channel : S32); external MSS_lib;

function  AIL_true_sequence_channel (S : HSEQUENCE;
                                     seq_channel : S32): S32; external MSS_lib;

procedure AIL_send_channel_voice_message (mdi : HMDIDRIVER;
                                          S : HSEQUENCE;
                                          status : S32;
                                          data_1 : S32;
                                          data_2 : S32); external MSS_lib;

procedure AIL_send_sysex_message (mdi : HMDIDRIVER;
                                  buffer : PCHAR); external MSS_lib;

function  AIL_create_wave_synthesizer (dig : HDIGDRIVER;
                                       mdi : HMDIDRIVER;
                                       wave_lib : PCHAR;
                                       polyphony : S32): HWAVESYNTH; external MSS_lib;

procedure AIL_destroy_wave_synthesizer (W : HWAVESYNTH); external MSS_lib;

function  AIL_redbook_open_drive(c : S32): HREDBOOK; external MSS_lib;

function  AIL_redbook_open(which : U32): HREDBOOK; external MSS_lib;

procedure AIL_redbook_close(hand : HREDBOOK); external MSS_lib;

procedure AIL_redbook_eject(hand : HREDBOOK); external MSS_lib;

procedure AIL_redbook_retract(hand : HREDBOOK); external MSS_lib;

function  AIL_redbook_status(hand : HREDBOOK): U32; external MSS_lib;

function  AIL_redbook_tracks(hand : HREDBOOK): U32; external MSS_lib;

function  AIL_redbook_track(hand : HREDBOOK): U32; external MSS_lib;

procedure AIL_redbook_track_info(hand : HREDBOOK; tracknum : U32;
                                 startmsec : PU32; endmsec : PU32); external MSS_lib;

function  AIL_redbook_id(hand : HREDBOOK): U32; external MSS_lib;

function  AIL_redbook_position(hand : HREDBOOK): U32; external MSS_lib;

function  AIL_redbook_play(hand : HREDBOOK;startmsec : U32; endmsec : U32): U32; external MSS_lib;

function  AIL_redbook_stop(hand : HREDBOOK): U32; external MSS_lib;

function  AIL_redbook_pause(hand : HREDBOOK): U32; external MSS_lib;

function  AIL_redbook_resume(hand : HREDBOOK): U32; external MSS_lib;

function  AIL_redbook_volume(hand : HREDBOOK):S32; external MSS_lib;

function  AIL_redbook_set_volume(hand:HREDBOOK;volume:S32):S32; external MSS_lib;


function  AIL_open_stream(dig:HDIGDRIVER; filename:pchar; stream_mem:S32):HSTREAM;
          external MSS_lib; 

procedure AIL_close_stream(stream:HSTREAM);
          external MSS_lib;

function  AIL_service_stream(stream:HSTREAM; fillup:S32):S32;
          external MSS_lib; 

procedure AIL_start_stream(stream:HSTREAM);
          external MSS_lib; 

procedure AIL_pause_stream(stream:HSTREAM; onoff:S32);
          external MSS_lib;

procedure AIL_set_stream_volume(stream:HSTREAM; volume:S32);
          external MSS_lib; 

procedure AIL_set_stream_pan(stream:HSTREAM; pan:S32);
          external MSS_lib;

function  AIL_stream_volume(stream:HSTREAM):S32;
          external MSS_lib;

function  AIL_stream_pan(stream:HSTREAM):S32;
          external MSS_lib;

procedure AIL_set_stream_playback_rate(stream:HSTREAM; rate:S32);
          external MSS_lib;

function  AIL_stream_playback_rate(stream:HSTREAM):S32;
          external MSS_lib;

function  AIL_stream_loop_count(stream:HSTREAM):S32;
          external MSS_lib;

procedure AIL_set_stream_loop_count(stream:HSTREAM; count:S32);
          external MSS_lib;

procedure AIL_set_stream_loop_block(stream:HSTREAM; loop_start,loop_end:S32);
          external MSS_lib;

function  AIL_stream_status(stream:HSTREAM):S32;
          external MSS_lib;

procedure AIL_set_stream_position(stream:HSTREAM; offset:S32);
          external MSS_lib;

function  AIL_stream_position(stream:HSTREAM):S32;
          external MSS_lib;

procedure AIL_set_stream_ms_position(stream:HSTREAM; milliseconds:S32);
          external MSS_lib;

procedure AIL_stream_ms_position (S : HSTREAM; var total_ms:S32;var cur_ms:S32);
          external MSS_lib;

procedure AIL_stream_info(stream:HSTREAM; datarate:PS32; sndtype:PS32; length:PS32; memory:PS32);
          external MSS_lib;

function  AIL_register_stream_callback(stream:HSTREAM; callback:AILSTREAMCB):AILSTREAMCB;
          external MSS_lib;

procedure AIL_auto_service_stream(stream:HSTREAM; onoff:S32);
          external MSS_lib;

function  AIL_stream_user_data(stream:HSTREAM; index:U32):S32;
          external MSS_lib;

procedure AIL_set_stream_user_data(stream:HSTREAM; index:U32; value:S32);
          external MSS_lib;

function AIL_DLS_open(mdi:HMDIDRIVER; dig:HDIGDRIVER;libname:PCHAR;
                      flags, rate:U32; bits, channels:S32):HDLSDEVICE;
          external MSS_lib;

procedure AIL_DLS_close(dls:HDLSDEVICE;flags:U32);
          external MSS_lib;

function AIL_DLS_load_file(dls:HDLSDEVICE;filename:PCHAR;flags:U32):HDLSFILEID ;
          external MSS_lib;

function AIL_DLS_load_memory(dls:HDLSDEVICE;buffer:PCHAR;flags:U32):HDLSFILEID ;
          external MSS_lib;

procedure AIL_DLS_unload(dls:HDLSDEVICE;dlsid:HDLSFILEID);
          external MSS_lib;

procedure AIL_DLS_compact(dls:HDLSDEVICE);
          external MSS_lib;

procedure AIL_DLS_get_info(dls:HDLSDEVICE;var info:AILDLSINFO; var PercentCPU:S32);
          external MSS_lib;

procedure AIL_DLS_set_reverb  (dls:HDLSDEVICE;
                               reverb_level:F32;
                               reverb_reflect_time:F32;
                               reverb_decay_time:F32);
          external MSS_lib;

procedure AIL_DLS_get_reverb  (dls:HDLSDEVICE;
                               var reverb_level:F32;
                               var reverb_reflect_time:F32;
                               var reverb_decay_time:F32);
          external MSS_lib;

function AIL_size_processed_digital_audio(dest_rate:U32;dest_format:U32;num_srcs:S32;
          src:PAILMIXINFO):S32;
          external MSS_lib;

function AIL_process_digital_audio(dest_buf:pchar;dest_size:S32;dest_rate:U32;dest_format:
          U32;num_srcs:S32;src:PAILMIXINFO):S32;
          external MSS_lib;

function AIL_WAV_info(data:PCHAR; var info:AILSOUNDINFO):S32;
          external MSS_lib;

function AIL_compress_ADPCM(var info:AILSOUNDINFO;var outdata:PCHAR;var outsize:U32):S32;
          external MSS_lib;

function AIL_decompress_ADPCM(var info:AILSOUNDINFO; var outdata:PCHAR; var outsize:U32):S32;
          external MSS_lib;

function AIL_compress_ASI(var info:AILSOUNDINFO;filename:pchar;var outdata:PCHAR;var outsize:U32;
          lengthy:AILLENGTHYCB):S32;
          external MSS_lib;

function AIL_decompress_ASI(var info:AILSOUNDINFO;insize:U32;filename:pchar; var outdata:PCHAR; 
          var outsize:U32;lengthy:AILLENGTHYCB):S32;
          external MSS_lib;

function AIL_compress_DLS(dls:PCHAR;var mls:PCHAR;var mlssize:U32;lengthy:AILLENGTHYCB):S32;
          external MSS_lib;

function AIL_merge_DLS_with_XMI(xmi:PCHAR;dls:PCHAR;var mss:PCHAR;var msssize:U32):S32;
          external MSS_lib;

function AIL_extract_DLS( source_image:PCHAR;source_size:U32;
                          var XMI_output_data:PCHAR; var XMI_output_size:U32;
                          var DLS_output_data:PCHAR; var DLS_output_size:U32;lengthy:AILLENGTHYCB):S32;
          external MSS_lib;

function AIL_filter_DLS_with_XMI(xmi:PCHAR;dls:PCHAR;
                                 var DLS_output_data:PCHAR; var DLS_output_size:U32;
                                 flags:S32;lengthy:AILLENGTHYCB):S32;
          external MSS_lib;

function AIL_MIDI_to_XMI       (MIDI:PCHAR;MIDI_size:U32;
                                var XMI_output_data:PCHAR; var XMI_output_size:U32;
                                flags:S32):S32;
          external MSS_lib;

function AIL_list_MIDI         (MIDI:PCHAR;MIDI_size:U32;
                                var lst_data:PCHAR; var lst_size:U32;
                                flags:S32):S32;
          external MSS_lib;

function AIL_list_DLS          (DLS:PCHAR;
                                var lst_data:PCHAR; var lst_size:U32;
                                flags:S32):S32;
          external MSS_lib;

function AIL_file_type(data:PCHAR;size:U32):S32;
          external MSS_lib;

function AIL_find_DLS    ( source_image:PCHAR;source_size:U32;
                          var XMI_output_data:PCHAR; var XMI_output_size:U32;
                          var DLS_output_data:PCHAR; var DLS_output_size:U32):S32;
          external MSS_lib;

end.
