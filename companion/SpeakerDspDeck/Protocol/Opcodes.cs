namespace SpeakerDspDeck.Protocol;

// Mirrors firmware/dsp_engine/protocol.h. See PROTOCOL.md. Keep in lockstep.
public static class Proto
{
    public const ushort ProtocolVersion = 1;

    // GATT UUIDs (base 9F3E7Axx-5C2B-4D8E-9A1F-6B0C1D2E3F40)
    public static readonly Guid Service = new("9F3E7A00-5C2B-4D8E-9A1F-6B0C1D2E3F40");
    public static readonly Guid CmdChar = new("9F3E7A01-5C2B-4D8E-9A1F-6B0C1D2E3F40");
    public static readonly Guid EvtChar = new("9F3E7A02-5C2B-4D8E-9A1F-6B0C1D2E3F40");
}

public enum CmdOp : byte
{
    Hello             = 0x01,
    GetStatus         = 0x02,
    SetMasterGain     = 0x10,
    SetMute           = 0x11,
    SetProfile        = 0x12,
    SetCrossoverHz    = 0x13,
    SetVoicingPreamp  = 0x14,
    SetVoicingBand    = 0x20,
    ClearVoicing      = 0x21,
    SetDriverLevel    = 0x30,
    SetDriverDelay    = 0x31,
    SetDriverEqBand   = 0x32,
    SavePreset        = 0x40,
    LoadPreset        = 0x41,
    SaveToNvs         = 0x42,
}

public enum EvtOp : byte
{
    Hello  = 0x81,
    Status = 0x82,
    Ack    = 0x83,
}
