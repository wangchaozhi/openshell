#include "SettingsStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {
constexpr auto kLanguage = "ui/language";
constexpr auto kMinimizeToTray = "ui/minimizeToTray";
constexpr auto kMainWindowGeometry = "ui/mainGeometry";
constexpr auto kDefaultLanguage = "system";
} // namespace

SettingsStore::SettingsStore(QObject *parent)
    : QObject(parent)
{
    // Organization/application names are set in main() before any settings access.
}

QString SettingsStore::language() const
{
    QSettings settings;
    return settings.value(kLanguage, QString::fromLatin1(kDefaultLanguage)).toString();
}

void SettingsStore::setLanguage(const QString &language)
{
    QSettings settings;
    settings.setValue(kLanguage, language);
}

bool SettingsStore::minimizeToTray() const
{
    QSettings settings;
    return settings.value(kMinimizeToTray, true).toBool();
}

void SettingsStore::setMinimizeToTray(bool enabled)
{
    QSettings settings;
    settings.setValue(kMinimizeToTray, enabled);
}

QRect SettingsStore::mainWindowGeometry() const
{
    QSettings settings;
    return settings.value(kMainWindowGeometry, QRect()).toRect();
}

void SettingsStore::setMainWindowGeometry(const QRect &geometry)
{
    QSettings settings;
    settings.setValue(kMainWindowGeometry, geometry);
}

QString SettingsStore::connectionsRoot() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dir = QDir(base).filePath(QStringLiteral("connections"));
    QDir().mkpath(dir);
    return dir;
}
