#include "HelpDialog.h"
#include "../utils/RuntimeI18n.h"
#include "../MadModemVersion.h"
#include "../settings/AppSettings.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWhatsThis>

#ifdef MADMODEM_WITH_QT_HELP
#include <QHelpContentWidget>
#include <QHelpEngine>
#include <QHelpIndexWidget>
#include <QTabWidget>
#endif

namespace {

constexpr const char *kHelpNamespacePrefix = "org.madmodem.mm.0578";

struct FallbackTopic
{
    const char *key;
    const char *fileName;
};

const FallbackTopic kFallbackTopics[] = {
    {"index", "index.html"},
    {"quickstart", "quickstart.html"},
    {"audio_cat_ptt", "audio_cat_ptt.html"},
    {"waterfall", "waterfall.html"},
    {"text_modes", "text_modes.html"},
    {"cw", "cw.html"},
    {"ft8_ft4", "ft8_ft4.html"},
    {"rotator", "rotator.html"},
    {"radio_telescope", "radio_telescope.html"},
    {"scheduler", "scheduler.html"},
    {"logbook_map", "logbook_map.html"},
    {"troubleshooting", "troubleshooting.html"},
};

class HelpBrowser final : public QTextBrowser
{
public:
#ifdef MADMODEM_WITH_QT_HELP
    explicit HelpBrowser(QHelpEngine *engine, QWidget *parent = nullptr)
        : QTextBrowser(parent)
        , m_engine(engine)
    {
        setOpenExternalLinks(false);
        setOpenLinks(false);
    }
#else
    explicit HelpBrowser(QWidget *parent = nullptr)
        : QTextBrowser(parent)
    {
        setOpenExternalLinks(false);
        setOpenLinks(false);
    }
#endif

protected:
    QVariant loadResource(int type, const QUrl &name) override
    {
#ifdef MADMODEM_WITH_QT_HELP
        if (m_engine != nullptr && name.scheme() == QLatin1String("qthelp")) {
            const QByteArray data = m_engine->fileData(name);
            if (!data.isEmpty()) {
                return data;
            }
        }
#endif
        return QTextBrowser::loadResource(type, name);
    }

private:
#ifdef MADMODEM_WITH_QT_HELP
    QHelpEngine *m_engine = nullptr;
#endif
};

QString collectionFilePath(const QString &lang)
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.trimmed().isEmpty()) {
        base = QDir::tempPath() + QStringLiteral("/MadModem");
    }
    QDir().mkpath(base);
    return QDir(base).filePath(QStringLiteral("MM_%1.qhc").arg(lang));
}

} // namespace

HelpDialog::HelpDialog(QWidget *parent, const QUrl &startUrl)
    : QDialog(parent)
{
    m_languageCode = currentUiLanguage();
    m_effectiveHelpLanguage = m_languageCode;
    setupUiShell();
    if (!setupQtHelpBackend(startUrl)) {
        setupFallbackBackend(startUrl);
    }
}

HelpDialog::~HelpDialog() = default;

void HelpDialog::setupUiShell()
{
    setWindowTitle(MadModemI18n::text(QStringLiteral("MM Help")));
    resize(1080, 720);
    setMinimumSize(820, 520);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    QHBoxLayout *toolbar = new QHBoxLayout();
    toolbar->setSpacing(6);

    m_homeButton = new QPushButton(MadModemI18n::text(QStringLiteral("Home")), this);
    m_backButton = new QPushButton(MadModemI18n::text(QStringLiteral("Back")), this);
    m_forwardButton = new QPushButton(MadModemI18n::text(QStringLiteral("Forward")), this);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(MadModemI18n::text(QStringLiteral("Filter contents / index...")));
    m_whatsThisButton = new QPushButton(MadModemI18n::text(QStringLiteral("What's This?")), this);
    QPushButton *closeButton = new QPushButton(MadModemI18n::text(QStringLiteral("Close")), this);

    toolbar->addWidget(m_homeButton);
    toolbar->addWidget(m_backButton);
    toolbar->addWidget(m_forwardButton);
    toolbar->addSpacing(10);
    toolbar->addWidget(new QLabel(MadModemI18n::text(QStringLiteral("Search:")), this));
    toolbar->addWidget(m_searchEdit, 1);
    toolbar->addWidget(m_whatsThisButton);
    toolbar->addWidget(closeButton);
    outer->addLayout(toolbar);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    outer->addWidget(m_splitter, 1);

    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_whatsThisButton, &QPushButton::clicked, this, [this]() {
        accept();
        QWhatsThis::enterWhatsThisMode();
    });
}

