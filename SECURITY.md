# Security Policy

## Supported versions

This is a single-branch hobby firmware project. Only the current `main` is
supported — fixes land there and you reflash. There are no maintained release
branches.

## Reporting a vulnerability

Please do **not** open a public issue for a security problem.

Use GitHub's private reporting: the repository's **Security → Report a
vulnerability** tab. If that is unavailable, email
<contact@patbaumgartner.com>.

Include what you found, how to reproduce it, and the commit you tested. Expect
an acknowledgement within a week; this is a spare-time project, so please be
patient with the fix timeline.

## Threat model

The firmware is designed for a trusted home LAN and deliberately assumes it:

- **The HTTP API is plain HTTP.** Every endpoint requires `API_TOKEN`, but the
  token and the audio stream travel unencrypted. Anyone who can passively
  observe your LAN can capture both. Do not expose port 80 of the device to the
  internet, and do not port-forward it.
- **There is no rate limiting.** A device on your LAN can guess tokens as fast
  as the ESP32 answers. Use a long random `API_TOKEN`; the build refuses
  anything shorter than 16 characters.
- **The device streams live audio and occupancy on request.** Treat `API_TOKEN`
  as being as sensitive as a microphone in your home, because that is what it
  unlocks.
- **Outbound pushes are validated.** An `https://` Gotify URL is verified
  against the pinned Let's Encrypt root in `src/certs.h`. An `http://` Gotify
  URL sends your Gotify application token in cleartext — only use one on a
  network you control.
- **`record.ps1` / `record.sh` pass the token to `ffmpeg` as an argument.**
  While a recording runs it is visible in the local process list. That is
  acceptable on a personal machine; it is not on a shared one.

Reports that a token can be read from `src/secrets.h` on the device's own flash
are out of scope — the ESP32-C3 has no encrypted flash configured here, and
physical access is game over.

## Credentials

`src/secrets.h` is gitignored and must never be committed. If you ever push it,
rotating the WiFi password, the Gotify application token, and `API_TOKEN` is
required — deleting the file from the history is not enough.
