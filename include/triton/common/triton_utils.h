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

#include <vector>
#include <string>
#include <iomanip>
#include <sstream>

namespace triton { namespace common {

//
// An ASCII table printer.
//
class TablePrinter {
  public:
    // Insert a row at the end of the table
    void insert_row(const std::vector<std::string>& row) {

      // Update max length of data items in each row
      for (size_t i = 0; i < row.size(); ++i)
      {
        if (row[i].size() > max_lens[i])
          max_lens[i] = row[i].size();
      }
      data_.emplace_back(row);
    }

    std::string& print_table() {
      std::stringstream table;
      table << "\n";

      auto append_row_divider =
          [&table](const std::vector<size_t>& col_sizes) {
            table << "+";
            for (const auto& col_size : col_sizes) {
              for (size_t i = 0; i < col_size + 2; i++) table << "-";
              table << "+";
            }
            table << "\n";
          };

      for (size_t i = 0; i < max_lens.size(); i++)
      {
        if (max_lens[i] < headers_[i].size())
          max_lens[i] = headers_[i].size();
      }

      append_row_divider(max_lens);

      table << "|" << std::left;
      for (size_t i = 0; i < max_lens.size(); i++) {
        table << " " << std::setw(max_lens[i]) << headers_[i] << " |";
      }
      table << "\n";

      append_row_divider(max_lens);

      for (size_t j = 0; j < data_[0].size(); j++) {
        table << "|" << std::left;
        for (size_t i = 0; i < max_lens.size(); i++) {
          table << " " << std::setw(max_lens[i]) << data_[i][j] << " |";
        }
        table << "\n";
      }

      append_row_divider(max_lens);

      std::string table_output = table.str();
      return table_output;
    }

    static void CreateTablePrinter(TablePrinter *table_printer, const std::vector<std::string>& headers) {
      *table_printer = TablePrinter(headers);
    }
  
  private:
    // Table headers
    std::vector<std::string> headers_;
    // Max lengths of the data items.
    std::vector<size_t> max_lens;
    // A vector of vectors containing data items for every column
    std::vector<std::vector<std::string>> data_;
    TablePrinter(const std::vector<std::string>& headers) : headers_(std::move(headers)) {
    }
};

}}  // namespace triton::common