bool HelpDialog::setupQtHelpBackend(const QUrl &startUrl)
{
#ifndef MADMODEM_WITH_QT_HELP
    Q_UNUSED(startUrl);
    return false;
#else
    QString qchPath;
    QString qchLang;
    for (const QString &lang : languageFallbackChain()) {
        qchPath = locateHelpFile(QStringLiteral("MM_%1.qch").arg(lang));
        if (!qchPath.isEmpty()) {
            qchLang = lang;
            break;
        }
    }
    if (qchPath.isEmpty()) {
        return false;
    }
    m_effectiveHelpLanguage = qchLang;

    m_helpEngine = new QHelpEngine(collectionFilePath(qchLang), this);
    if (!m_helpEngine->setupData()) {
        delete m_helpEngine;
        m_helpEngine = nullptr;
        return false;
    }

    const QString ns = helpNamespaceForLanguage(qchLang);
    const QStringList registered = m_helpEngine->registeredDocumentations();
    if (!registered.contains(ns)) {
        const bool ok = m_helpEngine->registerDocumentation(qchPath);
        if (!ok) {
            delete m_helpEngine;
            m_helpEngine = nullptr;
            return false;
        }
        m_helpEngine->setupData();
    }

    m_leftTabs = new QTabWidget(m_splitter);
    m_contentWidget = m_helpEngine->contentWidget();
    m_indexWidget = m_helpEngine->indexWidget();
    if (m_contentWidget != nullptr) {
        m_leftTabs->addTab(m_contentWidget, MadModemI18n::text(QStringLiteral("Contents")));
    }
    if (m_indexWidget != nullptr) {
        m_leftTabs->addTab(m_indexWidget, MadModemI18n::text(QStringLiteral("Index")));
    }

    m_viewer = new HelpBrowser(m_helpEngine, m_splitter);
    m_viewer->setSearchPaths(QStringList()
                             << QStringLiteral(":/help/%1").arg(qchLang)
                             << QStringLiteral(":/help"));

    m_splitter->addWidget(m_leftTabs);
    m_splitter->addWidget(m_viewer);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes(QList<int>() << 300 << 780);

    if (m_contentWidget != nullptr) {
        connect(m_contentWidget, &QHelpContentWidget::linkActivated,
                this, [this](const QUrl &url) { openUrl(url); });
    }
    if (m_indexWidget != nullptr) {
        connect(m_indexWidget, &QHelpIndexWidget::linkActivated,
                this, [this](const QUrl &url, const QString &) { openUrl(url); });
    }

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (m_indexWidget != nullptr) {
            m_indexWidget->filterIndices(text, QStringLiteral(""));
            if (m_leftTabs != nullptr) {
                m_leftTabs->setCurrentWidget(m_indexWidget);
            }
        }
    });
    connect(m_homeButton, &QPushButton::clicked, this, [this]() { openUrl(qthelpRootUrl(m_effectiveHelpLanguage)); });
    connect(m_backButton, &QPushButton::clicked, m_viewer, &QTextBrowser::backward);
    connect(m_forwardButton, &QPushButton::clicked, m_viewer, &QTextBrowser::forward);

    const QUrl initial = startUrl.isValid() ? startUrl : qthelpRootUrl(qchLang);
    openUrl(initial);
    return true;
#endif
}

