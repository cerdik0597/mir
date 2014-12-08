/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "src/platform/graphics/mesa/ipc_operations.h"
#include "src/platform/graphics/mesa/drm_authentication.h"
#include "mir/graphics/platform_ipc_package.h"
#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_buffer_ipc_message.h"
#include "mir_test_doubles/fd_matcher.h"
#include "mir_test_doubles/mock_drm.h"
#include <gtest/gtest.h>

namespace mg = mir::graphics;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;
namespace mgm = mir::graphics::mesa;
namespace geom = mir::geometry;

namespace
{

struct MockDRMOperations : public mgm::DRMAuthentication
{
    MOCK_METHOD1(auth_magic, void(drm_magic_t incantation));
    MOCK_METHOD0(authenticated_fd, mir::Fd());
};

struct IpcOperations : public ::testing::Test
{
    IpcOperations()
    {
        using namespace testing;
        ON_CALL(mock_buffer, native_buffer_handle())
            .WillByDefault(Return(mt::fake_shared(native_handle)));
        ON_CALL(mock_buffer, stride())
            .WillByDefault(Return(dummy_stride));
        ON_CALL(mock_buffer, size())
            .WillByDefault(Return(dummy_size));

        native_handle.data_items = 4;
        native_handle.fd_items = 2;
        for(auto i=0; i < mir_buffer_package_max; i++)
        {
            native_handle.fd[i] = i;
            native_handle.data[i] = i;
        }
    }

    MockDRMOperations mock_drm_ops;
    mgm::IpcOperations ipc_ops{mt::fake_shared(mock_drm_ops)};
    MirBufferPackage native_handle;
    testing::NiceMock<mtd::MockBuffer> mock_buffer;
    geom::Stride dummy_stride{4390};
    geom::Size dummy_size{123, 345};
    ::testing::NiceMock<mtd::MockDRM> mock_drm;
};
}

TEST_F(IpcOperations, packs_buffer_correctly)
{
    mtd::MockBufferIpcMessage mock_buffer_msg;
    for(auto i = 0; i < native_handle.fd_items; i++)
        EXPECT_CALL(mock_buffer_msg, pack_fd(mtd::RawFdMatcher(native_handle.fd[i])));
    for(auto i = 0; i < native_handle.data_items; i++)
        EXPECT_CALL(mock_buffer_msg, pack_data(native_handle.data[i]));
    EXPECT_CALL(mock_buffer_msg, pack_stride(dummy_stride));
    EXPECT_CALL(mock_buffer_msg, pack_flags(testing::_));
    EXPECT_CALL(mock_buffer_msg, pack_size(dummy_size));

    ipc_ops.pack_buffer(mock_buffer_msg, mock_buffer, mg::BufferIpcMsgType::full_msg);
    ipc_ops.pack_buffer(mock_buffer_msg, mock_buffer, mg::BufferIpcMsgType::update_msg);
}

TEST_F(IpcOperations, drm_auth_magic_called_for_platform_operation)
{
    int magic{112358};
    EXPECT_CALL(mock_drm_ops, auth_magic(magic));
    auto package = ipc_ops.platform_operation(0, mg::PlatformIPCPackage{{magic},{}});
    ASSERT_THAT(package.ipc_data.size(), testing::Eq(1));
    ASSERT_THAT(package.ipc_data[0], testing::Eq(0));
}

TEST_F(IpcOperations, gets_authentication_fd_for_connection_package)
{
    using namespace testing;
    mir::Fd stub_fd(fileno(tmpfile()));
    EXPECT_CALL(mock_drm_ops, authenticated_fd())
        .WillOnce(Return(stub_fd));
    auto connection_package = ipc_ops.connection_ipc_package();
    ASSERT_THAT(connection_package->ipc_fds.size(), Eq(1));
    EXPECT_THAT(connection_package->ipc_fds[0], mtd::RawFdMatcher(stub_fd));
}
