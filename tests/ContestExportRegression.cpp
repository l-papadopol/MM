#include "dialogs/ContestExportWizard.h"
#include "utils/CockpitTheme.h"
#include "utils/RuntimeI18n.h"
#include <QApplication>
#include <QAbstractButton>
#include <QComboBox>
#include <QTableWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QTemporaryDir>
#include <QImage>
#include <iostream>
#include <stdexcept>

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
    std::cout << "PASS " << message << '\n';
}
LogbookEntry qso(int year, int minute=5) {
    LogbookEntry e;
    e.callsign="K6ABC"; e.stationCallsign="IZ6NNH"; e.mode="RTTY"; e.band="20m";
    e.freq="14.085"; e.rstSent=e.rstReceived="599";
    e.utc=QDateTime(CqWwRtty::startDate(year),QTime(12,minute),Qt::UTC);
    e.adifFields={{"CONTEST_ID","CQ-WW-RTTY"},{"MY_CQ_ZONE","15"},{"CQZ","03"},{"STATE","CA"}};
    return e;
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        auto e=qso(2026), restart=qso(2026,6), old=qso(2025);
        e.adifFields["APP_MADMODEM_RTTY_SESSION"]="a";
        restart.adifFields["APP_MADMODEM_RTTY_SESSION"]="b";
        auto inferred=qso(2026,7); inferred.adifFields.remove("CONTEST_ID");
        auto other=qso(2026); other.adifFields["CONTEST_ID"]="OTHER-RTTY";
        auto outside=qso(2026); outside.utc=outside.utc.addDays(2);
        auto badDate=qso(2026); badDate.utc={};
        auto ft=inferred; ft.mode="FT8";
        auto noExchange=inferred; noExchange.adifFields.clear();
        auto station=e; station.stationCallsign="IK1ABC";
        const QVector<LogbookEntry> input{old,restart,outside,e,inferred,other,badDate,ft,noExchange,station};
        const auto catalog=ContestExport::discover(input);
        check(catalog.editions.size()==4,"only observed contest editions/stations listed");
        check(catalog.outsidePeriod==1 && catalog.invalidDates==1,"bad UTC and out-of-period rows counted");
        const ContestExport::Edition* selected=nullptr;
        for(const auto& edition:catalog.editions)
            if(edition.contestId=="CQ-WW-RTTY" && edition.station=="IZ6NNH" && edition.firstDate.year()==2026) selected=&edition;
        check(selected && selected->records.size()==3,"restarts combined; other years, modes, contests and stations excluded");
        check(selected->inferred.count(true)==1,"untagged exchange/date inference visible");
        check(CqWwRtty::startDate(2028)==QDate(2028,9,23),"last full September weekend");
        auto edge=e; edge.utc=QDateTime(QDate(2026,9,27),QTime(23,59,59),Qt::UTC);
        check(CqWwRtty::inPeriod(edge.utc) && !CqWwRtty::inPeriod(edge.utc.addSecs(1)),"UTC end boundary exclusive");
        QTemporaryDir profilesDir;
        QFile profileFile(profilesDir.filePath("rules.json"));
        check(profileFile.open(QIODevice::WriteOnly), "profile fixture writable");
        profileFile.write(R"({"profiles":[{"id":"arrl_rtty_roundup","name":"ARRL RTTY Roundup","cabrillo_id":"ARRL-RTTY"}]})");
        profileFile.close();
        auto profileRecord=e;
        profileRecord.adifFields.remove("CONTEST_ID");
        profileRecord.adifFields["APP_MADMODEM_RTTY_RULE"]="arrl_rtty_roundup";
        const auto named=ContestExport::discover({profileRecord},ContestExport::loadProfiles({profileFile.fileName()}));
        check(named.editions.size()==1 && named.editions[0].name=="ARRL RTTY Roundup", "installed profile names and legacy IDs resolved");
        QString error;
        check(!CqWwRtty::cabrillo({ContestExport::prepared(inferred,"CQ-WW-RTTY")},{},&error).isEmpty(),
            "inferred contact exports without modifying its missing source contest tag");
        check(!CqWwRtty::cabrillo({e,restart},{},&error).isEmpty(),"multiple sessions export with standard ADIF fields");
        check(CqWwRtty::cabrillo({e,old},{},&error).isEmpty(),"mixed editions rejected by writer");
        check(CqWwRtty::cabrillo({e,station},{},&error).isEmpty(),"mixed station callsigns rejected by writer");
        auto exchange=e; exchange.adifFields={{"CONTEST_ID","CQ-WW-RTTY"},{"STX_STRING","599 15"},{"SRX_STRING","599 03 CA"}};
        check(CqWwRtty::cabrillo({exchange},{},&error).contains("K6ABC 599 03 CA"),"recorded exchange strings recovered without invented zones");
        exchange.adifFields["SRX_STRING"]="599 1234";
        check(CqWwRtty::cabrillo({exchange},{},&error).isEmpty(),"serial number is not guessed as a CQ zone");
        auto rule=e; rule.adifFields.remove("CONTEST_ID"); rule.adifFields["APP_MADMODEM_RTTY_RULE"]="cq_ww_rtty";
        check(ContestExport::discover({rule}).editions.size()==1,"legacy rule-only record recognized");
        auto generic=other; generic.utc=generic.utc.addDays(7);
        check(ContestExport::discover({other,generic}).editions.size()==2,"separate runs of a generic contest in one year");
        for(const auto& theme: {QStringLiteral("classic_dark"),QStringLiteral("qt_default"),QStringLiteral("avionica")}) {
            MadModemUi::applyUiTheme(app,theme);
            MadModemI18n::setLanguageCode("it");
            auto invalid=qso(2026,8); invalid.freq.clear();
            ContestExportWizard wizard({e,restart,invalid,old});
            wizard.show(); app.processEvents();
            auto* editions=wizard.findChild<QComboBox*>("contestEdition");
            check(editions && editions->count()==2,"wizard offers populated years only");
            check(editions->itemText(0).contains("2026-09-26") && editions->itemText(1).contains("2025-09-27"),"edition names include UTC dates");
            check(wizard.wizardStyle()==QWizard::ClassicStyle,"native wizard background disabled");
            const auto image=wizard.currentPage()->grab().toImage();
            const QColor background=image.pixelColor(image.width()/2,image.height()-5);
            check(background==wizard.palette().color(QPalette::Window),"page background matches active theme");
            wizard.next(); wizard.next(); app.processEvents();
            auto* table=wizard.findChild<QTableWidget*>("contestQsoPreview");
            check(table && table->rowCount()==3,"preview lists the entire chosen edition");
            check(table->horizontalHeaderItem(0)->text()==QString::fromUtf8("Esporta") &&
                  table->horizontalHeaderItem(3)->text()==QString::fromUtf8("Frequenza / modo"),
                  "translated headers retain their meaning beside literal UTC/MHz columns");
            check(!wizard.button(QWizard::FinishButton)->isEnabled() && wizard.generatedData().isEmpty(),"invalid row prevents saving");
            table->item(2,0)->setCheckState(Qt::Unchecked); app.processEvents();
            const auto bytes=wizard.generatedData();
            check(wizard.button(QWizard::FinishButton)->isEnabled() && bytes.count("QSO:")==2,"explicit exclusion updates export and enables finish");
            check(bytes.contains("2026-09-26") && !bytes.contains("2025-09-27"),"preview and generated edition agree");
            table->item(0,0)->setCheckState(Qt::Unchecked); table->item(1,0)->setCheckState(Qt::Unchecked);
            check(wizard.generatedData().isEmpty() && !wizard.button(QWizard::FinishButton)->isEnabled(),"empty selection cannot export");
            wizard.back(); wizard.back(); editions->setCurrentIndex(1); wizard.next(); wizard.next();
            check(table->rowCount()==1 && wizard.generatedData().count("QSO:")==1 && wizard.generatedData().contains("2025-09-27"),"back navigation regenerates exact selected year");
            if(qEnvironmentVariableIsSet("MM_EXPORT_SCREENSHOTS")) wizard.grab().save("cabrillo-"+theme+".png");
        }
        ContestExportWizard empty({ft,noExchange}); empty.show(); app.processEvents();
        check(!empty.button(QWizard::NextButton)->isEnabled(),"no contest means no empty wizard progression");
        auto missingStation=e; missingStation.stationCallsign.clear();
        ContestExportWizard stationWizard({missingStation}); stationWizard.show(); app.processEvents();
        stationWizard.next();
        stationWizard.findChild<QLineEdit*>("exportStation")->setText("IZ6NNH");
        stationWizard.next();
        check(stationWizard.generatedData().contains("CALLSIGN: IZ6NNH") &&
              stationWizard.currentPage()->subTitle().contains("IZ6NNH"),
              "missing station can be explicitly supplied and is visible in preview");
        auto wpx=e; wpx.adifFields={{"CONTEST_ID","CQ-WPX-RTTY"},{"STX","001"},{"SRX","002"}};
        wpx.utc=QDateTime(QDate(2026,2,14),QTime(12,0),Qt::UTC);
        ContestExportWizard wpxWizard({wpx}); wpxWizard.show(); app.processEvents();
        wpxWizard.next(); wpxWizard.next(); app.processEvents();
        auto* automatic=wpxWizard.findChild<QCheckBox*>("automaticContestExchange");
        auto* wpxTable=wpxWizard.findChild<QTableWidget*>("contestQsoPreview");
        check(automatic && automatic->isChecked() && wpxWizard.button(QWizard::FinishButton)->isEnabled(),
              "non-CQ contest wizard exports recorded standard fields automatically");
        check(wpxTable->item(0,4)->text()=="599 001" && wpxTable->item(0,5)->text()=="599 002" &&
              wpxWizard.generatedData().contains("CONTEST: CQ-WPX-RTTY"),
              "non-CQ preview shows the exchanges used by Cabrillo");
        check(input[0].adifFields==old.adifFields,"export never edits source logbook");
    } catch(const std::exception& ex) { std::cerr<<"FAIL "<<ex.what()<<'\n'; return 1; }
    return 0;
}
