"""Correct stdcall argument counts that argcount.py guessed for FlatOut.exe's imports
(real Win32 signatures). A wrong count corrupts the guest stack on return."""
import re, sys
REAL = {
 'acmStreamUnprepareHeader':3,'acmStreamPrepareHeader':3,'acmStreamSize':4,'acmStreamOpen':8,
 'acmFormatSuggest':5,'acmStreamClose':2,'acmStreamConvert':3,
 'waveOutOpen':6,'waveOutClose':1,'waveOutPrepareHeader':3,'waveOutUnprepareHeader':3,'waveOutWrite':3,
 'waveOutReset':1,'waveOutGetPosition':3,'waveOutGetDevCapsA':3,'waveOutGetNumDevs':0,
 'waveInAddBuffer':3,'waveInPrepareHeader':3,'waveInUnprepareHeader':3,'waveInGetDevCapsA':3,
 'waveInGetNumDevs':0,'waveInStart':1,'waveInOpen':6,'waveInClose':1,'waveInReset':1,
 'timeGetDevCaps':2,'timeBeginPeriod':1,'timeEndPeriod':1,'timeGetTime':0,
 'WSACloseEvent':1,'WSAResetEvent':1,'WSACreateEvent':0,
 'GetUserDefaultLangID':0,'GetDateFormatA':6,'GetTimeFormatA':6,
 'ZOOM_Init':0,
 'EnableWindow':2,'GetDlgItem':2,
}
p = sys.argv[1]; t = open(p).read(); n = 0
def f(m):
    global n
    a, d, name, k = m.groups()
    if name in REAL and int(k) != REAL[name]:
        n += 1; print(f'  {name}: {k} -> {REAL[name]}'); k = str(REAL[name])
    return '{%su,"%s","%s",%s}' % (a, d, name, k)
t = re.sub(r'\{(0x[0-9a-f]+)u,"([^"]+)","([^"]+)",(-?\d+)\}', f, t)
open(p, 'w').write(t); print(n, 'fixed')
# synthetic entries: dsound.dll exports FMOD resolves with GetProcAddress (iat 0 = not in the IAT)
EXTRA = [('DSOUND.dll','DirectSoundCreate',3),('DSOUND.dll','DirectSoundCreate8',3),('DSOUND.dll','DirectSoundEnumerateA',2),
         ('DSOUND.dll','DirectSoundCaptureCreate',3),('DSOUND.dll','DirectSoundCaptureEnumerateA',2)]
t = open(p).read()
if 'DirectSoundEnumerateA' not in t:
    cnt = int(re.search(r'g_imp_count = (\d+);', t).group(1))
    t = t.replace(f'g_imp_count = {cnt};', f'g_imp_count = {cnt+len(EXTRA)};')
    add = ''.join('  {0x00000000u,"%s","%s",%d},\n' % e for e in EXTRA)
    i = t.rindex('};'); t = t[:i] + add + t[i:]
    open(p,'w').write(t); print('added', len(EXTRA), 'synthetic imports')