void HelpDialog::setupFallbackBackend(const QUrl &startUrl)
{
    QString htmlLang;
    for (const QString &lang : languageFallbackChain()) {
        if (QFileInfo::exists(fallbackResourceForLanguage(lang, QStringLiteral("index.html")))) {
            htmlLang = lang;
            break;
        }
    }
    if (htmlLang.isEmpty()) {
        htmlLang = QStringLiteral("en");
    }
    m_effectiveHelpLanguage = htmlLang;

    m_fallbackTree = new QTreeWidget(m_splitter);
    m_fallbackTree->setHeaderLabel(MadModemI18n::text(QStringLiteral("Contents")));
    m_fallbackTree->setRootIsDecorated(false);
    m_fallbackTree->setUniformRowHeights(true);

#ifdef MADMODEM_WITH_QT_HELP
    m_viewer = new HelpBrowser(nullptr, m_splitter);
#else
    m_viewer = new HelpBrowser(m_splitter);
#endif
    m_viewer->setSearchPaths(QStringList()
                             << QStringLiteral(":/help/%1").arg(htmlLang)
                             << QStringLiteral(":/help"));
    m_viewer->setOpenExternalLinks(false);
    m_viewer->setOpenLinks(false);

    for (const FallbackTopic &topic : kFallbackTopics) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_fallbackTree, QStringList() << localizedTopicTitle(QString::fromLatin1(topic.key)));
        item->setData(0, Qt::UserRole, fallbackResourceForLanguage(htmlLang, QString::fromLatin1(topic.fileName)));
    }

    m_splitter->addWidget(m_fallbackTree);
    m_splitter->addWidget(m_viewer);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes(QList<int>() << 300 << 780);

    connect(m_fallbackTree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
                if (current == nullptr) {
                    return;
                }
                showFallbackTopic(current->data(0, Qt::UserRole).toString());
            });
    connect(m_viewer, &QTextBrowser::anchorClicked,
            this, [this](const QUrl &url) { openUrl(url); });
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &HelpDialog::filterFallbackTopics);
    connect(m_homeButton, &QPushButton::clicked, this, [this]() { showFallbackTopic(fallbackResourceForLanguage(m_effectiveHelpLanguage, QStringLiteral("index.html"))); });
    connect(m_backButton, &QPushButton::clicked, m_viewer, &QTextBrowser::backward);
    connect(m_forwardButton, &QPushButton::clicked, m_viewer, &QTextBrowser::forward);

    if (startUrl.isValid() && startUrl.scheme() == QLatin1String("qrc")) {
        showFallbackTopic(QStringLiteral(":") + startUrl.path());
    } else {
        showFallbackTopic(fallbackResourceForLanguage(htmlLang, QStringLiteral("index.html")));
    }
}

void HelpDialog::openUrl(const QUrl &url)
{
    if (!url.isValid()) {
        return;
    }

#ifdef MADMODEM_WITH_QT_HELP
    if (m_helpEngine != nullptr && url.scheme() == QLatin1String("qthelp")) {
        m_viewer->setSource(url);
        return;
    }
#endif

    if (url.scheme() == QLatin1String("qrc")) {
        showFallbackTopic(QStringLiteral(":") + url.path());
        return;
    }
    if (url.isRelative()) {
        const QUrl base = m_viewer != nullptr ? m_viewer->source() : QUrl(QStringLiteral("qrc") + fallbackResourceForLanguage(m_effectiveHelpLanguage, QStringLiteral("index.html")));
        openUrl(base.resolved(url));
        return;
    }
    QDesktopServices::openUrl(url);
}

void HelpDialog::showFallbackTopic(const QString &resourcePath)
{
    QString path = resourcePath;
    if (path.startsWith(QStringLiteral("qrc:"))) {
        path = QStringLiteral(":") + QUrl(path).path();
    }
    if (!path.startsWith(QStringLiteral(":"))) {
        path = fallbackResourceForLanguage(m_effectiveHelpLanguage, path);
    }
    if (!QFileInfo::exists(path)) {
        path = fallbackResourceForLanguage(QStringLiteral("en"), QFileInfo(path).fileName());
    }
    if (!QFileInfo::exists(path)) {
        path = fallbackResourceForLanguage(QStringLiteral("it"), QFileInfo(path).fileName());
    }
    m_viewer->setSource(QUrl(QStringLiteral("qrc") + path));

    if (m_fallbackTree != nullptr) {
        for (int i = 0; i < m_fallbackTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = m_fallbackTree->topLevelItem(i);
            if (item != nullptr && item->data(0, Qt::UserRole).toString() == path) {
                m_fallbackTree->setCurrentItem(item);
                break;
            }
        }
    }
}

void HelpDialog::filterFallbackTopics(const QString &text)
{
    if (m_fallbackTree == nullptr) {
        return;
    }
    const QString needle = text.trimmed();
    for (int i = 0; i < m_fallbackTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_fallbackTree->topLevelItem(i);
        if (item == nullptr) {
            continue;
        }
        const bool visible = needle.isEmpty() || item->text(0).contains(needle, Qt::CaseInsensitive);
        item->setHidden(!visible);
    }
}

QString HelpDialog::locateHelpFile(const QString &fileName) const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("help/") + fileName),
        QDir(appDir).filePath(QStringLiteral("docs/help/") + fileName),
        QDir(appDir).filePath(fileName),
        QDir(appDir).filePath(QStringLiteral("../share/MadModem/help/") + fileName),
        QDir(QDir::currentPath()).filePath(QStringLiteral("docs/help/") + fileName),
        QDir(QDir::currentPath()).filePath(QStringLiteral("help/") + fileName),
        QDir(QDir::currentPath()).filePath(fileName)
    };

    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return QString();
}

