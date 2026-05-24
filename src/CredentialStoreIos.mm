#include "CredentialStore.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <QString>

// iOS Keychain backend for CredentialStore.
// Uses kSecClassGenericPassword with service = "OpenShell" and account = "<id>/<field>".

namespace {

NSString *toNs(const QString &s)
{
    return [NSString stringWithUTF8String:s.toUtf8().constData()];
}

NSDictionary *baseQuery(const QString &id, const QString &field)
{
    return @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: @"OpenShell",
        (__bridge id)kSecAttrAccount: toNs(id + QStringLiteral("/") + field)
    };
}

} // namespace

bool CredentialStore::save(const QString &id, const QString &field,
                           const QString &secret, QString *error)
{
    NSDictionary *q = baseQuery(id, field);
    // Upsert: remove the prior entry then add the new value. Errors other
    // than "not found" are surfaced to the caller.
    OSStatus deleteStatus = SecItemDelete((__bridge CFDictionaryRef)q);
    if (deleteStatus != errSecSuccess && deleteStatus != errSecItemNotFound) {
        if (error) {
            *error = QString::fromUtf8("Keychain delete failed: %1").arg(deleteStatus);
        }
        return false;
    }
    if (secret.isEmpty()) {
        return true;
    }
    NSData *data = [NSData dataWithBytes:secret.toUtf8().constData()
                                  length:static_cast<NSUInteger>(secret.toUtf8().size())];
    NSMutableDictionary *add = [q mutableCopy];
    add[(__bridge id)kSecValueData] = data;
    add[(__bridge id)kSecAttrAccessible] =
        (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)add, NULL);
    if (status != errSecSuccess) {
        if (error) {
            *error = QString::fromUtf8("Keychain add failed: %1").arg(status);
        }
        return false;
    }
    return true;
}

QString CredentialStore::load(const QString &id, const QString &field)
{
    NSMutableDictionary *q = [baseQuery(id, field) mutableCopy];
    q[(__bridge id)kSecReturnData] = @YES;
    q[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)q, &result);
    if (status != errSecSuccess || !result) {
        return QString();
    }
    NSData *data = (__bridge_transfer NSData *)result;
    return QString::fromUtf8(static_cast<const char *>([data bytes]),
                             static_cast<int>([data length]));
}

void CredentialStore::remove(const QString &id, const QString &field)
{
    save(id, field, QString(), nullptr);
}
