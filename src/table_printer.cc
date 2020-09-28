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

#include "triton/common/table_printer.h"

#include <algorithm>
#include <iomanip>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>
#include <iostream>

namespace triton { namespace common {

//
// ASCII table printer.
//
void TablePrinter::InsertRow(const std::vector<std::string> &row) {
  // Update max length of data items in each row
  for (size_t i = 0; i < row.size(); ++i) {
    if (row[i].size() > max_lens_[i])
      max_lens_[i] = row[i].size();
  }
  data_.emplace_back(row);
}

void TablePrinter::FairShare() {
  // initialize original index locations
  size_t array_size = max_lens_.size();
  std::vector<size_t> idx(array_size);
  iota(idx.begin(), idx.end(), 0);

  stable_sort(idx.begin(), idx.end(), [this](size_t i1, size_t i2) {
    return this->max_lens_[i1] < this->max_lens_[i2];
  });

  size_t loop_index = 1;
  for (auto itr = idx.begin(); itr != idx.end(); ++itr) {
    // If a column is not using all the space allocated to it
    if (max_lens_[*itr] < shares_[*itr]) {
      float excess = shares_[*itr] - max_lens_[*itr];
      shares_[*itr] -= excess;

      if (itr == idx.end() - 1)
        break;
      auto update_itr = idx.begin() + (itr - idx.begin() + 1);

      // excess amount of unused space that must be distributed evenly to the
      // next columns
      float excess_per_column = excess / (array_size - loop_index);

      for (; update_itr != idx.end(); ++update_itr) {
        shares_[*update_itr] += excess_per_column;
        excess -= excess_per_column;
      }
    }
    ++loop_index;
  }

  // Remove any decimal shares
  for (auto itr = idx.begin(); itr != idx.end(); ++itr) {
    shares_[*itr] = (size_t)shares_[*itr];
  }
}

void TablePrinter::AddRow(std::stringstream &table,
                            const std::vector<std::string> &row) {
  size_t max_rows = 0;

  for (size_t i = 0; i < row.size(); i++) {
    size_t current_rows = (row[i].size() + shares_[i] - 1) / shares_[i];
    if (max_rows < current_rows)
      max_rows = current_rows;
  }
  for (size_t j = 0; j < max_rows; j++) {
    table << "|" << std::left;

    for (size_t i = 0; i < row.size(); i++) {
      size_t start_index = std::min((size_t)(j * shares_[i]), row[i].size());
      size_t end_index =
          std::min((size_t)((j + 1) * shares_[i]), row[i].size());

      // Print part of the column that fits into the fair share of that column
      table << " " << std::setw(shares_[i])
            << row[i].substr(start_index, end_index - start_index) << " |";
    }

    // Do not add new line if this is the last row of this record
    if (j != max_rows - 1)
      table << "\n";
  }
  table << "\n";
}

void TablePrinter::AddRowDivider(std::stringstream &table) {
  table << "+";
  for (const auto &share : shares_) {
    for (size_t i = 0; i < share + 2; i++)
      table << "-";
    table << "+";
  }
  table << "\n";
}

std::string TablePrinter::PrintTable() {
  std::stringstream table;
  table << "\n";

  for (size_t i = 0; i < max_lens_.size(); i++) {
    if (this->max_lens_[i] < headers_[i].size())
      max_lens_[i] = headers_[i].size();
  }

  FairShare();

  AddRowDivider(table);
  AddRow(table, headers_);
  AddRowDivider(table);

  for (size_t j = 0; j < data_.size(); j++) {
    AddRow(table, data_[j]);
  }

  AddRowDivider(table);

  return std::move(table.str());
}

// TablePrinter will take the ownership of `headers`.
TablePrinter::TablePrinter(const std::vector<std::string> &headers)
    : headers_(std::move(headers)) {
  int status = ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminal_size_);
  if (status != 0 || terminal_size_.ws_col == 0) {
    // Failed to get output size
    // Set the column size to a default size
    terminal_size_.ws_col = 500;
  }

  for (size_t i = 0; i < headers_.size(); ++i) {
    max_lens_.emplace_back(0);
  }

  // Calculate fair share of every column
  size_t number_of_columns = headers_.size();

  // Terminal width is the actual terminal width minus two times spaces
  // required before and after each column and number of columns plus 1 for
  // the pipes between the columns
  size_t terminal_width =
      terminal_size_.ws_col - (2 * number_of_columns) - (number_of_columns + 1);
  int equal_share = terminal_width / headers_.size();

  for (size_t i = 0; i < headers_.size(); ++i) {
    shares_.emplace_back(equal_share);
    terminal_width -= equal_share;
  }
}

} // namespace common
} // namespace triton
