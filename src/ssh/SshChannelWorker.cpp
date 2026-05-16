#include "SshChannelWorker.h"

SshChannelWorker::SshChannelWorker(const ConnectionProfile &profile, QObject *parent)
    : QObject(parent)
    , m_profile(profile)
{
}

SshChannelWorker::~SshChannelWorker() = default;

ConnectionProfile SshChannelWorker::profile() const
{
    return m_profile;
}

void SshChannelWorker::resizePty(int cols, int rows)
{
    Q_UNUSED(cols);
    Q_UNUSED(rows);
}
