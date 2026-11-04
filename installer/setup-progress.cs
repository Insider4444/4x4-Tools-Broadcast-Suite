// Shared progress reporting for the NSIS window and the installer transcript.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

namespace FourByFour {
    public static class SetupProgress {
        sealed class Transfer { public string Label; public long Done, Total, Network; }
        static readonly object Gate = new object();
        static readonly Dictionary<string, Transfer> Transfers = new Dictionary<string, Transfer>();
        static readonly Stopwatch Clock = Stopwatch.StartNew();
        static IntPtr Bar, Status, Metrics;
        static double Start, Span, LastPercent, LastPaint = -10, LastLog = -10;
        static long Total, CryptoFirst;
        static string Label = "Preparing Setup";
        static readonly Stopwatch PhaseClock = Stopwatch.StartNew();
        public static string LastStatus = "", LastMetrics = "";
        public static double Percent { get { return LastPercent; } }
        public static void Reset() {
            lock(Gate) { Transfers.Clear(); }
            Bar=Status=Metrics=IntPtr.Zero;LastPercent=0;LastPaint=LastLog=-10;LastStatus=LastMetrics="";
        }

        delegate bool EnumChild(IntPtr window, IntPtr unused);
        [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent, EnumChild callback, IntPtr unused);
        [DllImport("user32.dll")] static extern int GetDlgCtrlID(IntPtr window);
        [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder name, int size);
        [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageTimeoutW")]
        static extern IntPtr SendText(IntPtr window, uint message, IntPtr wparam, string text, uint flags, uint timeout, out IntPtr result);
        [DllImport("user32.dll", EntryPoint="SendMessageTimeoutW")]
        static extern IntPtr SendValue(IntPtr window, uint message, IntPtr wparam, IntPtr lparam, uint flags, uint timeout, out IntPtr result);

        public static void Attach(long parent) {
            if (parent == 0) return;
            EnumChildWindows(new IntPtr(parent), delegate(IntPtr window, IntPtr unused) {
                int id = GetDlgCtrlID(window);
                if (id == 1004) {
                    StringBuilder name = new StringBuilder(160); GetClassName(window, name, name.Capacity);
                    if (name.ToString().IndexOf("msctls_progress32", StringComparison.OrdinalIgnoreCase) >= 0) Bar = window;
                } else if (id == 1006) Status = window;
                else if (id == 17476) Metrics = window;
                return true;
            }, IntPtr.Zero);
            IntPtr result;
            if (Bar != IntPtr.Zero) SendValue(Bar, 0x406, IntPtr.Zero, new IntPtr(10000), 2, 100, out result);
        }
        public static void Begin(string label, double start, double span, long total) {
            Label = label; Start = start; Span = span; Total = Math.Max(1, total); CryptoFirst = 0;
            PhaseClock.Restart(); LastPaint = -10; Report(0, true);
        }
        static string Amount(long bytes) { return bytes >= 1073741824 ? (bytes / 1073741824.0).ToString("0.00") + " GB" : (bytes / 1048576.0).ToString("0.0") + " MB"; }
        static string Remaining(double seconds) {
            if (Double.IsNaN(seconds) || Double.IsInfinity(seconds) || seconds < 0) return "Estimating time";
            return seconds >= 60 ? "~" + Math.Ceiling(seconds / 60).ToString("0") + " min" : "~" + Math.Ceiling(seconds).ToString("0") + " sec";
        }
        static void Paint(string label, string metrics, double percent, bool force) {
            double now = Clock.Elapsed.TotalSeconds;
            if (!force && now - LastPaint < 0.25) return;
            LastPaint = now; LastPercent = Math.Max(LastPercent, Math.Min(100, Math.Max(0, percent)));
            LastStatus = label; LastMetrics = metrics;
            IntPtr result;
            if (Status != IntPtr.Zero) SendText(Status, 12, IntPtr.Zero, label, 2, 100, out result);
            if (Metrics != IntPtr.Zero) SendText(Metrics, 12, IntPtr.Zero, metrics, 2, 100, out result);
            if (Bar != IntPtr.Zero) SendValue(Bar, 0x402, new IntPtr((int)(LastPercent * 100)), IntPtr.Zero, 2, 100, out result);
            if (force || now - LastLog >= 4) { LastLog = now; Console.WriteLine(label + " | " + metrics); }
        }
        public static void Report(long done, bool force) {
            double fraction = Math.Min(1, Math.Max(0, done / (double)Total));
            double speed = done / Math.Max(0.1, PhaseClock.Elapsed.TotalSeconds);
            string metrics = Amount(done) + " / " + Amount(Total) + " (" + (fraction * 100).ToString("0") + "%)";
            if (done > 0) metrics += " | " + (speed / 1048576).ToString("0.0") + " MB/s | Step ETA " + Remaining((Total - done) / speed);
            else metrics += " | Starting...";
            Paint(Label, metrics, Start + Span * fraction, force);
        }
        public static void FinishPhase() { Report(Total, true); }
        public static void Message(string label, string detail, double percent) { Paint(label, detail, percent, true); }
        public static Action<string, long, long> CryptoReporter() {
            return delegate(string phase, long done, long total) {
                if (phase == "Authenticate") { CryptoFirst = total; Report(done, false); }
                else Report(CryptoFirst + done, false);
            };
        }
        public static void DownloadStart(string key, string label, long total, long done) {
            lock (Gate) { Transfers[key] = new Transfer { Label=label, Total=total, Done=done }; }
            Console.WriteLine((done>0?"Resuming: ":"Downloading: ")+label+" ("+Amount(total)+")");
        }
        public static void DownloadComplete(string key) {
            string label="";lock(Gate){Transfer item;if(Transfers.TryGetValue(key,out item)){item.Done=item.Total;label=item.Label;}}
            if(label!="")Console.WriteLine("Downloaded and verified: "+label);
        }
        public static void DownloadUpdate(string key, long done, long network) {
            lock (Gate) { Transfer item; if (Transfers.TryGetValue(key, out item)) { item.Done=done; item.Network+=network; } }
        }
        public static void RefreshDownloads(long expected, double elapsed) {
            long done=0, network=0; int active=0; string first="";
            lock (Gate) { foreach (Transfer item in Transfers.Values) { done+=item.Done; network+=item.Network; if(item.Done<item.Total) {active++; if(first=="") first=item.Label;} } }
            double fraction = expected == 0 ? 1 : Math.Min(1, done/(double)expected);
            double speed = network/Math.Max(0.1, elapsed);
            string title = active == 0 ? "Finishing downloads" : "Downloading: " + first + (active>1 ? " + " + (active-1) + " more" : "");
            string metrics = Amount(done)+" / "+Amount(expected)+" ("+(fraction*100).ToString("0")+"%) | "+(speed/1048576).ToString("0.0")+" MB/s | "+(speed>0?"ETA "+Remaining((expected-done)/speed):"Connecting ("+elapsed.ToString("0")+"s)");
            Paint(title, metrics, 5 + 40*fraction, false);
        }
    }

