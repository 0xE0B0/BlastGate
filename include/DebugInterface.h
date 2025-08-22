#pragma once

#include <Arduino.h>
#include <PrintStream.h>

// toggles output of time stamp for each debug output line
static constexpr bool timeStampEnabled = true;

// if enabled ANSI color escape sequences are included into the output
// note: requires a terminal which suports ANSI escape sequences
static constexpr bool useAnsiColor = true;

// debug output baud rate
static constexpr uint32_t debugBaudRate = 115200;

enum class Color {
    Default,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White
};

class EscapeSequence {
public:

    static EscapeSequence reset() { return EscapeSequence(RESET); }
    static EscapeSequence clearLine() { return EscapeSequence(CLEAR_LINE); }

    EscapeSequence(const char* seq) : sequence(seq) {}
    const char* sequence;

    static const char* get(Color color) {
        switch (color) {
            case Color::Red:     return RED;
            case Color::Green:   return GREEN;
            case Color::Yellow:  return YELLOW;
            case Color::Blue:    return BLUE;
            case Color::Magenta: return MAGENTA;
            case Color::Cyan:    return CYAN;
            case Color::White:   return WHITE;
            default:             return RESET;
        }
    }

private:
    static constexpr const char* RED = "\033[1;31m";
    static constexpr const char* GREEN = "\033[1;32m";
    static constexpr const char* YELLOW = "\033[1;33m";
    static constexpr const char* BLUE = "\033[1;34m";
    static constexpr const char* MAGENTA = "\033[1;35m";
    static constexpr const char* CYAN = "\033[1;36m";
    static constexpr const char* WHITE = "\033[1;37m";
    static constexpr const char* RESET = "\033[0m";
    static constexpr const char* CLEAR_LINE = "\033[F\033[K";

};

inline Print &operator<<(Print &stream, const EscapeSequence &sequ) {
    stream << sequ.sequence;
    return stream;
}

inline Print &operator<<(Print &stream, const Color &color) {
    if constexpr (useAnsiColor)
        return stream << EscapeSequence::get(color);
    else return stream;
}

inline Print& red(Print &stream) {
    stream << Color::Red;
    return stream;
}

inline Print& green(Print &stream) {
    stream << Color::Green;
    return stream;
}

inline Print& yellow(Print &stream) {
    stream << Color::Yellow;
    return stream;
}
inline Print& blue(Print &stream) {
    stream << Color::Blue;
    return stream;
}

inline Print& magenta(Print &stream) {
    stream << Color::Magenta;
    return stream;
}

inline Print& cyan(Print &stream) {
    stream << Color::Cyan;
    return stream;
}

inline Print& white(Print &stream) {
    stream << Color::White;
    return stream;
}

inline Print& resetColor(Print &stream) {
    stream << Color::Default;
    return stream;
}

inline Print& clearLine(Print &stream) {
    // move cursor up one line and clear the line
    stream << EscapeSequence::clearLine();
    return stream;
}

template <const char* moduleName>
inline Print& beginl(Print &stream) {
    if constexpr (timeStampEnabled)
        stream << "[" << millis() << "] ";
    stream << moduleName << ": ";
    return stream;
}

// specify the module name in each module, like:
// static inline Print& beginl(Print &stream) {
//     static constexpr const char name[] = "MAIN";
//     return beginl<name>(stream);
// }

struct DI {
    static inline Print& endl(Print &stream) {
        if constexpr(useAnsiColor)
            stream << EscapeSequence::reset();
        stream << "\n";
        return stream;
    }
};
