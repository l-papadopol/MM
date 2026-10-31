// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QFile>
#include <QByteArray>
#include <array>
#include <stdexcept>
#include <zlib.h>
// Independent decoder for tests: require gzip CRC/trailer unless reading a
// deliberately live/crashed stream after an explicit durability barrier.
inline QByteArray readGzipLog(const QString &path, bool allowIncomplete=false) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) throw std::runtime_error("open gzip test log");
    auto compressed=file.readAll();z_stream stream{};
    if(inflateInit2(&stream,15+16)!=Z_OK) throw std::runtime_error("inflate init");
    stream.next_in=reinterpret_cast<Bytef *>(compressed.data());stream.avail_in=uInt(compressed.size());
    std::array<char,16384> buffer;QByteArray plain;bool ended=false,valid=true;
    do {
        stream.next_out=reinterpret_cast<Bytef *>(buffer.data());stream.avail_out=buffer.size();
        const int rc=inflate(&stream,Z_NO_FLUSH);
        plain.append(buffer.data(),int(buffer.size()-stream.avail_out));
        if(rc==Z_STREAM_END){ended=true;valid=stream.avail_in==0;break;}
        if(rc!=Z_OK && !(rc==Z_BUF_ERROR && stream.avail_in==0)){valid=false;break;}
        if(stream.avail_in==0 && stream.avail_out>0) break;
    }while(true);
    inflateEnd(&stream);
    if(!valid || (!ended && !allowIncomplete)) throw std::runtime_error("gzip CRC/trailer or stream integrity failure");
    return plain;
}
