// Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace triton { namespace common {

//
// An ASCII table printer.
//
class TablePrinter {
 public:
  // Insert a row at the end of the table
  void InsertRow(const std::vector<std::string>& row);

  // Print the table
  std::string PrintTable();

  // TablePrinter will take the ownership of `headers`.
  TablePrinter(const std::vector<std::string>& headers);

 private:
  // Update the `shares_` such that all the excess
  // amount of space not used a column is fairly allocated
  // to the other columns
  void FairShare();

  // Append a row to `table`. This function handles the cases where a wrapping
  // occurs.
  void AddRow(std::stringstream& table, size_t row_index);

  // Add a row divider
  void AddRowDivider(std::stringstream& table);

  // Max row width
  std::vector<size_t> max_widths_;

  // Max row height
  std::vector<size_t> max_heights_;

  // A vector of vectors of vectors containing data items for every column
  // The record is stored in a vector of string, where each of the vector items
  // contains a single line from the record. For example, ["Item 1", "Item 2",
  // "Item 3\n Item 3 line 2"] will be stored as [["Item 1"], ["Item 2"], ["Item
  // 3", "Item 3 line 2"]]
  std::vector<std::vector<std::vector<std::string>>> data_;

  // Fair share of every column
  std::vector<float> shares_;
};

}}  // namespace triton::common
