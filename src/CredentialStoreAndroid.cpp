#include "CredentialStore.h"

#include <QByteArray>
#include <QSettings>
#include <QStandardPaths>

// Android backend for CredentialStore.
//
// FIXME: This is a sandbox-only fallback that base64-wraps secrets into the
// app-private QSettings store. App-private storage is protected by Android's
// per-app sandbox, but it is NOT encrypted at rest. Anyone with a rooted
// device or a backup containing the app's sandbox can read these values.
//
// The correct implementation is to wrap each secret with an AES key generated
// in the AndroidKeyStore via QJniObject (see KeyGenParameterSpec.Builder with
// PURPOSE_ENCRYPT|PURPOSE_DECRYPT and BLOCK_MODE_GCM), storing only the IV +
// ciphertext in QSettings. That requires wiring AndroidX Security or a hand-
// rolled JNI stack and is tracked separately. This stub keeps the API working
// so the rest of the app compiles and stops storing secrets in JSON.

namespace {

QString settingsGroup()
{
    return QStringLiteral("credentials");
}

QString settingsKey(const QString &id, const QString &field)
{
    return settingsGroup() + QStringLiteral("/") + id + QStringLiteral("/") + field;
}

} // namespace

bool CredentialStore::save(const QString &id, const QString &field,
                           const QString &secret, QString *error)
{
    Q_UNUSED(error);
    QSettings settings;
    const QString key = settingsKey(id, field);
    if (secret.isEmpty()) {
        settings.remove(key);
        return true;
    }
    // Base64 to keep INI/XML serialization happy with non-ASCII bytes.
    settings.setValue(key, secret.toUtf8().toBase64());
    return true;
}

QString CredentialStore::load(const QString &id, const QString &field)
{
    QSettings settings;
    const QVariant raw = settings.value(settingsKey(id, field));
    if (!raw.isValid()) {
        return QString();
    }
    const QByteArray decoded = QByteArray::fromBase64(raw.toByteArray());
    return QString::fromUtf8(decoded);
}

void CredentialStore::remove(const QString &id, const QString &field)
{
    save(id, field, QString(), nullptr);
}
