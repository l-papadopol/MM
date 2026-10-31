#include "CwMorseSequence.h"
#include <QHash>
namespace {
QHash<QChar, QString> morseEncodeTable()
{
    return {
        {'A', ".-"}, {'B', "-..."}, {'C', "-.-."}, {'D', "-.."}, {'E', "."},
        {'F', "..-."}, {'G', "--."}, {'H', "...."}, {'I', ".."}, {'J', ".---"},
        {'K', "-.-"}, {'L', ".-.."}, {'M', "--"}, {'N', "-."}, {'O', "---"},
        {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."}, {'S', "..."}, {'T', "-"},
        {'U', "..-"}, {'V', "...-"}, {'W', ".--"}, {'X', "-..-"}, {'Y', "-.--"},
        {'Z', "--.."},
        {'1', ".----"}, {'2', "..---"}, {'3', "...--"}, {'4', "....-"}, {'5', "....."},
        {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."}, {'0', "-----"},
        {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {'/', "-..-."},
        {'=', "-...-"}, {'+', ".-.-."}, {'-', "-....-"}, {'(', "-.--."},
        {')', "-.--.-"}, {'@', ".--.-."}, {'!', "-.-.--"}, {'&', ".-..."},
        {':', "---..."}, {';', "-.-.-."}, {'\'', ".----."}, {'\"', ".-..-."}
    };
}
}
QVector<CwMorseSegment> cwMorseSequence(const QString& text)
{
    static const auto table = morseEncodeTable();
    QVector<CwMorseSegment> result;
    bool previous = false, space = false;
    for (int source = 0; source < text.size(); ++source) {
        for (QChar ch : text.mid(source, 1).toUpper()) {
            if (ch.isSpace()) { if (previous) space = true; continue; }
            const QString code = table.value(ch);
            if (code.isEmpty()) continue;
            if (previous) result.append({false, space ? 7.0 : 3.0, source});
            for (int i = 0; i < code.size(); ++i) {
                result.append({true, code.at(i) == '-' ? 3.0 : 1.0, i == 0 ? source : -1});
                if (i + 1 < code.size()) result.append({false, 1.0, -1});
            }
            previous = true; space = false;
        }
    }
    return result;
}
