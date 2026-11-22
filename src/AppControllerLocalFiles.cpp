#include "AppController.h"

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#endif

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
#include <QFileDialog>
#endif
#include <QStandardPaths>

namespace {
constexpr auto kTransferHistory = "transfers/history";
}

QString AppController::localHomePath() const
{
    return QDir::homePath();
}

QString AppController::localParentPath(const QString &path) const
{
    QDir dir(path.isEmpty() ? QDir::homePath() : path);
    dir.cdUp();
    return QDir::toNativeSeparators(dir.absolutePath());
}

QVariantList AppController::localDirectoryEntries(const QString &path) const
{
    const QString target = path.isEmpty() ? QDir::homePath() : path;
    const QDir dir(target);
    QVariantList entries;
    const QFileInfoList items = dir.entryInfoList(QDir::AllEntries
                                                      | QDir::NoDotAndDotDot
                                                      | QDir::Readable,
                                                  QDir::DirsFirst
                                                      | QDir::IgnoreCase
                                                      | QDir::Name);
    entries.reserve(items.size());
    for (const QFileInfo &item : items) {
        QVariantMap row;
        row.insert(QStringLiteral("name"), item.fileName());
        row.insert(QStringLiteral("path"), QDir::toNativeSeparators(item.absoluteFilePath()));
        row.insert(QStringLiteral("isDir"), item.isDir());
        row.insert(QStringLiteral("size"), item.isDir() ? QStringLiteral("--")
                                                        : QString::number(item.size()));
        row.insert(QStringLiteral("modified"),
                   item.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        // Windows 简化权限：映射成 Unix 数字供 QML 统一转换
        QString perm;
        if (item.isDir()) {
            perm = item.isWritable() ? QStringLiteral("755") : QStringLiteral("555");
        } else {
            perm = item.isWritable() ? QStringLiteral("644") : QStringLiteral("444");
        }
        row.insert(QStringLiteral("permissions"), perm);
        entries.append(row);
    }
    return entries;
}

QString AppController::localPathFromUrl(const QString &url) const
{
    const QUrl parsed(url);
    if (parsed.isLocalFile()) {
        return parsed.toLocalFile();
    }
    return url;
}

QString AppController::chooseLocalFile()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    // Mobile's native file picker is hosted by MobileFilesPage.qml.
    return {};
#else
    return QFileDialog::getOpenFileName(nullptr, tr("Select file to upload"), QDir::homePath());
#endif
}

QString AppController::chooseLocalFolder()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    // Mobile's native folder picker is hosted by MobileFilesPage.qml.
    return {};
#else
    return QFileDialog::getExistingDirectory(nullptr, tr("Select folder to upload"), QDir::homePath());
#endif
}

QString AppController::chooseDownloadFolder()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    // On mobile, downloads go to the app sandbox automatically. Return the
    // app data location so the caller has a usable destination.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
#else
    return QFileDialog::getExistingDirectory(nullptr, tr("Select download folder"), QDir::homePath());
#endif
}

QString stagePickedUpload(const QUrl &source, bool folder, const QTemporaryDir &sandbox)
{
    // Qt's iOS file engine resolves the security scope for picker paths when
    // QFile or QDir accesses them.
    const QFileInfo local(source.toLocalFile());
    if (source.isLocalFile() && (folder ? local.isDir() : local.isFile())) {
        return source.toLocalFile();
    }
#if defined(Q_OS_ANDROID)
    if (!sandbox.isValid()) {
        return {};
    }
    const auto context = QNativeInterface::QAndroidApplication::context();
    const auto uri = QJniObject::fromString(source.toString(QUrl::FullyEncoded));
    const auto path = QJniObject::fromString(sandbox.path());
    return QJniObject::callStaticObjectMethod("com/wangchaozhi/openshell/UploadStaging", "copy",
               "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Z)Ljava/lang/String;",
               context.object<jobject>(), uri.object<jstring>(), path.object<jstring>(),
               static_cast<jboolean>(folder)).toString();
#else
    Q_UNUSED(sandbox);
    return {};
#endif
}

bool AppController::openLocalFolderForPath(const QString &path) const
{
    if (path.trimmed().isEmpty()) {
        return false;
    }

    const QFileInfo info(path);
    const QString folder = info.exists() && info.isDir()
                               ? info.absoluteFilePath()
                               : info.absolutePath();
    if (folder.isEmpty() || !QDir(folder).exists()) {
        return false;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

QVariantList AppController::transferHistory() const
{
    QSettings settings;
    const QByteArray payload = settings.value(QString::fromLatin1(kTransferHistory)).toByteArray();
    if (payload.isEmpty()) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isArray()) {
        return {};
    }
    return document.array().toVariantList();
}

void AppController::saveTransferHistory(const QVariantList &history) const
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(kTransferHistory),
                      QJsonDocument(QJsonArray::fromVariantList(history)).toJson(QJsonDocument::Compact));
}


void AppController::copyTextToClipboard(const QString &text) const
{
    QGuiApplication::clipboard()->setText(text);
}

QString AppController::clipboardText() const
{
    return QGuiApplication::clipboard()->text();
}

