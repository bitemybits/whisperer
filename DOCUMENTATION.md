# Whisperer Documentation

Whisperer is a minimal web interface for batch-transcribing video files with Faster-Whisper on an NVIDIA GPU.

## Architecture

- `whisperer.c` — tiny C HTTP server and embedded web UI
- `worker.py` — Faster-Whisper batch worker
- `Makefile` — builds the C server
- `README.md` — quick start

The C server handles folder browsing, folder creation, input validation, job launching, and status display. The Python worker loads the Whisper model once and processes the selected videos.

## Requirements

The system assumes the NVIDIA driver, CUDA-capable GPU, Python environment, Faster-Whisper, and FFmpeg are already installed and working.

Default Python environment:

```bash
~/whisper-env/bin/python
```

You can override it with:

```bash
export WHISPER_PYTHON=/path/to/python
```

## Build

```bash
sudo apt update
sudo apt install -y build-essential libmicrohttpd-dev
git clone https://github.com/bitemybits/whisperer.git
cd whisperer
make
```

## Run

Choose the highest directory the web interface is allowed to access.

Example:

```bash
./whisperer /home/pierre/Videos 8080
```

Then open:

```text
http://MINT-VM-IP:8080
```

Example using the whole home directory:

```bash
./whisperer /home/pierre 8080
```

The UI cannot browse outside the root directory supplied at startup.

## Web interface

### Video folder

Select the folder containing the videos to transcribe.

### Include subfolders

Enable this option to search recursively through all directories below the selected video folder.

### Output mode

Two modes are available:

#### Same folder as each video

Each transcript is written beside its source video.

Example:

```text
/videos/course/lesson01.mp4
/videos/course/lesson01.txt
```

#### Destination folder

All transcripts are written into one selected output directory.

Example:

```text
/videos/course/lesson01.mp4
/transcripts/lesson01.txt
```

### Create folder

Select a parent folder, enter a new folder name, and click **Create**. The new folder immediately becomes available as a destination.

### Start transcription

Click **Start transcription**. The status panel refreshes automatically while the worker runs.

## Whisper settings

The default worker configuration is intended as a practical starting point for a 4 GB Quadro P1000:

```python
WhisperModel(
    "small",
    device="cuda",
    compute_type="float16"
)
```

The model is loaded once per batch, not once per video.

To change the model, edit `worker.py`.

Common choices:

```text
tiny
base
small
medium
```

For the P1000, `small` is the recommended starting point.

## Supported workflow

Typical sequence:

```text
Select source folder
        ↓
Optionally enable subfolders
        ↓
Choose output mode
        ↓
Optionally create output folder
        ↓
Start transcription
        ↓
Whisper loads once
        ↓
Videos are processed sequentially
        ↓
TXT transcripts are saved
```

## Security model

Whisperer deliberately uses a restricted filesystem root.

If started with:

```bash
./whisperer /home/pierre/Videos 8080
```

then the web interface can access only directories below:

```text
/home/pierre/Videos
```

The server rejects paths containing traversal attempts and resolves real filesystem paths before accepting them.

For this reason, do not run Whisperer with `/` as the root unless you intentionally want the entire machine exposed to the application.

## Networking

By default the service listens on port `8080`.

Example:

```bash
./whisperer /home/pierre/Videos 8080
```

You can use another port:

```bash
./whisperer /home/pierre/Videos 8090
```

For LAN-only use, access it using the Mint VM IP address.

If exposing it through a reverse proxy or Cloudflare Tunnel, authentication should be added before making it publicly accessible.

## Troubleshooting

### GPU not detected

Check:

```bash
nvidia-smi
```

The Quadro P1000 should appear.

### Faster-Whisper environment not found

Check:

```bash
~/whisper-env/bin/python --version
```

If your environment is elsewhere:

```bash
export WHISPER_PYTHON=/your/python/path
./whisperer /home/pierre/Videos 8080
```

### Build fails with `microhttpd.h` missing

Install:

```bash
sudo apt install libmicrohttpd-dev
```

Then rebuild:

```bash
make clean
make
```

### GPU memory error

Use a smaller model in `worker.py`, for example:

```python
"base"
```

or:

```python
"tiny"
```

### Verify GPU usage while transcribing

In another terminal:

```bash
watch -n 1 nvidia-smi
```

## Current design goals

Whisperer intentionally avoids unnecessary components:

- no database
- no framework
- no account system
- no external frontend libraries
- no job database
- no container requirement
- minimal C web server
- one lightweight Python inference worker

The goal is to keep the application small, fast, easy to understand, and easy to self-host.
