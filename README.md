# Whisperer

Tiny local web UI for batch video/audio transcription with Faster-Whisper + CUDA.

## Features

- Pick a source folder
- Optionally include subfolders
- Save `.txt` beside each video, or under another destination folder
- Create destination folders from the UI
- Uses one Whisper model load per batch
- Filesystem access is restricted to the root folder passed at startup

## Requirements

Assumes NVIDIA/CUDA and Faster-Whisper are already working in `~/whisper-env`.

```bash
sudo apt install -y build-essential libmicrohttpd-dev
make
```

## Run

Expose only the media tree you want the app to access:

```bash
./whisperer /home/pierre/Videos 8080
```

Open:

```text
http://VM-IP:8080
```

If your Faster-Whisper virtualenv is elsewhere:

```bash
WHISPER_PYTHON=/path/to/venv/bin/python ./whisperer /path/to/media 8080
```

The default model is `small` (appropriate for a 4 GB Quadro P1000). To change it:

```bash
WHISPER_MODEL=medium ./whisperer /home/pierre/Videos 8080
```

When saving to a separate destination, recursive input subfolders are preserved under that destination to avoid filename collisions.
