#include "SettingsStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {
constexpr auto kLanguage = "ui/language";
constexpr auto kMinimizeToTray = "ui/minimizeToTray";
constexpr auto kMainWindowGeometry = "ui/mainGeometry";
constexpr auto kUiTheme = "ui/theme";
constexpr auto kRemoteFileOpenMode = "remoteFiles/openMode";
constexpr auto kExternalTextEditorPath = "remoteFiles/externalTextEditorPath";
constexpr auto kAutoUploadRemoteEdits = "remoteFiles/autoUploadEdits";
constexpr auto kDefaultLanguage = "system";
constexpr auto kDefaultUiTheme = "dark";
constexpr auto kDefaultRemoteFileOpenMode = "system";
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

QString SettingsStore::uiTheme() const
{
    QSettings settings;
    return settings.value(kUiTheme, QString::fromLatin1(kDefaultUiTheme)).toString();
}

void SettingsStore::setUiTheme(const QString &theme)
{
    QSettings settings;
    settings.setValue(kUiTheme, theme);
}

QString SettingsStore::remoteFileOpenMode() const
{
    QSettings settings;
    return settings.value(kRemoteFileOpenMode, QString::fromLatin1(kDefaultRemoteFileOpenMode)).toString();
}

void SettingsStore::setRemoteFileOpenMode(const QString &mode)
{
    QSettings settings;
    settings.setValue(kRemoteFileOpenMode, mode);
}

QString SettingsStore::externalTextEditorPath() const
{
    QSettings settings;
    return settings.value(kExternalTextEditorPath).toString();
}

void SettingsStore::setExternalTextEditorPath(const QString &path)
{
    QSettings settings;
    settings.setValue(kExternalTextEditorPath, path);
}

bool SettingsStore::autoUploadRemoteEdits() const
{
    QSettings settings;
    return settings.value(kAutoUploadRemoteEdits, true).toBool();
}

void SettingsStore::setAutoUploadRemoteEdits(bool enabled)
{
    QSettings settings;
    settings.setValue(kAutoUploadRemoteEdits, enabled);
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
