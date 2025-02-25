//
// detail/impl/win_iocp_file_service.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_IMPL_WIN_IOCP_FILE_SERVICE_IPP
#define ASIO_DETAIL_IMPL_WIN_IOCP_FILE_SERVICE_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#if defined(ASIO_HAS_FILE) \
  && defined(ASIO_HAS_WINDOWS_RANDOM_ACCESS_HANDLE)

#include <cstring>
#include <sys/stat.h>
#include "asio/detail/win_iocp_file_service.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

win_iocp_file_service::win_iocp_file_service(
    execution_context& context)
  : execution_context_service_base<win_iocp_file_service>(context),
    handle_service_(context)
{
}

void win_iocp_file_service::shutdown()
{
  handle_service_.shutdown();
}

asio::error_code win_iocp_file_service::open(
    win_iocp_file_service::implementation_type& impl,
    const char* path, file_base::flags open_flags,
    asio::error_code& ec)
{
  if (is_open(impl))
  {
    ec = asio::error::already_open;
    return ec;
  }

  DWORD access = 0;
  if ((open_flags & file_base::read_only) != 0)
    access = GENERIC_READ;
  else if ((open_flags & file_base::write_only) != 0)
    access = GENERIC_WRITE;
  else if ((open_flags & file_base::read_write) != 0)
    access = GENERIC_READ | GENERIC_WRITE;

  DWORD disposition = 0;
  if ((open_flags & file_base::create) != 0)
  {
    if ((open_flags & file_base::exclusive) != 0)
      disposition = CREATE_NEW;
    else
      disposition = OPEN_ALWAYS;
  }
  else
  {
    if ((open_flags & file_base::truncate) != 0)
      disposition = TRUNCATE_EXISTING;
    else
      disposition = OPEN_EXISTING;
  }

  DWORD flags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED;
  if (impl.is_stream_)
    flags |= FILE_FLAG_SEQUENTIAL_SCAN;
  else
    flags |= FILE_FLAG_RANDOM_ACCESS;

  HANDLE handle = ::CreateFileA(path, access, 0, 0, disposition, flags, 0);
  if (handle != INVALID_HANDLE_VALUE)
  {
    if (disposition == OPEN_ALWAYS && (open_flags & file_base::truncate) != 0)
    {
      if (!::SetEndOfFile(handle))
      {
        DWORD last_error = ::GetLastError();
        ::CloseHandle(handle);
        ec.assign(last_error, asio::error::get_system_category());
        return ec;
      }
    }

    handle_service_.assign(impl, handle, ec);
    if (ec)
      ::CloseHandle(handle);
    impl.offset_ = 0;
    return ec;
  }
  else
  {
    DWORD last_error = ::GetLastError();
    ec.assign(last_error, asio::error::get_system_category());
    return ec;
  }
}

uint64_t win_iocp_file_service::size(
    const win_iocp_file_service::implementation_type& impl,
    asio::error_code& ec) const
{
  LARGE_INTEGER result;
  if (::GetFileSizeEx(native_handle(impl), &result))
  {
    ec.assign(0, ec.category());
    return static_cast<uint64_t>(result.QuadPart);
  }
  else
  {
    DWORD last_error = ::GetLastError();
    ec.assign(last_error, asio::error::get_system_category());
    return 0;
  }
}

asio::error_code win_iocp_file_service::resize(
    win_iocp_file_service::implementation_type& impl,
    uint64_t n, asio::error_code& ec)
{
  LARGE_INTEGER distance;
  distance.QuadPart = n;
  if (::SetFilePointerEx(native_handle(impl), distance, 0, FILE_BEGIN))
  {
    BOOL result = ::SetEndOfFile(native_handle(impl));
    DWORD last_error = ::GetLastError();

    distance.QuadPart = static_cast<LONGLONG>(impl.offset_);
    if (!::SetFilePointerEx(native_handle(impl), distance, 0, FILE_BEGIN))
    {
      result = FALSE;
      last_error = ::GetLastError();
    }

    if (result)
      ec.assign(0, ec.category());
    else
      ec.assign(last_error, asio::error::get_system_category());
    return ec;
  }
  else
  {
    DWORD last_error = ::GetLastError();
    ec.assign(last_error, asio::error::get_system_category());
    return ec;
  }
}

uint64_t win_iocp_file_service::seek(
    win_iocp_file_service::implementation_type& impl, int64_t offset,
    file_base::seek_basis whence, asio::error_code& ec)
{
  DWORD method;
  switch (whence)
  {
  case file_base::seek_set:
    method = FILE_BEGIN;
    break;
  case file_base::seek_cur:
    method = FILE_CURRENT;
    break;
  case file_base::seek_end:
    method = FILE_END;
    break;
  default:
    ec = asio::error::invalid_argument;
    return 0;
  }

  LARGE_INTEGER distance, new_offset;
  distance.QuadPart = offset;
  if (::SetFilePointerEx(native_handle(impl), distance, &new_offset, method))
  {
    impl.offset_ = new_offset.QuadPart;
    ec.assign(0, ec.category());
    return impl.offset_;
  }
  else
  {
    DWORD last_error = ::GetLastError();
    ec.assign(last_error, asio::error::get_system_category());
    return 0;
  }
}

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // defined(ASIO_HAS_FILE)
       //   && defined(ASIO_HAS_WINDOWS_RANDOM_ACCESS_HANDLE)

#endif // ASIO_DETAIL_IMPL_WIN_IOCP_FILE_SERVICE_IPP
