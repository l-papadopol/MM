#!/usr/bin/env python3
"""Guard the production wiring as well as the executable concurrency tests."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
main=(root/'mainwindow.cpp').read_text()
worker=(root/'runtime/RxDecoderWorker.cpp').read_text()
gate=(root/'runtime/AsyncCatCommand.h').read_text()
assert 'm_rxDecoderWorker->moveToThread(m_rxDecoderThread)' in main
assert 'm_rxDecoderWorker->queue(),&BoundedAudioDispatcher::enqueue,Qt::DirectConnection' in main
assert not re.search(r'm_(?:rtty|rttyMulti|cw|bpsk31|mfsk|hell|weatherFax|sstv|msk144)Decoder->',main), 'GUI must use queued commands or immutable snapshots'
assert 'm_continuity.accept(block)' in worker and 'if (dropped) { resetActive();' in worker
assert 'reversePolarityRequested,this' in worker, 'Automatic RTTY polarity must not wait for GUI'
assert 'invokeRigPttBlocking' not in main and 'invokeRigBeginFtSplitBlocking' not in main
cat=main[main.index('void MainWindow::requestRigPtt'):main.index('bool MainWindow::startAudioInputBlocking')]
assert 'BlockingQueuedConnection' not in cat
start=main[main.index('void MainWindow::startFtPreparedSlotTransmit'):main.index('void MainWindow::stopImageTx')]
assert 'BlockingQueuedConnection' not in start and 'stopAudioInputBlocking()' not in start
assert start.index('if(!m_pendingFt8PttPrearmed || !m_pendingFt8PttKeyed)') < start.index('"startScheduledOutput"')
assert 'if(ok && m_cancelled->load())' in gate and 'CatCommandJob::compensate' in gate
assert 'm_faulted=!recovered' in gate
print('PASS: independent RX queue, serialized decoder ownership, CAT acknowledgements and late-cancel compensation')