QString HelpDialog::currentUiLanguage() const
{
    QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
    return normalizedLanguageCode(settings.value(QStringLiteral("UI/language"), QStringLiteral("en")).toString());
}

QString HelpDialog::normalizedLanguageCode(const QString &code) const
{
    QString c = code.trimmed().toLower();
    if (c.startsWith(QStringLiteral("it"))) return QStringLiteral("it");
    if (c.startsWith(QStringLiteral("fr"))) return QStringLiteral("fr");
    if (c.startsWith(QStringLiteral("de"))) return QStringLiteral("de");
    if (c.startsWith(QStringLiteral("no")) || c.startsWith(QStringLiteral("nb")) || c.startsWith(QStringLiteral("nn"))) return QStringLiteral("no");
    if (c.startsWith(QStringLiteral("cs")) || c.startsWith(QStringLiteral("cz"))) return QStringLiteral("cs");
    return QStringLiteral("en");
}

QStringList HelpDialog::languageFallbackChain() const
{
    QStringList chain;
    chain << normalizedLanguageCode(m_languageCode);
    if (!chain.contains(QStringLiteral("en"))) chain << QStringLiteral("en");
    if (!chain.contains(QStringLiteral("it"))) chain << QStringLiteral("it");
    return chain;
}

QString HelpDialog::helpNamespaceForLanguage(const QString &lang) const
{
    return QStringLiteral("%1.%2").arg(QString::fromLatin1(kHelpNamespacePrefix), normalizedLanguageCode(lang));
}

QUrl HelpDialog::qthelpRootUrl(const QString &lang) const
{
    const QString l = normalizedLanguageCode(lang);
    return QUrl(QStringLiteral("qthelp://%1/doc/%2/index.html").arg(helpNamespaceForLanguage(l), l));
}

QString HelpDialog::fallbackResourceForLanguage(const QString &lang, const QString &fileName) const
{
    return QStringLiteral(":/help/%1/%2").arg(normalizedLanguageCode(lang), fileName);
}

