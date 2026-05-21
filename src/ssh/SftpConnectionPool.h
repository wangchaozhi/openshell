#pragma once

#include "ConnectionCatalog.h"

#include <QString>

#include <libssh2.h>
#include <libssh2_sftp.h>

// Pooled, cached libssh2 SSH/SFTP connections keyed by connection profile + lane.
// A lane keeps unrelated kinds of work (directory browsing, file transfer, command
// execution) on separate connection sets so a long transfer never blocks a refresh.
namespace SftpConnectionPool {

enum class Lane
{
    Browse,
    Transfer,
    Exec
};

// Opaque cached connection; defined in the .cpp. Callers only hold a pointer and
// reach the libssh2 handles through session()/sftp().
struct CachedConnection;

// RAII lease that owns the per-connection lock for as long as it is alive.
class Lease
{
public:
    Lease() = default;
    Lease(CachedConnection *connection, bool alreadyLocked);
    ~Lease();

    Lease(const Lease &) = delete;
    Lease &operator=(const Lease &) = delete;

    Lease(Lease &&other) noexcept;
    Lease &operator=(Lease &&other) noexcept;

    CachedConnection *get() const { return m_connection; }

private:
    CachedConnection *m_connection = nullptr;
};

// Initialises libssh2 once per process.
bool ensureLibssh2(QString *errorOut);

// Leases a connection for the given profile/lane. Blocks only on the cache map,
// then on the chosen connection's own mutex.
Lease acquire(const ConnectionProfile &profile, Lane lane);

// Ensures the leased connection has a live session + SFTP channel, reconnecting
// if necessary.
bool ensureConnected(CachedConnection *connection,
                     const ConnectionProfile &profile,
                     QString *errorOut);

// Tears down the session/socket so the next ensureConnected() rebuilds it.
void reset(CachedConnection *connection);

LIBSSH2_SESSION *session(CachedConnection *connection);
LIBSSH2_SFTP *sftp(CachedConnection *connection);

// Human-readable last libssh2 error for the connection's session.
QString lastError(CachedConnection *connection);

} // namespace SftpConnectionPool
