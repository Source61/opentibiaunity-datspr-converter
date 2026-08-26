using System;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;

namespace OpenTibiaUnity.Core.Assets
{
    public class ContentSprites
    {
        IO.BinaryStream m_BinaryReader;
        bool m_UseAlpha = false;

        int m_ClientVersion = 0;

        public uint Signature { get; private set; } = 0;
        public uint SpritesCount { get; private set; } = 0;
        public long SpritesOffset { get; private set; } = 0;
        
        public ContentSprites(byte[] buffer, int clientVersion, bool alpha = false) {
            m_BinaryReader = new IO.BinaryStream(buffer);
            m_ClientVersion = clientVersion;
            m_UseAlpha = alpha;

            Parse();
        }

        public static uint ClientVersionToSprSignature(int version) {
            switch (version) {
                case 770: return 0x439852be;
                case 1098: return 0x57bbd603;
                default: return 0;
            }
        }

        public byte[] ConvertTo(int newVersion) {
            var binaryWriter = new IO.BinaryStream();
            binaryWriter.WriteUnsignedInt(ClientVersionToSprSignature(newVersion));

            int padding = 0;
            if (newVersion >= 960) {
                binaryWriter.WriteUnsignedInt(SpritesCount);
                padding = m_ClientVersion < 960 ? 2 : 0;
            } else {
                binaryWriter.WriteUnsignedShort((ushort)SpritesCount);
                padding = m_ClientVersion >= 960 ? -2 : 0;
            }

            m_BinaryReader.Seek(SpritesOffset, System.IO.SeekOrigin.Begin);
            for (uint i = 0; i < SpritesCount; i++) {
                var spriteAddress = m_BinaryReader.ReadUnsignedInt();
                if (spriteAddress == 0)
                    binaryWriter.WriteUnsignedInt(0);
                else
                    binaryWriter.WriteUnsignedInt((uint)(spriteAddress + padding));
            }

            var pixels = m_BinaryReader.ReadRemaining();
            binaryWriter.Write(pixels, 0, pixels.Length);

            return binaryWriter.GetBuffer();
        }

        private void Parse() {
            Signature = m_BinaryReader.ReadUnsignedInt();
            SpritesCount = m_ClientVersion >= 960 ? m_BinaryReader.ReadUnsignedInt() : m_BinaryReader.ReadUnsignedShort();
            SpritesOffset = m_BinaryReader.Position;
        }

        public Image<Rgba32> GetSprite(uint id) {
            lock (m_BinaryReader)
                return RawGetSprite(id);
        }

        private Image<Rgba32> RawGetSprite(uint id) {
            if (id == 0 || m_BinaryReader == null)
                return null;
            
            m_BinaryReader.Seek((int)((id-1) * 4) + SpritesOffset, System.IO.SeekOrigin.Begin);

            uint spriteAddress = m_BinaryReader.ReadUnsignedInt();
            if (spriteAddress == 0)
                return null;

            m_BinaryReader.Seek((int)spriteAddress, System.IO.SeekOrigin.Begin);
            m_BinaryReader.Skip(3); // color values

            ushort pixelDataSize = m_BinaryReader.ReadUnsignedShort();

            int writePos = 0;
            int read = 0;
            byte channels = (byte)(m_UseAlpha ? 4 : 3);

            var pixels = new byte[4096]; // 32x32 RGBA, zero-initialized = transparent

            while (read < pixelDataSize && writePos < 4096) {
                ushort transparentPixels = m_BinaryReader.ReadUnsignedShort();
                ushort coloredPixels = m_BinaryReader.ReadUnsignedShort();

                writePos += Math.Min(transparentPixels * 4, 4096 - writePos);

                for (int i = 0; i < coloredPixels && writePos < 4096; i++) {
                    pixels[writePos++] = m_BinaryReader.ReadUnsignedByte();
                    pixels[writePos++] = m_BinaryReader.ReadUnsignedByte();
                    pixels[writePos++] = m_BinaryReader.ReadUnsignedByte();
                    pixels[writePos++] = m_UseAlpha ? m_BinaryReader.ReadUnsignedByte() : (byte)0xFF;
                }

                read += 4 + (channels * coloredPixels);
            }

            return Image.LoadPixelData<Rgba32>(pixels, 32, 32);
        }
    }
}
