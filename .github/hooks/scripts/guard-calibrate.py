#!/usr/bin/env python3
"""PreToolUse guard: ask before starting a real radar calibration.

POST /calibrate (and the WebSocket `calibrate` command) kicks off a ~2 minute
background correction on the LD2412. It rewrites the radar's baseline and must
run with the room empty — if someone is present, their reflection is learned as
background and detection is degraded until it is redone. There is no undo.

An authentication probe should use an invalid token instead, which this hook
lets through untouched.

It only sees the command line, so a calibration buried inside a script it
invokes goes unnoticed. That is why the check-in rule lives in AGENTS.md too.

Fails open: a guard that breaks the agent on unexpected input is worse than the
risk it manages.
"""
import json
import re
import sys

ALLOW = {"hookSpecificOutput": {"hookEventName": "PreToolUse",
                                "permissionDecision": "allow"}}

# Values that only ever show up in an auth test, never in a real call.
TEST_TOKEN = re.compile(
    r"wrong|invalid|bogus|definitely-not|not-the-right|fake|dummy|placeholder",
    re.I,
)

CALIBRATE_HTTP = re.compile(r"/calibrate\b", re.I)
POST_METHOD = re.compile(r"-X\s*POST|--request\s+POST|\bmethod\s*=\s*['\"]POST", re.I)
CALIBRATE_WS = re.compile(r"""send\(\s*['"]calibrate['"]""", re.I)


def decision(reason):
    return {"hookSpecificOutput": {"hookEventName": "PreToolUse",
                                   "permissionDecision": "ask",
                                   "permissionDecisionReason": reason}}


def main():
    try:
        payload = json.load(sys.stdin)
    except Exception:
        return ALLOW

    # Matchers in hook configs are not always honoured, so self-filter.
    tool = str(payload.get("tool_name") or payload.get("toolName") or "")
    if "terminal" not in tool.lower():
        return ALLOW

    tool_input = payload.get("tool_input") or payload.get("toolInput") or {}
    if isinstance(tool_input, dict):
        command = str(tool_input.get("command") or "")
    else:
        command = str(tool_input)
    if not command:
        return ALLOW

    http_call = CALIBRATE_HTTP.search(command) and POST_METHOD.search(command)
    ws_call = CALIBRATE_WS.search(command)
    if not (http_call or ws_call):
        return ALLOW

    if TEST_TOKEN.search(command):
        return ALLOW  # an auth probe, which is the safe way to exercise this

    return decision(
        "This starts a real ~2 minute radar background calibration. It rewrites "
        "the LD2412 baseline and must run with the room empty — anyone present "
        "is learned as background and detection stays degraded until it is "
        "redone. There is no undo. To test authentication instead, use an "
        "invalid token."
    )


if __name__ == "__main__":
    json.dump(main(), sys.stdout)
