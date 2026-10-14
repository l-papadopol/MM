#pragma once

#include <QByteArray>
#include <QString>

// TS-890S PC command reference, pp. 24, 27, 60-61. Read commands only.
// A poll executes at most one extra transaction, leaving PTT/Stop an event-loop
// turn between queries. Width is published only after a complete valid cycle.
class Ts890FilterQuery
{
public:
    void reset() { m_stage = 0; m_width = 0; m_low = 0; m_shiftWidth = false; }
    int widthHz() const { return m_width; }
    QByteArray command(const QString &mode) const {
        if (isSsb(mode)) {
            if (m_stage == 0) return mode.startsWith("PKT") ? "EX00612;" : "EX00611;";
            if (m_stage == 2) return "SH0;";
        }
        return supported(mode) ? QByteArray("SL0;") : QByteArray();
    }
    static bool supported(const QString &mode) {
        return isSsb(mode) || mode == "CW" || mode == "CWR" ||
               mode == "RTTY" || mode == "RTTYR" || mode == "PSK" || mode == "PSKR";
    }
    // false = malformed/unsupported response; caller must invalidate, not reuse
    // a width from a previous mode or an earlier successful connection.
    bool accept(const QString &mode, const QByteArray &response, bool &complete) {
        complete = false;
        const QByteArray cmd = command(mode);
        if (cmd.isEmpty()) { reset(); return false; }
        const QByteArray prefix = cmd.left(cmd.size() - 1);
        if (!response.startsWith(prefix) || !response.endsWith(';')) { reset(); return false; }
        QByteArray value = response.mid(prefix.size(), response.size() - prefix.size() - 1);
        if (prefix.startsWith("EX")) {
            if (value.size() != 4 || value[0] != ' ') { reset(); return false; }
            value.remove(0, 1);
        } else if (value.size() != (prefix == "SL0" ? 2 : 3)) { reset(); return false; }
        for (char c : value) if (c < '0' || c > '9') { reset(); return false; }
        const int id = value.toInt();
        if (isSsb(mode) && m_stage == 0) {
            if (id > 1) { reset(); return false; }
            m_shiftWidth = id == 1; m_stage = 1; return true;
        }
        int width = 0;
        if (isSsb(mode) && !m_shiftWidth) {
            if (m_stage == 1) {
                if (id > 21) { reset(); return false; }
                m_low = id == 0 ? 0 : id == 1 ? 50 : (id - 1) * 100;
                m_stage = 2; return true;
            }
            // 600..3000 in 100-Hz steps, then 3400/4000/5000 Hz.
            const int high = id <= 24 ? 600 + 100 * id : id == 25 ? 3400 : id == 26 ? 4000 : id == 27 ? 5000 : 0;
            width = high - m_low;
            if (high == 0) width = 0;
        } else {
            static const int common[] = {50,80,100,150,200,250,300,350,400,450,500,600,700,800,900,1000};
            static const int fsk[] = {250,300,350,400,450,500,1000,1500};
            static const int cw[] = {1500,2000,2500};
            static const int psk[] = {1200,1400,1500,1600,1800,2000,2200,2400,2600,2800,3000};
            if (mode.startsWith("RTTY")) { if (id < 8) width = fsk[id]; }
            else if (id < 16) width = common[id];
            else if (isSsb(mode) && id <= 35) width = 1100 + (id - 16) * 100;
            else if (mode.startsWith("CW") && id <= 18) width = cw[id - 16];
            else if (mode.startsWith("PSK") && id <= 26) width = psk[id - 16];
        }
        if (width <= 0 || width > 5000) { reset(); return false; }
        m_width = width; m_stage = 0; complete = true; return true;
    }
private:
    static bool isSsb(const QString &m) { return m == "USB" || m == "LSB" || m == "PKTUSB" || m == "PKTLSB"; }
    int m_stage = 0, m_width = 0, m_low = 0;
    bool m_shiftWidth = false;
};
