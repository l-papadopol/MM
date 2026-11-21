// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "utils/RuntimeI18n.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {
QString uiText(const char *key, const char *fallback)
{
    return MadModemI18n::translate(QString::fromUtf8(key), QString::fromUtf8(fallback));
}
}

void MainWindow::showFtDecodeFilters()
{
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("ftDecodeFilterDialog"));
    dialog.setWindowTitle(uiText("ft_rx_filters_title", "FT8/FT4 display filters"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    layout->addLayout(form);
    const std::array<QString, FtDecodeFilter::FieldCount> names{
        uiText("ft_rx_filters_continents", "Continents"),
        uiText("ft_rx_filters_countries", "Countries / DXCC"),
        uiText("ft_rx_filters_cq_zone", "CQ zones (WAZ)"),
        uiText("ft_rx_filters_itu_zone", "ITU zones"),
        uiText("ft_rx_filters_wap", "WAP references")
    };
    const std::array<QString, FtDecodeFilter::FieldCount> examples{
        QStringLiteral("AF, AN, AS, EU, NA, OC, SA"),
        QStringLiteral("I, DL, 291"),
        QStringLiteral("14, 15, 20-25"),
        QStringLiteral("28, 29, 37-39"),
        uiText("ft_rx_filters_wap_example", "* or WAP-ITA-01")
    };
    std::array<QComboBox *, FtDecodeFilter::FieldCount> modes{};
    std::array<QLineEdit *, FtDecodeFilter::FieldCount> values{};
    for (int i = 0; i < FtDecodeFilter::FieldCount; ++i) {
        auto *row = new QWidget(&dialog);
        auto *line = new QHBoxLayout(row);
        line->setContentsMargins(0, 0, 0, 0);
        modes[i] = new QComboBox(row);
        modes[i]->setObjectName(QStringLiteral("ftFilterMode%1").arg(i));
        modes[i]->addItems({uiText("ft_rx_filters_all", "All"),
                            uiText("ft_rx_filters_only", "Include"),
                            uiText("ft_rx_filters_exclude", "Exclude")});
        modes[i]->setCurrentIndex(m_ftDecodeFilter.rules[i].mode);
        values[i] = new QLineEdit(m_ftDecodeFilter.rules[i].input, row);
        values[i]->setObjectName(QStringLiteral("ftFilterValues%1").arg(i));
        values[i]->setPlaceholderText(examples[i]);
        values[i]->setEnabled(modes[i]->currentIndex() != FtDecodeFilter::All);
        connect(modes[i], qOverload<int>(&QComboBox::currentIndexChanged), values[i],
                [value = values[i]](int mode) { value->setEnabled(mode != FtDecodeFilter::All); });
        line->addWidget(modes[i]);
        line->addWidget(values[i], 1);
        form->addRow(names[i], row);
    }

    auto *traffic = new QComboBox(&dialog);
    traffic->setObjectName(QStringLiteral("ftFilterTraffic"));
    traffic->addItems({uiText("ft_rx_filters_traffic_all", "All messages"),
                       uiText("ft_rx_filters_cq_only", "CQ only"),
                       uiText("ft_rx_filters_hide_cq", "Hide CQ")});
    traffic->setCurrentIndex(m_ftDecodeFilter.traffic);
    form->addRow(uiText("ft_rx_filters_messages", "Messages"), traffic);

    auto *wap = new QPlainTextEdit(m_ftDecodeFilter.wapDirectory, &dialog);
    wap->setObjectName(QStringLiteral("ftFilterWapDirectory"));
    wap->setPlaceholderText(uiText("ft_rx_filters_wap_directory_example",
                                  "One callsign and WAP reference per line, separated by a space or semicolon."));
    wap->setMaximumHeight(100);
    wap->setEnabled(modes[FtDecodeFilter::Wap]->currentIndex() != FtDecodeFilter::All);
    connect(modes[FtDecodeFilter::Wap], qOverload<int>(&QComboBox::currentIndexChanged), wap,
            [wap](int mode) { wap->setEnabled(mode != FtDecodeFilter::All); });
    form->addRow(uiText("ft_rx_filters_wap_directory", "Local WAP list"), wap);

    auto *help = new QLabel(uiText("ft_rx_filters_help",
        "Filters apply together. Direct replies to your callsign stay visible. Hidden rows return when you clear the filters."), &dialog);
    help->setWordWrap(true);
    layout->addWidget(help);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, &dialog);
    buttons->button(QDialogButtonBox::Reset)->setText(uiText("ft_rx_filters_clear", "Clear filters"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked, &dialog, [&] {
        for (auto *mode : modes) mode->setCurrentIndex(FtDecodeFilter::All);
        traffic->setCurrentIndex(FtDecodeFilter::AnyTraffic);
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        FtDecodeFilter filter;
        for (int i = 0; i < FtDecodeFilter::FieldCount; ++i) {
            filter.rules[i].mode = FtDecodeFilter::Mode(modes[i]->currentIndex());
            filter.rules[i].input = values[i]->text();
        }
        filter.traffic = FtDecodeFilter::Traffic(traffic->currentIndex());
        filter.wapDirectory = wap->toPlainText();
        FtDecodeFilter::Field invalid = FtDecodeFilter::Continent;
        if (!filter.compile(&invalid)) {
            QMessageBox::warning(&dialog, dialog.windowTitle(),
                uiText("ft_rx_filters_invalid", "Check the values for %1.").arg(names[invalid]));
            values[invalid]->setFocus();
            return;
        }
        m_ftDecodeFilter = std::move(filter);
        QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
        m_ftDecodeFilter.save(settings);
        refreshFtLotwFilter();
        dialog.accept();
    });
    dialog.resize(530, dialog.sizeHint().height());
    dialog.exec();
}