    public static class SetupFiles {
        public static string Hash(string path, bool progress) {
            using (SHA256 hash=SHA256.Create())
            using (FileStream input=new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1048576, FileOptions.SequentialScan)) {
                byte[] buffer=new byte[1048576]; int read; long done=0;
                while ((read=input.Read(buffer,0,buffer.Length))>0) { hash.TransformBlock(buffer,0,read,buffer,0);done+=read;if(progress)SetupProgress.Report(done,false); }
                hash.TransformFinalBlock(new byte[0],0,0);
                return BitConverter.ToString(hash.Hash).Replace("-","").ToLowerInvariant();
            }
        }
        public static void Extract(Stream input, string target, long expected, string digest) {
            bool success=false;
            try {
                using (SHA256 hash=SHA256.Create())
                using (FileStream output=new FileStream(target,FileMode.CreateNew,FileAccess.Write,FileShare.None,1048576,FileOptions.SequentialScan)) {
                    byte[] buffer=new byte[1048576];long done=0;int read;
                    while ((read=input.Read(buffer,0,buffer.Length))>0) {
                        done+=read;if(done>expected)throw new InvalidDataException("Archive entry exceeds its manifest size.");
                        output.Write(buffer,0,read);hash.TransformBlock(buffer,0,read,buffer,0);SetupProgress.Report(done,false);
                    }
                    hash.TransformFinalBlock(new byte[0],0,0);
                    if(done!=expected || BitConverter.ToString(hash.Hash).Replace("-","").ToLowerInvariant()!=digest)
                        throw new InvalidDataException("Extracted file failed its SHA-256 check.");
                    success=true;
                }
            } finally { if(!success && File.Exists(target))File.Delete(target); }
            SetupProgress.FinishPhase();
        }
    }
}
