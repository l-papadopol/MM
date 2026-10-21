// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/FtCqWorkList.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>
static void check(bool b,const char *why){if(!b)throw std::runtime_error(why);}
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);
 for(int period:{15000,7500}){
  FtCqWorkList list;FtCqWorkList::Entry e;e.call="G0JEI";e.grid="IO93";e.mode=period==15000?"FT8":"FT4";e.band="20m";e.decode.slotStartUtcMs=period*100LL;e.decode.slotPeriodMs=period;e.decode.message="CQ G0JEI IO93";e.confirmed=true;
  check(list.add(e),"first add");check(!list.add(e),"duplicate target accepted");check(!list.running,"adding target started automation");
  const qint64 now=period*101LL+1000;list.beginCheck(now,period);
  check(list.availability(list.entries[0],now,period)==FtCqWorkList::Availability::Wait,"old CQ authorized a call after next-QSO check");
  auto fresh=e.decode;fresh.slotStartUtcMs=period*101LL;list.observe(e.call,true,fresh,e.mode,e.band);
  check(list.availability(list.entries[0],now,period)==FtCqWorkList::Availability::Ready,"fresh CQ not ready");
  list.requireFreshCheck();list.beginCheck(now,period);
  check(list.availability(list.entries[0],now,period)==FtCqWorkList::Availability::Wait,"CQ heard during previous QSO reused without a new observation");
  list.observe(e.call,true,fresh,e.mode,e.band);
  check(list.availability(list.entries[0],now,period)==FtCqWorkList::Availability::Ready,"new same-slot CQ observation not accepted");
  fresh.message="DL2LAU G0JEI -10";list.observe(e.call,false,fresh,e.mode,e.band);
  check(list.availability(list.entries[0],now,period)==FtCqWorkList::Availability::Busy,"directed traffic did not invalidate CQ");
  list.observe(e.call,true,fresh,e.mode,e.band);check(!list.entries[0].cq,"same-slot duplicate CQ undid busy evidence");
  auto older=fresh;older.slotStartUtcMs-=period;list.observe(e.call,true,older,e.mode,e.band);check(!list.entries[0].cq,"older publication undid newer traffic");
  fresh.slotStartUtcMs+=period*2;list.observe(e.call,true,fresh,e.mode,"40m");check(!list.entries[0].cq,"different-band observation used");
  list.observe(e.call,true,fresh,e.mode,e.band);check(list.entries[0].cq,"newer CQ not accepted");
  check(list.availability(list.entries[0],now,period)==FtCqWorkList::Availability::Wait,"future timestamp authorized TX");
  list.requireFreshCheck();list.beginCheck(now+period*4,period);
  check(list.availability(list.entries[0],now+period*4,period)==FtCqWorkList::Availability::Wait,"freshness not reset after QSO");
  check(list.availability(list.entries[0],now+period*6+2500,period)==FtCqWorkList::Availability::Absent,"unheard target did not expire after a cycle");
  list.running=true;list.clear();check(!list.running&&list.entries.isEmpty(),"clear left automation armed");
  std::cout<<e.mode.toStdString()<<" PASS duplicate/explicit-start/fresh-CQ/busy/old/future/band/reset/expiry/clear\n";
 }
}
