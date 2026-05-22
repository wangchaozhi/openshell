#pragma once

#include <QString>

// CredentialStore 把连接的敏感字段（密码、私钥口令）存进操作系统的钥匙串：
// Windows 凭据管理器 / macOS Keychain / Linux libsecret(Secret Service)。
// 仅桌面端编译（见 CMake 的 OPENSHELL_USE_KEYCHAIN）。QtKeychain 的作业是
// 异步的，这里用局部事件循环包成同步接口，方便 ConnectionCatalog 直接调用。
namespace CredentialStore
{
// 写入 secret；secret 为空表示删除该条目。失败时把错误写入 *error。
bool save(const QString &id, const QString &field, const QString &secret, QString *error = nullptr);

// 读取 secret，条目不存在时返回空字符串。
QString load(const QString &id, const QString &field);

// 删除条目（等价于 save 一个空 secret）。
void remove(const QString &id, const QString &field);
}
