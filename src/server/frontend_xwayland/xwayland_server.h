/*
 * Copyright (C) 2018 Marius Gripsgard <marius@ubports.com>
 * Copyright (C) 2019-2020 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MIR_FRONTEND_XWAYLAND_SERVER_H
#define MIR_FRONTEND_XWAYLAND_SERVER_H

#include "mir/fd.h"

#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

namespace mir
{
namespace dispatch
{
class ReadableFd;
class ThreadedDispatcher;
class MultiplexingDispatchable;
} /*dispatch */
namespace frontend
{
class WaylandConnector;
class XWaylandWM;

class XWaylandServer
{
public:
    XWaylandServer(std::shared_ptr<WaylandConnector> wayland_connector, std::string const& xwayland_path);
    ~XWaylandServer();

    auto x11_display() const -> std::string;

private:
    /// Kills the X server (called by the destructor)
    void stop();
    /// Forks off the XWayland process
    void spawn();
    /// Called after fork() if we should turn into XWayland
    void execl_xwayland(int wl_client_client_fd, int wm_client_fd);
    /// Called after fork() if we should continue on as Mir
    void connect_wm_to_xwayland(
        Fd const& wl_client_server_fd,
        Fd const& wm_server_fd,
        std::unique_lock<std::mutex>& spawn_thread_lock);
    void new_spawn_thread();

    struct SocketFd
    {
        int const xdisplay;
        std::vector<Fd> fd;

        SocketFd();
        ~SocketFd();
    };

    enum Status {
        STARTING = 1,
        RUNNING = 2,
        STOPPED = -1,
        FAILED = -2
    };

    std::shared_ptr<WaylandConnector> const wayland_connector;
    std::shared_ptr<dispatch::MultiplexingDispatchable> const dispatcher;
    std::unique_ptr<dispatch::ThreadedDispatcher> const xserver_thread;
    SocketFd const sockets;
    std::vector<std::shared_ptr<dispatch::ReadableFd>> dispatcher_fd;
    std::string const xwayland_path;

    struct
    {
        std::mutex mutex;
        std::thread thread;
    } spawn_thread;

    struct
    {
        std::mutex mutex;
        pid_t pid;
        Status status{Status::STOPPED};
        bool stopped_by_us{false};
    } xserver;

    struct
    {
        std::mutex mutex;
        std::unique_ptr<XWaylandWM> wm; ///< can be null
    } window_manager;
};
} /* frontend */
} /* mir */

#endif /* end of include guard: MIR_FRONTEND_XWAYLAND_SERVER_H */
