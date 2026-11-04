// Release envelope: AES-256-CBC + encrypt-then-HMAC-SHA256.
// This protects packaged data in transit/storage; the installer contains its
// release key and this is not DRM or a promise of model confidentiality.
using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace FourByFour {
    public static class PayloadCrypto {
        static readonly byte[] Magic = Encoding.ASCII.GetBytes("4X4ENC01");
        public static Action<string, long, long> Progress;
        public static void Decrypt(string source, string destination, byte[] keys) {
            if (keys.Length != 64) throw new InvalidDataException("Invalid release key.");
            byte[] aesKey = new byte[32], macKey = new byte[32];
            Buffer.BlockCopy(keys, 0, aesKey, 0, 32);
            Buffer.BlockCopy(keys, 32, macKey, 0, 32);
            try {
                using (FileStream input = File.OpenRead(source)) {
                    if (input.Length < 72 || (input.Length - 56) % 16 != 0)
                        throw new InvalidDataException("Invalid encrypted payload length.");
                    long authenticatedLength = input.Length - 32;
                    byte[] buffer = new byte[1024 * 1024];
                    using (HMACSHA256 mac = new HMACSHA256(macKey)) {
                        long remaining = authenticatedLength;
                        while (remaining > 0) {
                            int read = input.Read(buffer, 0, (int)Math.Min(buffer.Length, remaining));
                            if (read == 0) throw new EndOfStreamException();
                            mac.TransformBlock(buffer, 0, read, buffer, 0);
                            remaining -= read;
                            if (Progress != null) Progress("Authenticate", authenticatedLength - remaining, authenticatedLength);
                        }
                        mac.TransformFinalBlock(new byte[0], 0, 0);
                        byte[] tag = new byte[32];
                        ReadExactly(input, tag);
                        int difference = 0;
                        for (int i = 0; i < 32; ++i) difference |= tag[i] ^ mac.Hash[i];
                        if (difference != 0) throw new CryptographicException("Payload authentication failed.");
                    }
                    input.Position = 0;
                    byte[] header = new byte[8], iv = new byte[16];
                    ReadExactly(input, header); ReadExactly(input, iv);
                    for (int i = 0; i < 8; ++i)
                        if (header[i] != Magic[i]) throw new InvalidDataException("Invalid payload header.");
                    using (Aes aes = Aes.Create()) {
                        aes.KeySize = 256; aes.Key = aesKey; aes.IV = iv;
                        aes.Mode = CipherMode.CBC; aes.Padding = PaddingMode.PKCS7;
                        using (FileStream output = File.Create(destination))
                        using (CryptoStream decrypted = new CryptoStream(output, aes.CreateDecryptor(), CryptoStreamMode.Write)) {
                            long remaining = authenticatedLength - 24;
                            while (remaining > 0) {
                                int read = input.Read(buffer, 0, (int)Math.Min(buffer.Length, remaining));
                                if (read == 0) throw new EndOfStreamException();
                                decrypted.Write(buffer, 0, read); remaining -= read;
                                if (Progress != null) Progress("Decrypt", authenticatedLength - 24 - remaining, authenticatedLength - 24);
                            }
                            decrypted.FlushFinalBlock();
                        }
                    }
                }
            } catch {
                if (File.Exists(destination)) File.Delete(destination);
                throw;
            } finally { Array.Clear(aesKey, 0, aesKey.Length); Array.Clear(macKey, 0, macKey.Length); }
        }
        static void ReadExactly(Stream input, byte[] bytes) {
            int offset = 0;
            while (offset < bytes.Length) {
                int read = input.Read(bytes, offset, bytes.Length - offset);
                if (read == 0) throw new EndOfStreamException();
                offset += read;
            }
        }
    }
}
