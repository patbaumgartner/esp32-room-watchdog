#include "json_escape.h"

#include <stdio.h>

String jsonEscape(const String &text)
{
    String out;
    out.reserve(text.length());
    for (size_t i = 0; i < text.length(); ++i)
    {
        const char character = text[i];
        switch (character)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<uint8_t>(character) < 0x20)
            {
                char escape[7];
                snprintf(escape, sizeof(escape), "\\u%04x",
                         static_cast<unsigned>(static_cast<uint8_t>(character)));
                out += escape;
            }
            else
            {
                out += character;
            }
            break;
        }
    }
    return out;
}
