#pragma once
#include <QDialog>
#include "../settings/AppSettings.h"
class CwTxSettingsDialog final : public QDialog {
public:
    explicit CwTxSettingsDialog(const AppSettings& settings, QWidget* parent = nullptr);
    const AppSettings& resultSettings() const { return m_result; }
private:
    AppSettings m_result;
};
