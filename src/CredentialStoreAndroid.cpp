#include "CredentialStore.h"

#include <QJniObject>
#include <QObject>
#include <QSettings>

namespace {

QString settingsKey(const QString &id, const QString &field)
{
    return QStringLiteral("credentials/") + id + QLatin1Char('/') + field;
}

QString crypt(const char *method, const QString &value, const QString &key)
{
    const auto input = QJniObject::fromString(value);
    const auto field = QJniObject::fromString(key);
    return QJniObject::callStaticObjectMethod("com/wangchaozhi/openshell/CredentialCrypto", method,
                                            "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                                            input.object<jstring>(), field.object<jstring>()).toString();
}

} // namespace

bool CredentialStore::save(const QString &id, const QString &field,
                           const QString &secret, QString *error)
{
    QSettings settings;
    const QString key = settingsKey(id, field);
    if (secret.isEmpty()) {
        settings.remove(key);
    } else {
        const QString sealed = crypt("seal", secret, key);
        if (sealed.isEmpty()) {
            if (error) {
                *error = QObject::tr("Cannot encrypt credentials with Android Keystore");
            }
            return false;
        }
        settings.setValue(key, sealed);
    }
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (error) {
            *error = QObject::tr("Cannot save Android credentials");
        }
        return false;
    }
    return true;
}

QString CredentialStore::load(const QString &id, const QString &field)
{
    const QString key = settingsKey(id, field);
    const QString value = QSettings().value(key).toString();
    if (value.startsWith(QStringLiteral("v1:"))) {
        return crypt("unseal", value, key);
    }
    if (!value.isEmpty()) {
        const QString legacy = QString::fromUtf8(QByteArray::fromBase64(value.toLatin1()));
        if (!legacy.isEmpty()) {
            save(id, field, legacy, nullptr);
        }
        return legacy;
    }
    return {};
}

void CredentialStore::remove(const QString &id, const QString &field)
{
    save(id, field, QString(), nullptr);
}
