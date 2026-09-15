// protocol.h — BLE control protocol v1 (FROZEN 2026-08-23) + additive
// read-back extension (2026-09-15: CMD_GET_PARAMS / EVT_PARAM / EVT_PARAMS_DONE).
// Canonical opcode/UUID definitions. The C# app mirrors these in
// Protocol/Opcodes.cs. See PROTOCOL.md for the full spec. Keep in lockstep with
// dsp_params.h (Params.version) — bump PROTOCOL_VERSION on any breaking change.
// The read-back extension is NOT breaking: an old app never sends 0x03, and an
// old firmware NAKs it with 0xFF, which the new app treats as "push instead".
#pragma once
#include <stdint.h>

constexpr uint16_t PROTOCOL_VERSION = 1;
constexpr uint16_t FW_VERSION       = 3;   // 2 = read-back + boot-profile restore; 3 = USB audio source

// GATT UUIDs (base 9F3E7Axx-5C2B-4D8E-9A1F-6B0C1D2E3F40)
#define DSP_SVC_UUID "9F3E7A00-5C2B-4D8E-9A1F-6B0C1D2E3F40"
#define DSP_CMD_UUID "9F3E7A01-5C2B-4D8E-9A1F-6B0C1D2E3F40"  // write
#define DSP_EVT_UUID "9F3E7A02-5C2B-4D8E-9A1F-6B0C1D2E3F40"  // notify

// CMD opcodes (app -> device)
enum CmdOp : uint8_t {
  CMD_HELLO                = 0x01,
  CMD_GET_STATUS           = 0x02,
  CMD_GET_PARAMS           = 0x03,   // device streams EVT_PARAM x N then EVT_PARAMS_DONE
  CMD_SET_MASTER_GAIN      = 0x10,   // f32 linear
  CMD_SET_MUTE             = 0x11,   // u8
  CMD_SET_PROFILE          = 0x12,   // u8 (0=Normal/48k, 1=HighRes/96k)
  CMD_SET_CROSSOVER_HZ     = 0x13,   // f32
  CMD_SET_VOICING_PREAMP   = 0x14,   // f32 dB
  CMD_SET_VOICING_BAND     = 0x20,   // u8 idx,u8 en,u8 type,f32 f,f32 Q,f32 gDb
  CMD_CLEAR_VOICING        = 0x21,
  CMD_SET_DRIVER_LEVEL     = 0x30,   // u8 drv, f32 dB
  CMD_SET_DRIVER_DELAY     = 0x31,   // u8 drv, u16 samples
  CMD_SET_DRIVER_EQ_BAND   = 0x32,   // u8 drv,u8 idx,u8 en,u8 type,f32 f,f32 Q,f32 gDb
  CMD_SAVE_PRESET          = 0x40,   // u8 slot
  CMD_LOAD_PRESET          = 0x41,   // u8 slot
  CMD_SAVE_TO_NVS          = 0x42,
};

// EVT events (device -> app)
enum EvtOp : uint8_t {
  EVT_HELLO  = 0x81,   // u16 fw,u16 proto,u8 maxVoicing,u8 maxDriver,u8 numDrivers
  EVT_STATUS = 0x82,   // u8 profile,u8 muted,u32 rate,u8 linkFlags
  EVT_ACK    = 0x83,   // u8 opcode,u8 result
  // Read-back (reply to CMD_GET_PARAMS). Each EVT_PARAM carries one SET opcode
  // followed by that opcode's exact CMD payload, so the app decodes it with the
  // same field layout it encodes. Largest is SET_DRIVER_EQ_BAND: 18 bytes total,
  // inside the 20-byte notify limit of the default 23-byte MTU.
  EVT_PARAM       = 0x84,   // u8 setOpcode, <that SET's payload>
  EVT_PARAMS_DONE = 0x85,   // u8 count (number of EVT_PARAM frames sent)
};

enum Profile : uint8_t { PROFILE_NORMAL = 0, PROFILE_HIRES = 1 };
