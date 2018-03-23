// Copyright 2010-2016, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef MOZC_DICTIONARY_USER_POS_H_
#define MOZC_DICTIONARY_USER_POS_H_

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/port.h"
#include "base/serialized_string_array.h"
#include "base/string_piece.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/user_pos_interface.h"

namespace mozc {
namespace dictionary {

// This implementation of UserPOSInterface uses a sorted array of tokens to
// efficiently lookup required data.  There are two required data, string array
// and token array, which are generated by ./gen_user_pos_data.py.
//
// * Prerequisite
// Little endian is assumed.
//
// * Binary format
//
// ** String array
// All the strings, such as key and value suffixes, are serialized into one
// array using SerializedStringArray in such a way that array is sorted in
// ascending order.  In the token array (see below), every string is stored as
// an index to this array.
//
// ** Token array
//
// The token array is an array of 8 byte blocks each of which has the following
// layout:
//
// Token layout (8 bytes)
// +---------------------------------------+
// | POS index  (2 bytes)                  |
// + - - - - - - - - - - - - - - - - - - - +
// | Value suffix index (2 bytes)          |
// + - - - - - - - - - - - - - - - - - - - +
// | Key suffix index  (2 bytes)           |
// + - - - - - - - - - - - - - - - - - - - +
// | Conjugation ID (2 bytes)              |
// +---------------------------------------+
//
// The array is sorted in ascending order of POS index so that we can use binary
// search to lookup necessary information efficiently.  Note that there are
// tokens having the same POS index.
class UserPOS : public UserPOSInterface {
 public:
  static const size_t kTokenByteLength = 8;

  class iterator
      : public std::iterator<std::random_access_iterator_tag, uint16> {
   public:
    iterator() = default;
    explicit iterator(const char *ptr) : ptr_(ptr) {}
    iterator(const iterator &x) = default;

    uint16 pos_index() const {
      return *reinterpret_cast<const uint16 *>(ptr_);
    }
    uint16 value_suffix_index() const {
      return *reinterpret_cast<const uint16 *>(ptr_ + 2);
    }
    uint16 key_suffix_index() const {
      return *reinterpret_cast<const uint16 *>(ptr_ + 4);
    }
    uint16 conjugation_id() const {
      return *reinterpret_cast<const uint16 *>(ptr_ + 6);
    }

    uint16 operator*() const { return pos_index(); }

    void swap(iterator &x) {
      using std::swap;
      swap(ptr_, x.ptr_);
    }

    friend void swap(iterator &x, iterator &y) { x.swap(y); }

    iterator &operator++() {
      ptr_ += kTokenByteLength;
      return *this;
    }

    iterator operator++(int) {
      const char *tmp = ptr_;
      ptr_ += kTokenByteLength;
      return iterator(tmp);
    }

    iterator &operator--() {
      ptr_ -= kTokenByteLength;
      return *this;
    }

    iterator operator--(int) {
      const char *tmp = ptr_;
      ptr_ -= kTokenByteLength;
      return iterator(tmp);
    }

    iterator &operator+=(difference_type n) {
      ptr_ += n * kTokenByteLength;
      return *this;
    }

    iterator &operator-=(difference_type n) {
      ptr_ -= n * kTokenByteLength;
      return *this;
    }

    friend iterator operator+(iterator x, difference_type n) {
      return iterator(x.ptr_ + n * kTokenByteLength);
    }

    friend iterator operator+(difference_type n, iterator x) {
      return iterator(x.ptr_ + n * kTokenByteLength);
    }

    friend iterator operator-(iterator x, difference_type n) {
      return iterator(x.ptr_ - n * kTokenByteLength);
    }

    friend difference_type operator-(iterator x, iterator y) {
      return (x.ptr_ - y.ptr_) / kTokenByteLength;
    }

    friend bool operator==(iterator x, iterator y) { return x.ptr_ == y.ptr_; }
    friend bool operator!=(iterator x, iterator y) { return x.ptr_ != y.ptr_; }
    friend bool operator<(iterator x, iterator y) { return x.ptr_ < y.ptr_; }
    friend bool operator<=(iterator x, iterator y) { return x.ptr_ <= y.ptr_; }
    friend bool operator>(iterator x, iterator y) { return x.ptr_ > y.ptr_; }
    friend bool operator>=(iterator x, iterator y) { return x.ptr_ >= y.ptr_; }

   private:
    const char *ptr_ = nullptr;
  };

  using const_iterator = iterator;

  static UserPOS *CreateFromDataManager(const DataManagerInterface &manager);

  // Initializes the user pos from the given binary data.  The provided byte
  // data must outlive this instance.
  UserPOS(StringPiece token_array_data, StringPiece string_array_data);
  ~UserPOS() override;

  // Implementation of UserPOSInterface.
  void GetPOSList(std::vector<string> *pos_list) const override;
  bool IsValidPOS(const string &pos) const override;
  bool GetPOSIDs(const string &pos, uint16 *id) const override;
  bool GetTokens(const string &key, const string &value, const string &pos,
                 std::vector<Token> *tokens) const override;

  iterator begin() const { return iterator(token_array_data_.data()); }
  iterator end() const {
    return iterator(token_array_data_.data() + token_array_data_.size());
  }

 private:
  StringPiece token_array_data_;
  SerializedStringArray string_array_;

  DISALLOW_COPY_AND_ASSIGN(UserPOS);
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_USER_POS_H_