QString HelpDialog::localizedTopicTitle(const QString &topicKey) const
{
    const QString lang = normalizedLanguageCode(m_effectiveHelpLanguage);
    static const QMap<QString, QMap<QString, QString>> titles = {
        {QStringLiteral("en"), {
            {QStringLiteral("index"), QStringLiteral("MM Help")},
            {QStringLiteral("quickstart"), QStringLiteral("Quick start")},
            {QStringLiteral("audio_cat_ptt"), QStringLiteral("Audio, CAT and PTT")},
            {QStringLiteral("waterfall"), QStringLiteral("Waterfall and markers")},
            {QStringLiteral("text_modes"), QStringLiteral("Text modes")},
            {QStringLiteral("cw"), QStringLiteral("CW / Morse")},
            {QStringLiteral("ft8_ft4"), QStringLiteral("FT4 / FT8")},
            {QStringLiteral("rotator"), QStringLiteral("Rotator")},
            {QStringLiteral("radio_telescope"), QStringLiteral("Radio Telescope")},
            {QStringLiteral("scheduler"), QStringLiteral("Scheduler")},
            {QStringLiteral("logbook_map"), QStringLiteral("Logbook and map")},
            {QStringLiteral("troubleshooting"), QStringLiteral("Troubleshooting")}
        }},
        {QStringLiteral("it"), {
            {QStringLiteral("index"), QStringLiteral("Guida MM")},
            {QStringLiteral("quickstart"), QStringLiteral("Avvio rapido")},
            {QStringLiteral("audio_cat_ptt"), QStringLiteral("Audio, CAT e PTT")},
            {QStringLiteral("waterfall"), QStringLiteral("Waterfall e marker")},
            {QStringLiteral("text_modes"), QStringLiteral("Modi testuali")},
            {QStringLiteral("cw"), QStringLiteral("CW / Morse")},
            {QStringLiteral("ft8_ft4"), QStringLiteral("FT4 / FT8")},
            {QStringLiteral("rotator"), QStringLiteral("Rotore")},
            {QStringLiteral("radio_telescope"), QStringLiteral("Radio Telescope")},
            {QStringLiteral("scheduler"), QStringLiteral("Pianificatore")},
            {QStringLiteral("logbook_map"), QStringLiteral("Logbook e mappa")},
            {QStringLiteral("troubleshooting"), QStringLiteral("Risoluzione problemi")}
        }},
        {QStringLiteral("fr"), {
            {QStringLiteral("index"), QStringLiteral("Aide MM")},
            {QStringLiteral("quickstart"), QStringLiteral("Démarrage rapide")},
            {QStringLiteral("audio_cat_ptt"), QStringLiteral("Audio, CAT et PTT")},
            {QStringLiteral("waterfall"), QStringLiteral("Waterfall et marqueurs")},
            {QStringLiteral("text_modes"), QStringLiteral("Modes texte")},
            {QStringLiteral("cw"), QStringLiteral("CW / Morse")},
            {QStringLiteral("ft8_ft4"), QStringLiteral("FT4 / FT8")},
            {QStringLiteral("rotator"), QStringLiteral("Rotateur")},
            {QStringLiteral("radio_telescope"), QStringLiteral("Radiotélescope")},
            {QStringLiteral("scheduler"), QStringLiteral("Planificateur")},
            {QStringLiteral("logbook_map"), QStringLiteral("Journal et carte")},
            {QStringLiteral("troubleshooting"), QStringLiteral("Dépannage")}
        }},
        {QStringLiteral("de"), {
            {QStringLiteral("index"), QStringLiteral("MM-Hilfe")},
            {QStringLiteral("quickstart"), QStringLiteral("Schnellstart")},
            {QStringLiteral("audio_cat_ptt"), QStringLiteral("Audio, CAT und PTT")},
            {QStringLiteral("waterfall"), QStringLiteral("Wasserfall und Marker")},
            {QStringLiteral("text_modes"), QStringLiteral("Textmodi")},
            {QStringLiteral("cw"), QStringLiteral("CW / Morse")},
            {QStringLiteral("ft8_ft4"), QStringLiteral("FT4 / FT8")},
            {QStringLiteral("rotator"), QStringLiteral("Rotor")},
            {QStringLiteral("radio_telescope"), QStringLiteral("Radioteleskop")},
            {QStringLiteral("scheduler"), QStringLiteral("Planer")},
            {QStringLiteral("logbook_map"), QStringLiteral("Logbuch und Karte")},
            {QStringLiteral("troubleshooting"), QStringLiteral("Fehlersuche")}
        }},
        {QStringLiteral("no"), {
            {QStringLiteral("index"), QStringLiteral("MM-hjelp")},
            {QStringLiteral("quickstart"), QStringLiteral("Hurtigstart")},
            {QStringLiteral("audio_cat_ptt"), QStringLiteral("Lyd, CAT og PTT")},
            {QStringLiteral("waterfall"), QStringLiteral("Waterfall og markører")},
            {QStringLiteral("text_modes"), QStringLiteral("Tekstmodi")},
            {QStringLiteral("cw"), QStringLiteral("CW / Morse")},
            {QStringLiteral("ft8_ft4"), QStringLiteral("FT4 / FT8")},
            {QStringLiteral("rotator"), QStringLiteral("Rotor")},
            {QStringLiteral("radio_telescope"), QStringLiteral("Radioteleskop")},
            {QStringLiteral("scheduler"), QStringLiteral("Planlegger")},
            {QStringLiteral("logbook_map"), QStringLiteral("Logg og kart")},
            {QStringLiteral("troubleshooting"), QStringLiteral("Feilsøking")}
        }},
        {QStringLiteral("cs"), {
            {QStringLiteral("index"), QStringLiteral("Nápověda MM")},
            {QStringLiteral("quickstart"), QStringLiteral("Rychlý start")},
            {QStringLiteral("audio_cat_ptt"), QStringLiteral("Audio, CAT a PTT")},
            {QStringLiteral("waterfall"), QStringLiteral("Waterfall a značky")},
            {QStringLiteral("text_modes"), QStringLiteral("Textové módy")},
            {QStringLiteral("cw"), QStringLiteral("CW / Morse")},
            {QStringLiteral("ft8_ft4"), QStringLiteral("FT4 / FT8")},
            {QStringLiteral("rotator"), QStringLiteral("Rotátor")},
            {QStringLiteral("radio_telescope"), QStringLiteral("Radioteleskop")},
            {QStringLiteral("scheduler"), QStringLiteral("Plánovač")},
            {QStringLiteral("logbook_map"), QStringLiteral("Deník a mapa")},
            {QStringLiteral("troubleshooting"), QStringLiteral("Řešení problémů")}
        }}
    };
    return titles.value(lang, titles.value(QStringLiteral("en"))).value(topicKey, topicKey);
}
