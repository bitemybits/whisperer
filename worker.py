#!/usr/bin/env python3
import os, sys
from pathlib import Path
from faster_whisper import WhisperModel

src, dst = Path(sys.argv[1]), Path(sys.argv[2])
recursive, same = sys.argv[3] == '1', sys.argv[4] == '1'
status = Path(sys.argv[5])
VIDEO = {'.mp4','.mkv','.mov','.avi','.m4v','.webm','.mp3','.wav','.m4a','.flac'}
model_name = os.getenv('WHISPER_MODEL','small')

def note(s): status.write_text(s, encoding='utf-8')
files = [p for p in (src.rglob('*') if recursive else src.iterdir()) if p.is_file() and p.suffix.lower() in VIDEO]
if not files:
    note('Done. No supported video/audio files found.')
    raise SystemExit
note(f'Loading {model_name} model...\n{len(files)} file(s) found.')
model = WhisperModel(model_name, device='cuda', compute_type='float16')
for i, video in enumerate(files, 1):
    out = video.with_suffix('.txt') if same else (dst / video.relative_to(src)).with_suffix('.txt')
    out.parent.mkdir(parents=True, exist_ok=True)
    note(f'[{i}/{len(files)}] {video.name}')
    segments, _ = model.transcribe(str(video), vad_filter=True)
    with out.open('w', encoding='utf-8') as f:
        for s in segments: f.write(s.text.strip() + '\n')
note(f'Done. {len(files)} file(s) transcribed.')
