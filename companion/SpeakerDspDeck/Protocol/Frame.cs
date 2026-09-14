using System.Buffers.Binary;

namespace SpeakerDspDeck.Protocol;

// Little-endian CMD frame builder. [opcode][payload...]. Floats IEEE-754 LE.
public sealed class Frame
{
    private readonly List<byte> _b = new();

    public static Frame Cmd(CmdOp op)
    {
        var f = new Frame();
        f._b.Add((byte)op);
        return f;
    }

    public Frame U8(int v)  { _b.Add((byte)v); return this; }
    public Frame Bool(bool v){ _b.Add((byte)(v ? 1 : 0)); return this; }

    public Frame U16(int v)
    {
        Span<byte> s = stackalloc byte[2];
        BinaryPrimitives.WriteUInt16LittleEndian(s, (ushort)v);
        _b.AddRange(s.ToArray());
        return this;
    }

    public Frame F32(float v)
    {
        Span<byte> s = stackalloc byte[4];
        BinaryPrimitives.WriteSingleLittleEndian(s, v);
        _b.AddRange(s.ToArray());
        return this;
    }

    public byte[] ToArray() => _b.ToArray();
}

// EVT frame reader (device -> app).
public readonly struct EvtReader
{
    private readonly byte[] _data;
    public EvtReader(byte[] data) { _data = data; }

    public EvtOp Op => (EvtOp)_data[0];
    public byte U8(int off) => _data[1 + off];
    public ushort U16(int off) => BinaryPrimitives.ReadUInt16LittleEndian(_data.AsSpan(1 + off, 2));
    public uint U32(int off) => BinaryPrimitives.ReadUInt32LittleEndian(_data.AsSpan(1 + off, 4));
    public int Length => _data.Length;
}
