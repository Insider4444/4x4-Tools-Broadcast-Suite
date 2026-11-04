"""Verify native progress-control updates in an owned, never-shown test window."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    (ROOT / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='progress-fixture-', dir=ROOT / 'build') as temp:
        script = Path(temp) / 'run.ps1'
        code = r'''$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -Path '@PROGRESS@' -ReferencedAssemblies ([Uri].Assembly.Location)
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class FixtureWindow {
 [DllImport("user32.dll",EntryPoint="SetWindowLongPtrW")]public static extern IntPtr SetId(IntPtr window,int index,IntPtr value);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern int GetWindowText(IntPtr window,StringBuilder value,int length);
 [DllImport("user32.dll")]public static extern IntPtr SendMessage(IntPtr window,uint message,IntPtr wparam,IntPtr lparam);
 public static string Text(IntPtr window){StringBuilder value=new StringBuilder(512);GetWindowText(window,value,value.Capacity);return value.ToString();}
}
'@
$form=[Windows.Forms.Form]::new();$form.ShowInTaskbar=$false
try {
 $formHandle=$form.Handle
 $bar=[Windows.Forms.ProgressBar]::new();$status=[Windows.Forms.Label]::new();$metrics=[Windows.Forms.Label]::new()
 $form.Controls.AddRange([Windows.Forms.Control[]]@($bar,$status,$metrics))
 $null=[FixtureWindow]::SetId($bar.Handle,-12,[IntPtr]1004)
 $null=[FixtureWindow]::SetId($status.Handle,-12,[IntPtr]1006)
 $null=[FixtureWindow]::SetId($metrics.Handle,-12,[IntPtr]17476)
 [FourByFour.SetupProgress]::Reset();[FourByFour.SetupProgress]::Attach($form.Handle.ToInt64())
 [FourByFour.SetupProgress]::Begin('Unpacking: NVIDIA processing library',45,40,100MB)
 [FourByFour.SetupProgress]::Report(50MB,$true)
 if([FixtureWindow]::Text($status.Handle) -ne 'Unpacking: NVIDIA processing library'){throw 'Native phase label was not updated'}
 if([FixtureWindow]::Text($metrics.Handle) -notmatch '50.0 MB / 100.0 MB \(50%\).*MB/s.*Step ETA'){throw 'Live byte/speed/ETA line was not updated'}
 if([FixtureWindow]::SendMessage($bar.Handle,0x408,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 6500){throw 'Native progress bar does not reflect weighted bytes'}
 [FourByFour.SetupProgress]::FinishPhase()
 if([FixtureWindow]::SendMessage($bar.Handle,0x408,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 8500){throw 'Phase completion did not advance progress'}
 [FourByFour.SetupProgress]::Message('Installation complete','Ready to open Adobe.',100)
 if([FixtureWindow]::Text($metrics.Handle) -ne 'Ready to open Adobe.'){throw 'Final status missing'}
} finally {$form.Dispose()}
Write-Output 'PASS: native byte-weighted bar, current file label, MB/s, ETA and completion status; no test window shown.'
'''.replace('@PROGRESS@', str(ROOT / 'installer/setup-progress.cs').replace("'", "''"))
        script.write_text(code, encoding='utf-8-sig')
        result = subprocess.run(['powershell.exe', '-STA', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script)],
                                cwd=ROOT, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        print(result.stdout.strip().splitlines()[-1])


if __name__ == '__main__':
    main()
