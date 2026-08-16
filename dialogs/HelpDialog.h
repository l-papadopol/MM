#ifndef HELPDIALOG_H
#define HELPDIALOG_H

#include <QDialog>
#include <QUrl>

class QTextBrowser;
class QTreeWidget;
class QTreeWidgetItem;
class QSplitter;
class QPushButton;
class QLineEdit;

#ifdef MADMODEM_WITH_QT_HELP
class QHelpEngine;
class QHelpContentWidget;
class QHelpIndexWidget;
class QTabWidget;
#endif

/**
 * @brief Old-school two-pane MM help browser backed by localized Qt Help.
 *
 * v2.74 keeps the manual as multilingual HTML sources and optional per-language
 * Qt Help .qch files.  The dialog automatically follows the UI language saved
 * in settings.mad and falls back in this order: selected language -> English ->
 * Italian.  The same HTML pages are embedded in resources.qrc, so the Help
 * window is useful even when qhelpgenerator or QtHelp are not available.
 */
class HelpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr, const QUrl &startUrl = QUrl());
    ~HelpDialog() override;

private:
    void setupUiShell();
    bool setupQtHelpBackend(const QUrl &startUrl);
    void setupFallbackBackend(const QUrl &startUrl);
    void openUrl(const QUrl &url);
    void showFallbackTopic(const QString &resourcePath);
    void filterFallbackTopics(const QString &text);
    QString locateHelpFile(const QString &fileName) const;
    QString currentUiLanguage() const;
    QString normalizedLanguageCode(const QString &code) const;
    QStringList languageFallbackChain() const;
    QString helpNamespaceForLanguage(const QString &lang) const;
    QUrl qthelpRootUrl(const QString &lang) const;
    QString fallbackResourceForLanguage(const QString &lang, const QString &fileName) const;
    QString localizedTopicTitle(const QString &topicKey) const;

    QSplitter *m_splitter = nullptr;
    QTextBrowser *m_viewer = nullptr;
    QPushButton *m_homeButton = nullptr;
    QPushButton *m_backButton = nullptr;
    QPushButton *m_forwardButton = nullptr;
    QPushButton *m_whatsThisButton = nullptr;
    QLineEdit *m_searchEdit = nullptr;

#ifdef MADMODEM_WITH_QT_HELP
    QHelpEngine *m_helpEngine = nullptr;
    QHelpContentWidget *m_contentWidget = nullptr;
    QHelpIndexWidget *m_indexWidget = nullptr;
    QTabWidget *m_leftTabs = nullptr;
#endif

    QTreeWidget *m_fallbackTree = nullptr;
    QString m_languageCode = QStringLiteral("en");
    QString m_effectiveHelpLanguage = QStringLiteral("en");
};

#endif // HELPDIALOG_H
