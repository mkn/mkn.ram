
/**
Copyright (c) 2024, Philip Deegan.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.
    * Neither the name of Philip Deegan nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef _MKN_RAM_MPI_HPP_
#define _MKN_RAM_MPI_HPP_

#include "mkn/kul/dbg.hpp"
#include "mpi.h"

namespace mkn {
namespace mpi {

class Exception : public mkn::kul::Exception {
 public:
  Exception(char const* f, uint16_t const& l, std::string const& s)
      : mkn::kul::Exception(f, l, s) {}
};

template <typename Data>
std::vector<Data> collect(Data const& data, int mpi_size = 0);

std::size_t max(std::size_t const local, int mpi_size = 0);

int size();

template <typename Data>
auto getDataType() {
  if constexpr (std::is_same_v<double, Data>)
    return MPI_DOUBLE;
  else if constexpr (std::is_same_v<float, Data>)
    return MPI_FLOAT;
  else if constexpr (std::is_same_v<int, Data>)
    return MPI_INT;
  else if constexpr (std::is_same_v<std::uint32_t, Data>)
    return MPI_UNSIGNED;
  else if constexpr (std::is_same_v<uint8_t, Data>)
    return MPI_UNSIGNED_SHORT;
  else if constexpr (std::is_same_v<std::size_t, Data>)
    return MPI_UINT64_T;
  else if constexpr (std::is_same_v<char, Data>)
    return MPI_CHAR;
  else
    throw std::runtime_error(std::string("Unhandled MPI data type collection: ") +
                             typeid(Data).name());
}

template <typename Data>
void _collect(Data const* const sendbuf, std::vector<Data>& rcvBuff,
              std::size_t const sendcount = 1, std::size_t const recvcount = 1) {
  auto mpi_type = getDataType<Data>();
  MPI_Allgather(       // MPI_Allgather
      sendbuf,         //   void         *sendbuf,
      sendcount,       //   int          sendcount,
      mpi_type,        //   MPI_Datatype sendtype,
      rcvBuff.data(),  //   void         *recvbuf,
      recvcount,       //   int          recvcount,
      mpi_type,        //   MPI_Datatype recvtype,
      MPI_COMM_WORLD   //   MPI_Comm     comm
  );
}

template <typename Data, typename SendBuff, typename RcvBuff>
void _collect_vector(SendBuff const& sendBuff, RcvBuff& rcvBuff, std::vector<int> const& recvcounts,
                     std::vector<int> const& displs, int const mpi_size) {
  std::vector<int> displs(mpi_size);
  for (int i = 0, offset = 0; i < mpi_size; i++) {
    displs[i] = offset;
    offset += recvcounts[i];
  }

  auto mpi_type = getDataType<Data>();
  MPI_Allgatherv(         // MPI_Allgatherv
      sendBuff.data(),    //   void         *sendbuf,
      sendBuff.size(),    //   int          sendcount,
      mpi_type,           //   MPI_Datatype sendtype,
      rcvBuff.data(),     //   void         *recvbuf,
      recvcounts.data(),  //   int          *recvcounts,
      displs.data(),      //   int          *displs,
      mpi_type,           //   MPI_Datatype recvtype,
      MPI_COMM_WORLD      //   MPI_Comm     comm
  );
}

template <typename Vector, typename Data = typename Vector::value_type>
std::vector<Vector> collectVector(Vector const& sendBuff, int mpi_size = 0) {
  if (mpi_size == 0) MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  std::vector<int> const perMPISize = collect(static_cast<int>(sendBuff.size()), mpi_size);
  std::vector<int> const displs = core::displacementFrom(perMPISize);
  std::vector<Data> rcvBuff(std::accumulate(perMPISize.begin(), perMPISize.end(), 0));
  _collect_vector<Data>(sendBuff, rcvBuff, perMPISize, displs, mpi_size);

  std::size_t offset = 0;
  std::vector<Vector> collected;
  for (int i = 0; i < mpi_size; i++) {
    collected.emplace_back(&rcvBuff[offset], &rcvBuff[offset] + perMPISize[i]);
    offset += perMPISize[i];
  }
  return collected;
}

template <typename T, typename Vector>
SpanSet<T, int> collectSpanSet(Vector const& sendBuff, int mpi_size = 0) {
  if (mpi_size == 0) MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  SpanSet<T, int> rcvBuff{collect(static_cast<int>(sendBuff.size()), mpi_size)};
  _collect_vector<T>(sendBuff, rcvBuff, rcvBuff.sizes(), rcvBuff.displs(), mpi_size);

  return rcvBuff;
}

template <typename Array, std::size_t size, typename Data = typename Array::value_type>
auto collectArrays(Array const& arr, int mpi_size) {
  if (mpi_size == 0) MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  std::vector<Data> datas(arr.size() * mpi_size);
  _collect(arr.data(), datas, arr.size(), arr.size());

  std::vector<Array> values(mpi_size);
  for (int i = 0; i < mpi_size; i++) std::memcpy(&values[i], &datas[arr.size() * i], arr.size());

  return values;
}

template <typename T>
SpanSet<T, int> collect_raw(std::vector<T> const& data, int mpi_size) {
  if (mpi_size == 0) MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  return collectSpanSet<T>(data, mpi_size);
}

template <typename Data>
std::vector<Data> collect(Data const& data, int mpi_size) {
  if (mpi_size == 0) MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  if constexpr (std::is_same_v<std::string, Data> or core::is_std_vector_v<Data>)
    return collectVector(data, mpi_size);
  else if constexpr (core::is_std_array_v<Data, 1>)
    return collectArrays(data, mpi_size);
  else {
    std::vector<Data> values(mpi_size);
    _collect(&data, values);
    return values;
  }
}

}  // namespace mpi
}  // namespace mkn

#endif  //_MKN_RAM_MPI_HPP_
