#include "AppController.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QUrl>

#include <QFileDialog>
#include <QStandardPaths>
#include <QPointer>

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
    // Sync path-returning API is incompatible with SAF / UIDocumentPicker which
    // hand back content streams asynchronously. Mobile code paths must use
    // pickMobileFileAsync() + the mobileFilePicked signal instead.
    setLastError(tr("Use pickMobileFileAsync on mobile platforms"));
    return {};
#else
    return QFileDialog::getOpenFileName(nullptr, tr("Select file to upload"), QDir::homePath());
#endif
}

QString AppController::chooseLocalFolder()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    // Folder pickers map to SAF tree URIs on Android and security-scoped tree
    // bookmarks on iOS — neither expose a real filesystem path that the rest
    // of the SFTP upload code can walk via QDir. Tracked separately.
    setLastError(tr("Folder upload from mobile is not yet supported (needs SAF tree walker)"));
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

void AppController::pickMobileFileAsync()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    QPointer<AppController> self(this);
    QFileDialog::getOpenFileContent(
        QStringLiteral("*"),
        [self](const QString &fileName, const QByteArray &content) {
            if (!self) {
                return;
            }
            if (fileName.isEmpty() || content.isEmpty()) {
                emit self->mobileFilePicked({}, tr("File picker cancelled or empty selection"));
                return;
            }
            const QString sandbox =
                QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            QDir().mkpath(sandbox);
            const QString destPath = QDir(sandbox).filePath(QFileInfo(fileName).fileName());
            QFile out(destPath);
            if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                emit self->mobileFilePicked({}, tr("Cannot stage picked file to %1").arg(destPath));
                return;
            }
            if (out.write(content) != content.size()) {
                out.close();
                emit self->mobileFilePicked({}, tr("Short write while staging %1").arg(destPath));
                return;
            }
            out.close();
            emit self->mobileFilePicked(destPath, QString());
        });
#else
    // Desktop fallback: synchronous picker, emit immediately so QML can use the
    // same signal-based flow as mobile.
    const QString path = QFileDialog::getOpenFileName(nullptr, tr("Select file to upload"),
                                                      QDir::homePath());
    if (path.isEmpty()) {
        emit mobileFilePicked({}, tr("File picker cancelled"));
    } else {
        emit mobileFilePicked(path, QString());
    }
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

