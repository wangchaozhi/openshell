#include "CredentialStore.h"

#include <keychain.h>

#include <QEventLoop>

namespace {

// 钥匙串里的服务名；每个连接的字段用 "<id>/<field>" 作为 key。
const QString kService = QStringLiteral("OpenShell");

QString keyFor(const QString &id, const QString &field)
{
    return id + QLatin1Char('/') + field;
}

// 跑一个局部事件循环，把 QtKeychain 的异步作业等成同步。
void runJob(QKeychain::Job *job)
{
    QEventLoop loop;
    QObject::connect(job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job->start();
    loop.exec();
}

} // namespace

bool CredentialStore::save(const QString &id, const QString &field,
                           const QString &secret, QString *error)
{
    const QString key = keyFor(id, field);

    QKeychain::Job *job = nullptr;
    if (secret.isEmpty()) {
        auto *del = new QKeychain::DeletePasswordJob(kService);
        del->setKey(key);
        job = del;
    } else {
        auto *write = new QKeychain::WritePasswordJob(kService);
        write->setKey(key);
        write->setTextData(secret);
        job = write;
    }
    job->setAutoDelete(false);
    runJob(job);

    const QKeychain::Error err = job->error();
    const QString message = job->errorString();
    job->deleteLater();

    // 删除一个本来就不存在的条目不算失败。
    if (err == QKeychain::NoError || err == QKeychain::EntryNotFound) {
        return true;
    }
    if (error) {
        *error = message;
    }
    return false;
}

QString CredentialStore::load(const QString &id, const QString &field)
{
    QKeychain::ReadPasswordJob job(kService);
    job.setAutoDelete(false);
    job.setKey(keyFor(id, field));
    runJob(&job);

    if (job.error() == QKeychain::NoError) {
        return job.textData();
    }
    return QString();
}

void CredentialStore::remove(const QString &id, const QString &field)
{
    save(id, field, QString(), nullptr);
}
