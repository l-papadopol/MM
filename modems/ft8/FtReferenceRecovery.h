// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QVector>
#include <functional>
#include <memory>
class FtReferenceRecovery final {
public:
 struct Row {QString message, method; int frequency=0,snr=0;double dt=0,quality=0;};
 struct Request {QString mode,myCall,dxCall,phase;double radioHz=0;int lowHz=100,highHz=3000,markerHz=1500,workers=1,epoch=0; qint64 slotUtcMs=0,deadlineUtcMs=0;bool offline=false;std::function<bool()> cancelled;};
 struct Result {QVector<Row> rows;int workers=0,rejected=0;double elapsedMs=0;bool timedOut=false,cached=false;};
 FtReferenceRecovery();~FtReferenceRecovery();
 // Calls on one instance are serialized. onRow is serialized across pool workers
 // and runs before they join; callers must not acquire their coordinator lock.
 Result decode(const QVector<double>&,const Request&,const std::function<void(const Row&)>& onRow={});
private:struct State;std::unique_ptr<State> state;
};
