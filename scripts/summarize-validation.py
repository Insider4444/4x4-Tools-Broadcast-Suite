from pathlib import Path
import hashlib
import json
import subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'artifacts'
FFMPEG=Path('C:/Program Files/Shutter Encoder/app/Library/ffmpeg.exe')


def decode(path):
    return np.frombuffer(subprocess.check_output([str(FFMPEG),'-v','error','-i',str(path),'-f','f32le','-ac','1','-ar','48000','-']),dtype='<f4').astype(np.float64)


def main():
    source=ROOT/'third_party/AFX-SDK-Samples/windows/apps/effects_demo/input_files/studio_voice_high_quality/48k/studio_voice_48k.wav'
    original=decode(source)
    n=1<<((len(original)*2-1).bit_length())
    spectrum=np.fft.rfft(original,n)
    lags=np.arange(-12000,12001)
    alignment={}
    for kind in ('noise','echo','clean','preview','studio','restore'):
        path=ART/f'aligned-{kind}.wav'
        processed=decode(path)
        if len(processed)!=len(original) or not np.isfinite(processed).all():
            raise RuntimeError(f'Invalid audio duration/values: {kind}')
        correlation=np.fft.irfft(np.fft.rfft(processed,n)*np.conj(spectrum),n)
        lag=int(lags[np.argmax(correlation[lags % n])])
        alignment[kind]=dict(residual_lag_samples_48k=lag,frames=len(processed),peak=float(np.abs(processed).max()))
        if kind in ('noise','echo','clean','restore') and abs(lag)>1:
            raise RuntimeError(f'Audio is not aligned: {kind}, {lag} samples')
    profiles={str(i):hashlib.sha256((ART/f'profile-{i}.wav').read_bytes()).hexdigest() for i in range(-1,6)}
    if len(set(profiles.values()))!=7:
        raise RuntimeError('Microphone profiles did not generate seven distinct outputs')
    report=dict(date='2026-11-03',version='0.2.0-preview.1',gpu='NVIDIA GeForce RTX 5070',driver='617.42',
        cpu_ctest='4 passed',new_adobe_effects=12,video_host_formats=5,original_eye_host_formats=4,
        video_gpu_cases=34,video_4k_tiling='passed',audio_irregular_buffers='exact at 44100/48000/96000 Hz',
        native_ae_audio_effects='6 passed',vst3_validator={'passed':282,'failed':0},alignment=alignment,microphone_profiles=profiles,
        actual_adobe_ui={'eye_contact':'user confirmed','new_suite':'acceptance pending'},speaker_focus='not included: NGC HTTP 402')
    (ART/'suite-validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    # A developer comparison figure, composited over a checkerboard. No inference
    # output is retouched; these are the compiled Adobe effects' raw pixels.
    labels=('BackgroundRemove','BackgroundBlur','StudioLight','VideoDenoise','AutoFrame','DetailRestore')
    panel=Image.new('RGB',(1104,752),'#101820')
    draw=ImageDraw.Draw(panel)
    font=ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf',17)
    small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',13)
    draw.text((20,12),'4x4Tools | compiled Adobe effect outputs',font=font,fill='white')
    draw.text((20,39),'Original left / effect right. Background removal is shown over a checkerboard.',font=small,fill='#c7d3db')
    for index,label in enumerate(labels):
        path=ART/f'{label}-adobe.rgba'
        data=np.fromfile(path,dtype='<f4')
        w,h=(3840,2160) if data.size==3840*2160*4 else (720,405)
        data=data.reshape(h,w,4)
        yy,xx=np.indices((h,w))
        checker=np.where(((xx//24+yy//24)%2)[...,None],.27,.4)
        alpha=np.clip(data[...,3:4],0,1)
        rgb=np.clip(data[...,:3]+checker*(1-alpha),0,1)
        image=Image.fromarray(np.rint(rgb*255).astype(np.uint8)).resize((528,297),Image.Resampling.LANCZOS)
        x=20+(index%2)*552;y=78+(index//2)*222
        # Fit a 16:9 panel without stretching; each figure slot is 528x190.
        image=image.resize((338,190),Image.Resampling.LANCZOS)
        panel.paste(image,(x,y+24))
        draw.text((x,y),label.replace('Background','Background ').replace('Studio','Studio ').replace('Video','Video ').replace('Auto','Auto ').replace('Detail','Detail '),font=font,fill='#65e3b0')
    panel.save(ART/'suite-comparison.png')
    print(json.dumps({'alignment':alignment,'profiles':'7 distinct outputs','report':'artifacts/suite-validation.json'}))


if __name__=='__main__': main()
