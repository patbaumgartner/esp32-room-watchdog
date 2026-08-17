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

- **The API is plain HTTP and plain WebSocket.** Every endpoint on port 80
  (`/status`, `/calibrate`, `/ws`, `/audio.pcm`) requires `API_TOKEN`, but the
  token, the telemetry and the audio stream all travel unencrypted. Anyone who
  can passively observe your LAN can capture them. Do not expose the port to
  the internet, and do not port-forward it.
- **The device does not terminate TLS.** There is no `https://` or `wss://`
  listener on the ESP32 — the async server has no TLS support and the chip has
  no headroom for it. To reach the node from outside your LAN, put a reverse
  proxy in front of it and let the proxy terminate TLS:

  ```nginx
  location / {
      proxy_pass http://watchdog.local;
      proxy_http_version 1.1;           # required for the WebSocket upgrade
      proxy_set_header Upgrade $http_upgrade;
      proxy_set_header Connection "upgrade";
      proxy_buffering off;              # /audio.pcm is a live stream
      proxy_read_timeout 24h;           # longer than any recording
  }
  ```

  Clients then use `https://` and `wss://`; the firmware is unchanged and still
  sees plain HTTP on the LAN hop. That hop is only as private as the network
  between the proxy and the device, so keep them on the same trusted segment.
- **`TRUST_PROXY_HEADERS` affects logging only.** When enabled it makes the
  serial log show `X-Forwarded-For` instead of the proxy's own address. The
  header is trivially forgeable and is never used for authentication or access
  control. Leave it `false` unless a proxy you control is the only way in.
- **There is no rate limiting.** A device on your LAN can guess tokens as fast
  as the ESP32 answers. Use a long random `API_TOKEN`; the build refuses
  anything shorter than 16 characters. Request parsing also happens before
  authentication, so an unauthenticated client on the LAN can keep the server
  busy or exhaust its heap. That is a denial of service on a sensor, not a
  disclosure, and it is accepted.
- **The telemetry socket accepts one client, and the newest wins.** An
  authenticated client can disconnect another authenticated client by
  connecting. Both hold the same token, so this is not a privilege boundary —
  it exists so a half-dead socket cannot lock you out of your own sensor.
- **The device streams live audio and occupancy on request.** Treat `API_TOKEN`
  as being as sensitive as a microphone in your home, because that is what it
  unlocks.
- **Outbound pushes are validated.** An `https://` Gotify URL is verified
  against the pinned Let's Encrypt root in `src/certs.h`. An `http://` Gotify
  URL sends your Gotify application token in cleartext — only use one on a
  network you control.
- **mDNS announces the node on the LAN.** `watchdog.local` and its HTTP/WS
  services are broadcast unauthenticated, so anyone on the network learns the
  device exists and where it listens. The token still guards every endpoint;
  this only removes the obscurity of an unknown IP.
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
