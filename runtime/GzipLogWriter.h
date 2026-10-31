// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QFile>
#include <QByteArray>
#include <array>
#include <zlib.h>

// Writer-thread only. QFile owns the Unicode path; zlib never opens a filename.
// Bound both staging buffers to 64 KiB instead of retaining a session in RAM.
class GzipLogWriter {
public:
    explicit GzipLogWriter(const QString &path) : m_file(path) {}
    ~GzipLogWriter() { if(m_initialized) deflateEnd(&m_stream); }
    GzipLogWriter(const GzipLogWriter &) = delete;
    GzipLogWriter &operator=(const GzipLogWriter &) = delete;
    bool open() {
        if(!m_file.open(QIODevice::WriteOnly|QIODevice::NewOnly)) return false;
        const int rc=deflateInit2(&m_stream,Z_BEST_SPEED,Z_DEFLATED,15+16,8,Z_DEFAULT_STRATEGY);
        if(rc!=Z_OK){m_error=QStringLiteral("Cannot initialize gzip compression (%1)").arg(rc);return false;}
        m_initialized=true;m_pending.reserve(BufferSize);return true;
    }
    bool write(const QByteArray &bytes) {
        if(!m_initialized || m_finished || !m_error.isEmpty()) return false;
        qint64 offset=0;
        while(offset<bytes.size()) {
            const int n=int(qMin<qint64>(BufferSize-m_pending.size(),bytes.size()-offset));
            m_pending.append(bytes.constData()+offset,n);offset+=n;m_dirty=true;
            if(m_pending.size()==BufferSize && !pump(Z_NO_FLUSH)) return false;
        }
        return true;
    }
    bool flush() {
        if(!m_initialized || !m_error.isEmpty()) return false;
        // Z_SYNC_FLUSH exposes all admitted JSONL bytes, even in a live file.
        if(m_dirty && !pump(Z_SYNC_FLUSH)) return false;
        m_dirty=false;return m_file.flush();
    }
    bool finish() {
        if(!m_initialized || !m_error.isEmpty()) return false;
        if(!m_finished && !pump(Z_FINISH)) return false;
        m_dirty=false;
        return m_file.flush();
    }
    QString errorString() const { return m_error.isEmpty()?m_file.errorString():m_error; }
private:
    bool pump(int mode) {
        m_stream.next_in=reinterpret_cast<Bytef *>(m_pending.data());
        m_stream.avail_in=uInt(m_pending.size());
        int rc;
        do {
            m_stream.next_out=m_output.data();m_stream.avail_out=BufferSize;
            rc=deflate(&m_stream,mode);
            if(rc!=Z_OK && rc!=Z_STREAM_END){m_error=QStringLiteral("Gzip compression failed (%1)").arg(rc);return false;}
            qint64 offset=0;const qint64 size=BufferSize-m_stream.avail_out;
            while(offset<size){const qint64 n=m_file.write(reinterpret_cast<const char *>(m_output.data())+offset,size-offset);if(n<=0){m_error=m_file.errorString();return false;}offset+=n;}
        } while(m_stream.avail_in || m_stream.avail_out==0 || (mode==Z_FINISH && rc!=Z_STREAM_END));
        m_pending.clear();m_finished=rc==Z_STREAM_END;return true;
    }
    static constexpr int BufferSize=64*1024;
    QFile m_file;
    z_stream m_stream{};
    std::array<Bytef,BufferSize> m_output;
    QByteArray m_pending;
    bool m_initialized=false,m_finished=false,m_dirty=false;
    QString m_error;
};
